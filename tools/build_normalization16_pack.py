#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, os, shutil, struct, subprocess, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'fixtures/packs/normalization16-v1.mpack'
EXPECTED=ROOT/'fixtures/packs/normalization16-v1.expected.toml'
EXTRACT=ROOT/'tools/normalization16_extract.c'
HEADER=96; SECTION_ENTRY=32; NHEADER=80

def align(v,b=8): return (v+b-1)//b*b

def extract_raw()->bytes:
    cc=shutil.which('cc'); pkg=shutil.which('pkg-config')
    if not cc or not pkg: raise SystemExit('cc and pkg-config are required to regenerate normalization16 pack')
    ver=subprocess.check_output([pkg,'--modversion','icu-uc'],text=True).strip()
    if ver!='76.1': raise SystemExit(f'expected ICU 76.1 generator, got {ver}')
    flags=subprocess.check_output([pkg,'--cflags','--libs','icu-uc'],text=True).split()
    with tempfile.TemporaryDirectory() as td:
        exe=Path(td)/'extract'
        subprocess.run([cc,'-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',str(EXTRACT),*flags,'-o',str(exe)],check=True)
        raw=subprocess.check_output([str(exe)])
    if raw[:12] != b'MSNEX16\0'+bytes((16,0,0,0)): raise SystemExit('normalization extractor did not report Unicode 16.0.0')
    return raw

def parse_raw(raw:bytes):
    i=12; ccc=[]; canon=[]; compat=[]; casefold=[]; comp=[]
    while i<len(raw):
        t=raw[i]; i+=1
        if t==0: break
        if t==1:
            value=raw[i]; i+=1; reserved=struct.unpack_from('<H',raw,i)[0]; i+=2; cp=struct.unpack_from('<I',raw,i)[0]; i+=4
            if reserved: raise SystemExit('extractor CCC reserved nonzero')
            ccc.append((cp,value))
        elif t in (2,3,4):
            reserved=raw[i]; i+=1; n=struct.unpack_from('<H',raw,i)[0]; i+=2; cp=struct.unpack_from('<I',raw,i)[0]; i+=4
            if reserved: raise SystemExit('extractor mapping reserved nonzero')
            seq=tuple(struct.unpack_from('<'+'I'*n,raw,i)) if n else (); i+=4*n
            (canon if t==2 else compat if t==3 else casefold).append((cp,seq))
        elif t==5:
            reserved=raw[i];i+=1; reserved2=struct.unpack_from('<H',raw,i)[0];i+=2
            a,b,c=struct.unpack_from('<III',raw,i);i+=12
            if reserved or reserved2: raise SystemExit('extractor composition reserved nonzero')
            comp.append((a,b,c))
        else: raise SystemExit(f'unknown extractor record {t}')
    if i!=len(raw): raise SystemExit('trailing extractor bytes')
    # Compress sparse CCC points into same-value contiguous ranges.
    ranges=[]
    for cp,v in ccc:
        if ranges and ranges[-1][1]==cp and ranges[-1][2]==v:
            ranges[-1]=(ranges[-1][0],cp+1,v)
        else: ranges.append((cp,cp+1,v))
    return ranges,canon,compat,casefold,sorted(comp)

def build_section(raw:bytes):
    ccc,canon,compat,casefold,comp=parse_raw(raw)
    seq_blob=[]; seq_index={}
    def intern(seq):
        if seq in seq_index:return seq_index[seq]
        off=len(seq_blob);seq_blob.extend(seq);seq_index[seq]=off;return off
    def metas(rows): return [(cp,intern(seq),len(seq)) for cp,seq in rows]
    cm,km,fm=metas(canon),metas(compat),metas(casefold)
    ccc_off=NHEADER
    canon_off=align(ccc_off+len(ccc)*12,4)
    compat_off=align(canon_off+len(cm)*12,4)
    fold_off=align(compat_off+len(km)*12,4)
    comp_off=align(fold_off+len(fm)*12,4)
    seq_off=align(comp_off+len(comp)*12,4)
    total=seq_off+len(seq_blob)*4
    out=bytearray(total);out[:4]=b'MSNM'
    struct.pack_into('<HHHHHHIIIIII',out,4,1,NHEADER,16,0,0,0,len(ccc),len(cm),len(km),len(fm),len(comp),len(seq_blob))
    struct.pack_into('<IIIIII',out,40,ccc_off,canon_off,compat_off,fold_off,comp_off,seq_off)
    # 64..79 stays canonical zero.
    for j,(a,b,v) in enumerate(ccc):struct.pack_into('<IIB3x',out,ccc_off+j*12,a,b,v)
    for off,rows in [(canon_off,cm),(compat_off,km),(fold_off,fm)]:
        for j,(cp,so,n) in enumerate(rows):struct.pack_into('<IIHH',out,off+j*12,cp,so,n,0)
    for j,(a,b,c) in enumerate(comp):struct.pack_into('<III',out,comp_off+j*12,a,b,c)
    for j,cp in enumerate(seq_blob):struct.pack_into('<I',out,seq_off+j*4,cp)
    return bytes(out),(len(ccc),len(cm),len(km),len(fm),len(comp),len(seq_blob))

def build():
    raw=extract_raw(); sec,counts=build_section(raw)
    lock=b'MSLK'+struct.pack('<HHII',1,48,0,0)
    manifest=b'MSMF'+struct.pack('<HHIIIIII',1,0,1,1,1,1,1,0)+hashlib.sha256(lock).digest()
    sections=[(1,manifest),(2,lock),(8,sec)]
    cursor=align(HEADER+SECTION_ENTRY*len(sections));payload=bytearray(cursor);entries=[]
    for kind,data in sections:
        cursor=align(cursor);payload.extend(bytes(cursor-len(payload)));off=cursor;payload.extend(data);cursor+=len(data)
        entries.append(struct.pack('<IIQQIHBB',kind,0,off,len(data),0,0,3,0))
    header=bytearray(HEADER);header[:8]=b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII',header,8,1,2,HEADER,0,len(payload),len(sections),SECTION_ENTRY,1,HEADER,0,1)
    payload[:HEADER]=header
    for j,e in enumerate(entries):payload[HEADER+j*SECTION_ENTRY:HEADER+(j+1)*SECTION_ENTRY]=e
    canonical=bytearray(payload);canonical[48:80]=bytes(32);payload[48:80]=hashlib.sha256(canonical).digest()
    return bytes(payload),counts,hashlib.sha256(raw).hexdigest()

def expected(data,counts,raw_hash):
    cr,ca,co,cf,cp,sq=counts
    return (f'file_length = {len(data)}\ncanonical_content_hash = "{data[48:80].hex()}"\nfile_sha256 = "{hashlib.sha256(data).hexdigest()}"\n'
            f'unicode = "16.0.0"\nicu = "76.1"\nextractor_sha256 = "{hashlib.sha256(EXTRACT.read_bytes()).hexdigest()}"\nraw_extract_sha256 = "{raw_hash}"\n'
            f'ccc_ranges = {cr}\ncanonical_mappings = {ca}\ncompatibility_mappings = {co}\nnfkc_casefold_mappings = {cf}\ncomposition_pairs = {cp}\nsequence_codepoints = {sq}\n')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args();data,counts,raw_hash=build();exp=expected(data,counts,raw_hash)
    if a.check:
        if not OUT.exists() or OUT.read_bytes()!=data:raise SystemExit('normalization16 pack differs from deterministic output')
        if not EXPECTED.exists() or EXPECTED.read_text()!=exp:raise SystemExit('normalization16 expected metadata differs')
        print(f'OK: normalization16 deterministic bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()} counts={counts}')
        return 0
    OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(data);EXPECTED.write_text(exp);print(f'wrote {OUT.relative_to(ROOT)} ({len(data)} bytes)');print(exp,end='');return 0
if __name__=='__main__':raise SystemExit(main())
