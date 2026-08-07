# M2 Deterministic Pack Executor Implementation Report

Date: 2026-08-07  
Status: source implementation complete for the M2 baseline; Rust qualification pending

## Implemented runtime source

### Pack v1 outer container

- 96-byte little-endian header.
- 32-byte checked section-directory entries.
- file-size, section-count, section-size, alignment, overlap, ordering, and bounds limits.
- fail-closed unknown pack/section flags.
- canonical zero padding and no undeclared trailing bytes.
- algorithm-declared 32-byte content identity.
- safe no-std SHA-256 implementation for the M2 canonical fixture profile.
- content hash computed with the hash field zeroed to avoid self-reference.

### Canonical manifest and lock graph

- fixed manifest carrying runtime, leaf, cost, tie-break, and control semantic versions.
- manifest binds the complete dependency-lock section hash.
- resolved dependency entries contain UTF-8 publisher/name, semantic version, pack-format version, and exact content hash.
- zero hashes rejected.
- duplicate logical identities rejected with bounded O(n^2) validation.
- dependency presence checked by exact hash; unresolved dependencies reject canonical execution.

### Top-level TokenizerManifest identity

- role/ordinal-sorted exact pack references.
- exact resource-policy hash.
- canonical SHA-256 identity with domain separation.
- no mutable aliases or unresolved ranges in execution identity.

### DFA executable data

- byte DFA v1 with sorted `(state, byte)` transitions.
- sorted accepting states.
- signed i32 transition/accept costs and checked i64 runtime accumulation.
- state, layout, sort-order, and resource-limit validation.
- scalar reference executor uses intentionally linear transition lookup.
- production engine uses independent binary-search transition/accept lookup.

### Canonical path ordering

- `i32` edge cost and `i64` accumulated path cost types.
- reference and engine path comparators independently implement MOS-REQ-022 ordering.
- committed adversarial equal-cost/tie vectors.

## Deterministic fixtures

`fixtures/packs/m2-v1.mpack` is generated deterministically and contains:

- one manifest section;
- one exact dependency lock entry;
- one DFA section accepting exactly byte string `M` as token ID 42;
- transition cost 7 and accept cost -2, producing canonical cost 5.

Current fixture identity:

- file bytes: 412;
- canonical content hash: `69865af97c99565081f3932f1b35384d828669f3981bb796df11591065c74986`;
- file SHA-256: `26ebc78fc205320f364f41f33a355e205ca4704ac69f942281e47c4d8238dc88`;
- exact dependency hash: `e973e5d7e8c522eb89116f21090dfc8b9657a29bc220a3702e87f1bb3251522c`.

The top-level TokenizerManifest conformance vector currently hashes to:

`c6fc3e217dff6aef17d588a9ed7c12bf4db1b7fcc2938292f0a286719ec62764`

## Malicious-pack regression corpus

Fifteen deterministic malformed fixtures currently cover:

- invalid magic;
- file-length lie;
- content-hash tampering;
- nonzero reserved header bytes;
- unsupported pack flags;
- unsupported section flags;
- noncanonical padding;
- overlapping sections;
- section out of bounds;
- lock hash mismatch;
- invalid UTF-8 dependency identity;
- zero dependency hash;
- duplicate dependency identity;
- DFA transition to out-of-range state.
- nonzero DFA transition flags.

## Defect found during implementation

The independent Python format oracle exposed a validation-order defect: section padding was sliced before proving the section offset was inside the file. An attacker-controlled out-of-range offset could therefore have caused a Rust slice panic instead of a fail-closed `SectionOutOfBounds` result. Validation was reordered to prove offset/end bounds before constructing the padding slice, and the triggering malformed pack remains a permanent regression fixture.

This is precisely why an independent oracle exists. The parser is not allowed to certify itself merely because both sides of a test were written on the same optimistic afternoon.

A second reference/oracle asymmetry was found during the same hardening pass: the binary DFA transition format contains both a one-byte flags field and a two-byte reserved field. The Python oracle rejected nonzero transition flags, while the Rust parser initially validated only the adjacent reserved field. The Rust parser now rejects nonzero transition flags explicitly with `UnsupportedDfaTransitionFlags`, and the triggering fixture is permanently retained.

## Executed verification in this environment

Passed:

```text
python tools/build_m2_fixture.py --check
python tools/generate_m2_malformed.py --check
python tools/validate_m2_fixture.py
python tools/validate_path_order.py
python tools/validate_manifest_identity.py
python tools/validate_rust_structure.py
python tools/validate_repo.py
```

The independent M2 oracle accepts the valid pack, resolves one exact dependency, validates the DFA behavior, and rejects all fifteen malformed classes with the intended reason.

## Still required before M2 qualification

- `cargo fmt --all -- --check`;
- `cargo clippy --workspace --all-targets -- -D warnings`;
- `cargo test --workspace`;
- `cargo check -p mosaic-core --no-default-features`;
- Miri on `mosaic-core` and `mosaic-pack`;
- cargo-fuzz campaigns for v0/v1 pack and DFA parsers;
- cross-platform Rust output comparison on CI;
- fix every compiler, Clippy, Miri, or fuzz finding before M2 is marked qualified.

M3 implementation should not be treated as qualified work until those gates are green.
