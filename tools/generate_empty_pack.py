#!/usr/bin/env python3
"""Generate/check the deterministic M0 empty Mosaic pack fixture."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "fixtures" / "packs" / "empty-v0.mpack"
MAGIC = b"MOSPACK\x00"
HEADER_LEN = 32
FLAG_TEST_FIXTURE = 1


def build() -> bytes:
    return b"".join(
        [
            MAGIC,
            struct.pack("<HHHH", 0, 1, HEADER_LEN, FLAG_TEST_FIXTURE),
            struct.pack("<Q", HEADER_LEN),
            struct.pack("<I", 0),
            struct.pack("<I", 0),
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    expected = build()
    if len(expected) != HEADER_LEN:
        raise SystemExit(f"internal error: generated {len(expected)} bytes")

    if args.check:
        if not OUTPUT.exists():
            raise SystemExit(f"missing fixture: {OUTPUT}")
        actual = OUTPUT.read_bytes()
        if actual != expected:
            raise SystemExit("empty pack fixture differs from deterministic generator output")
        print(f"OK: {OUTPUT.relative_to(ROOT)} is deterministic ({len(actual)} bytes)")
        return 0

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(expected)
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({len(expected)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
