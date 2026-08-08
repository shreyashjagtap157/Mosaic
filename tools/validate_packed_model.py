#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C
import hashlib
import os
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'))
MODEL=ROOT/'fixtures/packs/model-v2.mpack';UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'
MOSAIC_OK=0;INVALID=3;MODEL_FLAG=1
class Info(C.Structure):
    _fields_=[('format_version',C.c_uint32),('id_bit_width',C.c_uint32),('token_count',C.c_uint64),('source_length',C.c_uint64),('encoded_bytes',C.c_uint64),('source_sha256',C.c_uint8*32),('tokenizer_fingerprint_sha256',C.c_uint8*32),('content_sha256',C.c_uint8*32)]

def canonical_hash(blob:bytearray)->bytes:
    h=hashlib.sha256();h.update(blob[:120]);h.update(b'\0'*32);h.update(blob[152:]);return h.digest()
def rehash(blob:bytearray):blob[120:152]=canonical_hash(blob)
def check_invalid(lib,blob:bytes,label:str):
    arr=(C.c_uint8*len(blob)).from_buffer_copy(blob);info=Info();rc=lib.mosaic_packed_model_inspect(arr,len(blob),C.byref(info));
    if rc!=INVALID:raise AssertionError(f'{label}: expected INVALID_PACK got {rc}')
def main():
    lib=C.CDLL(str(LIB));P8=C.POINTER(C.c_uint8)
    tok=C.c_void_p();doc=C.c_void_p();packed=P8();packed_n=C.c_size_t()
    lib.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_tokenizer_load_files.restype=C.c_int
    lib.mosaic_tokenizer_free.argtypes=[C.c_void_p]
    lib.mosaic_tokenizer_token_document_create.argtypes=[C.c_void_p,P8,C.c_size_t,C.c_uint32,C.POINTER(C.c_void_p)];lib.mosaic_tokenizer_token_document_create.restype=C.c_int
    lib.mosaic_token_document_free.argtypes=[C.c_void_p]
    lib.mosaic_token_document_pack_model.argtypes=[C.c_void_p,C.POINTER(P8),C.POINTER(C.c_size_t)];lib.mosaic_token_document_pack_model.restype=C.c_int
    lib.mosaic_packed_model_inspect.argtypes=[P8,C.c_size_t,C.POINTER(Info)];lib.mosaic_packed_model_inspect.restype=C.c_int
    lib.mosaic_free.argtypes=[C.c_void_p]
    assert lib.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNICODE),C.byref(tok))==MOSAIC_OK
    data=(b'tokenizer hello world 12345\n'*100)
    arr=(C.c_uint8*len(data)).from_buffer_copy(data)
    assert lib.mosaic_tokenizer_token_document_create(tok,arr,len(data),MODEL_FLAG,C.byref(doc))==MOSAIC_OK
    assert lib.mosaic_token_document_pack_model(doc,C.byref(packed),C.byref(packed_n))==MOSAIC_OK
    good=bytes(C.string_at(packed,packed_n.value));lib.mosaic_free(packed)
    info=Info();garr=(C.c_uint8*len(good)).from_buffer_copy(good);assert lib.mosaic_packed_model_inspect(garr,len(good),C.byref(info))==MOSAIC_OK
    assert info.encoded_bytes==len(good) and bytes(info.content_sha256)==canonical_hash(bytearray(good))
    cases=[]
    b=bytearray(good);b[0]^=1;cases.append(('magic',b))
    b=bytearray(good);struct.pack_into('<I',b,12,1);rehash(b);cases.append(('flags',b))
    b=bytearray(good);struct.pack_into('<Q',b,24,info.source_length+1);rehash(b);cases.append(('source-length',b))
    b=bytearray(good);struct.pack_into('<I',b,32,33);rehash(b);cases.append(('id-width',b))
    b=bytearray(good);b[152]=1;rehash(b);cases.append(('reserved',b))
    b=bytearray(good);b[-1]^=1;cases.append(('stale-content-hash',b))
    # Make the first token length zero and authenticate the malicious payload.
    b=bytearray(good);b[160]=0;rehash(b);cases.append(('zero-token-length',b))
    b=bytearray(good);struct.pack_into('<Q',b,16,(1<<64)-1);rehash(b);cases.append(('huge-count',b))
    b=bytearray(good);struct.pack_into('<Q',b,40,len(good));rehash(b);cases.append(('length-stream-overflow',b))
    # If the packed ID stream has padding bits, authenticate a non-canonical high padding bit.
    count=struct.unpack_from('<Q',good,16)[0];bits=struct.unpack_from('<I',good,32)[0];used=(count*bits)%8
    if used:
        b=bytearray(good);b[-1]|=1<<used;rehash(b);cases.append(('nonzero-padding',b))
    for name,blob in cases:check_invalid(lib,bytes(blob),name)
    check_invalid(lib,good[:-1],'truncated')
    lib.mosaic_token_document_free(doc);lib.mosaic_tokenizer_free(tok)
    print(f'OK packed-model forged corpus cases={len(cases)+1} bytes={len(good)} tokens={info.token_count} bits={info.id_bit_width}')
    return 0
if __name__=='__main__':raise SystemExit(main())
