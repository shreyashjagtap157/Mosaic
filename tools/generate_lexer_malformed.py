#!/usr/bin/env python3
from pathlib import Path
import hashlib,struct
ROOT=Path(__file__).resolve().parents[1]
src=ROOT/'fixtures/packs/lexer/c-v1.mpack'; out=ROOT/'fixtures/packs/malformed-lexer';out.mkdir(parents=True,exist_ok=True)
base=bytearray(src.read_bytes())
def sec_offset(data,kind):
    count=struct.unpack_from('<I',data,24)[0]; d=struct.unpack_from('<Q',data,32)[0]
    for i in range(count):
        k=struct.unpack_from('<I',data,d+i*32)[0]
        if k==kind:return struct.unpack_from('<Q',data,d+i*32+8)[0]
    raise ValueError(kind)
o=sec_offset(base,9)
def write(name,fn):
    d=bytearray(base);fn(d,o);d[48:80]=b'\0'*32;d[48:80]=hashlib.sha256(d).digest();(out/name).write_bytes(d)
def u16(d,p,v):struct.pack_into('<H',d,p,v)
def u32(d,p,v):struct.pack_into('<I',d,p,v)
write('lexer-bad-magic.mpack',lambda d,o:d.__setitem__(slice(o,o+4),b'BAD!'))
write('lexer-version.mpack',lambda d,o:u16(d,o+4,2))
write('lexer-reserved.mpack',lambda d,o:u32(d,o+60,1))
write('lexer-line-count.mpack',lambda d,o:u32(d,o+8,65))
write('lexer-empty-name.mpack',lambda d,o:u16(d,o+44,0))
write('lexer-line-flags.mpack',lambda d,o:u16(d,o+64+6,1))
def block_flags(d,o): bo=struct.unpack_from('<I',d,o+28)[0];u16(d,o+bo+12,2)
write('lexer-block-flags.mpack',block_flags)
def keyword_order(d,o):
    ko=struct.unpack_from('<I',d,o+36)[0];a=bytes(d[o+ko:o+ko+8]);b=bytes(d[o+ko+8:o+ko+16]);d[o+ko:o+ko+8]=b;d[o+ko+8:o+ko+16]=a
write('lexer-keyword-order.mpack',keyword_order)
def maxdelim(d,o):u32(d,o+56,struct.unpack_from('<I',d,o+56)[0]+1)
write('lexer-max-delimiter.mpack',maxdelim)
print(f'generated {len(list(out.glob("*.mpack")))} lexer malformed fixtures')
