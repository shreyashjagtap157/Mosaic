#!/usr/bin/env python3
from __future__ import annotations
import hashlib, tomllib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'fixtures/conformance/tokenizer-manifest-v1.toml'
DOMAIN=b'MOSAIC-TOKENIZER-MANIFEST-V1\0'

def main():
    data=tomllib.loads(FIXTURE.read_text(encoding="utf-8"))
    h=hashlib.sha256(); h.update(DOMAIN)
    for key in ['runtime_semantics_version','canonical_leaf_version','cost_semantics_version','tie_break_version','control_protocol_version','routing_policy_version','normalization_view_id']:
        h.update(int(data[key]).to_bytes(4,'little'))
    h.update(bytes.fromhex(data['resource_policy_hash']))
    packs=data['pack']; h.update(len(packs).to_bytes(4,'little'))
    previous=None
    for pack in packs:
        key=(pack['role'],pack['ordinal'])
        if previous is not None and previous >= key: raise SystemExit('manifest pack ordering is not canonical')
        previous=key
        h.update(int(pack['role']).to_bytes(2,'little')); h.update(int(pack['ordinal']).to_bytes(2,'little')); h.update(bytes.fromhex(pack['content_hash']))
    actual=h.hexdigest()
    if actual != data['expected_identity']: raise SystemExit(f'manifest identity mismatch: {actual}')
    print(f'OK: TokenizerManifest v1 identity {actual}')
if __name__=='__main__': main()
