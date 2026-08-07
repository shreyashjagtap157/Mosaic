#!/usr/bin/env python3
"""Independent executable oracle for the M3 static vocabulary tokenizer."""
from __future__ import annotations

import hashlib
import random
import struct
import tomllib
from dataclasses import dataclass
from pathlib import Path

from validate_m2_fixture import parse_outer, section_bytes, parse_lock, parse_manifest, Reject

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "fixtures" / "packs" / "m3-model-v1.mpack"
EXPECTED = ROOT / "fixtures" / "packs" / "m3-model-v1.expected.toml"


@dataclass(frozen=True)
class VocabEntry:
    token_id: int
    cost: int
    surface: bytes


@dataclass(frozen=True)
class Token:
    start: int
    end: int
    token_id: int
    cost: int
    surface: bytes


def parse_vocab(data: bytes) -> tuple[list[VocabEntry], list[int], list[int]]:
    if len(data) < 40 or data[:4] != b"MSVC":
        raise Reject("InvalidVocabularyHeader")
    version, flags, count, entry_len, reserved = struct.unpack_from("<HHIHH", data, 4)
    entries_off, id_index_off, first_off, blob_off, blob_len, reserved2 = struct.unpack_from("<IIIIII", data, 16)
    if version != 1:
        raise Reject("UnsupportedVocabularyVersion")
    if flags or reserved or reserved2:
        raise Reject("ReservedNotZero")
    if entry_len != 16 or entries_off != 40:
        raise Reject("InvalidVocabularyLayout")
    entries_end = entries_off + count * entry_len
    id_end = id_index_off + count * 4
    first_end = first_off + 257 * 4
    blob_end = blob_off + blob_len
    if not (entries_end <= id_index_off <= id_end <= first_off <= first_end <= blob_off <= blob_end == len(data)):
        raise Reject("InvalidVocabularyLayout")
    for a, b in [(entries_end, id_index_off), (id_end, first_off), (first_end, blob_off)]:
        if any(data[a:b]):
            raise Reject("NonCanonicalPadding")

    entries: list[VocabEntry] = []
    prior_surface: tuple[bytes, int] | None = None
    for index in range(count):
        base = entries_off + index * entry_len
        token_id, cost, surface_off, surface_len, eflags = struct.unpack_from("<IiIHH", data, base)
        if eflags:
            raise Reject("UnsupportedVocabularyEntryFlags")
        if not surface_len or surface_off + surface_len > blob_len:
            raise Reject("InvalidVocabularySurface")
        surface = data[blob_off + surface_off: blob_off + surface_off + surface_len]
        key = (surface, token_id)
        if prior_surface is not None and prior_surface >= key:
            raise Reject("VocabularyNotCanonical")
        prior_surface = key
        entries.append(VocabEntry(token_id, cost, surface))

    id_index = list(struct.unpack_from(f"<{count}I", data, id_index_off)) if count else []
    if sorted(id_index) != list(range(count)):
        raise Reject("InvalidVocabularyIdIndex")
    ids = [entries[i].token_id for i in id_index]
    if any(a >= b for a, b in zip(ids, ids[1:])):
        raise Reject("VocabularyTokenIdsNotUnique")

    first = list(struct.unpack_from("<257I", data, first_off))
    if first[0] != 0 or first[-1] != count or any(a > b for a, b in zip(first, first[1:])):
        raise Reject("InvalidVocabularyFirstByteIndex")
    for byte_value in range(256):
        for index in range(first[byte_value], first[byte_value + 1]):
            if entries[index].surface[0] != byte_value:
                raise Reject("InvalidVocabularyFirstByteIndex")

    # Mandatory Mosaic byte fallback: token ID b is exactly the one-byte surface b.
    by_id = {entry.token_id: entry for entry in entries}
    for value in range(256):
        entry = by_id.get(value)
        if entry is None or entry.surface != bytes([value]):
            raise Reject("MissingByteFallback")

    return entries, id_index, first


def parse_pack(data: bytes):
    sections, manifest_index, lock_index = parse_outer(data)
    lock = section_bytes(data, sections[lock_index])
    parse_lock(lock)
    parse_manifest(section_bytes(data, sections[manifest_index]), lock)
    vocab_sections = [s for s in sections if s.kind == 4]
    if len(vocab_sections) != 1:
        raise Reject("InvalidVocabularySectionCount")
    entries, id_index, first = parse_vocab(section_bytes(data, vocab_sections[0]))
    return entries, id_index, first


def path_key(path: tuple[Token, ...]):
    total = sum(token.cost for token in path)
    # Lower tuple wins. Longer span at first divergence therefore uses negative len.
    lexical = tuple((-(t.end - t.start), 1, t.token_id, t.start, t.end) for t in path)
    return total, len(path), lexical


def tokenize_reference(entries: list[VocabEntry], payload: bytes) -> tuple[Token, ...]:
    best: list[tuple[Token, ...] | None] = [None] * (len(payload) + 1)
    best[0] = ()
    for start in range(len(payload)):
        prefix = best[start]
        if prefix is None:
            continue
        for entry in entries:  # intentionally scans all entries
            if payload.startswith(entry.surface, start):
                end = start + len(entry.surface)
                candidate = prefix + (Token(start, end, entry.token_id, entry.cost, entry.surface),)
                if best[end] is None or path_key(candidate) < path_key(best[end]):
                    best[end] = candidate
    result = best[len(payload)]
    if result is None:
        raise AssertionError("byte fallback must make every payload reachable")
    return result


def tokenize_indexed(entries: list[VocabEntry], first: list[int], payload: bytes) -> tuple[Token, ...]:
    best: list[tuple[Token, ...] | None] = [None] * (len(payload) + 1)
    best[0] = ()
    for start, byte_value in enumerate(payload):
        prefix = best[start]
        if prefix is None:
            continue
        for index in range(first[byte_value], first[byte_value + 1]):
            entry = entries[index]
            if payload.startswith(entry.surface, start):
                end = start + len(entry.surface)
                candidate = prefix + (Token(start, end, entry.token_id, entry.cost, entry.surface),)
                if best[end] is None or path_key(candidate) < path_key(best[end]):
                    best[end] = candidate
    result = best[len(payload)]
    if result is None:
        raise AssertionError("byte fallback must make every payload reachable")
    return result


def decode(entries: list[VocabEntry], id_index: list[int], token_ids: list[int]) -> bytes:
    # Independent binary lookup through the pack's ID index.
    out = bytearray()
    for token_id in token_ids:
        lo, hi = 0, len(id_index)
        found = None
        while lo < hi:
            mid = (lo + hi) // 2
            entry = entries[id_index[mid]]
            if entry.token_id < token_id:
                lo = mid + 1
            elif entry.token_id > token_id:
                hi = mid
            else:
                found = entry
                break
        if found is None:
            raise Reject("UnknownTokenId")
        out.extend(found.surface)
    return bytes(out)


def main() -> int:
    data = PACK.read_bytes()
    expected = tomllib.loads(EXPECTED.read_text())
    if len(data) != expected["file_length"]:
        raise SystemExit("M3 expected file length mismatch")
    if data[48:80].hex() != expected["canonical_content_hash"]:
        raise SystemExit("M3 canonical content hash mismatch")
    if hashlib.sha256(data).hexdigest() != expected["file_sha256"]:
        raise SystemExit("M3 file SHA-256 mismatch")
    entries, id_index, first = parse_pack(data)
    if len(entries) != expected["vocabulary_entries"]:
        raise SystemExit("M3 vocabulary count mismatch")

    cases = [
        b"",
        b"hello",
        b"hello world",
        b"the tokenizer",
        "नमस्ते दुनिया".encode(),
        "こんにちは世界".encode(),
        bytes(range(256)),
        b"\x00\xffhello\x80world",
    ]
    rng = random.Random(0x4D4F53414943)
    for _ in range(1000):
        cases.append(bytes(rng.randrange(256) for _ in range(rng.randrange(0, 128))))

    for payload in cases:
        reference = tokenize_reference(entries, payload)
        indexed = tokenize_indexed(entries, first, payload)
        if reference != indexed:
            raise SystemExit(f"reference/indexed segmentation mismatch for {payload!r}")
        ids = [token.token_id for token in indexed]
        if decode(entries, id_index, ids) != payload:
            raise SystemExit(f"round trip failed for {payload!r}")
        cursor = 0
        for token in indexed:
            if token.start != cursor or token.end <= token.start:
                raise SystemExit("token spans are not a gapless positive-length partition")
            cursor = token.end
        if cursor != len(payload):
            raise SystemExit("token spans do not cover payload")

    malformed_root = ROOT / "fixtures" / "packs" / "malformed-m3"
    expected_errors = {
        "vocab-bad-magic.mpack": "InvalidVocabularyHeader",
        "vocab-version.mpack": "UnsupportedVocabularyVersion",
        "vocab-flags.mpack": "ReservedNotZero",
        "vocab-entry-flags.mpack": "UnsupportedVocabularyEntryFlags",
        "vocab-zero-surface.mpack": "InvalidVocabularySurface",
        "vocab-surface-oob.mpack": "InvalidVocabularySurface",
        "vocab-id-index-duplicate.mpack": "InvalidVocabularyIdIndex",
        "vocab-first-index-bad.mpack": "InvalidVocabularyFirstByteIndex",
        "vocab-missing-byte-fallback.mpack": "MissingByteFallback",
        "vocab-noncanonical-order.mpack": "VocabularyNotCanonical",
        "vocab-duplicate-token-id.mpack": "VocabularyTokenIdsNotUnique",
        "vocab-wrong-bucket.mpack": "InvalidVocabularyFirstByteIndex",
    }
    for name, expected_error in expected_errors.items():
        try:
            parse_pack((malformed_root / name).read_bytes())
        except Reject as exc:
            if str(exc) != expected_error:
                raise SystemExit(f"{name}: expected {expected_error}, got {exc}") from exc
        else:
            raise SystemExit(f"{name}: malformed vocabulary pack was accepted")

    sample = tokenize_indexed(entries, first, b"hello world")
    print(f"OK: M3 vocabulary accepted with {len(entries)} entries and full byte fallback")
    print(f"OK: {len(cases)} reference/indexed differential round-trip cases")
    print(f"OK: {len(expected_errors)} malformed vocabulary fixtures rejected")
    print("sample hello world:", [(t.token_id, t.start, t.end, t.cost) for t in sample])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
