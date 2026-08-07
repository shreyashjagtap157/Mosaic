#!/usr/bin/env python3
from __future__ import annotations
import tomllib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'fixtures/conformance/path-order-v1.toml'
PACK_HASH=bytes([9])*32

def path_key(path):
    total=sum(edge[5] for edge in path)
    # lower cost and fewer tokens first
    prefix=(total,len(path))
    structural=[]
    for start,end,namespace,kind,token_id,cost in path:
        if end < start: raise ValueError('invalid edge range')
        # longer span first -> negative length, then namespace, token ID, full edge key
        structural.append((-(end-start),namespace,token_id,start,end,namespace,kind,token_id,PACK_HASH))
    return prefix+tuple(structural)

def main():
    data=tomllib.loads(FIXTURE.read_text())
    for case in data['case']:
        left=path_key(case['left']); right=path_key(case['right'])
        winner='left' if left < right else 'right' if right < left else 'equal'
        if winner != case['expected']:
            raise SystemExit(f"{case['name']}: expected {case['expected']}, got {winner}")
    print(f"OK: {len(data['case'])} canonical path-order vectors")
if __name__=='__main__': main()
