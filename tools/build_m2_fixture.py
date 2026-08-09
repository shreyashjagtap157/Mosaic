#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "fixtures" / "packs" / "m2-v1.mpack"
MAGIC = b"MOSPACK\x00"
HEADER = 96
ENTRY = 32


def align(value: int, boundary: int = 8) -> int:
    return (value + boundary - 1) // boundary * boundary


def build_lock() -> tuple[bytes, bytes]:
    dependency_hash = hashlib.sha256(b"mosaic-m2-placeholder-dependency").digest()
    publisher = b"org.mosaic.fixture"
    name = b"unicode-placeholder"
    entry = bytearray()
    entry += struct.pack(
        "<HHHHHHHH",
        len(publisher),
        len(name),
        17,
        0,
        0,
        1,
        0,
        0,
    )
    entry += dependency_hash
    entry += publisher + name
    entry += bytes((-len(entry)) % 4)
    lock = b"MSLK" + struct.pack("<HHII", 1, 48, 1, 0) + bytes(entry)
    return lock, dependency_hash


def build() -> tuple[bytes, bytes]:
    lock, dependency_hash = build_lock()
    manifest_prefix = b"MSMF" + struct.pack("<HHIIIIII", 1, 0, 1, 1, 1, 1, 1, 0)
    manifest = manifest_prefix + hashlib.sha256(lock).digest()

    # Byte DFA accepts exactly b"M". Transition cost 7 plus accept cost -2 = 5.
    dfa = bytearray(b"MSDF" + struct.pack("<HHIIII", 1, 0, 2, 0, 1, 1))
    dfa += struct.pack("<IIiBBH", 0, 1, 7, ord("M"), 0, 0)
    dfa += struct.pack("<IIi", 1, 42, -2)

    sections = [(1, manifest), (2, lock), (3, bytes(dfa))]
    directory_offset = HEADER
    cursor = align(HEADER + ENTRY * len(sections))
    payload = bytearray(cursor)
    entries: list[bytes] = []

    for kind, data in sections:
        cursor = align(cursor)
        payload.extend(bytes(cursor - len(payload)))
        offset = cursor
        payload.extend(data)
        cursor += len(data)
        entries.append(struct.pack("<IIQQIHBB", kind, 0, offset, len(data), 0, 0, 3, 0))

    file_len = len(payload)
    header = bytearray(HEADER)
    header[:8] = MAGIC
    struct.pack_into(
        "<HHHHQIHHQII",
        header,
        8,
        1,
        0,
        HEADER,
        0,
        file_len,
        len(sections),
        ENTRY,
        1,  # SHA-256
        directory_offset,
        0,  # manifest section index
        1,  # dependency lock section index
    )
    payload[:HEADER] = header
    for index, entry in enumerate(entries):
        payload[HEADER + index * ENTRY : HEADER + (index + 1) * ENTRY] = entry

    canonical = bytearray(payload)
    canonical[48:80] = bytes(32)
    payload[48:80] = hashlib.sha256(canonical).digest()
    return bytes(payload), dependency_hash


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data, dependency_hash = build()

    if args.check:
        if not OUT.exists() or OUT.read_bytes() != data:
            raise SystemExit("m2 fixture differs from deterministic output")
        print(
            f"OK: {OUT.relative_to(ROOT)} deterministic ({len(data)} bytes) "
            f"file_sha256={hashlib.sha256(data).hexdigest()} "
            f"dependency={dependency_hash.hex()}"
        )
        return 0

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(data)
    print(f"wrote {OUT.relative_to(ROOT)} ({len(data)} bytes)")
    print(f"dependency hash: {dependency_hash.hex()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
