#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
import build_m3_model_fixture as base

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'fixtures/packs/detector/reference-v1.mpack'
EXPECTED = ROOT / 'fixtures/packs/detector/reference-v1.expected.toml'

# Mechanism/conformance data only. These are deliberately tiny exact-byte features,
# not a production language-identification model.
PROFILES = {
    'en': {
        'min_score': 100,
        'features': [(b'tokenizer', 120), (b'hello', 80), (b'world', 50)],
    },
    'hi': {
        'min_score': 100,
        'features': [('नमस्ते'.encode(), 120), ('दुनिया'.encode(), 80)],
    },
    'ja': {
        'min_score': 100,
        'features': [('こんにちは'.encode(), 120), ('世界'.encode(), 80)],
    },
}
MIN_MARGIN = 20


def detector_section() -> bytes:
    tags = sorted(PROFILES)
    tag_index = {tag: i for i, tag in enumerate(tags)}
    blob = bytearray()
    profile_rows = []
    tag_locations = {}
    for tag in tags:
        tag_bytes = tag.encode('ascii')
        off = len(blob); blob += tag_bytes
        tag_locations[tag] = (off, len(tag_bytes))
    features = []
    for tag in tags:
        for surface, weight in PROFILES[tag]['features']:
            if not surface or weight <= 0:
                raise ValueError('detector features must be non-empty with positive weight')
            off = len(blob); blob += surface
            features.append((surface[0], surface, tag_index[tag], weight, off))
    # Canonical first-byte, surface, then profile ordering.
    features.sort(key=lambda x: (x[0], x[1], x[2]))
    if len({(surface, profile) for _, surface, profile, _, _ in features}) != len(features):
        raise ValueError('duplicate detector feature')

    profiles_off = 48
    features_off = base.align(profiles_off + len(tags) * 16, 8)
    first_off = base.align(features_off + len(features) * 16, 4)
    blob_off = base.align(first_off + 257 * 4, 8)
    out = bytearray(blob_off + len(blob))
    out[:4] = b'MSDT'
    max_feature = max((len(surface) for _, surface, _, _, _ in features), default=0)
    struct.pack_into('<HHIIHHIIIIIII', out, 4,
                     1, 0, len(tags), len(features), 16, 16,
                     profiles_off, features_off, first_off, blob_off, len(blob),
                     max_feature, MIN_MARGIN)
    for i, tag in enumerate(tags):
        tag_off, tag_len = tag_locations[tag]
        struct.pack_into('<IHHii', out, profiles_off + i * 16,
                         tag_off, tag_len, 0, PROFILES[tag]['min_score'], 0)

    for i, (_, surface, profile, weight, off) in enumerate(features):
        struct.pack_into('<IHHii', out, features_off + i * 16,
                         off, len(surface), profile, weight, 0)

    first = [0] * 257
    cursor = 0
    for b in range(256):
        first[b] = cursor
        while cursor < len(features) and features[cursor][0] == b:
            cursor += 1
    first[256] = cursor
    for i, value in enumerate(first):
        struct.pack_into('<I', out, first_off + i * 4, value)
    out[blob_off:] = blob
    return bytes(out)


def build_pack() -> bytes:
    lock = base.build_lock(); manifest = base.build_manifest(lock); detector = detector_section()
    sections = [(1, manifest), (2, lock), (6, detector)]
    cursor = base.align(96 + 32 * len(sections)); payload = bytearray(cursor); entries = []
    for kind, data in sections:
        cursor = base.align(cursor); payload.extend(bytes(cursor - len(payload))); off = cursor
        payload.extend(data); cursor += len(data)
        entries.append(struct.pack('<IIQQIHBB', kind, 0, off, len(data), 0, 0, 3, 0))
    header = bytearray(96); header[:8] = b'MOSPACK\0'
    struct.pack_into('<HHHHQIHHQII', header, 8, 1, 3, 96, 0, len(payload), len(sections), 32, 1, 96, 0, 1)
    payload[:96] = header
    for i, entry in enumerate(entries): payload[96+i*32:128+i*32] = entry
    canonical = bytearray(payload); canonical[48:80] = bytes(32)
    payload[48:80] = hashlib.sha256(canonical).digest()
    return bytes(payload)


def expected(data: bytes) -> str:
    return (
        f'profiles = {len(PROFILES)}\n'
        f'features = {sum(len(p["features"]) for p in PROFILES.values())}\n'
        f'min_margin = {MIN_MARGIN}\n'
        f'file_length = {len(data)}\n'
        f'file_sha256 = "{hashlib.sha256(data).hexdigest()}"\n'
    )


def main() -> int:
    ap = argparse.ArgumentParser(); ap.add_argument('--check', action='store_true'); args = ap.parse_args()
    data = build_pack(); exp = expected(data); OUT.parent.mkdir(parents=True, exist_ok=True)
    if args.check:
        if not OUT.exists() or OUT.read_bytes() != data or not EXPECTED.exists() or EXPECTED.read_text() != exp:
            raise SystemExit('detector fixture differs')
        print(f'OK: detector deterministic profiles={len(PROFILES)} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}')
        return 0
    OUT.write_bytes(data); EXPECTED.write_text(exp)
    print(f'wrote {OUT.relative_to(ROOT)} sha256={hashlib.sha256(data).hexdigest()}')
    return 0

if __name__ == '__main__': raise SystemExit(main())
