#!/usr/bin/env python3
"""Independent M2 pack-format oracle used when Rust qualification is unavailable."""
from __future__ import annotations

import hashlib
import struct
import tomllib
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALID = ROOT / "fixtures" / "packs" / "m2-v1.mpack"
MALFORMED = ROOT / "fixtures" / "packs" / "malformed"


class Reject(ValueError):
    pass


@dataclass(frozen=True)
class Section:
    kind: int
    offset: int
    length: int


def reject(name: str) -> None:
    raise Reject(name)


def parse_outer(data: bytes) -> tuple[list[Section], int, int]:
    if len(data) < 96: reject("TooShort")
    if data[:8] != b"MOSPACK\0": reject("InvalidMagic")
    major, minor, header_len, flags = struct.unpack_from("<HHHH", data, 8)
    file_len = struct.unpack_from("<Q", data, 16)[0]
    count = struct.unpack_from("<I", data, 24)[0]
    entry_len, hash_alg = struct.unpack_from("<HH", data, 28)
    directory = struct.unpack_from("<Q", data, 32)[0]
    manifest_index, lock_index = struct.unpack_from("<II", data, 40)
    if major != 1: reject("UnsupportedFormatMajor")
    if flags != 0: reject("UnsupportedPackFlags")
    if header_len != 96: reject("UnsupportedHeaderLength")
    if entry_len != 32: reject("UnsupportedSectionEntryLength")
    if hash_alg != 1: reject("UnsupportedHashAlgorithm")
    if file_len != len(data): reject("LengthMismatch")
    if any(data[80:96]): reject("ReservedNotZero")
    if directory < 96 or directory + count * 32 > len(data): reject("SectionDirectoryOutOfBounds")
    if manifest_index >= count or lock_index >= count: reject("InvalidSectionIndex")

    canonical = bytearray(data)
    declared = bytes(canonical[48:80])
    canonical[48:80] = bytes(32)
    if hashlib.sha256(canonical).digest() != declared: reject("ContentHashMismatch")

    sections: list[Section] = []
    previous_end = directory + count * 32
    for index in range(count):
        base = directory + index * 32
        kind, section_flags, offset, length, element_count, element_width, alignment_log2, reserved = struct.unpack_from("<IIQQIHBB", data, base)
        if reserved: reject("ReservedNotZero")
        if kind == 0: reject("InvalidSectionKind")
        if section_flags != 0: reject("UnsupportedSectionFlags")
        alignment = 1 << alignment_log2
        if offset % alignment: reject("MisalignedSection")
        if offset < previous_end: reject("OverlappingOrUnsortedSections")
        if offset > len(data): reject("SectionOutOfBounds")
        end = offset + length
        if end > len(data): reject("SectionOutOfBounds")
        if any(data[previous_end:offset]): reject("NonCanonicalPadding")
        if element_width and element_count * element_width > length: reject("InvalidElementLayout")
        sections.append(Section(kind, offset, length))
        previous_end = end
    if previous_end != len(data): reject("NonCanonicalPadding")
    if sections[manifest_index].kind != 1: reject("ManifestSectionKindMismatch")
    if sections[lock_index].kind != 2: reject("LockSectionKindMismatch")
    return sections, manifest_index, lock_index


def section_bytes(data: bytes, section: Section) -> bytes:
    return data[section.offset:section.offset + section.length]


def parse_lock(data: bytes) -> list[bytes]:
    if len(data) < 16: reject("InvalidLockGraphLength")
    if data[:4] != b"MSLK": reject("InvalidLockGraphMagic")
    version, entry_header_len, count, reserved = struct.unpack_from("<HHII", data, 4)
    if version != 1: reject("UnsupportedLockGraphVersion")
    if entry_header_len != 48: reject("UnsupportedLockEntryLength")
    if reserved: reject("ReservedNotZero")
    offset = 16
    seen: set[tuple[str, str, int, int, int]] = set()
    hashes: list[bytes] = []
    for _ in range(count):
        if offset + 48 > len(data): reject("InvalidLockGraphLength")
        publisher_len, name_len, ma, mi, pa, fmt_ma, fmt_mi, flags = struct.unpack_from("<HHHHHHHH", data, offset)
        if not publisher_len or not name_len or flags: reject("InvalidDependencyIdentity")
        content_hash = data[offset + 16:offset + 48]
        if not any(content_hash): reject("ZeroDependencyHash")
        start = offset + 48
        end_pub = start + publisher_len
        end_name = end_pub + name_len
        if end_name > len(data): reject("InvalidLockGraphLength")
        try:
            publisher = data[start:end_pub].decode("utf-8")
            name = data[end_pub:end_name].decode("utf-8")
        except UnicodeDecodeError:
            reject("InvalidIdentityUtf8")
        key = (publisher, name, ma, mi, pa)
        if key in seen: reject("DuplicateDependencyIdentity")
        seen.add(key); hashes.append(content_hash)
        offset = (end_name + 3) & ~3
        if offset > len(data) or any(data[end_name:offset]): reject("ReservedNotZero")
    if offset != len(data): reject("InvalidLockGraphLength")
    return hashes


def parse_manifest(data: bytes, lock: bytes) -> None:
    if len(data) != 64: reject("InvalidManifestLength")
    if data[:4] != b"MSMF": reject("InvalidManifestMagic")
    version, flags = struct.unpack_from("<HH", data, 4)
    if version != 1: reject("UnsupportedManifestVersion")
    if flags or struct.unpack_from("<I", data, 28)[0]: reject("ReservedNotZero")
    if data[32:64] != hashlib.sha256(lock).digest(): reject("DependencyLockHashMismatch")


def parse_dfa(data: bytes):
    if len(data) < 24 or data[:4] != b"MSDF": reject("InvalidDfaHeader")
    version, flags, states, start, transition_count, accept_count = struct.unpack_from("<HHIIII", data, 4)
    if version != 1: reject("UnsupportedDfaVersion")
    if flags: reject("ReservedNotZero")
    if not states or start >= states: reject("DfaStateOutOfBounds")
    expected = 24 + transition_count * 16 + accept_count * 12
    if expected != len(data): reject("InvalidDfaLayout")
    transitions=[]; prior=None
    for i in range(transition_count):
        base=24+i*16
        frm,to,cost,symbol,tflags,reserved=struct.unpack_from("<IIiBBH",data,base)
        if to >= states or frm >= states: reject("DfaStateOutOfBounds")
        if tflags: reject("UnsupportedDfaTransitionFlags")
        if reserved: reject("ReservedNotZero")
        key=(frm,symbol)
        if prior is not None and prior >= key: reject("DfaTransitionsNotStrictlySorted")
        prior=key; transitions.append((frm,symbol,to,cost))
    accept_entries=[]; prior_state=None
    for i in range(accept_count):
        base=24+transition_count*16+i*12
        state,token_id,cost=struct.unpack_from("<IIi",data,base)
        if state >= states: reject("DfaStateOutOfBounds")
        if prior_state is not None and prior_state >= state: reject("DfaAcceptsNotStrictlySorted")
        prior_state=state; accept_entries.append((state,token_id,cost))
    return start, transitions, accept_entries


def run_dfa(dfa, payload: bytes):
    state, transitions, accepts = dfa
    total=0
    table={(frm,symbol):(to,cost) for frm,symbol,to,cost in transitions}
    for symbol in payload:
        found=table.get((state,symbol))
        if found is None: return None
        state,cost=found; total += cost
    for accept_state,token_id,cost in accepts:
        if accept_state == state: return token_id,total+cost
    return None


def validate(data: bytes, deep: bool = True) -> list[bytes]:
    sections, manifest_index, lock_index = parse_outer(data)
    if not deep: return []
    lock = section_bytes(data, sections[lock_index])
    hashes = parse_lock(lock)
    parse_manifest(section_bytes(data, sections[manifest_index]), lock)
    for section in sections:
        if section.kind == 3:
            parse_dfa(section_bytes(data, section))
    return hashes


def main() -> int:
    valid = VALID.read_bytes()
    hashes = validate(valid)
    expected = tomllib.loads((ROOT / "fixtures" / "packs" / "m2-v1.expected.toml").read_text(encoding="utf-8"))
    if len(valid) != expected["file_length"]:
        raise SystemExit("m2 expected file length mismatch")
    if valid[48:80].hex() != expected["canonical_content_hash"]:
        raise SystemExit("m2 expected canonical content hash mismatch")
    if hashlib.sha256(valid).hexdigest() != expected["file_sha256"]:
        raise SystemExit("m2 expected file hash mismatch")
    expected_dep = hashlib.sha256(b"mosaic-m2-placeholder-dependency").digest()
    assert hashes == [expected_dep]
    sections, _, _ = parse_outer(valid)
    dfa_section = next(section for section in sections if section.kind == 3)
    dfa = parse_dfa(section_bytes(valid, dfa_section))
    assert run_dfa(dfa, b"M") == (42, 5)
    assert run_dfa(dfa, b"") is None
    assert run_dfa(dfa, b"X") is None
    assert run_dfa(dfa, b"MM") is None

    expected_errors = {
        "bad-magic.mpack": "InvalidMagic",
        "bad-file-length.mpack": "LengthMismatch",
        "bad-content-hash.mpack": "ContentHashMismatch",
        "header-reserved-nonzero.mpack": "ReservedNotZero",
        "unsupported-pack-flags.mpack": "UnsupportedPackFlags",
        "unsupported-section-flags.mpack": "UnsupportedSectionFlags",
        "noncanonical-padding.mpack": "NonCanonicalPadding",
        "overlapping-sections.mpack": "OverlappingOrUnsortedSections",
        "section-out-of-bounds.mpack": "SectionOutOfBounds",
        "lock-hash-mismatch.mpack": "DependencyLockHashMismatch",
        "dependency-invalid-utf8.mpack": "InvalidIdentityUtf8",
        "dependency-zero-hash.mpack": "ZeroDependencyHash",
        "dependency-duplicate-identity.mpack": "DuplicateDependencyIdentity",
        "dfa-state-out-of-bounds.mpack": "DfaStateOutOfBounds",
        "dfa-transition-flags.mpack": "UnsupportedDfaTransitionFlags",
    }
    for name, error in expected_errors.items():
        try:
            validate((MALFORMED / name).read_bytes())
        except Reject as exc:
            if str(exc) != error:
                raise SystemExit(f"{name}: expected {error}, got {exc}") from exc
        else:
            raise SystemExit(f"{name}: malformed fixture was accepted")

    print(f"OK: M2 valid fixture accepted with {len(hashes)} exact dependency")
    print(f"OK: {len(expected_errors)} malformed fixture classes rejected as expected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
