#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GOOD=ROOT/'fixtures/packs/detector/reference-v1.mpack'; OUT=ROOT/'fixtures/packs/malformed-detector'

def detector_offset(data: bytes)->int:
    count=struct.unpack_from('<I',data,24)[0]; directory=struct.unpack_from('<Q',data,32)[0]
    for i in range(count):
        kind=struct.unpack_from('<I',data,directory+i*32)[0]
        if kind==6:return struct.unpack_from('<Q',data,directory+i*32+8)[0]
    raise ValueError('no detector')
def rehash(b:bytearray):
    b[48:80]=bytes(32);b[48:80]=hashlib.sha256(b).digest()
def mutate(name, fn):
    b=bytearray(GOOD.read_bytes());off=detector_offset(b);fn(b,off);rehash(b);return name,bytes(b)

def cases():
    def u16(b,o,v):struct.pack_into('<H',b,o,v)
    def u32(b,o,v):struct.pack_into('<I',b,o,v)
    def i32(b,o,v):struct.pack_into('<i',b,o,v)
    out=[]
    out.append(mutate('detector-bad-magic.mpack',lambda b,o:b.__setitem__(slice(o,o+4),b'BAD!')))
    out.append(mutate('detector-version.mpack',lambda b,o:u16(b,o+4,2)))
    out.append(mutate('detector-flags.mpack',lambda b,o:u16(b,o+6,1)))
    out.append(mutate('detector-zero-profiles.mpack',lambda b,o:u32(b,o+8,0)))
    out.append(mutate('detector-profile-width.mpack',lambda b,o:u16(b,o+16,12)))
    out.append(mutate('detector-negative-margin.mpack',lambda b,o:i32(b,o+44,-1)))
    def bad_tag(b,o):
        po=struct.unpack_from('<I',b,o+20)[0];bo=struct.unpack_from('<I',b,o+32)[0];tagoff=struct.unpack_from('<I',b,o+po)[0];b[o+bo+tagoff]=ord('_')
    out.append(mutate('detector-invalid-tag.mpack',bad_tag))
    def profile_reserved(b,o):
        po=struct.unpack_from('<I',b,o+20)[0];u32(b,o+po+12,1)
    out.append(mutate('detector-profile-reserved.mpack',profile_reserved))
    def feature_weight(b,o):
        fo=struct.unpack_from('<I',b,o+24)[0];i32(b,o+fo+8,0)
    out.append(mutate('detector-zero-weight.mpack',feature_weight))
    def feature_profile(b,o):
        fo=struct.unpack_from('<I',b,o+24)[0];pc=struct.unpack_from('<I',b,o+8)[0];u16(b,o+fo+6,pc)
    out.append(mutate('detector-profile-oob.mpack',feature_profile))
    def feature_reserved(b,o):
        fo=struct.unpack_from('<I',b,o+24)[0];u32(b,o+fo+12,1)
    out.append(mutate('detector-feature-reserved.mpack',feature_reserved))
    out.append(mutate('detector-max-feature.mpack',lambda b,o:u32(b,o+40,999)))
    def first_end(b,o):
        first=struct.unpack_from('<I',b,o+28)[0];u32(b,o+first+256*4,0)
    out.append(mutate('detector-first-endpoint.mpack',first_end))
    def first_order(b,o):
        first=struct.unpack_from('<I',b,o+28)[0];u32(b,o+first+ord('t')*4,999)
    out.append(mutate('detector-first-range.mpack',first_order))
    return out

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args();OUT.mkdir(parents=True,exist_ok=True)
    expected={name:data for name,data in cases()}
    if a.check:
        existing={p.name:p.read_bytes() for p in OUT.glob('*.mpack')}
        if existing!=expected:raise SystemExit('malformed detector fixtures differ')
    else:
        for p in OUT.glob('*.mpack'):p.unlink()
        for name,data in expected.items():(OUT/name).write_bytes(data)
    print(f'OK: malformed detector fixtures={len(expected)}')
if __name__=='__main__':raise SystemExit(main())
