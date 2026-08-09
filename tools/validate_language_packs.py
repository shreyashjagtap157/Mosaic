#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C, os, random
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'))
MODEL=ROOT/'fixtures/packs/model-v2.mpack'; UNI=ROOT/'fixtures/packs/unicode17-v1.mpack'
LANG=ROOT/'fixtures/packs/language'; BAD=ROOT/'fixtures/packs/malformed-language'
P8=C.POINTER(C.c_uint8);P32=C.POINTER(C.c_uint32)

def setup():
    l=C.CDLL(str(LIB));
    l.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_load_files.restype=C.c_int
    l.mosaic_tokenizer_add_language_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_add_language_file.restype=C.c_int
    l.mosaic_tokenizer_language_count.argtypes=[C.c_void_p];l.mosaic_tokenizer_language_count.restype=C.c_size_t
    l.mosaic_tokenizer_language_tag.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p,C.c_size_t,C.POINTER(C.c_size_t)];l.mosaic_tokenizer_language_tag.restype=C.c_int
    l.mosaic_tokenizer_fingerprint.argtypes=[C.c_void_p,P8];l.mosaic_tokenizer_fingerprint.restype=C.c_int
    l.mosaic_tokenizer_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_tokenizer_encode.restype=C.c_int
    l.mosaic_tokenizer_stream_create.argtypes=[C.c_void_p,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_stream_create.restype=C.c_int
    l.mosaic_stream_push.argtypes=[C.c_void_p,P8,C.c_size_t];l.mosaic_stream_push.restype=C.c_int
    l.mosaic_stream_finish.argtypes=[C.c_void_p,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_stream_finish.restype=C.c_int
    l.mosaic_stream_free.argtypes=[C.c_void_p]
    l.mosaic_tokenizer_document_create.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_document_create.restype=C.c_int
    l.mosaic_document_encode.argtypes=[C.c_void_p,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_document_encode.restype=C.c_int
    l.mosaic_document_free.argtypes=[C.c_void_p]
    l.mosaic_tokenizer_free.argtypes=[C.c_void_p];l.mosaic_free.argtypes=[C.c_void_p]
    return l

def ptr(data):
    if not data:return None,None
    a=(C.c_uint8*len(data)).from_buffer_copy(data);return a,C.cast(a,P8)
def load(l):
    t=C.c_void_p(); assert l.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNI),C.byref(t))==0;return t
def encode(l,t,data):
    a,p=ptr(data);ids=P32();n=C.c_size_t();assert l.mosaic_tokenizer_encode(t,p,len(data),C.byref(ids),C.byref(n))==0
    out=[ids[i] for i in range(n.value)];l.mosaic_free(ids);return out
def fp(l,t):
    x=(C.c_uint8*32)();assert l.mosaic_tokenizer_fingerprint(t,x)==0;return bytes(x)

def main():
    l=setup()
    checks=[('en',b'tokenizer',[263,264],[271]),('hi','नमस्ते दुनिया'.encode(),[265,272],[273]),('ja','こんにちは世界'.encode(),[267,266],[274])]
    for tag,text,base,special in checks:
        t=load(l)
        try:
            assert encode(l,t,text)==base
            assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{tag}-v1.mpack'))==0
            assert encode(l,t,text)==special
            assert l.mosaic_tokenizer_language_count(t)==1
            need=C.c_size_t();assert l.mosaic_tokenizer_language_tag(t,0,None,0,C.byref(need))==0
            buf=C.create_string_buffer(need.value);assert l.mosaic_tokenizer_language_tag(t,0,buf,len(buf),C.byref(need))==0 and buf.value.decode()==tag
            assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{tag}-v1.mpack'))==8
        finally:l.mosaic_tokenizer_free(t)

    mixed=b'tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode()
    outputs=[];fingerprints=[]
    for order in [('en','hi','ja'),('ja','en','hi'),('hi','ja','en')]:
        t=load(l)
        try:
            for tag in order:assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{tag}-v1.mpack'))==0
            outputs.append(encode(l,t,mixed));fingerprints.append(fp(l,t))
            # Stream and document snapshots inherit the language set.
            a,p=ptr(mixed); stream=C.c_void_p();assert l.mosaic_tokenizer_stream_create(t,C.byref(stream))==0
            assert l.mosaic_stream_push(stream,p,len(mixed))==0; ids=P32();n=C.c_size_t();assert l.mosaic_stream_finish(stream,C.byref(ids),C.byref(n))==0
            assert [ids[i] for i in range(n.value)]==outputs[-1];l.mosaic_free(ids);l.mosaic_stream_free(stream)
            doc=C.c_void_p();assert l.mosaic_tokenizer_document_create(t,p,len(mixed),C.byref(doc))==0
            ids=P32();n=C.c_size_t();assert l.mosaic_document_encode(doc,C.byref(ids),C.byref(n))==0
            assert [ids[i] for i in range(n.value)]==outputs[-1];l.mosaic_free(ids);l.mosaic_document_free(doc)
        finally:l.mosaic_tokenizer_free(t)
    assert outputs[0]==outputs[1]==outputs[2]
    assert fingerprints[0]==fingerprints[1]==fingerprints[2]
    assert 271 in outputs[0] and 273 in outputs[0] and 274 in outputs[0]

    # Every malformed language pack fails before modifying the live tokenizer.
    t=load(l)
    try:
        basefp=fp(l,t)
        for pth in sorted(BAD.glob('*.mpack')):
            assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(pth))==3,pth
            assert l.mosaic_tokenizer_language_count(t)==0 and fp(l,t)==basefp
    finally:l.mosaic_tokenizer_free(t)
    print(f'OK: language packs specialize en/hi/ja; mixed composition is order-independent; {len(list(BAD.glob("*.mpack")))} malformed packs rejected')
    return 0
if __name__=='__main__':raise SystemExit(main())
