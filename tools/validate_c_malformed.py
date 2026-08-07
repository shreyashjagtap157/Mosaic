#!/usr/bin/env python3
from __future__ import annotations
import os
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BIN=Path(os.environ.get('MOSAIC_REF_BIN', ROOT/'build/mosaic-ref'))
VALID=ROOT/'fixtures/packs/m3-model-v1.mpack'
MAL=ROOT/'fixtures/packs/malformed-m3'

def main():
    subprocess.run([str(BIN),'validate',str(VALID)],check=True,capture_output=True,text=True)
    files=sorted(MAL.glob('*.mpack'))
    for p in files:
        run=subprocess.run([str(BIN),'validate',str(p)],capture_output=True,text=True)
        if run.returncode==0: raise SystemExit(f'C reference accepted malformed pack: {p.name}')
    print(f'OK: C reference rejects all {len(files)} malformed M3 vocabulary packs')
    return 0
if __name__=='__main__': raise SystemExit(main())
