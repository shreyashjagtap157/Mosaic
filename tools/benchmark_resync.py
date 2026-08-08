#!/usr/bin/env python3
from __future__ import annotations
import re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    exe=ROOT/'build/mosaic-resync-bench'
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/resync_bench.c','build/libmosaic.a','-o',exe],cwd=ROOT,check=True)
    timefile=ROOT/'build/resync-time.txt'
    p=subprocess.run(['/usr/bin/time','-f','%M','-o',timefile,exe,ROOT/'fixtures/packs/model-v2.mpack'],cwd=ROOT,text=True,capture_output=True,check=True)
    out=p.stdout.strip(); print(out)
    m=re.search(r'bytes=(\d+) reprocessed=(\d+) reused_prefix=(\d+) reused_suffix=(\d+) inc=([0-9.]+) full=([0-9.]+) speedup=([0-9.]+)',out)
    if not m: raise SystemExit('FAIL: malformed resync benchmark output')
    n,r,pre,suf,inc,full,speed=m.groups(); n=int(n);r=int(r);pre=int(pre);suf=int(suf);speed=float(speed)
    rss_kb=int(timefile.read_text(encoding="utf-8").strip())
    if r>n//50: raise SystemExit(f'FAIL: middle edit reprocessed {r}/{n} > 2%')
    if pre<n//3 or suf<n//3: raise SystemExit('FAIL: checkpoint resync did not reuse both sides materially')
    if speed<3.0: raise SystemExit(f'FAIL: resync speedup {speed:.2f}x < 3x')
    if rss_kb>262144: raise SystemExit(f'FAIL: resync comparison benchmark RSS {rss_kb} KiB > 262144 KiB')
    print(f'PASS resync benchmark: reprocessed={100*r/n:.3f}% speedup={speed:.2f}x maxrss={rss_kb/1024:.1f} MiB')
if __name__=='__main__': main()
