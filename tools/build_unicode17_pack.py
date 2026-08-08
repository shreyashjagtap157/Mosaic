#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from importlib.metadata import metadata
from pathlib import Path
import regex

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'fixtures/packs/unicode17-v1.mpack'
EXPECTED=ROOT/'fixtures/packs/unicode17-v1.expected.toml'
HEADER=96; SECTION_ENTRY=32; UHEADER=48
GCB_VALUES={
    'Control':1,'LF':2,'CR':3,'Extend':4,'Prepend':5,'SpacingMark':6,
    'L':7,'V':8,'T':9,'ZWJ':10,'LV':11,'LVT':12,'Regional_Indicator':13,
}
INCB_VALUES={'Extend':1,'Consonant':2,'Linker':3}


def align(v,b=8): return (v+b-1)//b*b

def property_ranges(pattern:str):
    # String index equals code point value because every scalar/code point position
    # from 0..0x10FFFF is represented exactly once, including surrogate code points
    # which remain property-default and are never emitted by strict UTF-8 decoding.
    text=property_ranges.text
    return [(m.start(),m.end()) for m in regex.finditer(pattern,text)]
property_ranges.text=''.join(map(chr,range(0x110000)))


def build_unicode_section():
    desc=metadata('regex').get('Description','')
    if 'supports Unicode 17.0.0' not in desc:
        raise SystemExit('installed regex package is not the expected Unicode 17.0.0 property oracle')
    g=[]
    for name,value in GCB_VALUES.items():
        for start,end in property_ranges(fr'\p{{gcb={name}}}+'):
            g.append((start,end,value))
    g.sort()
    incb=[]
    for name,value in INCB_VALUES.items():
        for start,end in property_ranges(fr'\p{{InCB={name}}}+'):
            incb.append((start,end,value))
    incb.sort()
    ep=property_ranges(r'\p{Extended_Pictographic}+')

    goff=UHEADER
    ioff=align(goff+len(g)*12,4)
    eoff=align(ioff+len(incb)*12,4)
    total=eoff+len(ep)*8
    out=bytearray(total)
    out[:4]=b'MSUC'
    struct.pack_into('<HHHHHHIII',out,4,1,0,17,0,0,0,len(g),len(incb),len(ep))
    struct.pack_into('<IIIQ',out,28,goff,ioff,eoff,0)
    for idx,(start,end,value) in enumerate(g): struct.pack_into('<IIB3x',out,goff+idx*12,start,end,value)
    for idx,(start,end,value) in enumerate(incb): struct.pack_into('<IIB3x',out,ioff+idx*12,start,end,value)
    for idx,(start,end) in enumerate(ep): struct.pack_into('<II',out,eoff+idx*8,start,end)
    return bytes(out),len(g),len(incb),len(ep)


def build():
    unicode_section,gc,ic,ec=build_unicode_section()
    lock=b'MSLK'+struct.pack('<HHII',1,48,0,0)
    manifest=b'MSMF'+struct.pack('<HHIIIIII',1,0,1,1,1,1,1,0)+hashlib.sha256(lock).digest()
    sections=[(1,manifest),(2,lock),(5,unicode_section)]
    cursor=align(HEADER+SECTION_ENTRY*len(sections)); payload=bytearray(cursor); entries=[]
    for kind,data in sections:
        cursor=align(cursor); payload.extend(bytes(cursor-len(payload))); off=cursor; payload.extend(data); cursor+=len(data)
        entries.append(struct.pack('<IIQQIHBB',kind,0,off,len(data),0,0,3,0))
    header=bytearray(HEADER); header[:8]=b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII',header,8,1,2,HEADER,0,len(payload),len(sections),SECTION_ENTRY,1,HEADER,0,1)
    payload[:HEADER]=header
    for i,e in enumerate(entries): payload[HEADER+i*SECTION_ENTRY:HEADER+(i+1)*SECTION_ENTRY]=e
    canonical=bytearray(payload); canonical[48:80]=bytes(32); payload[48:80]=hashlib.sha256(canonical).digest()
    return bytes(payload),gc,ic,ec


def expected(data,gc,ic,ec):
    return (f'file_length = {len(data)}\ncanonical_content_hash = "{data[48:80].hex()}"\n'
            f'file_sha256 = "{hashlib.sha256(data).hexdigest()}"\nunicode = "17.0.0"\n'
            f'gcb_ranges = {gc}\nincb_ranges = {ic}\nextended_pictographic_ranges = {ec}\n')


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); a=ap.parse_args(); data,gc,ic,ec=build(); exp=expected(data,gc,ic,ec)
    if a.check:
        if not OUT.exists() or OUT.read_bytes()!=data: raise SystemExit('Unicode 17 pack differs from deterministic output')
        if not EXPECTED.exists() or EXPECTED.read_text(encoding="utf-8")!=exp: raise SystemExit('Unicode 17 expected metadata differs')
        print(f'OK: Unicode 17 pack deterministic bytes={len(data)} gcb={gc} incb={ic} ep={ec} sha256={hashlib.sha256(data).hexdigest()}')
        return 0
    OUT.parent.mkdir(parents=True,exist_ok=True); OUT.write_bytes(data); EXPECTED.write_text(exp, encoding="utf-8"); print(f'wrote {OUT.relative_to(ROOT)} ({len(data)} bytes)'); print(exp,end=''); return 0
if __name__=='__main__': raise SystemExit(main())
