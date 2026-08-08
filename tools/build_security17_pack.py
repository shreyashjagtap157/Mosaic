#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from importlib.metadata import metadata
from pathlib import Path
import regex, regex._regex as _rx

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'fixtures/packs/security17-v1.mpack'
EXPECTED=ROOT/'fixtures/packs/security17-v1.expected.toml'
HEADER=96; SECTION_ENTRY=32; SHEADER=72
TEXT=''.join(map(chr,range(0x110000)))

def align(v,b=8): return (v+b-1)//b*b

def ranges(pattern:str):
    return [(m.start(),m.end()) for m in regex.finditer(pattern,TEXT)]

def canonical_scripts():
    aliases=_rx.get_properties()['SCRIPT'][1]
    first={}
    for name,value in aliases.items():
        first.setdefault(value,name)
    # Prefer full names over ISO aliases when dictionary ordering differs.
    grouped={}
    for name,value in aliases.items(): grouped.setdefault(value,[]).append(name)
    result=[]
    for value,names in grouped.items():
        candidates=[n for n in names if len(n)>4]
        name=(candidates or names)[0]
        result.append(name)
    return sorted(set(result))

def build_security_section():
    desc=metadata('regex').get('Description','')
    if 'supports Unicode 17.0.0' not in desc:
        raise SystemExit('installed regex package is not the expected Unicode 17.0.0 property oracle')
    scripts=canonical_scripts()
    script_id={name:i+1 for i,name in enumerate(scripts)}
    sr=[]
    for name in scripts:
        sid=script_id[name]
        for a,b in ranges(fr'\p{{Script={name}}}+'): sr.append((a,b,sid))
    sr.sort()
    prev=0
    for i,(a,b,_) in enumerate(sr):
        if a>=b or (i and a<prev): raise SystemExit('script ranges overlap or are invalid')
        prev=b
    bidi=ranges(r'\p{Bidi_Control}+')
    ign=ranges(r'\p{Default_Ignorable_Code_Point}+')
    non=ranges(r'\p{Noncharacter_Code_Point}+')
    dep=ranges(r'\p{Deprecated}+')

    meta_off=SHEADER
    range_off=align(meta_off+len(scripts)*8,4)
    bidi_off=align(range_off+len(sr)*12,4)
    ign_off=align(bidi_off+len(bidi)*8,4)
    non_off=align(ign_off+len(ign)*8,4)
    dep_off=align(non_off+len(non)*8,4)
    blob_off=align(dep_off+len(dep)*8,4)
    blob=bytearray(); metas=[]
    for name in scripts:
        enc=name.encode('ascii'); off=len(blob); blob.extend(enc); metas.append((off,len(enc),script_id[name]))
    total=blob_off+len(blob)
    out=bytearray(total); out[:4]=b'MSSC'
    struct.pack_into('<HHHHHHIIIIII',out,4,1,SHEADER,17,0,0,0,len(scripts),len(sr),len(bidi),len(ign),len(non),len(dep))
    struct.pack_into('<IIIIIII',out,40,meta_off,range_off,bidi_off,ign_off,non_off,dep_off,blob_off)
    for i,(off,n,sid) in enumerate(metas): struct.pack_into('<IHH',out,meta_off+i*8,off,n,sid)
    for i,(a,b,sid) in enumerate(sr): struct.pack_into('<IIH2x',out,range_off+i*12,a,b,sid)
    for off,rr in [(bidi_off,bidi),(ign_off,ign),(non_off,non),(dep_off,dep)]:
        for i,(a,b) in enumerate(rr): struct.pack_into('<II',out,off+i*8,a,b)
    out[blob_off:]=blob
    return bytes(out),scripts,sr,bidi,ign,non,dep

def build():
    sec,scripts,sr,bidi,ign,non,dep=build_security_section()
    lock=b'MSLK'+struct.pack('<HHII',1,48,0,0)
    manifest=b'MSMF'+struct.pack('<HHIIIIII',1,0,1,1,1,1,1,0)+hashlib.sha256(lock).digest()
    sections=[(1,manifest),(2,lock),(7,sec)]
    cursor=align(HEADER+SECTION_ENTRY*len(sections)); payload=bytearray(cursor); entries=[]
    for kind,data in sections:
        cursor=align(cursor); payload.extend(bytes(cursor-len(payload))); off=cursor; payload.extend(data); cursor+=len(data)
        entries.append(struct.pack('<IIQQIHBB',kind,0,off,len(data),0,0,3,0))
    header=bytearray(HEADER); header[:8]=b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII',header,8,1,2,HEADER,0,len(payload),len(sections),SECTION_ENTRY,1,HEADER,0,1)
    payload[:HEADER]=header
    for i,e in enumerate(entries): payload[HEADER+i*SECTION_ENTRY:HEADER+(i+1)*SECTION_ENTRY]=e
    canonical=bytearray(payload); canonical[48:80]=bytes(32); payload[48:80]=hashlib.sha256(canonical).digest()
    return bytes(payload),scripts,sr,bidi,ign,non,dep

def expected(data,scripts,sr,bidi,ign,non,dep):
    return (f'file_length = {len(data)}\ncanonical_content_hash = "{data[48:80].hex()}"\n'
            f'file_sha256 = "{hashlib.sha256(data).hexdigest()}"\nunicode = "17.0.0"\n'
            f'scripts = {len(scripts)}\nscript_ranges = {len(sr)}\nbidi_control_ranges = {len(bidi)}\n'
            f'default_ignorable_ranges = {len(ign)}\nnoncharacter_ranges = {len(non)}\ndeprecated_ranges = {len(dep)}\n')

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); a=ap.parse_args()
    data,*meta=build(); exp=expected(data,*meta)
    if a.check:
        if not OUT.exists() or OUT.read_bytes()!=data: raise SystemExit('Unicode 17 security pack differs from deterministic output')
        if not EXPECTED.exists() or EXPECTED.read_text()!=exp: raise SystemExit('Unicode 17 security expected metadata differs')
        print(f'OK: Unicode 17 security pack deterministic bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}')
        return 0
    OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(data);EXPECTED.write_text(exp);print(f'wrote {OUT.relative_to(ROOT)} ({len(data)} bytes)');print(exp,end='')
    return 0
if __name__=='__main__': raise SystemExit(main())
