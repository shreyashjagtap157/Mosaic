#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
import build_m3_model_fixture as m3

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'fixtures/packs/model-v2.mpack'
EXPECTED=ROOT/'fixtures/packs/model-v2.expected.toml'

def rows():
    r=list(m3.vocabulary_rows())
    r += [
        (271,150,b'tokenizer'),
        (272,100,' दुनिया'.encode()),
        (273,230,'नमस्ते दुनिया'.encode()),
        (274,210,'こんにちは世界'.encode()),
    ]
    return r

def build_vocab():
    data=rows(); ordered=sorted(data,key=lambda x:(x[2],x[0]))
    blob=bytearray(); entries=[]
    for tid,cost,surface in ordered:
        off=len(blob); blob+=surface
        entries.append(struct.pack('<IiIHH',tid,cost,off,len(surface),0))
    ids=sorted(range(len(ordered)),key=lambda i:ordered[i][0])
    first=[0]*257; cur=0
    for b in range(256):
        first[b]=cur
        while cur<len(ordered) and ordered[cur][2][0]==b: cur+=1
    first[256]=cur
    entries_off=40
    id_off=m3.align(entries_off+len(entries)*16,4)
    first_off=m3.align(id_off+len(ids)*4,4)
    blob_off=m3.align(first_off+257*4,8)
    out=bytearray(blob_off+len(blob)); out[:4]=b'MSVC'
    struct.pack_into('<HHIHHIIIIII',out,4,1,0,len(entries),16,0,entries_off,id_off,first_off,blob_off,len(blob),0)
    for i,e in enumerate(entries): out[entries_off+i*16:entries_off+(i+1)*16]=e
    for i,idx in enumerate(ids): struct.pack_into('<I',out,id_off+i*4,idx)
    for i,v in enumerate(first): struct.pack_into('<I',out,first_off+i*4,v)
    out[blob_off:]=blob
    return bytes(out)

def build():
    lock=m3.build_lock(); manifest=m3.build_manifest(lock); vocab=build_vocab(); sections=[(1,manifest),(2,lock),(4,vocab)]
    cursor=m3.align(96+32*len(sections)); payload=bytearray(cursor); entries=[]
    for kind,data in sections:
        cursor=m3.align(cursor); payload.extend(bytes(cursor-len(payload))); off=cursor; payload.extend(data); cursor+=len(data)
        entries.append(struct.pack('<IIQQIHBB',kind,0,off,len(data),0,0,3,0))
    header=bytearray(96); header[:8]=b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII',header,8,1,2,96,0,len(payload),len(sections),32,1,96,0,1)
    payload[:96]=header
    for i,e in enumerate(entries): payload[96+i*32:128+i*32]=e
    canonical=bytearray(payload); canonical[48:80]=bytes(32); payload[48:80]=hashlib.sha256(canonical).digest()
    return bytes(payload)

def expected(data): return f'file_length = {len(data)}\nfile_sha256 = "{hashlib.sha256(data).hexdigest()}"\nvocabulary_entries = {len(rows())}\n'

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); a=ap.parse_args(); data=build(); exp=expected(data)
    if a.check:
        if not OUT.exists() or OUT.read_bytes()!=data or not EXPECTED.exists() or EXPECTED.read_text()!=exp: raise SystemExit('model v2 fixture differs')
        print(f'OK: model v2 deterministic entries={len(rows())} sha256={hashlib.sha256(data).hexdigest()}'); return 0
    OUT.write_bytes(data); EXPECTED.write_text(exp); print(f'wrote {OUT.relative_to(ROOT)}'); return 0
if __name__=='__main__': raise SystemExit(main())
