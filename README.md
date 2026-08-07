# Mosaic-µ

Mosaic-µ is a universal tokenization and token-native processing project built around one rule: **source bytes remain authoritative, and every higher-level interpretation must map back to them exactly**.

The project is evidence-first. Exact arbitrary-byte behavior, deterministic pack execution, Unicode conformance, reference/optimized differential testing, and explicit release gates come before ecosystem breadth or speculative optimization.

## Stable tokenizer core: 0.1.0

The repository now contains a stable native tokenizer-core release with:

- exact arbitrary-byte encode/decode;
- mandatory 256-byte fallback, therefore no unknown source bytes;
- deterministic `i32` token costs and checked `i64` Viterbi accumulation;
- token IDs with exact byte spans and costs;
- deterministic validated model packs;
- pinned Unicode 17.0 grapheme segmentation pack;
- invalid UTF-8 preserved as opaque one-byte regions;
- integrated `mosaic_tokenizer` handle combining model + Unicode projections;
- deterministic tokenizer runtime fingerprint;
- stable C 0.1 ABI, static library, shared library, and C++-compatible header;
- streaming-at-EOF equivalence;
- editable-document/full-tokenization equivalence;
- CLI and deterministic Linux x86-64 release packaging;
- GCC + Clang qualification, ASan + UBSan, malformed-pack tests, independent Python oracles, and C/C++ client tests.

This release is the **stable tokenizer core**, not a claim that every future universal-platform feature is complete. Language/script/domain pack composition, compiler/search/IDE projections, local incremental retokenization, rich Token IR, SIMD matching, the Wedge Tournament, and the Rust production port remain subsequent milestones.

## Repository layout

```text
native/                 stable native C runtime, public header, Make/CMake builds
conformance/            independent C/C++ client and malformed-input tests
crates/                 planned primary Rust implementation and reference/engine crates
fixtures/packs/          deterministic model, Unicode, and adversarial pack fixtures
tools/                   deterministic pack builders, oracles, qualification, packaging
docs/spec/               architecture baseline and binding convergence
docs/adr/                architectural decision records
docs/implementation/     milestone plans, audits, qualification reports
.github/workflows/       PR, nightly, and release qualification
```

The production/runtime implementation deliberately no longer lives under `conformance/`; tests consume the runtime rather than owning it.

## Build the native tokenizer

```bash
make native
```

Artifacts are written under `build/`:

- `mosaic-tokenizer`
- `libmosaic.a`
- `libmosaic.so` on Linux

CMake is also supported:

```bash
cmake -S native -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

## Test

```bash
python -m pip install -r requirements-dev.txt
make test
```

This executes deterministic pack regeneration, native C/C++ clients, ASan/UBSan stress, malformed-pack rejection, Python/native tokenizer differentials, Unicode differentials, streaming/full equality, and edit/full equality.

Full native release qualification:

```bash
python tools/qualify_native.py
```

## CLI

```bash
./build/mosaic-tokenizer --version
./build/mosaic-tokenizer fingerprint fixtures/packs/m3-model-v1.mpack fixtures/packs/unicode17-v1.mpack
./build/mosaic-tokenizer analyze fixtures/packs/m3-model-v1.mpack fixtures/packs/unicode17-v1.mpack INPUT
./build/mosaic-tokenizer encode fixtures/packs/m3-model-v1.mpack INPUT
./build/mosaic-tokenizer roundtrip fixtures/packs/m3-model-v1.mpack INPUT
./build/mosaic-tokenizer encode-u32 fixtures/packs/m3-model-v1.mpack INPUT IDS.bin
./build/mosaic-tokenizer decode-u32 fixtures/packs/m3-model-v1.mpack IDS.bin OUTPUT
./build/mosaic-tokenizer graphemes fixtures/packs/unicode17-v1.mpack INPUT
```

## Build a release bundle

```bash
python tools/build_release.py
```

The generated `dist/mosaic-tokenizer-0.1.0-<platform>.tar.gz` contains the CLI, libraries, public header, exact packs, runtime fingerprint manifest, checksums, and release/API documentation.

## Rust status

Stable Rust remains the intended primary implementation language. Rust source exists under `crates/`, but the environment that produced the first native release has no `rustc`/`cargo` and no outbound DNS. Consequently the Rust implementation is **not falsely labeled qualified**. GitHub CI is configured to compile, Clippy-check, test, and no-std-check it on Rust-capable runners.

## Current project boundary

The tokenizer core has reached a stable native release. The broader platform continues under the converged milestone model:

- M0 engineering substrate
- M1 exact source substrate
- M2 deterministic pack executor
- M3 Unicode + static tokenizer
- M4 Wedge Tournament
- M5A/B/C/H measured product branch
- M6–M8 incrementality, platform expansion, security/performance/storage hardening

See `docs/implementation/INTEGRATION_AUDIT_0.1.0.md` for the exact implemented/not-implemented boundary.

The working project name remains provisional.
