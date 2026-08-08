#!/usr/bin/env python3
from __future__ import annotations
import json,tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from mosaic_registry import Registry,canon

ROOT=Path(__file__).resolve().parents[1]
MODEL=ROOT/'fixtures/packs/model-v2.mpack';UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack';SIG=ROOT/'fixtures/packs/model-v2.mpack.sig';PUB=ROOT/'fixtures/trust/conformance-ed25519.pub'

def main()->int:
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)/'registry';r=Registry(root);r.init()
        # Concurrent idempotent installation of one exact signed identity.
        def install(_):
            rr=Registry(root);rr.init();return rr.install(MODEL,'org.mosaic','reference-model','1.0.0',SIG,PUB,True).sha256
        with ThreadPoolExecutor(max_workers=8) as ex:hashes=list(ex.map(install,range(32)))
        if len(set(hashes))!=1:raise SystemExit('concurrent install diverged')
        signed=r.rows()[0]
        if signed.trust_status!='verified' or len(signed.key_id)!=64:raise SystemExit('verified install did not bind key')
        r.install(MODEL,'org.mosaic','reference-model','1.1.0')
        r.install(UNICODE,'org.mosaic','unicode17','17.0.0')
        try:r.install(UNICODE,'org.mosaic','reference-model','1.0.0')
        except ValueError:pass
        else:raise SystemExit('immutable identity conflict accepted')
        try:r.install(MODEL,'org.mosaic','unsigned-required','1.0.0',require_signature=True)
        except ValueError:pass
        else:raise SystemExit('signature-required policy accepted unsigned pack')
        req={'schema':1,'requirements':[{'role':'model','publisher':'org.mosaic','name':'reference-model','constraint':'^1.0.0'},{'role':'unicode','publisher':'org.mosaic','name':'unicode17','constraint':'==17.0.0'}]}
        lock=r.resolve(req)
        packs={x['role']:x for x in lock['packs']}
        if packs['model']['version']!='1.1.0' or packs['unicode']['version']!='17.0.0':raise SystemExit('deterministic resolution chose wrong version')
        if r.verify_lock(lock):raise SystemExit('fresh lock failed verification')
        # lock must reject mutable alias syntax.
        try:r.resolve({'schema':1,'requirements':[{'role':'x','publisher':'org.mosaic','name':'reference-model','constraint':'latest'}]})
        except ValueError:pass
        else:raise SystemExit('mutable latest alias accepted')
        # Corruption must be visible to both lock and registry audit.
        obj=r.object_path(packs['model']['sha256']);original=obj.read_bytes();obj.write_bytes(original[:-1]+bytes([original[-1]^1]))
        if not r.verify_lock(lock) or not r.audit():raise SystemExit('content corruption not detected')
        obj.write_bytes(original)
        if r.verify_lock(lock) or r.audit():raise SystemExit('restored registry did not recover')
        # GC removes only unreferenced objects.
        orphan=r.objects/'ff'/('f'*64);orphan.parent.mkdir(parents=True,exist_ok=True);orphan.write_bytes(b'orphan')
        if r.gc()!=1 or orphan.exists():raise SystemExit('registry GC failed')
        # Canonical lock bytes are stable.
        if canon(lock)!=canon(r.resolve(req)):raise SystemExit('lock generation is nondeterministic')
        print(f'OK registry concurrent_installs=32 packs={len(r.rows())} catalog={r.catalog_hash()} lock={lock["lock_sha256"]}')
    return 0
if __name__=='__main__':raise SystemExit(main())
