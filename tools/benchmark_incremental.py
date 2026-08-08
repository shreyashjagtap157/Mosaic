#!/usr/bin/env python3
from __future__ import annotations
import re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    exe=ROOT/'build/mosaic-incremental-bench'
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/incremental_bench.c','build/libmosaic.a','-o',exe],cwd=ROOT,check=True)
    out=subprocess.check_output([exe,ROOT/'fixtures/packs/model-v2.mpack'],cwd=ROOT,text=True).strip();print(out)
    m=re.search(r'bytes=(\d+) reprocessed=(\d+) reused=(\d+) inc=([0-9.]+) full=([0-9.]+) speedup=([0-9.]+)',out)
    if not m:raise SystemExit('FAIL: malformed incremental benchmark output')
    n,r,reuse,inc,full,speed=m.groups();n=int(n);r=int(r);reuse=int(reuse);speed=float(speed)
    if r>n//50:raise SystemExit(f'FAIL: near-end edit reprocessed {r}/{n} > 2%')
    if reuse+r!=n:raise SystemExit('FAIL: incremental byte accounting mismatch')
    if speed<2.0:raise SystemExit(f'FAIL: near-end incremental speedup {speed:.2f}x < 2x')
    print(f'PASS incremental benchmark: reprocessed={100*r/n:.3f}% speedup={speed:.2f}x')
if __name__=='__main__':main()
