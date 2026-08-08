#!/usr/bin/env python3
from __future__ import annotations
import hashlib, struct
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'fixtures/packs/model-v2.mpack'
PUB=ROOT/'fixtures/trust/conformance-ed25519.pub'
SIG=ROOT/'fixtures/packs/model-v2.mpack.sig'
SEED=bytes(range(1,33))  # public conformance key only; NEVER use for production
DOMAIN=b'MOSAIC-PACK-SIGNATURE-v1'

def build()->tuple[bytes,bytes]:
    private=Ed25519PrivateKey.from_private_bytes(SEED)
    public=private.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw)
    key_id=hashlib.sha256(public).digest()
    pack_hash=hashlib.sha256(PACK.read_bytes()).digest()
    signature=private.sign(DOMAIN+key_id+pack_hash)
    record=(b'MSSIGV01'+struct.pack('<IIII',1,160,0,1)+key_id+pack_hash+signature+b'\0'*8)
    assert len(record)==160
    return public,record

def main()->int:
    public,record=build(); PUB.write_bytes(public); SIG.write_bytes(record)
    print(f'public={PUB.relative_to(ROOT)} key_id={hashlib.sha256(public).hexdigest()}')
    print(f'signature={SIG.relative_to(ROOT)} sha256={hashlib.sha256(record).hexdigest()}')
    return 0
if __name__=='__main__': raise SystemExit(main())
