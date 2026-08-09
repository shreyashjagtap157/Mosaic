# M0/M1 Bootstrap Implementation Report

Date: 2026-08-07

## Scope executed

The environment contained no pre-existing Mosaic source repository. A new clean Git repository was created from the converged architecture, while preserving the prior Markdown specification as historical input.

## Documentation generated

- convergence addendum with MOS-REQ-021 through MOS-REQ-025;
- converged implementation plan;
- M0 milestone plan;
- repository layout/reorganization record;
- CI topology;
- benchmark manifest template;
- reference hardware policy;
- Wedge Tournament protocol;
- empty pack fixture specification;
- implementation status;
- ADR-001 through ADR-007.

## Code implemented

### `mosaic-core`

- `#![no_std]` and unsafe forbidden;
- 64-bit byte offsets and lengths;
- checked byte ranges;
- conceptual one-byte canonical leaves;
- non-allocating leaf iterator;
- borrowed source provider;
- optional alloc-backed owned source;
- exact byte-range reads.

### `mosaic-ir`

- namespace, kind, token, and projection IDs;
- fixed-size content-hash holder;
- source mappings;
- minimal projected-token record.

### `mosaic-reference`

- intentionally simple materialized byte projection used as semantic oracle.

### `mosaic-engine`

- independently implemented byte projection;
- differential test against `mosaic-reference`.

### `mosaic-pack`

- checked parser for deterministic M0 fixture header;
- explicit malformed-header tests;
- no unsafe code.

## Fuzz/bootstrap infrastructure

- source-byte fuzz target;
- pack-header fuzz target;
- locations reserved by design for later fuzz corpus/artifacts;
- deterministic fixture generator.

## CI

Workflow skeletons exist for:

- PR validation;
- nightly cross-platform tests + Miri;
- manual release qualification.

These evolve as M1-M3 add conformance suites and optimized paths.

## Verification performed in this environment

Passed:

```text
python3 tools/validate_repo.py
python3 tools/generate_empty_pack.py --check
```

Validated:

- required repository files exist;
- workspace and template TOML parse correctly;
- converged requirements MOS-REQ-021..025 are present;
- M0 pack fixture has expected binary layout;
- fixture deterministically regenerates byte-for-byte;
- fixture SHA-256 = 130fc642ba0fac9b4a15a5228ba6e2c776c8bd3c3bceb1b985c3c774ec9dd615.

Not executable in the current environment:

```text
cargo fmt
cargo clippy
cargo test
cargo check --no-default-features
cargo miri
cargo fuzz
```

Reason: no `rustc` or `cargo` executable is installed in the environment. These remain mandatory qualification gates and are not reported as passing.

## Immediate next implementation sequence

1. Run Rust qualification on stable Rust and fix compile/lint findings.
2. Complete M1 source identity/version wrapper and cross-platform byte golden vectors.
3. Begin M2 production pack-header/section-directory design from ADR-004 rather than extending M0 fixture ad hoc.
4. Implement resolved manifest/lock graph data structures.
5. Implement checked DFA representation/executor in `mosaic-reference` first, then `mosaic-engine`.
6. Add canonical `i32`/`i64` cost types and adversarial path-order vectors before Viterbi work.

No language packs, scanner VM, CDC, registry, SIMD, or rich IR should be started before their milestone earns them.
