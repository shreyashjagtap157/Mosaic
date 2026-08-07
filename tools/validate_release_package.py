#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,subprocess,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];VERSION=(ROOT/'VERSION').read_text().strip()
def run(cmd,cwd=None):return subprocess.check_output([str(x) for x in cmd],cwd=cwd,text=True).strip()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('archive',nargs='?',default=str(ROOT/f'dist/mosaic-tokenizer-{VERSION}-linux-x86_64.tar.gz'));a=ap.parse_args();archive=Path(a.archive).resolve()
    if not archive.exists():raise SystemExit(f'missing archive: {archive}')
    with tempfile.TemporaryDirectory() as td:
        temp=Path(td)
        with tarfile.open(archive,'r:gz') as tf:tf.extractall(temp,filter='data')
        roots=[p for p in temp.iterdir() if p.is_dir()]
        if len(roots)!=1:raise SystemExit('archive must contain exactly one root directory')
        d=roots[0];cli=d/'bin/mosaic-tokenizer';model=d/'share/mosaic/packs/model-v2.mpack';uni=d/'share/mosaic/packs/unicode17-v1.mpack';langs={t:d/f'share/mosaic/packs/language/{t}-v1.mpack' for t in ('en','hi','ja')}
        if run([cli,'--version'])!=f'mosaic-tokenizer {VERSION}':raise SystemExit('packaged CLI version mismatch')
        manifest=json.loads((d/'share/mosaic/release-manifest.json').read_text())
        if run([cli,'fingerprint',model,uni])!=manifest['tokenizer_fingerprint_sha256']:raise SystemExit('packaged base fingerprint mismatch')
        lf=run([cli,'fingerprint-languages',model,uni,langs['ja'],langs['en'],langs['hi']])
        if lf!=manifest['reference_language_fingerprint_sha256']:raise SystemExit('packaged language fingerprint mismatch/order instability')
        for line in (d/'SHA256SUMS').read_text().splitlines():
            expected,rel=line.split('  ',1);actual=hashlib.sha256((d/rel).read_bytes()).hexdigest()
            if actual!=expected:raise SystemExit(f'checksum mismatch: {rel}')
        sample=temp/'mixed.txt';sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode())
        result=run([cli,'roundtrip-languages',model,uni,sample,langs['en'],langs['hi'],langs['ja']])
        if 'languages=3' not in result:raise SystemExit('packaged language CLI smoke failed')
        client=temp/'client.c';client.write_text('''#include <mosaic.h>\n#include <stddef.h>\nint main(int argc,char**argv){if(argc!=4)return 2;mosaic_tokenizer*t=0;if(mosaic_tokenizer_load_files(argv[1],argv[2],&t)!=MOSAIC_OK)return 3;if(mosaic_tokenizer_add_language_file(t,argv[3])!=MOSAIC_OK)return 4;const unsigned char in[]="tokenizer";unsigned int*ids=0;size_t n=0;if(mosaic_tokenizer_encode(t,in,9,&ids,&n)!=MOSAIC_OK)return 5;int ok=n==1&&ids[0]==271;mosaic_free(ids);mosaic_tokenizer_free(t);return ok?0:6;}\n''')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',f'-I{d}/include',client,d/'lib/libmosaic.a','-o',temp/'client'],check=True)
        subprocess.run([temp/'client',model,uni,langs['en']],check=True)
    print(f'OK: packaged release {archive.name} passes CLI, language packs, manifest, checksums, and external static-client smoke');return 0
if __name__=='__main__':raise SystemExit(main())
