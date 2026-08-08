#!/usr/bin/env python3
"""Run deterministic Mosaic runtime and control-plane reliability campaigns."""
from __future__ import annotations
import argparse, json, os, re, subprocess, sys, tempfile, threading, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mosaic_registry import Registry  # noqa: E402

TIERS={'smoke':2000,'release':25000,'soak':250000}
REPLAY_RE=re.compile(r'replay=([0-9a-f]{16})')


def run_native(iterations:int,seed:int)->dict:
    subprocess.run(['make','-C','native','../build/mosaic-reliability-smoke'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    env=os.environ.copy();env['MOSAIC_RELIABILITY_ITERS']=str(iterations);env['MOSAIC_RELIABILITY_SEED']=str(seed)
    cmd=[str(ROOT/'build/mosaic-reliability-smoke'),str(ROOT/'fixtures/packs/model-v2.mpack'),str(ROOT/'fixtures/packs/unicode17-v1.mpack')]
    start=time.monotonic();out=subprocess.check_output(cmd,cwd=ROOT,env=env,text=True).strip();elapsed=time.monotonic()-start
    m=REPLAY_RE.search(out)
    if not m:raise RuntimeError('native reliability output missing replay hash')
    return {'output':out,'replay':m.group(1),'elapsed_seconds':round(elapsed,6)}


def registry_chaos()->dict:
    model=ROOT/'fixtures/packs/model-v2.mpack';unicode=ROOT/'fixtures/packs/unicode17-v1.mpack'
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)/'registry';r=Registry(root);r.init();errors=[]
        def installer(idx:int):
            try:r.install(model,'org.mosaic','reference-model','1.0.0')
            except Exception as exc:errors.append(f'{idx}:{exc}')
        threads=[threading.Thread(target=installer,args=(i,)) for i in range(16)]
        for t in threads:t.start()
        for t in threads:t.join()
        if errors:raise RuntimeError('concurrent registry install failures: '+str(errors[:3]))
        r.install(unicode,'org.mosaic','unicode17','17.0.0')
        req={'schema':1,'requirements':[{'role':'model','publisher':'org.mosaic','name':'reference-model','constraint':'^1.0.0'},{'role':'unicode','publisher':'org.mosaic','name':'unicode17','constraint':'==17.0.0'}]}
        lock=r.resolve(req)
        if r.verify_lock(lock) or r.audit():raise RuntimeError('fresh registry did not verify')
        model_hash=next(x['sha256'] for x in lock['packs'] if x['role']=='model');obj=r.object_path(model_hash)
        damaged=bytearray(obj.read_bytes());damaged[-1]^=0x80;obj.write_bytes(damaged)
        if not r.verify_lock(lock):raise RuntimeError('corrupt object escaped lock verification')
        if not r.audit():raise RuntimeError('corrupt object escaped registry audit')
        repaired=r.repair(model)
        if repaired!=model_hash or r.verify_lock(lock) or r.audit():raise RuntimeError('registry repair did not restore exact lock')
        orphan=r.objects/'ff'/('f'*64);orphan.parent.mkdir(parents=True,exist_ok=True);orphan.write_bytes(b'orphan')
        removed=r.gc()
        if removed!=1 or orphan.exists():raise RuntimeError('registry GC did not remove unreferenced object')
        return {'concurrent_installs':16,'lock_sha256':lock['lock_sha256'],'repaired_sha256':repaired,'gc_removed':removed,'catalog_sha256':r.catalog_hash()}


def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--tier',choices=TIERS,default='smoke');ap.add_argument('--seed',type=lambda x:int(x,0),default=0x5A17C0DE);ap.add_argument('--output',type=Path);a=ap.parse_args()
    iterations=TIERS[a.tier]
    first=run_native(iterations,a.seed);second=run_native(iterations,a.seed)
    if first['replay']!=second['replay'] or first['output']!=second['output']:
        raise SystemExit('deterministic reliability replay diverged')
    registry=registry_chaos()
    report={'schema':1,'tier':a.tier,'iterations':iterations,'seed':a.seed,'native':{'replay':first['replay'],'first_elapsed_seconds':first['elapsed_seconds'],'second_elapsed_seconds':second['elapsed_seconds'],'summary':first['output']},'registry':registry}
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print(json.dumps(report,sort_keys=True))
    return 0
if __name__=='__main__':raise SystemExit(main())
