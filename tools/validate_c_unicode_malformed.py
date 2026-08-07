#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BIN=Path(os.environ.get('MOSAIC_REF_BIN', ROOT/'build/mosaic-ref'));MAL=ROOT/'fixtures/packs/malformed-unicode17'
def main():
 with tempfile.NamedTemporaryFile() as f:
  f.write(b'a');f.flush();files=sorted(MAL.glob('*.mpack'))
  for p in files:
   r=subprocess.run([str(BIN),'graphemes',str(p),f.name],capture_output=True,text=True)
   if r.returncode==0:raise SystemExit(f'C accepted malformed Unicode pack {p.name}')
 print(f'OK: C reference rejects all {len(files)} malformed Unicode packs');return 0
if __name__=='__main__':raise SystemExit(main())
