#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];GOOD=ROOT/'fixtures/packs/security17-v1.mpack';OUT=ROOT/'fixtures/packs/malformed-security17'
def off(data):
 c=struct.unpack_from('<I',data,24)[0];d=struct.unpack_from('<Q',data,32)[0]
 for i in range(c):
  if struct.unpack_from('<I',data,d+i*32)[0]==7:return struct.unpack_from('<Q',data,d+i*32+8)[0]
 raise ValueError
def rehash(b):b[48:80]=bytes(32);b[48:80]=hashlib.sha256(b).digest()
def mutate(name,fn):
 b=bytearray(GOOD.read_bytes());o=off(b);fn(b,o);rehash(b);return name,bytes(b)
def cases():
 u16=lambda b,o,v:struct.pack_into('<H',b,o,v);u32=lambda b,o,v:struct.pack_into('<I',b,o,v)
 a=[]
 a.append(mutate('security-bad-magic.mpack',lambda b,o:b.__setitem__(slice(o,o+4),b'BAD!')))
 a.append(mutate('security-version.mpack',lambda b,o:u16(b,o+4,2)))
 a.append(mutate('security-header-size.mpack',lambda b,o:u16(b,o+6,64)))
 a.append(mutate('security-unicode-version.mpack',lambda b,o:u16(b,o+8,16)))
 a.append(mutate('security-reserved.mpack',lambda b,o:u32(b,o+68,1)))
 a.append(mutate('security-zero-scripts.mpack',lambda b,o:u32(b,o+16,0)))
 def meta_id(b,o): mo=struct.unpack_from('<I',b,o+40)[0];u16(b,o+mo+6,99)
 a.append(mutate('security-script-id.mpack',meta_id))
 def name(b,o): mo=struct.unpack_from('<I',b,o+40)[0];bo=struct.unpack_from('<I',b,o+64)[0];no=struct.unpack_from('<I',b,o+mo)[0];b[o+bo+no]=ord('_')
 a.append(mutate('security-script-name.mpack',name))
 def overlap(b,o): ro=struct.unpack_from('<I',b,o+44)[0];first_end=struct.unpack_from('<I',b,o+ro+4)[0];u32(b,o+ro+12,first_end-1)
 a.append(mutate('security-script-overlap.mpack',overlap))
 def bool_overlap(b,o): bo=struct.unpack_from('<I',b,o+52)[0];first_end=struct.unpack_from('<I',b,o+bo+4)[0];u32(b,o+bo+8,first_end-1)
 a.append(mutate('security-ignorable-overlap.mpack',bool_overlap))
 def blob_before(b,o): u32(b,o+64,32)
 a.append(mutate('security-layout.mpack',blob_before))
 return a
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');x=ap.parse_args();OUT.mkdir(parents=True,exist_ok=True);exp=dict(cases())
 if x.check:
  got={p.name:p.read_bytes() for p in OUT.glob('*.mpack')}
  if got!=exp:raise SystemExit('malformed security fixtures differ')
 else:
  for p in OUT.glob('*.mpack'):p.unlink()
  for n,d in exp.items():(OUT/n).write_bytes(d)
 print(f'OK: malformed security fixtures={len(exp)}')
if __name__=='__main__':raise SystemExit(main())
