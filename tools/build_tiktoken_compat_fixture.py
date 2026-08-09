#!/usr/bin/env python3
from __future__ import annotations
import argparse, base64, hashlib
from pathlib import Path
from mosaic_author import parse_tiktoken, build_model_pack_rows
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'fixtures/compat/raw-bpe-v1.tiktoken'; PACK=ROOT/'fixtures/packs/raw-bpe-v1.mpack'

def expected_source()->bytes:
    vocab={bytes([b]):(b*73)%256 for b in range(256)}
    vocab.update({b'ab':256,b'bc':257,b'abc':258,b'cab':259,b'abcab':260})
    return b''.join(base64.b64encode(s)+b' '+str(rank).encode()+b'\n' for s,rank in sorted(vocab.items(),key=lambda kv:kv[1]))

def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');args=ap.parse_args()
    src=expected_source();
    # Parse exactly the same persisted source bytes users import.
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        tmp=Path(td)/'fixture.tiktoken';tmp.write_bytes(src);pack=build_model_pack_rows(parse_tiktoken(tmp),1)
    if args.check:
        if not SRC.exists() or SRC.read_bytes()!=src: raise SystemExit('raw BPE .tiktoken fixture drift')
        if not PACK.exists() or PACK.read_bytes()!=pack: raise SystemExit('raw BPE pack fixture drift')
    else:
        SRC.parent.mkdir(parents=True,exist_ok=True);PACK.parent.mkdir(parents=True,exist_ok=True);SRC.write_bytes(src);PACK.write_bytes(pack)
    print(f'OK: raw BPE compatibility fixture tokens=261 pack_sha256={hashlib.sha256(pack).hexdigest()}')
    return 0
if __name__=='__main__':raise SystemExit(main())
