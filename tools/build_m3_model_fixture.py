#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "fixtures" / "packs" / "m3-model-v1.mpack"
EXPECTED = ROOT / "fixtures" / "packs" / "m3-model-v1.expected.toml"
MAGIC = b"MOSPACK\x00"
HEADER = 96
SECTION_ENTRY = 32
VOCAB_HEADER = 40
VOCAB_ENTRY = 16
FIRST_BYTE_INDEX_COUNT = 257


def align(value: int, boundary: int = 8) -> int:
    return (value + boundary - 1) // boundary * boundary


def build_lock() -> bytes:
    # M3 model fixture is self-contained; an empty exact lock graph is still present
    # so canonical metadata shape remains invariant.
    return b"MSLK" + struct.pack("<HHII", 1, 48, 0, 0)


def build_manifest(lock: bytes) -> bytes:
    prefix = b"MSMF" + struct.pack("<HHIIIIII", 1, 0, 1, 1, 1, 1, 1, 0)
    return prefix + hashlib.sha256(lock).digest()


def vocabulary_rows() -> list[tuple[int, int, bytes]]:
    rows: list[tuple[int, int, bytes]] = []
    # Mandatory byte fallback. Cost is deliberately high enough that useful
    # multi-byte pieces win, while every arbitrary byte sequence remains reachable.
    for value in range(256):
        rows.append((value, 100, bytes([value])))

    extras = [
        (256, 120, b"hello"),
        (257, 110, b" world"),
        (258, 220, b"hello world"),
        (259, 75, b"the"),
        (260, 60, b"ing"),
        (261, 55, b"er"),
        (262, 40, b" "),
        (263, 65, b"token"),
        (264, 65, b"izer"),
        (265, 100, "नमस्ते".encode("utf-8")),
        (266, 100, "世界".encode("utf-8")),
        (267, 90, "こんにちは".encode("utf-8")),
        (268, 55, b"::"),
        (269, 55, b"->"),
        (270, 70, b"_id"),
    ]
    rows.extend(extras)
    return rows


def build_vocab() -> bytes:
    rows = vocabulary_rows()
    # Surface order is canonical and makes prefix buckets deterministic.
    ordered = sorted(rows, key=lambda row: (row[2], row[0]))

    blob = bytearray()
    entries: list[bytes] = []
    for token_id, cost, surface in ordered:
        offset = len(blob)
        blob += surface
        entries.append(struct.pack("<IiIHH", token_id, cost, offset, len(surface), 0))

    id_sorted_indices = sorted(range(len(ordered)), key=lambda i: ordered[i][0])
    ids = [ordered[i][0] for i in id_sorted_indices]
    if ids != sorted(ids) or len(set(ids)) != len(ids):
        raise AssertionError("token ids must be unique")

    first = [0] * FIRST_BYTE_INDEX_COUNT
    cursor = 0
    for byte_value in range(256):
        first[byte_value] = cursor
        while cursor < len(ordered) and ordered[cursor][2][0] == byte_value:
            cursor += 1
    first[256] = cursor
    assert cursor == len(ordered)

    entries_offset = VOCAB_HEADER
    id_index_offset = align(entries_offset + len(entries) * VOCAB_ENTRY, 4)
    first_byte_index_offset = align(id_index_offset + len(id_sorted_indices) * 4, 4)
    blob_offset = align(first_byte_index_offset + FIRST_BYTE_INDEX_COUNT * 4, 8)
    total = blob_offset + len(blob)
    out = bytearray(total)
    out[:4] = b"MSVC"
    struct.pack_into(
        "<HHIHHIIIIII",
        out,
        4,
        1,  # version
        0,  # flags
        len(entries),
        VOCAB_ENTRY,
        0,
        entries_offset,
        id_index_offset,
        first_byte_index_offset,
        blob_offset,
        len(blob),
        0,
    )
    for i, entry in enumerate(entries):
        start = entries_offset + i * VOCAB_ENTRY
        out[start:start + VOCAB_ENTRY] = entry
    for i, entry_index in enumerate(id_sorted_indices):
        struct.pack_into("<I", out, id_index_offset + i * 4, entry_index)
    for i, value in enumerate(first):
        struct.pack_into("<I", out, first_byte_index_offset + i * 4, value)
    out[blob_offset:blob_offset + len(blob)] = blob
    return bytes(out)


def build() -> bytes:
    lock = build_lock()
    manifest = build_manifest(lock)
    vocab = build_vocab()
    sections = [(1, manifest), (2, lock), (4, vocab)]

    directory_offset = HEADER
    cursor = align(HEADER + SECTION_ENTRY * len(sections))
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
        1,  # format minor for first vocabulary fixture
        HEADER,
        0,
        file_len,
        len(sections),
        SECTION_ENTRY,
        1,  # SHA-256 canonical profile available now
        directory_offset,
        0,
        1,
    )
    payload[:HEADER] = header
    for index, entry in enumerate(entries):
        start = HEADER + index * SECTION_ENTRY
        payload[start:start + SECTION_ENTRY] = entry

    canonical = bytearray(payload)
    canonical[48:80] = bytes(32)
    payload[48:80] = hashlib.sha256(canonical).digest()
    return bytes(payload)


def expected_text(data: bytes) -> str:
    return (
        f'file_length = {len(data)}\n'
        f'canonical_content_hash = "{data[48:80].hex()}"\n'
        f'file_sha256 = "{hashlib.sha256(data).hexdigest()}"\n'
        f'vocabulary_entries = {len(vocabulary_rows())}\n'
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = build()
    expected = expected_text(data)
    if args.check:
        if not OUT.exists() or OUT.read_bytes() != data:
            raise SystemExit("M3 model fixture differs from deterministic output")
        if not EXPECTED.exists() or EXPECTED.read_text() != expected:
            raise SystemExit("M3 expected metadata differs from deterministic output")
        print(
            f"OK: {OUT.relative_to(ROOT)} deterministic ({len(data)} bytes) "
            f"sha256={hashlib.sha256(data).hexdigest()} entries={len(vocabulary_rows())}"
        )
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(data)
    EXPECTED.write_text(expected)
    print(f"wrote {OUT.relative_to(ROOT)} ({len(data)} bytes)")
    print(expected, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
