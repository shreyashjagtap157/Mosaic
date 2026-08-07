#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,struct
from pathlib import Path
from validate_m2_fixture import parse_outer
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'fixtures/packs/unicode17-v1.mpack'; OUT=ROOT/'fixtures/packs/malformed-unicode17'

def rehash(d:bytearray): d[48:80]=bytes(32); d[48:80]=hashlib.sha256(d).digest(); return bytes(d)
def info(data):
 s=next(s for s in parse_outer(data)[0] if s.kind==5); u=s.offset; gc,ic,ec=struct.unpack_from('<III',data,u+16); go,io,eo=struct.unpack_from('<III',data,u+28); return u,gc,ic,ec,go,io,eo

def build():
 base=BASE.read_bytes();u,gc,ic,ec,go,io,eo=info(base); cases={}
 def case(name,fn): d=bytearray(base);fn(d);cases[name]=rehash(d)
 case('unicode-bad-magic.mpack',lambda d:d.__setitem__(u,ord('X')))
 case('unicode-version.mpack',lambda d:struct.pack_into('<H',d,u+4,2))
 case('unicode-flags.mpack',lambda d:struct.pack_into('<H',d,u+6,1))
 case('unicode-data-version.mpack',lambda d:struct.pack_into('<H',d,u+8,16))
 case('unicode-layout.mpack',lambda d:struct.pack_into('<I',d,u+28,52))
 case('unicode-gcb-zero-value.mpack',lambda d:d.__setitem__(u+go+8,0))
 case('unicode-gcb-reserved.mpack',lambda d:d.__setitem__(u+go+9,1))
 def overlap(d):
  first_end=struct.unpack_from('<I',d,u+go+4)[0]; struct.pack_into('<I',d,u+go+12,first_end-1)
 case('unicode-gcb-overlap.mpack',overlap)
 def reversed_ep(d):
  start=struct.unpack_from('<I',d,u+eo)[0]; struct.pack_into('<I',d,u+eo+4,start)
 case('unicode-ep-empty.mpack',reversed_ep)
 return cases

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args();cases=build()
 if a.check:
  for n,d in cases.items():
   p=OUT/n
   if not p.exists() or p.read_bytes()!=d: raise SystemExit(f'{n} differs')
  print(f'OK: {len(cases)} deterministic malformed Unicode fixtures');return 0
 OUT.mkdir(parents=True,exist_ok=True)
 for p in OUT.glob('*.mpack'):p.unlink()
 for n,d in cases.items():(OUT/n).write_bytes(d)
 print(f'wrote {len(cases)} malformed Unicode fixtures');return 0
if __name__=='__main__':raise SystemExit(main())
