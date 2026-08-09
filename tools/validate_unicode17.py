#!/usr/bin/env python3
"""Unicode 17 scalar/grapheme oracle for Mosaic generated property pack."""
from __future__ import annotations
import bisect, hashlib, random, struct, tomllib
from dataclasses import dataclass
from pathlib import Path
import regex
from validate_m2_fixture import parse_outer, section_bytes, parse_lock, parse_manifest, Reject

ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'fixtures/packs/unicode17-v1.mpack'
EXPECTED=ROOT/'fixtures/packs/unicode17-v1.expected.toml'

G_OTHER=0; G_CONTROL=1; G_LF=2; G_CR=3; G_EXTEND=4; G_PREPEND=5; G_SPACINGMARK=6
G_L=7; G_V=8; G_T=9; G_ZWJ=10; G_LV=11; G_LVT=12; G_RI=13
I_NONE=0; I_EXTEND=1; I_CONSONANT=2; I_LINKER=3

@dataclass(frozen=True)
class Unit:
    start:int; end:int; cp:int|None

@dataclass(frozen=True)
class UnicodeTables:
    gcb:list[tuple[int,int,int]]
    incb:list[tuple[int,int,int]]
    ep:list[tuple[int,int]]


def parse_unicode(data:bytes)->UnicodeTables:
    sections,mi,li=parse_outer(data); lock=section_bytes(data,sections[li]); parse_lock(lock); parse_manifest(section_bytes(data,sections[mi]),lock)
    us=[s for s in sections if s.kind==5]
    if len(us)!=1: raise Reject('InvalidUnicodeSectionCount')
    b=section_bytes(data,us[0])
    if len(b)<48 or b[:4]!=b'MSUC': raise Reject('InvalidUnicodeHeader')
    version,flags,major,minor,patch,res=struct.unpack_from('<HHHHHH',b,4)
    gc,ic,ec=struct.unpack_from('<III',b,16); go,io,eo=struct.unpack_from('<III',b,28); res2=struct.unpack_from('<Q',b,40)[0]
    if version!=1 or (major,minor,patch)!=(17,0,0): raise Reject('UnsupportedUnicodeVersion')
    if flags or res or res2: raise Reject('ReservedNotZero')
    ge=go+gc*12; ie=io+ic*12; ee=eo+ec*8
    if not (go==48 and ge<=io<=ie<=eo<=ee==len(b)): raise Reject('InvalidUnicodeLayout')
    if any(b[ge:io]) or any(b[ie:eo]): raise Reject('NonCanonicalPadding')
    g=[]; prior=-1
    for i in range(gc):
        start,end,val=struct.unpack_from('<IIB',b,go+i*12)
        if start>=end or end>0x110000 or start<prior or not 1<=val<=13 or any(b[go+i*12+9:go+i*12+12]): raise Reject('InvalidUnicodeRange')
        g.append((start,end,val)); prior=end
    inc=[]; prior=-1
    for i in range(ic):
        start,end,val=struct.unpack_from('<IIB',b,io+i*12)
        if start>=end or end>0x110000 or start<prior or not 1<=val<=3 or any(b[io+i*12+9:io+i*12+12]): raise Reject('InvalidUnicodeRange')
        inc.append((start,end,val)); prior=end
    ep=[]; prior=-1
    for i in range(ec):
        start,end=struct.unpack_from('<II',b,eo+i*8)
        if start>=end or end>0x110000 or start<prior: raise Reject('InvalidUnicodeRange')
        ep.append((start,end)); prior=end
    return UnicodeTables(g,inc,ep)


def prop(ranges,cp,default=0):
    # range count is small; bisect over starts keeps this independent of regex.
    starts=[r[0] for r in ranges]
    i=bisect.bisect_right(starts,cp)-1
    if i>=0 and ranges[i][0]<=cp<ranges[i][1]: return ranges[i][2] if len(ranges[i])==3 else 1
    return default


def utf8_units(data:bytes)->list[Unit]:
    out=[]; i=0; n=len(data)
    while i<n:
        b0=data[i]
        if b0<=0x7f:
            out.append(Unit(i,i+1,b0)); i+=1; continue
        if 0xC2<=b0<=0xDF and i+1<n and 0x80<=data[i+1]<=0xBF:
            cp=((b0&0x1f)<<6)|(data[i+1]&0x3f); out.append(Unit(i,i+2,cp)); i+=2; continue
        if 0xE0<=b0<=0xEF and i+2<n:
            b1,b2=data[i+1],data[i+2]
            valid_b1=(0xA0<=b1<=0xBF) if b0==0xE0 else (0x80<=b1<=0x9F) if b0==0xED else (0x80<=b1<=0xBF)
            if valid_b1 and 0x80<=b2<=0xBF:
                cp=((b0&0x0f)<<12)|((b1&0x3f)<<6)|(b2&0x3f); out.append(Unit(i,i+3,cp)); i+=3; continue
        if 0xF0<=b0<=0xF4 and i+3<n:
            b1,b2,b3=data[i+1],data[i+2],data[i+3]
            valid_b1=(0x90<=b1<=0xBF) if b0==0xF0 else (0x80<=b1<=0x8F) if b0==0xF4 else (0x80<=b1<=0xBF)
            if valid_b1 and 0x80<=b2<=0xBF and 0x80<=b3<=0xBF:
                cp=((b0&7)<<18)|((b1&0x3f)<<12)|((b2&0x3f)<<6)|(b3&0x3f); out.append(Unit(i,i+4,cp)); i+=4; continue
        out.append(Unit(i,i+1,None)); i+=1
    return out


def grapheme_spans(t:UnicodeTables,data:bytes)->list[tuple[int,int]]:
    units=utf8_units(data)
    if not units: return []
    g=[prop(t.gcb,u.cp) if u.cp is not None else None for u in units]
    inc=[prop(t.incb,u.cp) if u.cp is not None else None for u in units]
    ep=[bool(prop(t.ep,u.cp)) if u.cp is not None else False for u in units]
    boundaries=[0]
    for i in range(1,len(units)):
        # Opaque invalid bytes form individual graphemes and isolate adjacent Unicode.
        if units[i-1].cp is None or units[i].cp is None:
            br=True
        else:
            a,b=g[i-1],g[i]
            if a==G_CR and b==G_LF: br=False                         # GB3
            elif a in (G_CONTROL,G_CR,G_LF): br=True                # GB4
            elif b in (G_CONTROL,G_CR,G_LF): br=True                # GB5
            elif a==G_L and b in (G_L,G_V,G_LV,G_LVT): br=False     # GB6
            elif a in (G_LV,G_V) and b in (G_V,G_T): br=False       # GB7
            elif a in (G_LVT,G_T) and b==G_T: br=False              # GB8
            elif b in (G_EXTEND,G_ZWJ): br=False                    # GB9
            elif b==G_SPACINGMARK: br=False                         # GB9a
            elif a==G_PREPEND: br=False                             # GB9b
            elif inc[i]==I_CONSONANT and gb9c_no_break(inc,i): br=False
            elif ep[i] and a==G_ZWJ and gb11_no_break(g,ep,i): br=False
            elif a==G_RI and b==G_RI and preceding_ri_count(g,i)%2==1: br=False
            else: br=True
        if br: boundaries.append(i)
    boundaries.append(len(units))
    return [(units[a].start,units[b-1].end) for a,b in zip(boundaries,boundaries[1:])]


def gb9c_no_break(inc:list[int|None],i:int)->bool:
    j=i-1; seen_linker=False
    while j>=0 and inc[j] in (I_EXTEND,I_LINKER):
        if inc[j]==I_LINKER: seen_linker=True
        j-=1
    return seen_linker and j>=0 and inc[j]==I_CONSONANT


def gb11_no_break(g:list[int|None],ep:list[bool],i:int)->bool:
    j=i-2
    while j>=0 and g[j]==G_EXTEND: j-=1
    return j>=0 and ep[j]


def preceding_ri_count(g:list[int|None],i:int)->int:
    count=0; j=i-1
    while j>=0 and g[j]==G_RI: count+=1; j-=1
    return count


def regex_spans(text:str)->list[tuple[int,int]]:
    # Convert regex's Python-codepoint spans to UTF-8 byte spans.
    prefix=[0]
    for ch in text: prefix.append(prefix[-1]+len(ch.encode('utf-8')))
    return [(prefix[m.start()],prefix[m.end()]) for m in regex.finditer(r'\X',text)]


def main():
    data=PACK.read_bytes(); exp=tomllib.loads(EXPECTED.read_text(encoding="utf-8"))
    if len(data)!=exp['file_length'] or data[48:80].hex()!=exp['canonical_content_hash'] or hashlib.sha256(data).hexdigest()!=exp['file_sha256']: raise SystemExit('Unicode pack identity mismatch')
    t=parse_unicode(data)
    if len(t.gcb)!=exp['gcb_ranges'] or len(t.incb)!=exp['incb_ranges'] or len(t.ep)!=exp['extended_pictographic_ranges']: raise SystemExit('Unicode range count mismatch')

    crafted=[
        '', 'a', 'a\u0301', '\r\n', '\u1100\u1161\u11a8', '🇮🇳', '🇺🇸🇮🇳',
        '👩\u200d💻', '👨\u200d👩\u200d👧\u200d👦', 'क्\u200dष', 'क्ष', 'नमस्ते',
        'Z\u0308\u0301', '🏳️\u200d🌈', '1️⃣', 'こんにちは', '世界',
    ]
    for text in crafted:
        actual=grapheme_spans(t,text.encode('utf-8')); expected=regex_spans(text)
        if actual!=expected: raise SystemExit(f'crafted grapheme mismatch {text!r}: {actual} vs {expected}')

    # Build a property-heavy representative pool rather than uniform Unicode,
    # which would mostly sample boring GCB=Other code points.
    pool=[ord(c) for c in 'abcXYZ09 नमस्ते日本語']
    for ranges in (t.gcb,t.incb):
        for start,end,*_ in ranges:
            for cp in (start,min(start+1,end-1),end-1):
                if not 0xD800<=cp<=0xDFFF: pool.append(cp)
    for start,end in t.ep:
        for cp in (start,end-1):
            if not 0xD800<=cp<=0xDFFF: pool.append(cp)
    pool=list(dict.fromkeys(pool))
    rng=random.Random(0x554E49434F44453137)
    for case in range(20000):
        text=''.join(chr(rng.choice(pool)) for _ in range(rng.randrange(0,18)))
        actual=grapheme_spans(t,text.encode('utf-8')); expected=regex_spans(text)
        if actual!=expected:
            raise SystemExit(f'random grapheme mismatch case={case} text={text.encode("unicode_escape")!r}: {actual} vs {expected}')

    invalid=[b'\x80',b'\xc0\xaf',b'\xe0\x80\x80',b'\xed\xa0\x80',b'\xf4\x90\x80\x80',b'\xffa\xcc\x81',b'a\xf0\x9f']
    for raw in invalid:
        spans=grapheme_spans(t,raw)
        rebuilt=b''.join(raw[a:b] for a,b in spans)
        if rebuilt!=raw or any(a>=b for a,b in spans): raise SystemExit(f'invalid UTF-8 preservation failed {raw!r}')

    print(f'OK: Unicode 17 pack ranges gcb={len(t.gcb)} incb={len(t.incb)} ep={len(t.ep)}')
    print(f'OK: {len(crafted)} crafted + 20000 property-heavy grapheme cases match regex Unicode 17 \\X')
    print(f'OK: {len(invalid)} malformed UTF-8 cases preserve exact bytes as opaque units')
    return 0
if __name__=='__main__': raise SystemExit(main())
