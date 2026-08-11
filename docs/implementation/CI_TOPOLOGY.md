# CI Topology

## PR tier

Purpose: fast enough that developers do not route around it.

Required as components become available:

- stable Rust build;
- formatting;
- Clippy with warnings denied for project crates;
- unit tests;
- short property suite;
- reference-vs-engine differential smoke suite;
- pack schema/fixture tests;
- Unicode conformance subset;
- host `no_std` check plus one true no-OS target where practical;
- C ABI smoke tests once FFI exists;
- unsafe-policy check;
- fuzz regression corpus;
- targeted Miri for critical core code;
- sanitizer subset on supported runner.

## Nightly tier

- full property suite;
- full Unicode conformance;
- expanded Miri;
- ASan/UBSan where applicable;
- random streaming chunk differential;
- random edit-sequence differential;
- cross-platform golden vectors;
- scalar-vs-SIMD differential as ISAs arrive;
- 30-120 minute fuzz campaigns;
- pack deterministic rebuild sampling;
- benchmark regression smoke run;
- dependency and reference-pack license/provenance checks.

## Release/Milestone qualification

A milestone qualifies only its claimed subsystem; a public release qualifies every included subsystem.

Release candidates additionally require:

- all supported OS/architectures;
- full cross-platform pack byte-identity rebuild;
- all supported SIMD paths on available fleet;
- extended malicious-pack corpus;
- multi-hour randomized incremental traces where implemented;
- FFI ownership/lifetime and ABI compatibility checks;
- cold/warm benchmarks;
- executable, mapped-data, RSS, and dependency-footprint accounting;
- signature/revocation tests once trust infrastructure exists;
- benchmark artifacts tied to exact source/manifest/build hashes.

Local release-readiness runs may use the fast deferred-Miri lane when the long-running interpreter gate is intentionally postponed; that path still requires the repository, version, checksum, gate-alignment, and fuzz-smoke checks before the full package build is attempted.

## Gate ownership

No benchmark regression may waive a correctness invariant. Performance waivers require a written issue/ADR reference; correctness, exactness, reproducibility, or resource-bound failures block qualification.
