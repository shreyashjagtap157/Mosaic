#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import mosaic_author
OUT=ROOT/'fixtures/packs/online-adversarial-v1.mpack'
def build()->bytes:
    # max surface length 3. At pending bytes b'abx', best paths to boundary 1 and 2
    # begin with different first tokens (b'a' vs b'ab'), so the safe frontier LCA is zero.
    return mosaic_author.build_model_pack([
        {'text':'ab','cost':1,'id':256},
        {'text':'XYZ','cost':1,'id':257},
    ],100,0)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args();data=build()
    if a.check:
        if not OUT.exists() or OUT.read_bytes()!=data:raise SystemExit('FAIL: online adversarial fixture stale')
    else:OUT.write_bytes(data)
    print(f'OK: online adversarial fixture bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}');return 0
if __name__=='__main__':raise SystemExit(main())
