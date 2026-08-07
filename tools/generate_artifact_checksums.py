#!/usr/bin/env python3
"""Generate deterministic SHA-256 checksums for repository source artifacts."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EXCLUDED_TOP = {'.git', 'build', 'dist', 'target'}
EXCLUDED_PARTS = {'__pycache__'}
EXCLUDED_NAMES = {'ARTIFACT_CHECKSUMS.sha256'}
EXCLUDED_SUFFIXES = {'.pyc', '.profraw', '.profdata'}


def included(root: Path, path: Path) -> bool:
    rel = path.relative_to(root)
    if rel.name in EXCLUDED_NAMES or rel.suffix in EXCLUDED_SUFFIXES:
        return False
    if any(part in EXCLUDED_PARTS for part in rel.parts):
        return False
    if rel.parts and rel.parts[0] in EXCLUDED_TOP:
        return False
    if len(rel.parts) >= 2 and rel.parts[0] == 'fuzz' and rel.parts[1] in {'target', 'artifacts', 'corpus'}:
        return False
    return path.is_file()


def build(root: Path) -> str:
    lines = []
    for path in sorted((p for p in root.rglob('*') if included(root, p)), key=lambda p: p.as_posix()):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f'{digest}  {path.relative_to(root).as_posix()}')
    return '\n'.join(lines) + '\n'


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = root / 'ARTIFACT_CHECKSUMS.sha256'
    expected = build(root)
    if args.check:
        if not out.exists() or out.read_text() != expected:
            print('FAIL: ARTIFACT_CHECKSUMS.sha256 is stale')
            return 1
        print(f'OK: artifact checksums are deterministic ({len(expected.splitlines())} files)')
        return 0
    out.write_text(expected)
    print(f'OK: wrote {out.name} for {len(expected.splitlines())} files')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
