#!/usr/bin/env python3
"""Generate deterministic malformed M2 packs for fail-closed regression tests."""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from build_m2_fixture import build

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "fixtures" / "packs" / "malformed"


def rehash(data: bytearray) -> None:
    data[48:80] = bytes(32)
    data[48:80] = hashlib.sha256(data).digest()


def section(data: bytearray, index: int) -> tuple[int, int]:
    base = 96 + index * 32
    offset = struct.unpack_from("<Q", data, base + 8)[0]
    length = struct.unpack_from("<Q", data, base + 16)[0]
    return offset, length


def generate() -> dict[str, bytes]:
    valid, _ = build()
    cases: dict[str, bytes] = {}

    x = bytearray(valid); x[0] ^= 0x20; cases["bad-magic.mpack"] = bytes(x)
    x = bytearray(valid); struct.pack_into("<Q", x, 16, len(x) + 1); cases["bad-file-length.mpack"] = bytes(x)
    x = bytearray(valid); x[-1] ^= 1; cases["bad-content-hash.mpack"] = bytes(x)

    x = bytearray(valid); x[80] = 1; rehash(x); cases["header-reserved-nonzero.mpack"] = bytes(x)

    x = bytearray(valid); struct.pack_into("<H", x, 14, 1); rehash(x); cases["unsupported-pack-flags.mpack"] = bytes(x)
    x = bytearray(valid); struct.pack_into("<I", x, 96 + 4, 1); rehash(x); cases["unsupported-section-flags.mpack"] = bytes(x)

    # Create an aligned eight-byte gap before the first section and poison one padding byte.
    x = bytearray(valid)
    insert_at = 192
    x[insert_at:insert_at] = bytes(8)
    struct.pack_into("<Q", x, 16, len(x))
    for section_index in range(3):
        base = 96 + section_index * 32
        old_offset = struct.unpack_from("<Q", x, base + 8)[0]
        struct.pack_into("<Q", x, base + 8, old_offset + 8)
    x[insert_at] = 1
    rehash(x); cases["noncanonical-padding.mpack"] = bytes(x)

    x = bytearray(valid)
    struct.pack_into("<Q", x, 96 + 32 + 8, section(x, 0)[0])
    rehash(x); cases["overlapping-sections.mpack"] = bytes(x)

    x = bytearray(valid)
    struct.pack_into("<Q", x, 96 + 2 * 32 + 8, ((len(x) + 64 + 7) // 8) * 8)
    rehash(x); cases["section-out-of-bounds.mpack"] = bytes(x)

    x = bytearray(valid)
    manifest_offset, _ = section(x, 0)
    x[manifest_offset + 32] ^= 1
    rehash(x); cases["lock-hash-mismatch.mpack"] = bytes(x)

    x = bytearray(valid)
    lock_offset, _ = section(x, 1)
    # First publisher byte follows lock header (16) and entry header (48).
    publisher_offset = lock_offset + 64
    x[publisher_offset] = 0xFF
    lock_start, lock_len = section(x, 1)
    new_lock_hash = hashlib.sha256(x[lock_start:lock_start + lock_len]).digest()
    manifest_offset, _ = section(x, 0)
    x[manifest_offset + 32:manifest_offset + 64] = new_lock_hash
    rehash(x); cases["dependency-invalid-utf8.mpack"] = bytes(x)

    x = bytearray(valid)
    lock_offset, lock_len = section(x, 1)
    x[lock_offset + 16 + 16:lock_offset + 16 + 48] = bytes(32)
    manifest_offset, _ = section(x, 0)
    x[manifest_offset + 32:manifest_offset + 64] = hashlib.sha256(x[lock_offset:lock_offset + lock_len]).digest()
    rehash(x); cases["dependency-zero-hash.mpack"] = bytes(x)

    # Duplicate the only resolved dependency entry and shift the DFA section.
    x = bytearray(valid)
    lock_offset, lock_len = section(x, 1)
    entry = bytes(x[lock_offset + 16:lock_offset + lock_len])
    insert_at = lock_offset + lock_len
    x[insert_at:insert_at] = entry
    struct.pack_into("<I", x, lock_offset + 8, 2)
    struct.pack_into("<Q", x, 96 + 1 * 32 + 16, lock_len + len(entry))
    dfa_entry = 96 + 2 * 32
    old_dfa_offset = struct.unpack_from("<Q", x, dfa_entry + 8)[0]
    struct.pack_into("<Q", x, dfa_entry + 8, old_dfa_offset + len(entry))
    struct.pack_into("<Q", x, 16, len(x))
    manifest_offset, _ = section(x, 0)
    lock_offset2, lock_len2 = section(x, 1)
    x[manifest_offset + 32:manifest_offset + 64] = hashlib.sha256(x[lock_offset2:lock_offset2 + lock_len2]).digest()
    rehash(x); cases["dependency-duplicate-identity.mpack"] = bytes(x)

    x = bytearray(valid)
    dfa_offset, _ = section(x, 2)
    # Transition `to` state is at DFA header + 4 bytes within transition.
    struct.pack_into("<I", x, dfa_offset + 24 + 4, 99)
    rehash(x); cases["dfa-state-out-of-bounds.mpack"] = bytes(x)

    x = bytearray(valid)
    dfa_offset, _ = section(x, 2)
    x[dfa_offset + 24 + 13] = 1
    rehash(x); cases["dfa-transition-flags.mpack"] = bytes(x)

    return cases


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    cases = generate()
    OUT.mkdir(parents=True, exist_ok=True)

    if args.check:
        actual_names = {path.name for path in OUT.glob("*.mpack")}
        if actual_names != set(cases):
            raise SystemExit(
                f"malformed fixture set differs: expected {sorted(cases)}, got {sorted(actual_names)}"
            )
        for name, expected in cases.items():
            if (OUT / name).read_bytes() != expected:
                raise SystemExit(f"malformed fixture differs from generator: {name}")
        print(f"OK: {len(cases)} malformed M2 fixtures are deterministic")
        return 0

    for stale in OUT.glob("*.mpack"):
        if stale.name not in cases:
            stale.unlink()
    for name, data in sorted(cases.items()):
        (OUT / name).write_bytes(data)
    print(f"wrote {len(cases)} malformed M2 fixtures to {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
