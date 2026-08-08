#!/usr/bin/env python3
"""Generate/check Mosaic's additive C ABI contract from public headers and shared libraries."""
from __future__ import annotations
import argparse, json, re, shutil, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
HEADERS=[ROOT/'native/include/mosaic.h',ROOT/'native/include/mosaic_trust.h']
OUT=ROOT/'abi/stable-contract-v1.json'
EXCLUDED_DEFINES={'MOSAIC_RELEASE_VERSION'}

def norm(s:str)->str:return ' '.join(s.split())

def parse_header(path:Path)->dict:
    lines=path.read_text(encoding="utf-8").splitlines(); declarations=[];types=[];constants=[]
    i=0
    while i<len(lines):
        st=lines[i].strip()
        if st.startswith(('MOSAIC_API ','MOSAIC_TRUST_API ')):
            block=st
            while ';' not in block:
                i+=1; block+=' '+lines[i].strip()
            declarations.append(norm(block))
        elif (st.startswith('typedef struct ') and '{' in st) or st.startswith('typedef enum ') or st=='enum {':
            block=st; depth=st.count('{')-st.count('}')
            while depth>0 or not block.rstrip().endswith(';'):
                i+=1; part=lines[i].strip(); block+=' '+part; depth+=part.count('{')-part.count('}')
            types.append(norm(block))
        elif st.startswith('#define MOSAIC_'):
            parts=st.split(None,2)
            if len(parts)>=3 and parts[1] not in EXCLUDED_DEFINES and not parts[1].endswith('(x)'):
                constants.append(norm(st))
        i+=1
    return {'declarations':sorted(set(declarations)),'types':sorted(set(types)),'constants':sorted(set(constants))}

def symbol_from_decl(d:str)->str:
    m=re.search(r'\b(mosaic_[A-Za-z0-9_]+)\s*\(',d)
    if not m:raise ValueError(f'cannot parse public declaration: {d}')
    return m.group(1)

def build_contract()->dict:
    headers={p.name:parse_header(p) for p in HEADERS}
    symbols={name:sorted(symbol_from_decl(x) for x in data['declarations']) for name,data in headers.items()}
    return {'schema':1,'policy':'additive-public-c-abi','headers':headers,'required_symbols':symbols}

def tool_symbols(lib:Path)->set[str]:
    nm=shutil.which('llvm-nm') or shutil.which('nm')
    if not nm:raise RuntimeError('nm/llvm-nm unavailable')
    cmds=[[nm,'-D','--defined-only',str(lib)],[nm,'--defined-only',str(lib)]]
    last=''
    for cmd in cmds:
        p=subprocess.run(cmd,text=True,capture_output=True)
        if p.returncode==0:
            out=set()
            for line in p.stdout.splitlines():
                m=re.search(r'\b(mosaic_[A-Za-z0-9_]+)$',line.strip())
                if m:out.add(m.group(1))
            if out:return out
        last=p.stderr
    raise RuntimeError('unable to inspect shared-library symbols: '+last)

def check(contract:dict, core:Path|None, trust:Path|None)->list[str]:
    errors=[]; current={p.name:parse_header(p) for p in HEADERS}
    for name,frozen in contract['headers'].items():
        now=current.get(name,{})
        for family in ('declarations','types','constants'):
            missing=sorted(set(frozen[family])-set(now.get(family,[])))
            errors += [f'{name}: frozen {family[:-1]} changed/removed: {x}' for x in missing]
    for name,lib in [('mosaic.h',core),('mosaic_trust.h',trust)]:
        if lib:
            actual=tool_symbols(lib);required=set(contract['required_symbols'][name])
            for sym in sorted(required-actual):errors.append(f'{lib}: missing frozen export {sym}')
            # Hidden-by-default policy: exported mosaic_* functions must be declared public.
            for sym in sorted(actual-required):errors.append(f'{lib}: unreviewed mosaic export {sym}')
    return errors

def main()->int:
    ap=argparse.ArgumentParser();ap.add_argument('--generate',action='store_true');ap.add_argument('--core',type=Path);ap.add_argument('--trust',type=Path);a=ap.parse_args()
    if a.generate:
        OUT.write_text(json.dumps(build_contract(),indent=2,sort_keys=True)+'\n', encoding="utf-8");print(f'OK: wrote {OUT.relative_to(ROOT)}');return 0
    if not OUT.exists():raise SystemExit('stable contract missing; run --generate during freeze')
    errors=check(json.loads(OUT.read_text(encoding="utf-8")),a.core,a.trust)
    if errors:
        print('\n'.join('FAIL: '+e for e in errors));return 1
    print('OK: frozen public C declarations/types/constants and exports are compatible');return 0
if __name__=='__main__':raise SystemExit(main())
