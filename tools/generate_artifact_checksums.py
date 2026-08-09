#!/usr/bin/env python3
"""Generate cross-platform deterministic SHA-256 checksums for repository source artifacts.

The manifest is a source-identity artifact, not a checkout-byte snapshot. It therefore:
- includes Git-tracked files plus non-ignored untracked source files when Git metadata is available;
- ignores generated/ignored files;
- canonicalizes CRLF to LF for UTF-8 text so Windows and POSIX checkouts hash identically;
- preserves binary bytes exactly.
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
from pathlib import Path

EXCLUDED_TOP = {'.git', 'build', 'dist', 'target'}
EXCLUDED_PARTS = {'__pycache__'}
EXCLUDED_NAMES = {'ARTIFACT_CHECKSUMS.sha256'}
EXCLUDED_SUFFIXES = {'.pyc', '.profraw', '.profdata'}
BINARY_SUFFIXES = {'.mpack', '.bin', '.sig', '.pub'}


def included(root: Path, path: Path) -> bool:
    rel = path.relative_to(root)
    if rel.name in EXCLUDED_NAMES or rel.suffix.lower() in EXCLUDED_SUFFIXES:
        return False
    if any(part in EXCLUDED_PARTS for part in rel.parts):
        return False
    if rel.parts and rel.parts[0] in EXCLUDED_TOP:
        return False
    if len(rel.parts) >= 2 and rel.parts[0] == 'fuzz' and rel.parts[1] in {'target', 'artifacts', 'corpus'}:
        return False
    return path.is_file()


def source_paths(root: Path) -> list[Path]:
    """Return canonical source candidates, preferring Git's tracked/non-ignored view."""
    try:
        proc = subprocess.run(
            ['git', '-C', str(root), 'ls-files', '-z', '--cached', '--others', '--exclude-standard'],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        rels = [Path(item.decode('utf-8')) for item in proc.stdout.split(b'\0') if item]
        paths = [root / rel for rel in rels]
    except (OSError, subprocess.CalledProcessError, UnicodeDecodeError):
        paths = list(root.rglob('*'))
    return sorted((p for p in paths if included(root, p)), key=lambda p: p.relative_to(root).as_posix())


def canonical_bytes(path: Path) -> bytes:
    data = path.read_bytes()
    if path.suffix.lower() in BINARY_SUFFIXES:
        return data
    try:
        data.decode('utf-8')
    except UnicodeDecodeError:
        return data
    # Git text normalization is CRLF -> LF. Preserve intentional lone CR bytes.
    return data.replace(b'\r\n', b'\n')


def build(root: Path) -> str:
    lines = []
    for path in source_paths(root):
        digest = hashlib.sha256(canonical_bytes(path)).hexdigest()
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
        if not out.exists() or out.read_text(encoding='utf-8') != expected:
            print('FAIL: ARTIFACT_CHECKSUMS.sha256 is stale')
            return 1
        print(f'OK: artifact checksums are cross-platform deterministic ({len(expected.splitlines())} files)')
        return 0
    out.write_text(expected, encoding='utf-8', newline='\n')
    print(f'OK: wrote {out.name} for {len(expected.splitlines())} files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
