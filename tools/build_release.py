#!/usr/bin/env python3
from __future__ import annotations
import argparse, gzip, hashlib, json, os, platform, shutil, subprocess, sys, tarfile, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text(encoding="utf-8").strip()
BUILD_DIR=Path(os.environ.get('MOSAIC_BUILD_DIR', str(ROOT/'build')))
MODEL=ROOT/'fixtures/packs/model-v2.mpack'
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'
LANGUAGES={tag:ROOT/f'fixtures/packs/language/{tag}-v1.mpack' for tag in ('en','hi','ja')}
DETECTOR=ROOT/'fixtures/packs/detector/reference-v1.mpack'
SECURITY=ROOT/'fixtures/packs/security17-v1.mpack'
NORMALIZATION=ROOT/'fixtures/packs/normalization16-v1.mpack'
LEXERS={tag:ROOT/f'fixtures/packs/lexer/{tag}-v1.mpack' for tag in ('c','python','rust','json')}

def choose(*names:str)->Path:
    for name in names:
        path=BUILD_DIR/name
        if path.exists():
            return path
        if (BUILD_DIR/'cmake'/name).exists():
            return BUILD_DIR/'cmake'/name
        if (BUILD_DIR/'cmake'/'Release'/name).exists():
            return BUILD_DIR/'cmake'/'Release'/name
        if (BUILD_DIR/'preset-core-release'/'native'/name).exists():
            return BUILD_DIR/'preset-core-release'/'native'/name
    return BUILD_DIR/names[0]

BIN=Path(os.environ.get('MOSAIC_TOKENIZER', str(choose('mosaic-tokenizer.exe','mosaic-tokenizer'))))
CORE_DLL=choose('mosaic.dll','libmosaic.so')
CORE_STATIC=choose('mosaic_static.lib','libmosaic.a')
TRUST_DLL=choose('mosaic_trust.dll','libmosaic_trust.so')
TRUST_STATIC=choose('mosaic_trust_static.lib','libmosaic_trust.a')
OPENSSL_STAGE=Path(os.environ['MOSAIC_OPENSSL_STAGE']) if os.environ.get('MOSAIC_OPENSSL_STAGE') else None

def sha(path:Path)->str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1<<20),b''): h.update(chunk)
    return h.hexdigest()

def run(*cmd:str)->str:
    return subprocess.check_output(cmd,cwd=ROOT,text=True).strip()

def invoke(*cmd:str)->list[str]:
    items=[str(x) for x in cmd]
    if items and items[0].lower().endswith('.py'):
        return [sys.executable, *items]
    return items

def git_executable()->str:
    found=shutil.which('git')
    if found:
        return found
    bundled=Path(sys.executable).resolve().parents[1]/'native'/'git'/'cmd'/'git.exe'
    if bundled.exists():
        return str(bundled)
    return 'git'

def git_text(*args:str)->str:
    return subprocess.check_output([git_executable(),'-c',f'safe.directory={ROOT}',*args],cwd=ROOT,text=True).strip()

def c_api_version()->str:
    text=(ROOT/'native/include/mosaic.h').read_text(encoding="utf-8")
    values={}
    for key in ('MAJOR','MINOR','PATCH'):
        prefix=f'#define MOSAIC_C_API_VERSION_{key} '
        line=next(line for line in text.splitlines() if line.startswith(prefix))
        values[key]=int(line[len(prefix):])
    return f"{values['MAJOR']}.{values['MINOR']}.{values['PATCH']}"

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
    ap=argparse.ArgumentParser();ap.add_argument('--no-build',action='store_true');ap.add_argument('--allow-dirty',action='store_true',help='permit a preflight release from a dirty Git worktree');args=ap.parse_args()
    dirty=git_text('status','--porcelain')
    if dirty and not args.allow_dirty:
        raise SystemExit('refusing official release build from dirty Git worktree; commit changes or use --allow-dirty for preflight qualification')
    if not args.no_build and not os.environ.get('MOSAIC_BUILD_DIR'):subprocess.run(['make','-C','native','all'],cwd=ROOT,check=True)
    subprocess.run(invoke(ROOT/'tools/build_python_binding.py'),cwd=ROOT,check=True)
    machine=platform.machine().lower().replace('amd64','x86_64');osname='linux' if platform.system()=='Linux' else platform.system().lower();tag=os.environ.get('MOSAIC_RELEASE_TAG') or f'{osname}-{machine}'
    name=f'mosaic-tokenizer-{VERSION}-{tag}';dist=ROOT/'dist';dist.mkdir(exist_ok=True);stage=dist/name
    if stage.exists():shutil.rmtree(stage)
    for d in ['bin','lib','include','share/mosaic/packs/language','share/mosaic/packs/detector','share/mosaic/packs/lexer','share/mosaic/trust','share/mosaic','share/pkgconfig','docs','docs/release','examples/authoring','examples/integration','python']:(stage/d).mkdir(parents=True,exist_ok=True)
    trust_enabled = TRUST_DLL.exists() and TRUST_STATIC.exists()
    core_copies = [(BIN,stage/'bin/mosaic-tokenizer'),(CORE_DLL,stage/'lib/libmosaic.so'),(CORE_STATIC,stage/'lib/libmosaic.a'),(ROOT/'native/include/mosaic.h',stage/'include/mosaic.h'),(MODEL,stage/'share/mosaic/packs/model-v2.mpack'),(UNICODE,stage/'share/mosaic/packs/unicode17-v1.mpack'),(DETECTOR,stage/'share/mosaic/packs/detector/reference-v1.mpack'),(SECURITY,stage/'share/mosaic/packs/security17-v1.mpack'),(NORMALIZATION,stage/'share/mosaic/packs/normalization16-v1.mpack'),(ROOT/'README.md',stage/'README.md'),(ROOT/'tools/mosaic_author.py',stage/'bin/mosaic-author'),(ROOT/'tools/mosaic_registry.py',stage/'bin/mosaic-registry'),(ROOT/'tools/mosaic_registry.py',stage/'bin/mosaic_registry.py'),(ROOT/'tools/mosaic_registry_http.py',stage/'bin/mosaic-registry-http'),(ROOT/'tools/mosaicd.py',stage/'bin/mosaicd'),(ROOT/'tools/mosaicd_client.py',stage/'bin/mosaicd_client.py')]
    for src,dst in core_copies:shutil.copy2(src,dst)
    if trust_enabled:
        for src,dst in [(TRUST_DLL,stage/'lib/libmosaic_trust.so'),(TRUST_STATIC,stage/'lib/libmosaic_trust.a'),(ROOT/'native/include/mosaic_trust.h',stage/'include/mosaic_trust.h')]:
            shutil.copy2(src,dst)
    if OPENSSL_STAGE:
        for src,dst in [(OPENSSL_STAGE/'lib/libcrypto.lib',stage/'lib/libcrypto.lib'),(OPENSSL_STAGE/'bin/libcrypto-3-x64.dll',stage/'bin/libcrypto-3-x64.dll')]:
            if src.exists(): shutil.copy2(src,dst)
    for ltag,path in LANGUAGES.items():shutil.copy2(path,stage/f'share/mosaic/packs/language/{ltag}-v1.mpack')
    for ltag,path in LEXERS.items():shutil.copy2(path,stage/f'share/mosaic/packs/lexer/{ltag}-v1.mpack')
    if trust_enabled:
        shutil.copy2(ROOT/'fixtures/trust/conformance-ed25519.pub',stage/'share/mosaic/trust/conformance-ed25519.pub')
        shutil.copy2(ROOT/'fixtures/packs/model-v2.mpack.sig',stage/'share/mosaic/trust/model-v2.mpack.sig')
    for ex in sorted((ROOT/'examples/authoring').glob('*.json')): shutil.copy2(ex,stage/'examples/authoring'/ex.name)
    for ex in [ROOT/'examples/integration/README.md', ROOT/'examples/integration/low_memory_embed.c', ROOT/'examples/integration/low_memory_embed.py', ROOT/'examples/integration/mosaicd_client_example.py']:
        if ex.exists(): shutil.copy2(ex,stage/'examples/integration'/ex.name)
    shutil.copy2(ROOT/'tools/mosaicd_client.py', stage/'examples/integration/mosaicd_client.py')
    py_wheel=ROOT/f'dist/python/mosaic_tokenizer-{VERSION}-py3-none-any.whl'
    shutil.copy2(py_wheel,stage/'python'/py_wheel.name)
    shutil.copy2(ROOT/'docs/implementation/AUTHORING_0.5.md',stage/'docs/AUTHORING.md')
    shutil.copy2(ROOT/'docs/implementation/COMPATIBILITY_0.6.md',stage/'docs/COMPATIBILITY.md')
    shutil.copy2(ROOT/'docs/implementation/RUNTIME_POLICY_v1.md',stage/'docs/RUNTIME_POLICY.md')
    release_index=ROOT/'docs/release/README.md'
    if release_index.exists(): shutil.copy2(release_index,stage/'docs/release/README.md')
    release_qualification=ROOT/f'docs/release/QUALIFICATION_{VERSION}.md'
    if release_qualification.exists(): shutil.copy2(release_qualification,stage/f'docs/release/QUALIFICATION_{VERSION}.md')
    release_windows_qualification=ROOT/f'docs/release/QUALIFICATION_{VERSION}_WINDOWS_PACKAGE.md'
    if release_windows_qualification.exists(): shutil.copy2(release_windows_qualification,stage/f'docs/release/QUALIFICATION_{VERSION}_WINDOWS_PACKAGE.md')
    release_notes_100=ROOT/'docs/release/RELEASE_NOTES_1.0.0.md'
    if release_notes_100.exists(): shutil.copy2(release_notes_100,stage/'docs/release/RELEASE_NOTES_1.0.0.md')
    release_notes_101=ROOT/'docs/release/RELEASE_NOTES_1.0.1.md'
    if release_notes_101.exists(): shutil.copy2(release_notes_101,stage/'docs/release/RELEASE_NOTES_1.0.1.md')
    trust_doc=ROOT/'docs/implementation/TRUST_v1.md'
    if trust_doc.exists(): shutil.copy2(trust_doc,stage/'docs/TRUST.md')
    registry_doc=ROOT/'docs/implementation/REGISTRY_v1.md'
    if registry_doc.exists(): shutil.copy2(registry_doc,stage/'docs/REGISTRY.md')
    token_ir_doc=ROOT/'docs/implementation/TOKEN_DOCUMENT_SERIALIZATION_v1.md'
    if token_ir_doc.exists(): shutil.copy2(token_ir_doc,stage/'docs/TOKEN_DOCUMENT_SERIALIZATION.md')
    parallel_doc=ROOT/'docs/implementation/PARALLEL_EXECUTOR_0.23.md'
    if parallel_doc.exists(): shutil.copy2(parallel_doc,stage/'docs/PARALLEL_EXECUTOR.md')
    observability_doc=ROOT/'docs/implementation/OBSERVABILITY_0.24.md'
    if observability_doc.exists(): shutil.copy2(observability_doc,stage/'docs/OBSERVABILITY.md')
    python_doc=ROOT/'docs/implementation/PYTHON_BINDING_0.25.md'
    if python_doc.exists(): shutil.copy2(python_doc,stage/'docs/PYTHON_BINDING.md')
    for src,doc_name in [(ROOT/'SECURITY.md','SECURITY.md'),(ROOT/'SUPPORT.md','SUPPORT.md'),(ROOT/'docs/implementation/RELEASE_ENGINEERING_v1.md','RELEASE_ENGINEERING.md')]:
        if src.exists(): shutil.copy2(src,stage/'docs'/doc_name)
    (stage/'bin/mosaic-author').chmod(0o755);(stage/'bin/mosaic-registry').chmod(0o755);(stage/'bin/mosaic-registry-http').chmod(0o755);(stage/'bin/mosaicd').chmod(0o755)
    release_notes=ROOT/f'docs/release/RELEASE_NOTES_{VERSION}.md'
    if release_notes.exists():shutil.copy2(release_notes,stage/'docs/RELEASE_NOTES.md')
    api_version=c_api_version()
    api_major_minor='.'.join(api_version.split('.')[:2])
    api_doc=ROOT/f'docs/implementation/API_C_{api_major_minor}.md'
    if not api_doc.exists():
        raise RuntimeError(f'missing C API documentation for ABI {api_version}: {api_doc.relative_to(ROOT)}')
    shutil.copy2(api_doc,stage/'docs/API_C.md')
    for policy_src,policy_name in [
        (ROOT/'docs/VERSIONING_POLICY.md','VERSIONING_POLICY.md'),
        (ROOT/'docs/COMPATIBILITY_POLICY.md','COMPATIBILITY_POLICY.md'),
        (ROOT/'docs/DEPRECATION_POLICY.md','DEPRECATION_POLICY.md'),
    ]:
        shutil.copy2(policy_src,stage/'docs'/policy_name)
    pc=f'''prefix=/usr/local\nexec_prefix=${{prefix}}\nlibdir=${{exec_prefix}}/lib\nincludedir=${{prefix}}/include\n\nName: mosaic\nDescription: Mosaic exact byte tokenizer core\nVersion: {VERSION}\nLibs: -L${{libdir}} -lmosaic\nCflags: -I${{includedir}}\n''';(stage/'share/pkgconfig/mosaic.pc').write_text(pc, encoding="utf-8")
    fingerprint=run(str(BIN),'fingerprint',str(MODEL),str(UNICODE))
    language_fingerprint=run(str(BIN),'fingerprint-languages',str(MODEL),str(UNICODE),*(str(LANGUAGES[t]) for t in ('en','hi','ja')))
    security_fingerprint=run(str(BIN),'fingerprint-security',str(MODEL),str(UNICODE),str(SECURITY))
    normalization_fingerprint=run(str(BIN),'fingerprint-normalization',str(MODEL),str(UNICODE),str(NORMALIZATION))
    lexer_fingerprints={t:run(str(BIN),'fingerprint-lexer',str(MODEL),str(UNICODE),str(p)) for t,p in LEXERS.items()}
    auto_fingerprint=run(str(BIN),'fingerprint-auto',str(MODEL),str(UNICODE),str(DETECTOR),*(str(LANGUAGES[t]) for t in ('en','hi','ja')))
    manifest={'release':'Mosaic Tokenizer','version':VERSION,'platform':tag,'c_api':c_api_version(),'tokenizer_semantics_version':2,'tokenizer_fingerprint_sha256':fingerprint,'reference_language_fingerprint_sha256':language_fingerprint,'reference_auto_fingerprint_sha256':auto_fingerprint,'reference_security_fingerprint_sha256':security_fingerprint,'reference_normalization_fingerprint_sha256':normalization_fingerprint,'model_pack':{'file':'model-v2.mpack','sha256':sha(MODEL)},'unicode_pack':{'file':'unicode17-v1.mpack','sha256':sha(UNICODE),'unicode_version':'17.0.0'},'detector_pack':{'file':'detector/reference-v1.mpack','sha256':sha(DETECTOR)},'security_pack':{'file':'security17-v1.mpack','sha256':sha(SECURITY),'unicode_version':'17.0.0'},'normalization_pack':{'file':'normalization16-v1.mpack','sha256':sha(NORMALIZATION),'unicode_version':'16.0.0','icu_generator_version':'76.1'},'trust_enabled':trust_enabled,'language_packs':{t:{'file':f'language/{t}-v1.mpack','sha256':sha(p)} for t,p in LANGUAGES.items()},'lexer_packs':{t:{'file':f'lexer/{t}-v1.mpack','sha256':sha(p),'tokenizer_fingerprint_sha256':lexer_fingerprints[t]} for t,p in LEXERS.items()},'python_binding':{'file':f'mosaic_tokenizer-{VERSION}-py3-none-any.whl','sha256':sha(py_wheel)},'artifacts':{}}
    for rel in ['bin/mosaic-tokenizer','bin/mosaic-author','bin/mosaic-registry','bin/mosaic_registry.py','bin/mosaic-registry-http','bin/mosaicd','bin/mosaicd_client.py','lib/libmosaic.so','lib/libmosaic.a','include/mosaic.h','examples/integration/mosaicd_client.py']:
        manifest['artifacts'][rel]=sha(stage/rel)
    if trust_enabled:
        for rel in ['lib/libmosaic_trust.so','lib/libmosaic_trust.a','include/mosaic_trust.h']:
            manifest['artifacts'][rel]=sha(stage/rel)
    (stage/'share/mosaic/release-manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n', encoding="utf-8")
    subprocess.run(invoke(ROOT/'tools/generate_sbom.py',str(stage),'--version',VERSION,'--output',str(stage/'share/mosaic/sbom.spdx.json')),cwd=ROOT,check=True)
    subprocess.run(invoke(ROOT/'tools/generate_provenance.py',str(stage),'--version',VERSION,'--source-checksums',str(ROOT/'ARTIFACT_CHECKSUMS.sha256'),'--output',str(stage/'share/mosaic/provenance.intoto.json')),cwd=ROOT,check=True)
    sums=[]
    for p in sorted(stage.rglob('*')):
        if p.is_file() and p.name!='SHA256SUMS':sums.append(f'{sha(p)}  {p.relative_to(stage).as_posix()}')
    (stage/'SHA256SUMS').write_text('\n'.join(sums)+'\n', encoding="utf-8")
    archive=dist/f'{name}.tar.gz'
    if archive.name != f'mosaic-tokenizer-{VERSION}-{tag}.tar.gz':
        raise RuntimeError(f'unexpected release archive name: {archive.name}')
    deterministic_tgz(stage,archive)
    print(json.dumps({'stage':str(stage),'archive':str(archive),'archive_sha256':sha(archive),'fingerprint':fingerprint,'language_fingerprint':language_fingerprint,'auto_fingerprint':auto_fingerprint,'security_fingerprint':security_fingerprint,'normalization_fingerprint':normalization_fingerprint,'lexer_fingerprints':lexer_fingerprints},indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
