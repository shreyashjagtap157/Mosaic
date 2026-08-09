#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'fixtures/packs/language/en-v1.mpack'
OUT=ROOT/'fixtures/packs/malformed-language'

def rehash(b:bytearray):
    b[48:80]=bytes(32); b[48:80]=hashlib.sha256(b).digest()

def lang_offset(b:bytes)->int:
    count=struct.unpack_from('<I',b,24)[0]; directory=struct.unpack_from('<Q',b,32)[0]
    for i in range(count):
        e=directory+i*32; kind=struct.unpack_from('<I',b,e)[0]
        if kind==5:return struct.unpack_from('<Q',b,e+8)[0]
    raise ValueError('no language section')

def build_cases():
    src=SRC.read_bytes(); off=lang_offset(src); cases={}
    def add(name,mut):
        b=bytearray(src);mut(b,off);rehash(b);cases[name]=bytes(b)
    add('language-bad-magic',lambda b,o:b.__setitem__(slice(o,o+4),b'BAD!'))
    add('language-version',lambda b,o:struct.pack_into('<H',b,o+4,2))
    add('language-flags',lambda b,o:struct.pack_into('<H',b,o+6,1))
    add('language-invalid-tag',lambda b,o:b.__setitem__(struct.unpack_from('<I',b,o+20)[0]+o,ord('_')))
    add('language-entry-flags',lambda b,o:struct.pack_into('<H',b,o+40+6,1))
    add('language-entry-reserved',lambda b,o:struct.pack_into('<I',b,o+40+12,1))
    add('language-zero-surface',lambda b,o:struct.pack_into('<H',b,o+40+4,0))
    add('language-max-surface',lambda b,o:struct.pack_into('<I',b,o+32,1))
    add('language-min-cost',lambda b,o:struct.pack_into('<i',b,o+40+8,-2147483648))
    def swap(b,o):
        a=bytes(b[o+40:o+56]);c=bytes(b[o+56:o+72]);b[o+40:o+56]=c;b[o+56:o+72]=a
    add('language-noncanonical-order',swap)
    return cases

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args(); cases=build_cases();OUT.mkdir(parents=True,exist_ok=True)
    for name,data in cases.items():
        p=OUT/f'{name}.mpack'
        if a.check:
            if not p.exists() or p.read_bytes()!=data:raise SystemExit(f'{name} differs')
        else:p.write_bytes(data)
    print(f'OK: {len(cases)} deterministic malformed language fixtures');return 0
if __name__=='__main__':raise SystemExit(main())
