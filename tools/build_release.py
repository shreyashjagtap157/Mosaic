#!/usr/bin/env python3
from __future__ import annotations
import argparse, gzip, hashlib, json, os, platform, shutil, subprocess, tarfile, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text().strip()
MODEL=ROOT/'fixtures/packs/m3-model-v1.mpack'
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'

def sha(path:Path)->str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1<<20),b''): h.update(chunk)
    return h.hexdigest()

def run(*cmd:str)->str:
    return subprocess.check_output(cmd,cwd=ROOT,text=True).strip()

def add_tree(tf:tarfile.TarFile, root:Path, arc_root:str):
    for p in sorted(root.rglob('*')):
        if not p.is_file(): continue
        rel=p.relative_to(root)
        info=tf.gettarinfo(str(p), arcname=f'{arc_root}/{rel.as_posix()}')
        info.uid=0;info.gid=0;info.uname='';info.gname='';info.mtime=0
        with p.open('rb') as f: tf.addfile(info,f)

def deterministic_tgz(src_dir:Path, out:Path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tar_path=Path(tmp.name)
    try:
        with tarfile.open(tar_path,'w',format=tarfile.PAX_FORMAT) as tf:
            add_tree(tf,src_dir,src_dir.name)
        with tar_path.open('rb') as raw, out.open('wb') as fout:
            with gzip.GzipFile(filename='',mode='wb',fileobj=fout,mtime=0,compresslevel=9) as gz:
                shutil.copyfileobj(raw,gz)
    finally:
        tar_path.unlink(missing_ok=True)

def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--no-build',action='store_true');args=ap.parse_args()
    if not args.no_build: subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True)
    machine=platform.machine().lower().replace('amd64','x86_64')
    osname='linux' if platform.system()=='Linux' else platform.system().lower()
    tag=f'{osname}-{machine}'
    name=f'mosaic-tokenizer-{VERSION}-{tag}'
    dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    stage=dist/name
    if stage.exists(): shutil.rmtree(stage)
    for d in ['bin','lib','include','share/mosaic/packs','share/mosaic','share/pkgconfig','docs']:(stage/d).mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'build/mosaic-tokenizer',stage/'bin/mosaic-tokenizer')
    shutil.copy2(ROOT/'build/libmosaic.so',stage/'lib/libmosaic.so')
    shutil.copy2(ROOT/'build/libmosaic.a',stage/'lib/libmosaic.a')
    shutil.copy2(ROOT/'native/include/mosaic.h',stage/'include/mosaic.h')
    shutil.copy2(MODEL,stage/'share/mosaic/packs/model-v1.mpack')
    shutil.copy2(UNICODE,stage/'share/mosaic/packs/unicode17-v1.mpack')
    shutil.copy2(ROOT/'README.md',stage/'README.md')
    shutil.copy2(ROOT/'docs/release/RELEASE_NOTES_0.1.0.md',stage/'docs/RELEASE_NOTES.md')
    shutil.copy2(ROOT/'docs/implementation/API_C_0.1.md',stage/'docs/API_C.md')
    pc=f'''prefix=/usr/local\nexec_prefix=${{prefix}}\nlibdir=${{exec_prefix}}/lib\nincludedir=${{prefix}}/include\n\nName: mosaic\nDescription: Mosaic exact byte tokenizer core\nVersion: {VERSION}\nLibs: -L${{libdir}} -lmosaic\nCflags: -I${{includedir}}\n'''
    (stage/'share/pkgconfig/mosaic.pc').write_text(pc)
    fingerprint=run(str(ROOT/'build/mosaic-tokenizer'),'fingerprint',str(MODEL),str(UNICODE))
    manifest={
      'release':'Mosaic Tokenizer','version':VERSION,'platform':tag,'c_api':'0.1.0',
      'tokenizer_fingerprint_sha256':fingerprint,
      'model_pack':{'file':'model-v1.mpack','sha256':sha(MODEL)},
      'unicode_pack':{'file':'unicode17-v1.mpack','sha256':sha(UNICODE),'unicode_version':'17.0.0'},
      'artifacts':{}
    }
    for rel in ['bin/mosaic-tokenizer','lib/libmosaic.so','lib/libmosaic.a','include/mosaic.h']:
        manifest['artifacts'][rel]=sha(stage/rel)
    (stage/'share/mosaic/release-manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    sums=[]
    for p in sorted(stage.rglob('*')):
        if p.is_file() and p.name!='SHA256SUMS': sums.append(f'{sha(p)}  {p.relative_to(stage).as_posix()}')
    (stage/'SHA256SUMS').write_text('\n'.join(sums)+'\n')
    archive=dist/f'{name}.tar.gz';deterministic_tgz(stage,archive)
    print(json.dumps({'stage':str(stage),'archive':str(archive),'archive_sha256':sha(archive),'fingerprint':fingerprint},indent=2))
    return 0
if __name__=='__main__': raise SystemExit(main())
