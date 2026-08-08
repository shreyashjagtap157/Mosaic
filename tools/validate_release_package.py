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
        d=roots[0];cli=d/'bin/mosaic-tokenizer';author=d/'bin/mosaic-author';model=d/'share/mosaic/packs/model-v2.mpack';uni=d/'share/mosaic/packs/unicode17-v1.mpack';langs={t:d/f'share/mosaic/packs/language/{t}-v1.mpack' for t in ('en','hi','ja')};det=d/'share/mosaic/packs/detector/reference-v1.mpack'
        if run([cli,'--version'])!=f'mosaic-tokenizer {VERSION}':raise SystemExit('packaged CLI version mismatch')
        if run([author,'--version'])!='mosaic-author 0.6.0':raise SystemExit('packaged author version mismatch')
        manifest=json.loads((d/'share/mosaic/release-manifest.json').read_text())
        if run([cli,'fingerprint',model,uni])!=manifest['tokenizer_fingerprint_sha256']:raise SystemExit('packaged base fingerprint mismatch')
        lf=run([cli,'fingerprint-languages',model,uni,langs['ja'],langs['en'],langs['hi']])
        if lf!=manifest['reference_language_fingerprint_sha256']:raise SystemExit('packaged language fingerprint mismatch/order instability')
        af=run([cli,'fingerprint-auto',model,uni,det,langs['hi'],langs['ja'],langs['en']])
        if af!=manifest['reference_auto_fingerprint_sha256']:raise SystemExit('packaged auto fingerprint mismatch/order instability')
        for line in (d/'SHA256SUMS').read_text().splitlines():
            expected,rel=line.split('  ',1);actual=hashlib.sha256((d/rel).read_bytes()).hexdigest()
            if actual!=expected:raise SystemExit(f'checksum mismatch: {rel}')
        sample=temp/'mixed.txt';sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode())
        result=run([cli,'roundtrip-languages',model,uni,sample,langs['en'],langs['hi'],langs['ja']])
        if 'languages=3' not in result:raise SystemExit('packaged language CLI smoke failed')
        auto=run([cli,'roundtrip-auto',model,uni,det,sample,langs['en'],langs['hi'],langs['ja']])
        if 'route=none' not in auto:raise SystemExit('packaged ambiguous auto-routing fallback failed')
        en_sample=temp/'english.txt';en_sample.write_bytes(b'tokenizer')
        detected=run([cli,'roundtrip-auto',model,uni,det,en_sample,langs['en'],langs['hi'],langs['ja']])
        if 'route=en' not in detected or 'tokens=1' not in detected:raise SystemExit('packaged auto-routing specialization failed')

        # Packaged authoring tool must produce a deterministic usable model outside the source tree.
        corpus=temp/'author-corpus.txt';corpus.write_text('tokenizer tokenizer hello world\nनमस्ते दुनिया नमस्ते दुनिया\n',encoding='utf-8')
        authored=temp/'authored.mpack'
        subprocess.run([author,'train-model',corpus,'-o',authored,'--vocab-size','272','--min-frequency','1'],check=True)
        authored_sample=temp/'authored-sample.txt';authored_sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode())
        if 'OK' not in run([cli,'roundtrip',authored,authored_sample]):raise SystemExit('packaged author/native integration failed')
        import base64
        tik=temp/'compat.tiktoken';tikpack=temp/'compat.mpack'
        vocab={bytes([b]):(b*73)%256 for b in range(256)};vocab.update({b'ab':256,b'bc':257,b'abc':258})
        tik.write_bytes(b''.join(base64.b64encode(surface)+b' '+str(rank).encode()+b'\n' for surface,rank in sorted(vocab.items(),key=lambda kv:kv[1])))
        subprocess.run([author,'import-tiktoken',tik,tikpack],check=True)
        compat_sample=temp/'compat.bin';compat_sample.write_bytes(b'abc')
        ids=temp/'compat.ids';subprocess.run([cli,'encode-u32',tikpack,compat_sample,ids],check=True)
        if ids.read_bytes()!= (258).to_bytes(4,'little'):raise SystemExit('packaged raw-BPE compatibility failed')
        client=temp/'client.c';client.write_text('''#include <mosaic.h>
#include <stddef.h>
#include <string.h>
int main(int argc,char**argv){if(argc!=5)return 2;mosaic_tokenizer*t=0;if(mosaic_tokenizer_load_files(argv[1],argv[2],&t)!=MOSAIC_OK)return 3;if(mosaic_tokenizer_add_language_file(t,argv[3])!=MOSAIC_OK)return 4;if(mosaic_tokenizer_set_detector_file(t,argv[4])!=MOSAIC_OK)return 5;const unsigned char in[]="tokenizer";unsigned int*ids=0;size_t n=0;mosaic_detection d={0};if(mosaic_tokenizer_encode_auto(t,in,9,&ids,&n,&d)!=MOSAIC_OK)return 6;int ok=n==1&&ids[0]==271&&d.matched&&d.available&&!strcmp(d.language,"en");mosaic_free(ids);mosaic_tokenizer_free(t);return ok?0:7;}
''')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',f'-I{d}/include',client,d/'lib/libmosaic.a','-o',temp/'client'],check=True)
        subprocess.run([temp/'client',model,uni,langs['en'],det],check=True)
    print(f'OK: packaged release {archive.name} passes CLI, detector/language packs, manifest, checksums, and external static-client smoke');return 0
if __name__=='__main__':raise SystemExit(main())
