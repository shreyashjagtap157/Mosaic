# Mosaic 0.1.2.1 Qualification

Status: native/application Linux x86-64 qualification complete; Windows x86-64 dependency-minimal qualification complete; Rust 1.97.1 hardening gate complete on Windows.

## Fresh native qualification

- dependency-minimal fresh CMake build: **22/22 CTest PASS**;
- full fresh CMake build with ICU/trust: **24/24 CTest PASS**;
- stable C API differential: **PASS** — 1,004 byte round trips, 200 stream chunkings, 250 edit/full differentials;
- frozen C ABI contract: **PASS**;
- frozen binary-format/tokenizer-semantics contract: **PASS**.

## Application surfaces

Against the same freshly rebuilt `0.1.2.1` full library:

- Python binding suite: **4/4 PASS**;
- `mosaicd` authentication, arbitrary-byte round trip, batch executor, resumable streams, limits, saturation, JSON/Prometheus metrics: **PASS**;
- authenticated immutable registry HTTP catalog/object transport, SHA-256 verification, corrupt-CAS fail-closed behavior: **PASS**.

## Rust hardening

Implemented in source:

- `mosaic-pack` Unicode 17 section registration/export and resource/error contracts;
- incremental SHA-256 block-retention correctness fix;
- standard SHA-256 split-update and million-update regression vectors;
- canonical Unicode 17 fixture grapheme-span regression;
- invalid/truncated UTF-8 opaque-byte regression.

Executed locally on Windows x86-64 with `rustc 1.97.1 (8bab26f4f 2026-07-14)` and `cargo 1.97.1 (c980f4866 2026-06-30)`:

- `cargo fmt --all -- --check`: **PASS**;
- `cargo clippy --workspace --all-targets -- -D warnings`: **PASS**;
- `cargo test --workspace`: **PASS**;
- `cargo check -p mosaic-core --no-default-features`: **PASS**.

## Source identity

- product version synchronization: **PASS**;
- deterministic artifact checksum generation/check: **PASS**;
- repository validator: **PASS**, with its explicit note that Rust compile/test gates are separate.

No stable-generation `1.0.0.0` claim is made. Current-source macOS/ARM64, Miri/fuzz/race and final multi-platform stable-release gates remain open.
