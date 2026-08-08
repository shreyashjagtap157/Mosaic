#!/usr/bin/env python3
"""Release-qualify the executable native Mosaic v0.11 tokenizer on this host."""
from __future__ import annotations
import os, shutil, subprocess, sys, tempfile, statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MODEL=ROOT/'fixtures/packs/model-v2.mpack'
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'

def run(cmd, env=None):
    print('+',' '.join(map(str,cmd)),flush=True)
    subprocess.run([str(x) for x in cmd],cwd=ROOT,check=True,env=env)

def main()->int:
    required=['gcc','clang','make','ar','c++','python3','pkg-config','/usr/bin/time']
    missing=[x for x in required if not (Path(x).exists() if x.startswith('/') else shutil.which(x))]
    if missing:
        print('FAIL: missing native qualification tools: '+', '.join(missing),file=sys.stderr);return 2
    py=[sys.executable]
    gates=[
        ['tools/generate_empty_pack.py','--check'],['tools/build_m2_fixture.py','--check'],
        ['tools/generate_m2_malformed.py','--check'],['tools/build_m3_model_fixture.py','--check'],
        ['tools/generate_m3_malformed.py','--check'],['tools/build_model_v2_fixture.py','--check'],['tools/build_tiktoken_compat_fixture.py','--check'],
        ['tools/build_language_packs.py','--check'],['tools/generate_language_malformed.py','--check'],['tools/build_detector_pack.py','--check'],['tools/generate_detector_malformed.py','--check'],['tools/build_unicode17_pack.py','--check'],['tools/build_security17_pack.py','--check'],['tools/generate_security17_malformed.py','--check'],['tools/build_normalization16_pack.py','--check'],['tools/generate_normalization16_malformed.py','--check'],['tools/build_online_stream_fixture.py','--check'],
        ['tools/generate_unicode17_malformed.py','--check'],['tools/validate_m2_fixture.py'],
        ['tools/validate_path_order.py'],['tools/validate_manifest_identity.py'],
        ['tools/validate_m3_model.py'],['tools/validate_unicode17.py'],['tools/validate_repo.py']]
    for g in gates: run(py+g)
    run(['make','-C','native','clean','all','test'])
    for script in ['validate_c_reference.py','validate_c_malformed.py','validate_c_unicode.py','validate_c_unicode_malformed.py','validate_c_api.py','validate_language_packs.py','validate_detector.py','validate_authoring.py','validate_tiktoken_compat.py','validate_security17.py','benchmark_language_packs.py','benchmark_detector.py','benchmark_bounded_processing.py','benchmark_incremental.py','benchmark_resync.py']:
        run(py+['tools/'+script])
    # Sanitizer stress and all malformed classes run inside long-lived C processes above.
    # Clang is a genuinely independent compiler gate.
    clang=ROOT/'build/clang';clang.mkdir(parents=True,exist_ok=True)
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','native/src/mosaic.c','-o',clang/'mosaic-tokenizer'])
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-fPIC','-Inative/include','-DMOSAIC_LIBRARY_ONLY','-c','native/src/mosaic.c','-o',clang/'mosaic_lib.o'])
    run(['clang','-shared',clang/'mosaic_lib.o','-o',clang/'libmosaic.so'])
    # Exercise the Clang build in long-lived ABI tests; the full CLI differential already
    # qualified the GCC build above and would otherwise spend most of release time in process startup.
    run([clang/'mosaic-tokenizer','--version'])
    clang_api=os.environ.copy();clang_api['MOSAIC_LIB']=str(clang/'libmosaic.so');run(py+['tools/validate_c_api.py'],clang_api);run(py+['tools/validate_language_packs.py'],clang_api);run(py+['tools/validate_detector.py'],clang_api)
    icu_cflags=subprocess.check_output(['pkg-config','--cflags','icu-uc'],text=True).split()
    icu_libs=subprocess.check_output(['pkg-config','--libs','icu-uc'],text=True).split()
    norm_smoke=clang/'mosaic-normalization-smoke'
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include',*icu_cflags,'conformance/c/normalization_smoke.c',clang/'mosaic_lib.o',*icu_libs,'-o',norm_smoke])
    run([norm_smoke,ROOT/'fixtures/packs/normalization16-v1.mpack',MODEL,UNICODE])
    online_smoke=clang/'mosaic-online-stream-smoke'
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/online_stream_smoke.c',clang/'mosaic_lib.o','-o',online_smoke])
    run([online_smoke,MODEL,UNICODE,ROOT/'fixtures/packs/language/en-v1.mpack',ROOT/'fixtures/packs/language/hi-v1.mpack',ROOT/'fixtures/packs/language/ja-v1.mpack',ROOT/'fixtures/packs/raw-bpe-v1.mpack',ROOT/'fixtures/packs/online-adversarial-v1.mpack'])
    incremental_smoke=clang/'mosaic-incremental-smoke'
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/incremental_smoke.c',clang/'mosaic_lib.o','-o',incremental_smoke])
    run([incremental_smoke,MODEL,UNICODE,ROOT/'fixtures/packs/language/en-v1.mpack',ROOT/'fixtures/packs/language/hi-v1.mpack',ROOT/'fixtures/packs/language/ja-v1.mpack',ROOT/'fixtures/packs/raw-bpe-v1.mpack'])
    resync_smoke=clang/'mosaic-resync-smoke'
    run(['clang','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-Inative/include','conformance/c/resync_smoke.c',clang/'mosaic_lib.o','-o',resync_smoke])
    clang_resync_env=os.environ.copy(); clang_resync_env['MOSAIC_RESYNC_EDITS']='100'
    run([resync_smoke,MODEL,UNICODE,ROOT/'fixtures/packs/language/en-v1.mpack',ROOT/'fixtures/packs/language/hi-v1.mpack',ROOT/'fixtures/packs/language/ja-v1.mpack',ROOT/'fixtures/packs/raw-bpe-v1.mpack'],clang_resync_env)
    # Deterministic 10 MiB benchmark fixture and conservative regression floor.
    bench=Path(tempfile.gettempdir())/'mosaic-release-10m.bin'
    chunk=b'hello world tokenizers :: value->_id '+ 'नमस्ते 世界 こんにちは\n'.encode()
    n=10*1024*1024;bench.write_bytes((chunk*((n//len(chunk))+1))[:n])
    timefile=Path(tempfile.gettempdir())/'mosaic-release-time.txt'
    # Warm once, then use a median of three timings. A single cold/page-faulted sample is not a release metric.
    run([ROOT/'build/mosaic-tokenizer','roundtrip',MODEL,bench])
    elapsed_samples=[]; rss_samples=[]
    for _ in range(3):
        run(['/usr/bin/time','-f','%e %M','-o',timefile,ROOT/'build/mosaic-tokenizer','roundtrip',MODEL,bench])
        elapsed,rss=map(float,timefile.read_text().split());elapsed_samples.append(elapsed);rss_samples.append(rss)
    elapsed_s=statistics.median(elapsed_samples);rss_kb=max(rss_samples);throughput=(n/(1024*1024))/max(elapsed_s,1e-9)
    if throughput < 20.0: raise SystemExit(f'FAIL: median throughput floor: {throughput:.1f} MiB/s < 20')
    if rss_kb > 131072: raise SystemExit(f'FAIL: RSS ceiling: {rss_kb:.0f} KiB > 131072')
    if (ROOT/'build/mosaic-tokenizer').stat().st_size > 1024*1024: raise SystemExit('FAIL: native CLI exceeds 1 MiB')
    print(f'PASS benchmark: {throughput:.1f} MiB/s, maxrss={rss_kb/1024:.1f} MiB')
    print('PASS: Mosaic native v0.11 release qualification completed')
    print('NOTE: Stable Rust reference remains separately blocked by unavailable rustc/cargo on this host')
    return 0
if __name__=='__main__':raise SystemExit(main())
