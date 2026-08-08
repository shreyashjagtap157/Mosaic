#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, os, shutil, subprocess, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text(encoding="utf-8").strip()

def sha(path:Path)->str:
    h=hashlib.sha256();h.update(path.read_bytes());return h.hexdigest()

def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--output',default=str(ROOT/'dist/python'));args=ap.parse_args()
    out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
    expected=out/f'mosaic_tokenizer-{VERSION}-py3-none-any.whl'
    expected.unlink(missing_ok=True)
    env=os.environ.copy();env['SOURCE_DATE_EPOCH']='315532800';env.setdefault('PIP_DISABLE_PIP_VERSION_CHECK','1')
    subprocess.run([sys.executable,'-m','pip','wheel',str(ROOT/'bindings/python'),'--no-deps','--no-build-isolation','-w',str(out)],check=True,env=env,cwd=ROOT)
    if not expected.exists():raise SystemExit(f'expected wheel not produced: {expected}')
    print(f'{expected} sha256={sha(expected)}');return 0
if __name__=='__main__':raise SystemExit(main())
