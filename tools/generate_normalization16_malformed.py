#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];GOOD=ROOT/'fixtures/packs/normalization16-v1.mpack';OUT=ROOT/'fixtures/packs/malformed-normalization16'
def off(data):
 c=struct.unpack_from('<I',data,24)[0];d=struct.unpack_from('<Q',data,32)[0]
 for i in range(c):
  if struct.unpack_from('<I',data,d+i*32)[0]==8:return struct.unpack_from('<Q',data,d+i*32+8)[0]
 raise ValueError
def rehash(b):b[48:80]=bytes(32);b[48:80]=hashlib.sha256(b).digest()
def mutate(name,fn):
 b=bytearray(GOOD.read_bytes());o=off(b);fn(b,o);rehash(b);return name,bytes(b)
def cases():
 u16=lambda b,o,v:struct.pack_into('<H',b,o,v);u32=lambda b,o,v:struct.pack_into('<I',b,o,v);a=[]
 a.append(mutate('normalization-bad-magic.mpack',lambda b,o:b.__setitem__(slice(o,o+4),b'BAD!')))
 a.append(mutate('normalization-version.mpack',lambda b,o:u16(b,o+4,2)))
 a.append(mutate('normalization-header-size.mpack',lambda b,o:u16(b,o+6,64)))
 a.append(mutate('normalization-unicode-version.mpack',lambda b,o:u16(b,o+8,17)))
 a.append(mutate('normalization-reserved.mpack',lambda b,o:u32(b,o+64,1)))
 a.append(mutate('normalization-count-limit.mpack',lambda b,o:u32(b,o+20,100001)))
 def ccc_overlap(b,o):
  ro=struct.unpack_from('<I',b,o+40)[0];first_end=struct.unpack_from('<I',b,o+ro+4)[0];u32(b,o+ro+12,first_end-1)
 a.append(mutate('normalization-ccc-overlap.mpack',ccc_overlap))
 def map_unsorted(b,o):
  mo=struct.unpack_from('<I',b,o+44)[0];first=struct.unpack_from('<I',b,o+mo)[0];u32(b,o+mo+12,first)
 a.append(mutate('normalization-map-order.mpack',map_unsorted))
 def map_oob(b,o):
  mo=struct.unpack_from('<I',b,o+44)[0];sq=struct.unpack_from('<I',b,o+36)[0];u32(b,o+mo+4,sq)
 a.append(mutate('normalization-sequence-oob.mpack',map_oob))
 def comp_order(b,o):
  po=struct.unpack_from('<I',b,o+56)[0];a0,b0=struct.unpack_from('<II',b,o+po);struct.pack_into('<II',b,o+po+12,a0,b0)
 a.append(mutate('normalization-composition-order.mpack',comp_order))
 def layout(b,o):u32(b,o+60,32)
 a.append(mutate('normalization-layout.mpack',layout))
 return a
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');x=ap.parse_args();OUT.mkdir(parents=True,exist_ok=True);exp=dict(cases())
 if x.check:
  got={p.name:p.read_bytes() for p in OUT.glob('*.mpack')}
  if got!=exp:raise SystemExit('malformed normalization fixtures differ')
 else:
  for p in OUT.glob('*.mpack'):p.unlink()
  for n,d in exp.items():(OUT/n).write_bytes(d)
 print(f'OK: malformed normalization fixtures={len(exp)}')
if __name__=='__main__':raise SystemExit(main())
