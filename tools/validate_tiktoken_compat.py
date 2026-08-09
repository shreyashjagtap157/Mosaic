#!/usr/bin/env python3
from __future__ import annotations
import base64, random, struct, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; AUTHOR=ROOT/'tools/mosaic_author.py'; CLI=ROOT/'build/mosaic-tokenizer'

def raw_bpe(data:bytes,vocab:dict[bytes,int])->list[int]:
    parts=[bytes([x]) for x in data]
    while len(parts)>1:
        best=None
        for i in range(len(parts)-1):
            merged=parts[i]+parts[i+1];rank=vocab.get(merged)
            if rank is not None and (best is None or (rank,i)<best[:2]):best=(rank,i,merged)
        if best is None:break
        _,i,merged=best;parts[i:i+2]=[merged]
    return [vocab[p] for p in parts]

def encode(cli:Path,pack:Path,data:bytes,tmp:Path)->list[int]:
    inp=tmp/'in.bin';out=tmp/'ids.bin';inp.write_bytes(data)
    subprocess.run([cli,'encode-u32',pack,inp,out],check=True,stdout=subprocess.DEVNULL)
    raw=out.read_bytes();return list(struct.unpack('<'+'I'*(len(raw)//4),raw))

def recanonicalize(data:bytearray)->None:
    import hashlib
    data[48:80]=b'\0'*32;data[48:80]=hashlib.sha256(data).digest()

def vocab_offset(data:bytes)->int:
    count=struct.unpack_from('<I',data,24)[0];directory=struct.unpack_from('<Q',data,32)[0]
    for i in range(count):
        kind,_,off,_=struct.unpack_from('<IIQQ',data,directory+i*32)
        if kind==4:return off
    raise AssertionError('no vocabulary')

def validate_fails(pack:Path)->None:
    proc=subprocess.run([CLI,'validate',pack],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    assert proc.returncode!=0

def main()->int:
    with tempfile.TemporaryDirectory() as td:
        d=Path(td);source=d/'synthetic.tiktoken';pack=d/'synthetic.mpack';exported=d/'roundtrip.tiktoken'
        # Deliberately permute single-byte ranks/IDs so fallback is surface-based, not ID==byte.
        perm=[(b*73)%256 for b in range(256)];vocab={bytes([b]):perm[b] for b in range(256)}
        merges=[(b'ab',256),(b'bc',257),(b'abc',258),(b'hello',259),(b' world',260),(bytes.fromhex('e38193e38293'),261)]
        vocab.update(merges)
        source.write_bytes(b''.join(base64.b64encode(s)+b' '+str(r).encode()+b'\n' for s,r in sorted(vocab.items(),key=lambda kv:kv[1])))
        subprocess.run([AUTHOR,'import-tiktoken',source,pack],check=True,cwd=ROOT,stdout=subprocess.DEVNULL)
        subprocess.run([AUTHOR,'export-tiktoken',pack,exported],check=True,cwd=ROOT,stdout=subprocess.DEVNULL)
        assert source.read_bytes()==exported.read_bytes()
        cases=[b'',b'a',b'ab',b'abc',b'abcbc',b'hello world',b'\x00\xffabc','こんabc'.encode()]
        rng=random.Random(0xB0E)
        for _ in range(600):cases.append(bytes(rng.randrange(256) for _ in range(rng.randrange(0,80))))
        for data in cases:
            got=encode(CLI,pack,data,d);want=raw_bpe(data,vocab)
            assert got==want,(data,got,want)
        # Runtime rejects invalid algorithm and duplicate BPE ranks after canonical hash repair.
        raw=bytearray(pack.read_bytes());vo=vocab_offset(raw)
        bad_alg=d/'bad-alg.mpack';struct.pack_into('<I',raw,vo+36,2);recanonicalize(raw);bad_alg.write_bytes(raw);validate_fails(bad_alg)
        raw=bytearray(pack.read_bytes());vo=vocab_offset(raw);entries=struct.unpack_from('<I',raw,vo+16)[0]
        first_cost=struct.unpack_from('<i',raw,vo+entries+4)[0];struct.pack_into('<i',raw,vo+entries+16+4,first_cost);recanonicalize(raw)
        dup=d/'dup-rank.mpack';dup.write_bytes(raw);validate_fails(dup)

        # Invalid import cases fail cleanly.
        bad=d/'bad.tiktoken';badpack=d/'bad.mpack';bad.write_bytes(b'not-base64 1\n')
        assert subprocess.run([AUTHOR,'import-tiktoken',bad,badpack],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE).returncode!=0
        missing=d/'missing.tiktoken';missing.write_bytes(base64.b64encode(b'x')+b' 0\n')
        assert subprocess.run([AUTHOR,'import-tiktoken',missing,badpack],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE).returncode!=0
    print(f'OK: raw .tiktoken import/export and native BPE match independent oracle on {len(cases)} cases')
    return 0
if __name__=='__main__':raise SystemExit(main())
