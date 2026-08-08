#!/usr/bin/env python3
from __future__ import annotations
import re, statistics, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
N=10*1024*1024
CHUNK=b'hello world tokenizer :: '+ 'नमस्ते दुनिया こんにちは世界\n'.encode()

def timed(cmd, timefile):
    subprocess.run(['/usr/bin/time','-f','%e %M','-o',str(timefile),*map(str,cmd)],cwd=ROOT,check=True,stdout=subprocess.PIPE,text=True)
    e,r=timefile.read_text().split();return float(e),float(r)

def main():
    subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/online_stream_bench.c','build/libmosaic.a','-o','build/mosaic-online-stream-bench'],cwd=ROOT,check=True)
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/security_visitor_bench.c','build/libmosaic.a','-o','build/mosaic-security-visitor-bench'],cwd=ROOT,check=True)
    with tempfile.TemporaryDirectory() as td:
        td=Path(td); data=td/'input.bin'; data.write_bytes((CHUNK*((N//len(CHUNK))+1))[:N]);tf=td/'time.txt'
        # warm each path once
        out=subprocess.check_output([ROOT/'build/mosaic-online-stream-bench',ROOT/'fixtures/packs/model-v2.mpack',data,'1048576'],text=True)
        m=re.search(r'max_pending=(\d+)',out); assert m; max_pending=int(m.group(1))
        subprocess.check_output([ROOT/'build/mosaic-security-visitor-bench',ROOT/'fixtures/packs/security17-v1.mpack',data],text=True)
        oe=[];or_= []; se=[];sr=[]
        for _ in range(3):
            e,r=timed([ROOT/'build/mosaic-online-stream-bench',ROOT/'fixtures/packs/model-v2.mpack',data,'1048576'],tf);oe.append(e);or_.append(r)
            e,r=timed([ROOT/'build/mosaic-security-visitor-bench',ROOT/'fixtures/packs/security17-v1.mpack',data],tf);se.append(e);sr.append(r)
        o=statistics.median(oe);s=statistics.median(se);oth=10/max(o,1e-9);sth=10/max(s,1e-9)
        if max_pending>1048576: raise SystemExit('FAIL: online pending cap exceeded')
        if oth<20: raise SystemExit(f'FAIL: online throughput {oth:.1f} MiB/s < 20')
        if max(or_)>65536: raise SystemExit(f'FAIL: online RSS {max(or_):.0f} KiB > 65536')
        if sth<10: raise SystemExit(f'FAIL: visitor throughput {sth:.1f} MiB/s < 10')
        if max(sr)>65536: raise SystemExit(f'FAIL: visitor RSS {max(sr):.0f} KiB > 65536')
        print(f'PASS bounded processing: online={oth:.1f} MiB/s max_pending={max_pending} maxrss={max(or_)/1024:.1f} MiB; security_visit={sth:.1f} MiB/s maxrss={max(sr)/1024:.1f} MiB')
if __name__=='__main__':main()
