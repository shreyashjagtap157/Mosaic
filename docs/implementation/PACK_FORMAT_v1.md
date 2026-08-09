# Mosaic Pack Format v1: M2 Executable Baseline

Status: implementation baseline, subject to M2 Rust qualification  
Date: 2026-08-07

This format is deliberately small and explicit. It is not the final forever-format; it is the first format whose bytes have executable semantics and therefore must be deterministic and fail closed.

## 1. Canonical byte order

All multibyte integer fields are little-endian. No host-native structures, pointers, padding rules, or language serialization formats are part of the pack ABI.

## 2. Header, 96 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `MOSPACK\0` magic |
| 8 | 2 | format major |
| 10 | 2 | format minor |
| 12 | 2 | header length, must be 96 |
| 14 | 2 | flags |
| 16 | 8 | exact file length |
| 24 | 4 | section count |
| 28 | 2 | section-entry length, must be 32 |
| 30 | 2 | hash algorithm, 1 = SHA-256, 2 reserved for BLAKE3 profile |
| 32 | 8 | section-directory offset |
| 40 | 4 | manifest section index |
| 44 | 4 | dependency-lock section index |
| 48 | 32 | canonical content hash |
| 80 | 16 | reserved zero |

The canonical content hash is calculated over the complete file with bytes `[48,80)` replaced by zero. The M2 fixture uses SHA-256. Hash-algorithm agility is format-visible so BLAKE3 can become the optimized/default profile without changing the exact-identity rule.

## 3. Section-directory entry, 32 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | section kind |
| 4 | 4 | flags |
| 8 | 8 | file offset |
| 16 | 8 | byte length |
| 24 | 4 | optional element count |
| 28 | 2 | optional fixed element width |
| 30 | 1 | log2 alignment |
| 31 | 1 | reserved zero |

Entries must be ordered by file offset, must not overlap, and must lie after the section directory. Alignment and resource limits are validated before any section-specific parser runs.

Initial section kinds are:

- `1`: canonical manifest;
- `2`: exact dependency lock graph;
- `3`: byte DFA.

## 4. Manifest v1, 64 bytes

The manifest starts with `MSMF`. It contains versioned runtime-semantics, canonical-leaf, cost-semantics, tie-break, and control-protocol identifiers. Bytes `[32,64)` contain SHA-256 of the complete lock-graph section.

This means changing one dependency identity changes the manifest, and changing the manifest changes the pack content hash.

## 5. Dependency lock graph v1

The lock section starts with `MSLK` and contains only resolved identities. An entry carries:

- publisher and logical-name UTF-8 byte lengths;
- semantic-version triple;
- pack-format major/minor;
- exact 32-byte content hash;
- publisher and name bytes;
- canonical zero padding to a four-byte boundary.

There is no representation for `latest`, `*`, a registry alias, or an unresolved range in this executable section. Those exist only in authoring requirements and must be resolved before canonical execution.

## 6. Byte DFA v1

A DFA section starts with `MSDF` and contains:

- state count and start state;
- strictly sorted `(from_state, byte)` transitions;
- strictly sorted accepting states;
- signed `i32` transition and accept costs.

The M2 reference executor performs linear lookup. The production engine performs binary search over the same sorted transition data. Both accumulate cost using checked `i64` arithmetic and must be differential-equivalent.

## 7. Security and resource behavior

The parser rejects, before execution:

- unsupported header/entry versions;
- length mismatch;
- unsupported/nonzero pack, section, and DFA transition flags;
- nonzero reserved bytes;
- noncanonical inter-section padding or undeclared trailing bytes;
- section-directory overflow;
- too many/too-large sections;
- misalignment;
- overlap or out-of-order sections;
- section out-of-bounds;
- invalid content hash;
- manifest/lock kind mismatch;
- malformed lock identities;
- duplicate logical dependency identities;
- lock hash not matching the manifest;
- DFA state/index/layout violations.

The M2 malicious fixture corpus intentionally contains one regression fixture for each major failure class currently implemented.
