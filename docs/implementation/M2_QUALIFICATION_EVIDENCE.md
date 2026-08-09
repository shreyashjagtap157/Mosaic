# M2 Qualification Evidence

Date: 2026-08-07  
Status: **source baseline implemented; not Rust-qualified**

## Executed locally

The current artifact environment does not contain `rustc`, `cargo`, or `rustfmt`, and no cached Rust packages are available. The independent non-Rust qualification command is therefore the strongest executable local evidence available here:

```text
python3 tools/qualify.py --python-only
```

Observed result:

```text
OK: fixtures/packs/empty-v0.mpack is deterministic (32 bytes)
OK: fixtures/packs/m2-v1.mpack deterministic (412 bytes)
OK: 15 malformed M2 fixtures are deterministic
OK: M2 valid fixture accepted with 1 exact dependency
OK: 15 malformed fixture classes rejected as expected
OK: 4 canonical path-order vectors
OK: TokenizerManifest v1 identity c6fc3e217dff6aef17d588a9ed7c12bf4db1b7fcc2938292f0a286719ec62764
OK: 5 workspace members structurally present
OK: reference implementation is not a production dependency of mosaic-engine
PASS: Python-independent gates completed
NOT QUALIFIED: Rust gates intentionally skipped
```

Running the full qualifier reaches the same independent checks and then exits with status 2:

```text
FAIL: Rust qualification unavailable; missing rustc, cargo
```

That failure is intentional evidence of the gate, not a test failure being waived. M2 cannot be promoted until the unchecked Rust/CI items in `M2_QUALIFICATION_CHECKLIST.md` are green.

## Defects caught before Rust qualification

1. **Bounds-before-slice defect.** An attacker-controlled section offset could previously reach a padding slice before the offset was proven in-bounds. The order is now bounds/end validation first, slice second, and the malformed pack remains a regression fixture.
2. **DFA flags asymmetry.** The independent Python oracle rejected a nonzero one-byte DFA transition flags field while the Rust parser initially checked only the adjacent reserved `u16`. The Rust parser now rejects the flags field explicitly and the fixture remains in the corpus.

These are exactly the kinds of defects MOS-REQ-023's independent-reference strategy is meant to expose.

## Current immutable fixture identities

- M0 empty fixture SHA-256: `130fc642ba0fac9b4a15a5228ba6e2c776c8bd3c3bceb1b985c3c774ec9dd615`
- M2 canonical content hash: `69865af97c99565081f3932f1b35384d828669f3981bb796df11591065c74986`
- M2 file SHA-256: `26ebc78fc205320f364f41f33a355e205ca4704ac69f942281e47c4d8238dc88`
- M2 exact dependency hash: `e973e5d7e8c522eb89116f21090dfc8b9657a29bc220a3702e87f1bb3251522c`
- TokenizerManifest v1 identity: `c6fc3e217dff6aef17d588a9ed7c12bf4db1b7fcc2938292f0a286719ec62764`
