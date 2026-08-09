#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C, os, random
from pathlib import Path
import regex, regex._regex as RX
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'))
PACK=ROOT/'fixtures/packs/security17-v1.mpack'
class Span(C.Structure): _fields_=[('start',C.c_uint64),('length',C.c_uint64),('script_id',C.c_uint16),('reserved',C.c_uint16)]
class Finding(C.Structure): _fields_=[('kind',C.c_uint32),('script_id',C.c_uint16),('reserved',C.c_uint16),('start',C.c_uint64),('length',C.c_uint64)]
def pbytes(b):
    if not b:return None,None
    a=(C.c_uint8*len(b)).from_buffer_copy(b);return a,C.cast(a,C.POINTER(C.c_uint8))
def canonical_scripts():
    aliases=RX.get_properties()['SCRIPT'][1];group={}
    for n,v in aliases.items():group.setdefault(v,[]).append(n)
    out=[]
    for names in group.values():out.append(([n for n in names if len(n)>4] or names)[0])
    return sorted(set(out))
def main():
    lib=C.CDLL(str(LIB));P8=C.POINTER(C.c_uint8)
    lib.mosaic_security_load_file.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];lib.mosaic_security_load_file.restype=C.c_int
    lib.mosaic_security_free.argtypes=[C.c_void_p];lib.mosaic_free.argtypes=[C.c_void_p]
    lib.mosaic_security_script_name.argtypes=[C.c_void_p,C.c_uint16,C.c_char_p,C.c_size_t,C.POINTER(C.c_size_t)];lib.mosaic_security_script_name.restype=C.c_int
    lib.mosaic_security_script_ranges.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Span)),C.POINTER(C.c_size_t)];lib.mosaic_security_script_ranges.restype=C.c_int
    lib.mosaic_security_scan.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(C.POINTER(Finding)),C.POINTER(C.c_size_t)];lib.mosaic_security_scan.restype=C.c_int
    sec=C.c_void_p();assert lib.mosaic_security_load_file(os.fsencode(PACK),C.byref(sec))==0
    names={};
    try:
        for sid in range(1,1025):
            req=C.c_size_t();st=lib.mosaic_security_script_name(sec,sid,None,0,C.byref(req))
            if st!=0: break
            buf=C.create_string_buffer(req.value);assert lib.mosaic_security_script_name(sec,sid,buf,len(buf),C.byref(req))==0;names[buf.value.decode()]=sid
        expected_names=canonical_scripts();assert set(names)==set(expected_names),(len(names),len(expected_names),set(expected_names)-set(names))
        compiled={n:regex.compile(fr'\A\p{{Script={n}}}\Z') for n in expected_names}
        rng=random.Random(0x5345435552495459)
        cps=[0,65,0x0301,0x0905,0x3042,0x30A2,0x4E00,0x202E,0x200B,0xFDD0,0x0149]
        cps += [rng.randrange(0x110000) for _ in range(300)]
        for cp in cps:
            if 0xD800<=cp<=0xDFFF: continue
            b=chr(cp).encode();arr,p=pbytes(b);sp=C.POINTER(Span)();n=C.c_size_t();assert lib.mosaic_security_script_ranges(sec,p,len(b),C.byref(sp),C.byref(n))==0 and n.value==1
            got=sp[0].script_id;lib.mosaic_free(sp)
            expected=next(names[name] for name,pat in compiled.items() if pat.fullmatch(chr(cp)))
            assert got==expected,(hex(cp),got,expected)
            fs=C.POINTER(Finding)();fn=C.c_size_t();assert lib.mosaic_security_scan(sec,p,len(b),C.byref(fs),C.byref(fn))==0
            kinds={fs[i].kind for i in range(fn.value)};lib.mosaic_free(fs)
            checks={1:r'\p{Bidi_Control}',2:r'\p{Default_Ignorable_Code_Point}',3:r'\p{Noncharacter_Code_Point}',4:r'\p{Deprecated}'}
            for kind,pat in checks.items():assert (kind in kinds)==bool(regex.fullmatch(pat,chr(cp))),(hex(cp),kind,kinds)
        # Invalid UTF-8 stays represented as script 0, never coerced to replacement characters.
        bad=b'\xff\x80A';arr,p=pbytes(bad);sp=C.POINTER(Span)();n=C.c_size_t();assert lib.mosaic_security_script_ranges(sec,p,len(bad),C.byref(sp),C.byref(n))==0
        assert [(sp[i].start,sp[i].length,sp[i].script_id) for i in range(n.value)][0][:3]==(0,2,0);lib.mosaic_free(sp)
        # Equal-count Latin/Cyrillic mixed-script evidence uses lower stable script ID as primary.
        data='AЖ'.encode();arr,p=pbytes(data);fs=C.POINTER(Finding)();fn=C.c_size_t();assert lib.mosaic_security_scan(sec,p,len(data),C.byref(fs),C.byref(fn))==0
        mixed=[fs[i] for i in range(fn.value) if fs[i].kind==5]; assert len(mixed)==1
        expected_nonprimary=max(names['LATIN'],names['CYRILLIC']); assert mixed[0].script_id==expected_nonprimary
        lib.mosaic_free(fs)
    finally: lib.mosaic_security_free(sec)
    print(f'OK: Unicode17 security/script oracle matched {len(cps)} property cases; scripts={len(names)}')
    return 0
if __name__=='__main__':raise SystemExit(main())
