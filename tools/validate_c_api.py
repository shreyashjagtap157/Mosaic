#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C
import os, random
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB', ROOT/'build/libmosaic.so'))
MODEL=ROOT/'fixtures/packs/model-v2.mpack'
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'

class Token(C.Structure):
    _fields_=[('id',C.c_uint32),('start',C.c_uint64),('length',C.c_uint64),('cost',C.c_int32)]
class Range(C.Structure):
    _fields_=[('start',C.c_uint64),('length',C.c_uint64)]

def ptr_bytes(data:bytes):
    if not data:return None,None
    arr=(C.c_uint8*len(data)).from_buffer_copy(data)
    return arr,C.cast(arr,C.POINTER(C.c_uint8))

def main()->int:
    lib=C.CDLL(str(LIB))
    P8=C.POINTER(C.c_uint8); P32=C.POINTER(C.c_uint32)
    model=C.c_void_p(); uni=C.c_void_p(); tokenizer=C.c_void_p()
    lib.mosaic_model_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_model_load_file.restype=C.c_int
    lib.mosaic_unicode_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_unicode_load_file.restype=C.c_int
    lib.mosaic_model_free.argtypes=[C.c_void_p];lib.mosaic_unicode_free.argtypes=[C.c_void_p];lib.mosaic_free.argtypes=[C.c_void_p]
    lib.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_tokenizer_load_files.restype=C.c_int
    lib.mosaic_tokenizer_free.argtypes=[C.c_void_p]
    lib.mosaic_tokenizer_fingerprint.argtypes=[C.c_void_p,C.POINTER(C.c_uint8)];lib.mosaic_tokenizer_fingerprint.restype=C.c_int
    lib.mosaic_tokenizer_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];lib.mosaic_tokenizer_encode.restype=C.c_int
    lib.mosaic_tokenizer_decode.argtypes=[C.c_void_p,P32,C.c_size_t,C.POINTER(P8),C.POINTER(C.c_size_t)];lib.mosaic_tokenizer_decode.restype=C.c_int
    lib.mosaic_tokenizer_grapheme_ranges.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Range)),C.POINTER(C.c_size_t)];lib.mosaic_tokenizer_grapheme_ranges.restype=C.c_int
    lib.mosaic_version_string.restype=C.c_char_p
    lib.mosaic_tokenizer_semantics_version.restype=C.c_uint32
    lib.mosaic_status_string.argtypes=[C.c_int];lib.mosaic_status_string.restype=C.c_char_p
    lib.mosaic_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];lib.mosaic_encode.restype=C.c_int
    lib.mosaic_decode.argtypes=[C.c_void_p,P32,C.c_size_t,C.POINTER(P8),C.POINTER(C.c_size_t)];lib.mosaic_decode.restype=C.c_int
    lib.mosaic_encode_tokens.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Token)),C.POINTER(C.c_size_t)];lib.mosaic_encode_tokens.restype=C.c_int
    lib.mosaic_grapheme_ranges.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Range)),C.POINTER(C.c_size_t)];lib.mosaic_grapheme_ranges.restype=C.c_int
    lib.mosaic_stream_create.argtypes=[C.c_void_p,C.POINTER(C.c_void_p)];lib.mosaic_stream_create.restype=C.c_int
    lib.mosaic_stream_push.argtypes=[C.c_void_p,P8,C.c_size_t];lib.mosaic_stream_push.restype=C.c_int
    lib.mosaic_stream_finish.argtypes=[C.c_void_p,C.POINTER(P32),C.POINTER(C.c_size_t)];lib.mosaic_stream_finish.restype=C.c_int
    lib.mosaic_stream_reset.argtypes=[C.c_void_p];lib.mosaic_stream_reset.restype=C.c_int
    lib.mosaic_stream_free.argtypes=[C.c_void_p]
    lib.mosaic_document_create.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.c_void_p)];lib.mosaic_document_create.restype=C.c_int
    lib.mosaic_document_apply_edit.argtypes=[C.c_void_p,C.c_uint64,C.c_uint64,P8,C.c_size_t];lib.mosaic_document_apply_edit.restype=C.c_int
    lib.mosaic_document_encode.argtypes=[C.c_void_p,C.POINTER(P32),C.POINTER(C.c_size_t)];lib.mosaic_document_encode.restype=C.c_int
    lib.mosaic_document_copy_bytes.argtypes=[C.c_void_p,C.POINTER(P8),C.POINTER(C.c_size_t)];lib.mosaic_document_copy_bytes.restype=C.c_int
    lib.mosaic_document_free.argtypes=[C.c_void_p]
    assert lib.mosaic_version_string()==b'0.1.3.3'
    assert lib.mosaic_tokenizer_semantics_version()==2
    assert lib.mosaic_model_load_file(os.fsencode(MODEL),C.byref(model))==0
    assert lib.mosaic_unicode_load_file(os.fsencode(UNICODE),C.byref(uni))==0
    assert lib.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNICODE),C.byref(tokenizer))==0
    fingerprint=(C.c_uint8*32)(); assert lib.mosaic_tokenizer_fingerprint(tokenizer,fingerprint)==0
    assert any(fingerprint); stable_fingerprint=bytes(fingerprint)
    # v0.3-v0.5 used semantics-v1. Raw BPE support changes canonical runtime semantics,
    # so v0.6 must not retain the old base fingerprint for the same v2 model/Unicode packs.
    assert stable_fingerprint.hex() != '9e542a728d94b98fed083d61247a554e22a0f4dceb23f39c7106a41f4d1341ba'
    rng=random.Random(0x4D4F53414943)
    cases=[b'',bytes(range(256)),b'hello world\xff', 'नमस्ते 世界 こんにちは'.encode()]
    cases += [bytes(rng.randrange(256) for _ in range(rng.randrange(0,300))) for _ in range(1000)]
    try:
        for data in cases:
            arr,p=ptr_bytes(data); ids=P32(); n=C.c_size_t()
            assert lib.mosaic_encode(model,p,len(data),C.byref(ids),C.byref(n))==0
            out=P8(); outn=C.c_size_t(); assert lib.mosaic_decode(model,ids,n.value,C.byref(out),C.byref(outn))==0
            got=C.string_at(out,outn.value) if outn.value else b''
            assert got==data
            lib.mosaic_free(ids);lib.mosaic_free(out)
            toks=C.POINTER(Token)(); tn=C.c_size_t();assert lib.mosaic_encode_tokens(model,p,len(data),C.byref(toks),C.byref(tn))==0
            assert tn.value==n.value
            cursor=0
            for i in range(tn.value):
                t=toks[i];assert t.start==cursor and t.length>0;cursor+=t.length
            assert cursor==len(data)
            lib.mosaic_free(toks)
            if data is cases[0] or len(data) < 32:
                hi=P32(); hin=C.c_size_t(); assert lib.mosaic_tokenizer_encode(tokenizer,p,len(data),C.byref(hi),C.byref(hin))==0
                hout=P8(); houtn=C.c_size_t(); assert lib.mosaic_tokenizer_decode(tokenizer,hi,hin.value,C.byref(hout),C.byref(houtn))==0
                assert (C.string_at(hout,houtn.value) if houtn.value else b'')==data
                lib.mosaic_free(hi); lib.mosaic_free(hout)
        # Random streaming chunk boundaries must converge to one-shot IDs at EOF.
        for data in cases[:200]:
            arr,p=ptr_bytes(data); full=P32(); fulln=C.c_size_t()
            assert lib.mosaic_encode(model,p,len(data),C.byref(full),C.byref(fulln))==0
            expected=[full[i] for i in range(fulln.value)]
            lib.mosaic_free(full)
            stream=C.c_void_p(); assert lib.mosaic_stream_create(model,C.byref(stream))==0
            pos=0
            while pos<len(data):
                step=min(len(data)-pos,1+rng.randrange(23)); chunk=data[pos:pos+step]; carr,cp=ptr_bytes(chunk)
                assert lib.mosaic_stream_push(stream,cp,len(chunk))==0; pos+=step
            got=P32(); gotn=C.c_size_t(); assert lib.mosaic_stream_finish(stream,C.byref(got),C.byref(gotn))==0
            assert [got[i] for i in range(gotn.value)]==expected
            assert lib.mosaic_stream_push(stream,None,0)==1  # finalized streams reject pushes
            lib.mosaic_free(got)
            assert lib.mosaic_stream_reset(stream)==0
            if data:
                assert lib.mosaic_stream_push(stream,p,len(data))==0
            got=P32(); gotn=C.c_size_t(); assert lib.mosaic_stream_finish(stream,C.byref(got),C.byref(gotn))==0
            assert [got[i] for i in range(gotn.value)]==expected
            lib.mosaic_free(got);lib.mosaic_stream_free(stream)

        # Editable-document semantics: every edit must equal fresh full tokenization.
        expected=bytearray(b'hello world\xff')
        arr,p=ptr_bytes(bytes(expected)); doc=C.c_void_p(); assert lib.mosaic_document_create(model,p,len(expected),C.byref(doc))==0
        try:
            for _ in range(250):
                start=rng.randrange(len(expected)+1)
                delete=rng.randrange(min(9,len(expected)-start)+1)
                replacement=bytes(rng.randrange(256) for _ in range(rng.randrange(0,9)))
                rarr,rp=ptr_bytes(replacement)
                assert lib.mosaic_document_apply_edit(doc,start,delete,rp,len(replacement))==0
                expected[start:start+delete]=replacement
                ids=P32(); n=C.c_size_t(); assert lib.mosaic_document_encode(doc,C.byref(ids),C.byref(n))==0
                arr,p=ptr_bytes(bytes(expected)); fresh=P32(); fn=C.c_size_t(); assert lib.mosaic_encode(model,p,len(expected),C.byref(fresh),C.byref(fn))==0
                assert [ids[i] for i in range(n.value)]==[fresh[i] for i in range(fn.value)]
                lib.mosaic_free(ids);lib.mosaic_free(fresh)
            copied=P8(); cn=C.c_size_t(); assert lib.mosaic_document_copy_bytes(doc,C.byref(copied),C.byref(cn))==0
            assert C.string_at(copied,cn.value)==bytes(expected);lib.mosaic_free(copied)
        finally:
            lib.mosaic_document_free(doc)

        # Unicode API sanity, including invalid bytes.
        data='👩‍💻á🇮🇳'.encode()+b'\xff'
        arr,p=ptr_bytes(data); ranges=C.POINTER(Range)(); rn=C.c_size_t()
        assert lib.mosaic_grapheme_ranges(uni,p,len(data),C.byref(ranges),C.byref(rn))==0
        spans=[(ranges[i].start,ranges[i].start+ranges[i].length) for i in range(rn.value)]
        assert spans==[(0,11),(11,14),(14,22),(22,23)],spans
        lib.mosaic_free(ranges)
        hranges=C.POINTER(Range)(); hrn=C.c_size_t()
        assert lib.mosaic_tokenizer_grapheme_ranges(tokenizer,p,len(data),C.byref(hranges),C.byref(hrn))==0
        assert [(hranges[i].start,hranges[i].start+hranges[i].length) for i in range(hrn.value)]==spans
        lib.mosaic_free(hranges)
        fingerprint2=(C.c_uint8*32)(); assert lib.mosaic_tokenizer_fingerprint(tokenizer,fingerprint2)==0
        assert bytes(fingerprint2)==stable_fingerprint
        # Error/ownership surface: unknown IDs fail without producing output.
        bad=(C.c_uint32*1)(0xFFFFFFFF); out=P8(); outn=C.c_size_t(999)
        status=lib.mosaic_decode(model,bad,1,C.byref(out),C.byref(outn))
        assert status==6 and not bool(out) and outn.value==0
        assert lib.mosaic_status_string(status)==b'unknown token ID'
        # Malformed pack cannot become a live handle.
        bad_model=C.c_void_p(); junk=(C.c_uint8*4)(1,2,3,4)
        status=lib.mosaic_model_load_memory(junk,4,C.byref(bad_model))
        assert status==3 and not bad_model.value
    finally:
        lib.mosaic_tokenizer_free(tokenizer);lib.mosaic_model_free(model);lib.mosaic_unicode_free(uni)
    print(f'OK: stable C API round-tripped {len(cases)} cases; 200 stream chunkings; 250 edit/full differentials; Unicode spans')
    return 0
if __name__=='__main__':raise SystemExit(main())
