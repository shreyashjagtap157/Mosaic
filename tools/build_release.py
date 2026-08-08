#!/usr/bin/env python3
from __future__ import annotations
import argparse, gzip, hashlib, json, platform, shutil, subprocess, tarfile, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text().strip()
MODEL=ROOT/'fixtures/packs/model-v2.mpack'
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'
LANGUAGES={tag:ROOT/f'fixtures/packs/language/{tag}-v1.mpack' for tag in ('en','hi','ja')}
DETECTOR=ROOT/'fixtures/packs/detector/reference-v1.mpack'

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
        rel=p.relative_to(root);info=tf.gettarinfo(str(p),arcname=f'{arc_root}/{rel.as_posix()}')
        info.uid=0;info.gid=0;info.uname='';info.gname='';info.mtime=0
        with p.open('rb') as f: tf.addfile(info,f)

def deterministic_tgz(src_dir:Path,out:Path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp: tar_path=Path(tmp.name)
    try:
        with tarfile.open(tar_path,'w',format=tarfile.PAX_FORMAT) as tf:add_tree(tf,src_dir,src_dir.name)
        with tar_path.open('rb') as raw,out.open('wb') as fout:
            with gzip.GzipFile(filename='',mode='wb',fileobj=fout,mtime=0,compresslevel=9) as gz:shutil.copyfileobj(raw,gz)
    finally:tar_path.unlink(missing_ok=True)

def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--no-build',action='store_true');args=ap.parse_args()
    if not args.no_build:subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True)
    machine=platform.machine().lower().replace('amd64','x86_64');osname='linux' if platform.system()=='Linux' else platform.system().lower();tag=f'{osname}-{machine}'
    name=f'mosaic-tokenizer-{VERSION}-{tag}';dist=ROOT/'dist';dist.mkdir(exist_ok=True);stage=dist/name
    if stage.exists():shutil.rmtree(stage)
    for d in ['bin','lib','include','share/mosaic/packs/language','share/mosaic/packs/detector','share/mosaic','share/pkgconfig','docs','examples/authoring']:(stage/d).mkdir(parents=True,exist_ok=True)
    for src,dst in [(ROOT/'build/mosaic-tokenizer',stage/'bin/mosaic-tokenizer'),(ROOT/'build/libmosaic.so',stage/'lib/libmosaic.so'),(ROOT/'build/libmosaic.a',stage/'lib/libmosaic.a'),(ROOT/'native/include/mosaic.h',stage/'include/mosaic.h'),(MODEL,stage/'share/mosaic/packs/model-v2.mpack'),(UNICODE,stage/'share/mosaic/packs/unicode17-v1.mpack'),(DETECTOR,stage/'share/mosaic/packs/detector/reference-v1.mpack'),(ROOT/'README.md',stage/'README.md'),(ROOT/'tools/mosaic_author.py',stage/'bin/mosaic-author')]:shutil.copy2(src,dst)
    for ltag,path in LANGUAGES.items():shutil.copy2(path,stage/f'share/mosaic/packs/language/{ltag}-v1.mpack')
    for ex in sorted((ROOT/'examples/authoring').glob('*.json')): shutil.copy2(ex,stage/'examples/authoring'/ex.name)
    shutil.copy2(ROOT/'docs/implementation/AUTHORING_0.5.md',stage/'docs/AUTHORING.md')
    (stage/'bin/mosaic-author').chmod(0o755)
    release_notes=ROOT/f'docs/release/RELEASE_NOTES_{VERSION}.md'
    if release_notes.exists():shutil.copy2(release_notes,stage/'docs/RELEASE_NOTES.md')
    api_doc=ROOT/f'docs/implementation/API_C_{VERSION.rsplit(".",1)[0]}.md'
    if not api_doc.exists():api_doc=ROOT/'docs/implementation/API_C_0.1.md'
    shutil.copy2(api_doc,stage/'docs/API_C.md')
    pc=f'''prefix=/usr/local\nexec_prefix=${{prefix}}\nlibdir=${{exec_prefix}}/lib\nincludedir=${{prefix}}/include\n\nName: mosaic\nDescription: Mosaic exact byte tokenizer core\nVersion: {VERSION}\nLibs: -L${{libdir}} -lmosaic\nCflags: -I${{includedir}}\n''';(stage/'share/pkgconfig/mosaic.pc').write_text(pc)
    fingerprint=run(str(ROOT/'build/mosaic-tokenizer'),'fingerprint',str(MODEL),str(UNICODE))
    language_fingerprint=run(str(ROOT/'build/mosaic-tokenizer'),'fingerprint-languages',str(MODEL),str(UNICODE),*(str(LANGUAGES[t]) for t in ('en','hi','ja')))
    auto_fingerprint=run(str(ROOT/'build/mosaic-tokenizer'),'fingerprint-auto',str(MODEL),str(UNICODE),str(DETECTOR),*(str(LANGUAGES[t]) for t in ('en','hi','ja')))
    manifest={'release':'Mosaic Tokenizer','version':VERSION,'platform':tag,'c_api':'0.4.0','tokenizer_fingerprint_sha256':fingerprint,'reference_language_fingerprint_sha256':language_fingerprint,'reference_auto_fingerprint_sha256':auto_fingerprint,'model_pack':{'file':'model-v2.mpack','sha256':sha(MODEL)},'unicode_pack':{'file':'unicode17-v1.mpack','sha256':sha(UNICODE),'unicode_version':'17.0.0'},'detector_pack':{'file':'detector/reference-v1.mpack','sha256':sha(DETECTOR)},'language_packs':{t:{'file':f'language/{t}-v1.mpack','sha256':sha(p)} for t,p in LANGUAGES.items()},'artifacts':{}}
    for rel in ['bin/mosaic-tokenizer','bin/mosaic-author','lib/libmosaic.so','lib/libmosaic.a','include/mosaic.h']:manifest['artifacts'][rel]=sha(stage/rel)
    (stage/'share/mosaic/release-manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    sums=[]
    for p in sorted(stage.rglob('*')):
        if p.is_file() and p.name!='SHA256SUMS':sums.append(f'{sha(p)}  {p.relative_to(stage).as_posix()}')
    (stage/'SHA256SUMS').write_text('\n'.join(sums)+'\n')
    archive=dist/f'{name}.tar.gz';deterministic_tgz(stage,archive)
    print(json.dumps({'stage':str(stage),'archive':str(archive),'archive_sha256':sha(archive),'fingerprint':fingerprint,'language_fingerprint':language_fingerprint,'auto_fingerprint':auto_fingerprint},indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
