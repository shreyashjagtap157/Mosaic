#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, shutil, struct
from pathlib import Path
from validate_m2_fixture import parse_outer

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'fixtures/packs/m3-model-v1.mpack'
OUT=ROOT/'fixtures/packs/malformed-m3'


def rehash(data: bytearray) -> bytes:
    data[48:80]=bytes(32)
    data[48:80]=hashlib.sha256(data).digest()
    return bytes(data)


def vocab_info(data: bytes):
    sections,_,_=parse_outer(data)
    section=next(s for s in sections if s.kind==4)
    v=section.offset
    count=struct.unpack_from('<I',data,v+8)[0]
    entries=struct.unpack_from('<I',data,v+16)[0]
    ids=struct.unpack_from('<I',data,v+20)[0]
    first=struct.unpack_from('<I',data,v+24)[0]
    blob=struct.unpack_from('<I',data,v+28)[0]
    blob_len=struct.unpack_from('<I',data,v+32)[0]
    return v,count,entries,ids,first,blob,blob_len


def mutate() -> dict[str,bytes]:
    base=BASE.read_bytes(); v,count,entries,ids,first,blob,blob_len=vocab_info(base)
    cases={}
    def case(name, fn):
        d=bytearray(base); fn(d); cases[name]=rehash(d)
    case('vocab-bad-magic.mpack', lambda d: d.__setitem__(v, ord('X')))
    case('vocab-version.mpack', lambda d: struct.pack_into('<H',d,v+4,2))
    case('vocab-flags.mpack', lambda d: struct.pack_into('<H',d,v+6,1))
    case('vocab-entry-flags.mpack', lambda d: struct.pack_into('<H',d,v+entries+14,1))
    case('vocab-zero-surface.mpack', lambda d: struct.pack_into('<H',d,v+entries+12,0))
    case('vocab-surface-oob.mpack', lambda d: struct.pack_into('<I',d,v+entries+8,blob_len+1))
    def dup_index(d):
        first_idx=struct.unpack_from('<I',d,v+ids)[0]; struct.pack_into('<I',d,v+ids+4,first_idx)
    case('vocab-id-index-duplicate.mpack', dup_index)
    case('vocab-first-index-bad.mpack', lambda d: struct.pack_into('<I',d,v+first,1))
    def missing_fallback(d):
        # Entry 0 is token 0 surface b"\\x00" in canonical surface order.
        struct.pack_into('<H',d,v+entries+12,2)
    case('vocab-missing-byte-fallback.mpack', missing_fallback)
    def noncanonical(d):
        a=bytes(d[v+entries:v+entries+16]); b=bytes(d[v+entries+16:v+entries+32])
        d[v+entries:v+entries+16]=b; d[v+entries+16:v+entries+32]=a
    case('vocab-noncanonical-order.mpack', noncanonical)
    def duplicate_token_id(d):
        id0=struct.unpack_from('<I',d,v+entries)[0]; struct.pack_into('<I',d,v+entries+16,id0)
    case('vocab-duplicate-token-id.mpack', duplicate_token_id)
    def wrong_bucket(d):
        # Bucket 0 remains [0,0), bucket 1 begins at 0 and includes token surface 0x00.
        struct.pack_into('<I',d,v+first+4,0)
    case('vocab-wrong-bucket.mpack', wrong_bucket)
    return cases


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); args=ap.parse_args(); cases=mutate()
    if args.check:
        for name,data in cases.items():
            p=OUT/name
            if not p.exists() or p.read_bytes()!=data: raise SystemExit(f'{name} differs from deterministic output')
        print(f'OK: {len(cases)} deterministic M3 malformed vocabulary fixtures')
        return 0
    OUT.mkdir(parents=True,exist_ok=True)
    for old in OUT.glob('*.mpack'): old.unlink()
    for name,data in cases.items(): (OUT/name).write_bytes(data)
    print(f'wrote {len(cases)} M3 malformed vocabulary fixtures')
    return 0

if __name__=='__main__': raise SystemExit(main())
