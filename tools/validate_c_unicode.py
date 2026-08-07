#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C
import os, random
from pathlib import Path
from validate_unicode17 import parse_unicode,grapheme_spans
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'));PACK=ROOT/'fixtures/packs/unicode17-v1.mpack'
class Range(C.Structure):_fields_=[('start',C.c_uint64),('length',C.c_uint64)]
def ptr(data:bytes):
    if not data:return None,None
    a=(C.c_uint8*len(data)).from_buffer_copy(data);return a,C.cast(a,C.POINTER(C.c_uint8))
def main()->int:
    tables=parse_unicode(PACK.read_bytes());lib=C.CDLL(str(LIB));P8=C.POINTER(C.c_uint8)
    uni=C.c_void_p();lib.mosaic_unicode_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_unicode_load_file.restype=C.c_int
    lib.mosaic_unicode_free.argtypes=[C.c_void_p];lib.mosaic_free.argtypes=[C.c_void_p]
    lib.mosaic_grapheme_ranges.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Range)),C.POINTER(C.c_size_t)];lib.mosaic_grapheme_ranges.restype=C.c_int
    assert lib.mosaic_unicode_load_file(os.fsencode(PACK),C.byref(uni))==0
    cases=[b'',b'a','a\u0301'.encode(),'👩\u200d💻'.encode(),'🇮🇳🇺🇸'.encode(),'क्ष'.encode(),b'\xffa\xcc\x81',b'\xc0\xaf',bytes(range(256))]
    rng=random.Random(0x43554E49434F4445)
    for _ in range(int(os.environ.get('MOSAIC_RANDOM_CASES','512'))):cases.append(bytes(rng.randrange(256) for _ in range(rng.randrange(0,160))))
    try:
        for i,payload in enumerate(cases):
            arr,p=ptr(payload);out=C.POINTER(Range)();n=C.c_size_t()
            if lib.mosaic_grapheme_ranges(uni,p,len(payload),C.byref(out),C.byref(n))!=0:raise SystemExit(f'C Unicode failure case={i}')
            actual=[(int(out[j].start),int(out[j].start+out[j].length)) for j in range(n.value)];lib.mosaic_free(out)
            expected=grapheme_spans(tables,payload)
            if actual!=expected:raise SystemExit(f'C/Python Unicode mismatch case={i}: {actual} != {expected} raw={payload!r}')
    finally:lib.mosaic_unicode_free(uni)
    print(f'OK: C/Python Unicode differential matched {len(cases)} arbitrary and crafted cases')
    return 0
if __name__=='__main__':raise SystemExit(main())
