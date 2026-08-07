#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C
import os, random
from pathlib import Path
from validate_m3_model import parse_pack, tokenize_reference

ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB', ROOT/'build/libmosaic.so'))
PACK=ROOT/'fixtures/packs/m3-model-v1.mpack'

class Token(C.Structure):
    _fields_=[('id',C.c_uint32),('start',C.c_uint64),('length',C.c_uint64),('cost',C.c_int32)]

def ptr(data:bytes):
    if not data:return None,None
    a=(C.c_uint8*len(data)).from_buffer_copy(data);return a,C.cast(a,C.POINTER(C.c_uint8))

def main()->int:
    entries,_,_=parse_pack(PACK.read_bytes())
    lib=C.CDLL(str(LIB)); P8=C.POINTER(C.c_uint8)
    model=C.c_void_p();
    lib.mosaic_model_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_model_load_file.restype=C.c_int
    lib.mosaic_model_free.argtypes=[C.c_void_p];lib.mosaic_free.argtypes=[C.c_void_p]
    lib.mosaic_encode_tokens.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Token)),C.POINTER(C.c_size_t)];lib.mosaic_encode_tokens.restype=C.c_int
    assert lib.mosaic_model_load_file(os.fsencode(PACK),C.byref(model))==0
    cases=[b'',b'hello',b'hello world',b'the tokenizer','नमस्ते दुनिया'.encode(),'こんにちは世界'.encode(),bytes(range(256)),b'\x00\xffhello\x80world']
    rng=random.Random(0x434F4E464F524D)
    for _ in range(int(os.environ.get('MOSAIC_RANDOM_CASES','256'))):cases.append(bytes(rng.randrange(256) for _ in range(rng.randrange(0,256))))
    try:
        for index,payload in enumerate(cases):
            arr,p=ptr(payload);out=C.POINTER(Token)();n=C.c_size_t()
            if lib.mosaic_encode_tokens(model,p,len(payload),C.byref(out),C.byref(n))!=0: raise SystemExit(f'C encode failure case {index}')
            actual=[(out[i].id,int(out[i].start),int(out[i].start+out[i].length),out[i].cost) for i in range(n.value)]
            expected=[(t.token_id,t.start,t.end,t.cost) for t in tokenize_reference(entries,payload)]
            lib.mosaic_free(out)
            if actual!=expected: raise SystemExit(f'C/Python mismatch case {index}: expected={expected} actual={actual}')
    finally: lib.mosaic_model_free(model)
    print(f'OK: C/Python reference differential matched {len(cases)} cases')
    return 0
if __name__=='__main__':raise SystemExit(main())
