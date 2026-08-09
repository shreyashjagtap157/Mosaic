#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C, os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'))
MODEL=ROOT/'fixtures/packs/model-v2.mpack'; UNI=ROOT/'fixtures/packs/unicode17-v1.mpack'
LANG=ROOT/'fixtures/packs/language'; DET=ROOT/'fixtures/packs/detector/reference-v1.mpack'; BAD=ROOT/'fixtures/packs/malformed-detector'
P8=C.POINTER(C.c_uint8); P32=C.POINTER(C.c_uint32)
class Detection(C.Structure):
    _fields_=[('matched',C.c_uint32),('available',C.c_uint32),('score',C.c_int64),('margin',C.c_int64),('language',C.c_char*64)]

def setup():
    l=C.CDLL(str(LIB))
    l.mosaic_detector_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];l.mosaic_detector_load_file.restype=C.c_int
    l.mosaic_detector_detect.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(Detection)];l.mosaic_detector_detect.restype=C.c_int
    l.mosaic_detector_free.argtypes=[C.c_void_p]
    l.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_load_files.restype=C.c_int
    l.mosaic_tokenizer_add_language_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_add_language_file.restype=C.c_int
    l.mosaic_tokenizer_set_detector_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_set_detector_file.restype=C.c_int
    l.mosaic_tokenizer_detector_loaded.argtypes=[C.c_void_p];l.mosaic_tokenizer_detector_loaded.restype=C.c_int
    l.mosaic_tokenizer_detect_language.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(Detection)];l.mosaic_tokenizer_detect_language.restype=C.c_int
    l.mosaic_tokenizer_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_tokenizer_encode.restype=C.c_int
    l.mosaic_tokenizer_encode_auto.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t),C.POINTER(Detection)];l.mosaic_tokenizer_encode_auto.restype=C.c_int
    l.mosaic_tokenizer_decode.argtypes=[C.c_void_p,P32,C.c_size_t,C.POINTER(P8),C.POINTER(C.c_size_t)];l.mosaic_tokenizer_decode.restype=C.c_int
    l.mosaic_tokenizer_fingerprint.argtypes=[C.c_void_p,P8];l.mosaic_tokenizer_fingerprint.restype=C.c_int
    l.mosaic_tokenizer_free.argtypes=[C.c_void_p];l.mosaic_free.argtypes=[C.c_void_p]
    return l

def ptr(data):
    if not data:return None,None
    a=(C.c_uint8*len(data)).from_buffer_copy(data);return a,C.cast(a,P8)
def detect(l,d,data):
    a,p=ptr(data);o=Detection();assert l.mosaic_detector_detect(d,p,len(data),C.byref(o))==0
    return o
def load_tok(l,langs=(),detector=True):
    t=C.c_void_p();assert l.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNI),C.byref(t))==0
    for tag in langs: assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{tag}-v1.mpack'))==0
    if detector: assert l.mosaic_tokenizer_set_detector_file(t,os.fsencode(DET))==0
    return t
def enc(l,t,data,auto=False):
    a,p=ptr(data);ids=P32();n=C.c_size_t();d=Detection()
    st=(l.mosaic_tokenizer_encode_auto(t,p,len(data),C.byref(ids),C.byref(n),C.byref(d)) if auto else
        l.mosaic_tokenizer_encode(t,p,len(data),C.byref(ids),C.byref(n)))
    assert st==0
    out=[ids[i] for i in range(n.value)];l.mosaic_free(ids);return out,d
def fp(l,t):
    x=(C.c_uint8*32)();assert l.mosaic_tokenizer_fingerprint(t,x)==0;return bytes(x)

def main():
    l=setup();d=C.c_void_p();assert l.mosaic_detector_load_file(os.fsencode(DET),C.byref(d))==0
    try:
        cases=[(b'tokenizer','en'),('नमस्ते दुनिया'.encode(),'hi'),('こんにちは世界'.encode(),'ja')]
        for text,tag in cases:
            got=detect(l,d,text);assert got.matched==1 and got.language.decode()==tag and got.score>=100 and got.margin>=20
        for text in [b'',b'xyz',b'tokenizer '+ 'नमस्ते'.encode()+ 'こんにちは'.encode()]:
            got=detect(l,d,text); assert got.matched==0, (text,got.language,got.score,got.margin)
    finally:l.mosaic_detector_free(d)

    expected={'en':([263,264],[271]),'hi':([265,272],[273]),'ja':([267,266],[274])}
    texts={'en':b'tokenizer','hi':'नमस्ते दुनिया'.encode(),'ja':'こんにちは世界'.encode()}
    for tag in ('en','hi','ja'):
        t=load_tok(l,(tag,))
        try:
            assert l.mosaic_tokenizer_detector_loaded(t)==1
            base,_=enc(l,t,texts[tag],False) # explicit loaded pack applies in normal path
            auto,det=enc(l,t,texts[tag],True)
            assert base==expected[tag][1] and auto==expected[tag][1]
            assert det.matched==1 and det.available==1 and det.language.decode()==tag
            # Auto output decodes exactly.
            arr=(C.c_uint32*len(auto))(*auto);out=P8();n=C.c_size_t();assert l.mosaic_tokenizer_decode(t,arr,len(auto),C.byref(out),C.byref(n))==0
            assert bytes(out[i] for i in range(n.value))==texts[tag];l.mosaic_free(out)
        finally:l.mosaic_tokenizer_free(t)

    # Detector may identify a language that is not loaded. Auto mode must use the base model, not another pack.
    t=load_tok(l,('en',))
    try:
        auto,det=enc(l,t,texts['hi'],True);assert det.matched==1 and det.available==0 and det.language.decode()=='hi'
        assert auto==expected['hi'][0]
        low,det=enc(l,t,b'xyz',True);assert det.matched==0 and det.available==0
        base=C.c_void_p();assert l.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNI),C.byref(base))==0
        try: assert low==enc(l,base,b'xyz',False)[0]
        finally:l.mosaic_tokenizer_free(base)
    finally:l.mosaic_tokenizer_free(t)

    # Final fingerprint is independent of whether detector or language packs were attached first.
    fps=[]
    for order in (('det','en','hi','ja'),('ja','det','hi','en'),('hi','en','ja','det')):
        t=load_tok(l,(),False)
        try:
            for item in order:
                if item=='det': assert l.mosaic_tokenizer_set_detector_file(t,os.fsencode(DET))==0
                else: assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{item}-v1.mpack'))==0
            fps.append(fp(l,t))
            assert l.mosaic_tokenizer_set_detector_file(t,os.fsencode(DET))==8
        finally:l.mosaic_tokenizer_free(t)
    assert fps[0]==fps[1]==fps[2]

    for path in sorted(BAD.glob('*.mpack')):
        bad=C.c_void_p(); assert l.mosaic_detector_load_file(os.fsencode(path),C.byref(bad))==3,path
        assert not bad.value
        t=load_tok(l,('en',),False)
        try:
            before=fp(l,t); assert l.mosaic_tokenizer_set_detector_file(t,os.fsencode(path))==3,path
            assert l.mosaic_tokenizer_detector_loaded(t)==0 and fp(l,t)==before
        finally:l.mosaic_tokenizer_free(t)
    print(f'OK: detector routes en/hi/ja, fails soft on low confidence/unavailable packs, exact decode/fingerprint order verified; {len(list(BAD.glob("*.mpack")))} malformed packs rejected')
if __name__=='__main__':raise SystemExit(main())
