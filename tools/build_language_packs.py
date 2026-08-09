#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
import build_m3_model_fixture as base

ROOT=Path(__file__).resolve().parents[1]
PACKS={
 'en': [(b'tokenizer',-40),(b'hello world',-10)],
 'hi': [('नमस्ते दुनिया'.encode(),-60)],
 'ja': [('こんにちは世界'.encode(),-60)],
}

def language_section(tag:str,rows:list[tuple[bytes,int]])->bytes:
    ordered=sorted(rows,key=lambda x:x[0])
    if len({s for s,_ in ordered})!=len(ordered): raise ValueError('duplicate surface')
    blob=bytearray(); entries=[]; maxlen=0
    for surface,delta in ordered:
        if not surface: raise ValueError('empty surface')
        off=len(blob); blob+=surface; maxlen=max(maxlen,len(surface))
        entries.append(struct.pack('<IHHii',off,len(surface),0,delta,0))
    tagb=tag.encode('ascii'); entries_off=40
    tag_off=entries_off+len(entries)*16
    blob_off=base.align(tag_off+len(tagb),8)
    out=bytearray(blob_off+len(blob)); out[:4]=b'MSLG'
    struct.pack_into('<HHIHHIIIIII',out,4,1,0,len(entries),16,len(tagb),entries_off,tag_off,blob_off,len(blob),maxlen,0)
    for i,e in enumerate(entries): out[entries_off+i*16:entries_off+(i+1)*16]=e
    out[tag_off:tag_off+len(tagb)]=tagb; out[blob_off:]=blob
    return bytes(out)

def build_pack(tag,rows):
    lock=base.build_lock(); manifest=base.build_manifest(lock); lang=language_section(tag,rows); sections=[(1,manifest),(2,lock),(5,lang)]
    cursor=base.align(96+32*len(sections)); payload=bytearray(cursor); entries=[]
    for kind,data in sections:
        cursor=base.align(cursor); payload.extend(bytes(cursor-len(payload))); off=cursor; payload.extend(data); cursor+=len(data)
        entries.append(struct.pack('<IIQQIHBB',kind,0,off,len(data),0,0,3,0))
    header=bytearray(96); header[:8]=b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII',header,8,1,2,96,0,len(payload),len(sections),32,1,96,0,1)
    payload[:96]=header
    for i,e in enumerate(entries): payload[96+i*32:128+i*32]=e
    canonical=bytearray(payload); canonical[48:80]=bytes(32); payload[48:80]=hashlib.sha256(canonical).digest()
    return bytes(payload)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); a=ap.parse_args()
    outdir=ROOT/'fixtures/packs/language'; outdir.mkdir(parents=True,exist_ok=True)
    for tag,rows in PACKS.items():
        data=build_pack(tag,rows); path=outdir/f'{tag}-v1.mpack'; exp=outdir/f'{tag}-v1.expected.toml'
        text=f'language = "{tag}"\nentries = {len(rows)}\nfile_length = {len(data)}\nfile_sha256 = "{hashlib.sha256(data).hexdigest()}"\n'
        if a.check:
            if not path.exists() or path.read_bytes()!=data or not exp.exists() or exp.read_text(encoding="utf-8")!=text: raise SystemExit(f'{tag} language pack differs')
        else: path.write_bytes(data); exp.write_text(text, encoding="utf-8")
        print(f'OK: language {tag} entries={len(rows)} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}')
    return 0
if __name__=='__main__': raise SystemExit(main())
