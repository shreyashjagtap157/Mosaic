# Mosaic-µ

Mosaic-µ is a universal tokenization and token-native processing project built around one rule: **source bytes remain authoritative, and every higher-level interpretation must map back to them exactly**.

The project is evidence-first. Exact arbitrary-byte behavior, deterministic pack execution, Unicode conformance, reference/optimized differential testing, and explicit release gates come before ecosystem breadth or speculative optimization.

## Candidate tokenizer: 0.1.3.0

Mosaic Tokenizer 0.1.3.0 is the current pre-stable enterprise candidate tokenizer and token-processing runtime. It includes deterministic authoring, compatibility, Unicode/security/normalization, exact incremental processing, rich TokenDocument projections, multiscale storage, bounded caching, trust/registry control-plane support, sealed runtime policies, cold Token IR, bounded parallel execution, and privacy-preserving observability:

- exact arbitrary-byte encode/decode;
- mandatory 256-byte fallback, therefore no unknown source bytes;
- deterministic integer-cost Viterbi segmentation;
- exact byte spans for emitted model tokens;
- deterministic, validated, self-contained model packs;
- pinned Unicode 17.0 grapheme segmentation with malformed UTF-8 preserved as opaque bytes;
- one integrated `mosaic_tokenizer` handle for model + Unicode + optional language specialization;
- **external composable language packs** loaded from memory or files;
- **external detector packs** for deterministic document-level and span-level automatic language routing;
- fail-soft routing: ambiguity, low confidence, or an unavailable detected language uses the base model;
- mixed-language span routing with gapless byte ranges and exact decode preservation;
- pack-independent representability: removing every language pack never makes source bytes unencodable;
- language-pack projection performed once at attachment time, leaving the encode hot path with one indexed adjustment per candidate;
- deterministic order-independent tokenizer fingerprint for the exact set of loaded language packs;
- reference English, Hindi, and Japanese specialization packs demonstrating mixed-language composition;
- Unicode 17 script spans and security evidence for bidi controls, default-ignorables, noncharacters, deprecated characters, and mixed-script text;
- a separately versioned Unicode 16 normalization pack with exact source-mapped NFD/NFC/NFKD/NFKC/NFKC-casefold shadow views;
- normalization provenance that supports decomposition, combining-mark reordering, multi-source composition, compatibility mappings, Hangul, and malformed UTF-8 barriers without modifying source;
- static/shared C libraries and C++-compatible public header;
- exact online Viterbi streaming that commits only survivor-prefix token IDs before EOF under a caller-defined pending-byte ceiling;
- callback/visitor Unicode-security scanning that can process millions of findings without materializing a finding array;
- exact Viterbi incremental documents that reuse a provably unaffected canonical prefix and expose actual reprocessed/reused byte counts;
- exact checkpoint-resynchronizing Viterbi documents that reuse both unchanged prefix and suffix after the online survivor state converges;
- immutable TokenDocument/Core IR snapshots with exact source identity, compact model-token projection, and optional grapheme projection;
- streaming/full semantic equivalence and editable-document/full-tokenization equivalence;
- deterministic release packaging;
- supported deterministic `mosaic-author` CLI for model/language/detector packs;
- deterministic compression-first baseline vocabulary training with bounded candidate growth;
- raw-byte BPE model execution and deterministic `.tiktoken` mergeable-rank import/export;
- byte fallback is surface-based, so existing model token IDs need not equal byte values;
- GCC + Clang qualification, ASan + UBSan, malformed-pack tests, independent Python oracles, and C/C++ client tests.
- rich TokenDocument security/normalization/lexical/semantic projections and canonical cold serialization;
- token-aligned KiB blocks, MiB macroblocks, authenticated packed model projection, bounded content cache and backend protocol;
- immutable runtime limits/sealing, semantic-vs-deployment identity separation, lock-free runtime counters;
- optional Ed25519 publisher trust library and content-addressed SQLite/WAL pack registry/lockfiles;
- reusable bounded parallel executor with deterministic result order and per-item failure isolation;
- synchronous metadata-only observability with concurrent callback qualification and no source-content exposure.
- supported deterministic Python wheel with ownership-safe wrappers over the same native C ABI.
- cross-platform dependency-minimal root CMake build, portable Windows/POSIX threading internals, deterministic SPDX SBOM, SLSA-shaped provenance, and release checksum inventory.

All prior releases remain preserved by Git tags. The old three-component tags are historical milestone/release identifiers; the canonical product version now follows the four-part `S.M.N.P` policy in `docs/VERSIONING_POLICY.md`. The 0.13–0.24 historical line added rich projections, declarative lexing, semantic/sub-byte views, multiscale storage, enterprise caches, runtime policy, trust, registry, canonical cold Token IR, bounded parallel execution, and observability. Canonical tokenization semantics remain version 2. The enterprise freeze established C ABI 1.0.0, optional trust ABI 1.0.0, and the binary-format contracts under `abi/`; `0.1.3.0` advances the native C ABI to 1.1.0 with additive span-routing entry points. Those contract versions remain independent of the product release number.

Mosaic `0.1.3.0` is the current **universal tokenization and token-native processing platform candidate**. The native C runtime is the qualified implementation on the platforms already exercised; the dependency-minimal Windows Clang preset and pinned Rust 1.97.1 workspace gate now pass, while macOS/Miri/fuzz/race-detector and remaining support-matrix gates must complete before the product is allowed to graduate to stability generation `1`. Frozen ABI/format contracts remain enforced during this candidate period. Adaptive byte-patch LLMs, GPU acceleration, learned statistical span classifiers, very large community pack catalogs, and any scanner VM remain post-baseline research rather than hidden stabilization blockers.

## Repository layout

```text
native/                 native C runtime, public header, Make/CMake builds
conformance/            independent C/C++ clients and malformed-input tests
crates/                 intended primary Rust implementation and reference/engine crates
fixtures/packs/          deterministic model, Unicode, language, detector, and adversarial pack fixtures
tools/                   deterministic pack builders, oracles, qualification, packaging
docs/spec/               architecture baseline and binding convergence
docs/adr/                architectural decision records
docs/implementation/     milestone plans, audits, qualification reports
.github/workflows/       PR, nightly, and release qualification
```

## Build

```bash
make native
```

CMake is also supported:

```bash
cmake -S native -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

## Test and qualify

```bash
python -m pip install -r requirements-dev.txt
make test
python tools/qualify_native.py
make release-readiness
python tools/validate_release_readiness.py
```

Qualification covers exact bounded streaming and security visitors, deterministic fixture regeneration, native C/C++ clients, sanitizers, malformed model/Unicode/language/detector/security/normalization packs, Python/native differentials, Unicode 17 grapheme/security conformance, 10,000 ICU-backed Unicode-16 normalization comparisons, exact normalization provenance, stream/full equality, edit/full equality, language-pack composition/order independence, detector fail-soft routing, deterministic span routing, and performance/RSS regression floors.

## CLI

Base tokenizer:

```bash
./build/mosaic-tokenizer --version
./build/mosaic-tokenizer fingerprint fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack
./build/mosaic-tokenizer analyze fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack INPUT
./build/mosaic-tokenizer roundtrip fixtures/packs/model-v2.mpack INPUT
```

Language-specialized tokenizer:

```bash
./build/mosaic-tokenizer fingerprint-languages \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack \
  fixtures/packs/language/en-v1.mpack fixtures/packs/language/hi-v1.mpack

./build/mosaic-tokenizer analyze-languages \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack INPUT \
  fixtures/packs/language/en-v1.mpack fixtures/packs/language/hi-v1.mpack fixtures/packs/language/ja-v1.mpack

./build/mosaic-tokenizer roundtrip-languages \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack INPUT \
  fixtures/packs/language/en-v1.mpack fixtures/packs/language/hi-v1.mpack fixtures/packs/language/ja-v1.mpack
```

Automatic document routing:

```bash
./build/mosaic-tokenizer detect \
  fixtures/packs/detector/reference-v1.mpack INPUT

./build/mosaic-tokenizer analyze-auto \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack \
  fixtures/packs/detector/reference-v1.mpack INPUT \
  fixtures/packs/language/en-v1.mpack fixtures/packs/language/hi-v1.mpack fixtures/packs/language/ja-v1.mpack

./build/mosaic-tokenizer analyze-span-auto \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack \
  fixtures/packs/detector/reference-v1.mpack INPUT \
  fixtures/packs/language/en-v1.mpack fixtures/packs/language/hi-v1.mpack fixtures/packs/language/ja-v1.mpack
```

Auto mode applies a specialization only when the detector confidence gate passes and the exact language pack is loaded. Otherwise it uses the base model.


## Bounded processing

0.9 adds `mosaic_online_stream` for exact survivor-prefix Viterbi streaming and `mosaic_security_visit` for callback-based security evidence without output-array materialization. The online API fails explicitly with `MOSAIC_ERROR_RESOURCE_LIMIT` if the configured unresolved-source ceiling cannot be respected; raw-BPE models are explicitly unsupported by this Viterbi-specific API. See `docs/implementation/ONLINE_STREAMING_0.9.md`.

## Mapped normalization

0.8 adds a normalization shadow-view layer without changing authoritative source bytes or model token IDs. The bundled normalization pack is explicitly Unicode 16.0.0, generated offline with ICU 76.1; the deployed runtime itself has no ICU dependency. Unicode 17 segmentation/security packs remain independently versioned.

```bash
./build/mosaic-tokenizer normalize-map fixtures/packs/normalization16-v1.mpack nfd INPUT
./build/mosaic-tokenizer fingerprint-normalization \
  fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack \
  fixtures/packs/normalization16-v1.mpack
```

NFD, NFC, NFKD, NFKC and NFKC-casefold views return exact output bytes plus per-output-unit source span mappings. Invalid UTF-8 remains an opaque byte barrier instead of being replaced.

## Existing-tokenizer compatibility

0.6 supports raw/single-piece byte BPE and `.tiktoken` mergeable-rank files:

```bash
./tools/mosaic_author.py import-tiktoken ranks.tiktoken model.mpack
./build/mosaic-tokenizer roundtrip model.mpack INPUT
./tools/mosaic_author.py export-tiktoken model.mpack exported.tiktoken
```

This claim is intentionally narrower than full ordinary tiktoken text encoding: a complete tiktoken `Encoding` also includes its Unicode regex pre-tokenizer and special-token protocol. Mosaic does not approximate those silently. See `docs/implementation/COMPATIBILITY_0.6.md`.

## Pack authoring

The release ships `mosaic-author`, which can compile explicit model vocabularies, train a deterministic compression-first model from corpora, compile language-specialization packs, compile detector packs, and inspect MOSPACK metadata. Every authored model receives mandatory byte fallback automatically.

```bash
./tools/mosaic_author.py train-model corpus.txt -o model.mpack --vocab-size 4096
./tools/mosaic_author.py inspect model.mpack
```

The 0.5 trainer is a reproducible practical baseline, not yet the final constrained-Unigram/BPE quality-optimization toolchain. Runtime model-quality claims remain benchmark-gated. See `docs/implementation/AUTHORING_0.5.md`.

## Language-pack behavior

A v0.5 language pack is declarative data, not native executable code. It contributes deterministic cost adjustments keyed by byte surfaces already representable in the loaded model vocabulary. It **cannot add hidden model token IDs**. This preserves fixed-vocabulary model compatibility while allowing an external language pack to specialize segmentation.

The reference fixtures intentionally demonstrate this mechanism:

- `tokenizer`: base `[token, izer]` → English pack `[tokenizer]`;
- `नमस्ते दुनिया`: base two pieces → Hindi pack one existing vocabulary piece;
- `こんにちは世界`: base two pieces → Japanese pack one existing vocabulary piece.

All three packs may be attached simultaneously. Their effects and tokenizer fingerprint are independent of attachment order. Duplicate packs for the same language tag fail with `MOSAIC_ERROR_CONFLICT` in v0.5 rather than silently stacking contradictory policies.

These tiny packs are **conformance/reference packs**, not claims of production linguistic quality.

## Pack registry and lockfiles

0.21 ships `mosaic-registry`, a content-addressed SQLite/WAL control-plane tool. It atomically installs exact pack objects, cryptographically derives verified trust state, resolves numeric SemVer requirements into exact hash-pinned canonical lockfiles, audits corruption, and garbage-collects only unreferenced objects. Mutable `latest` is never a canonical execution identity. See `docs/implementation/REGISTRY_v1.md`.

0.22 adds a canonical cold TokenDocument format for durable caches, IPC, and reproducible preprocessing artifacts. The record is endian-defined rather than native-struct serialization, preserves only requested public projections (plus hidden lexical dependencies required by semantic components), binds source/tokenizer identities, rejects noncanonical or re-authenticated malformed records, and offers caller-configurable deserialization ceilings. See `docs/implementation/TOKEN_DOCUMENT_SERIALIZATION_v1.md`.

0.23 adds a bounded reusable worker pool for independent-input tokenization. Batches require a sealed tokenizer, use explicit queue/item/input ceilings, preserve input order, isolate per-item failures, and share the exact normal tokenizer path. See `docs/implementation/PARALLEL_EXECUTOR_0.23.md`.

## Pack trust

0.20 ships optional `libmosaic_trust` static/shared libraries. Trust verification authenticates exact SHA-256 pack identities with Ed25519 publisher keys and explicit revocation, after ordinary structural pack validation. The core tokenizer remains free of OpenSSL. See `docs/implementation/TRUST_v1.md`.

## Enterprise runtime policy

0.19 adds explicit serving ceilings, immutable tokenizer sealing, deployment runtime identities, and lock-free operational counters. Configure packs and limits first, call `mosaic_tokenizer_seal`, then share the tokenizer across worker threads. Runtime ceilings are deployment policy and do not change the semantic tokenizer fingerprint. See `docs/implementation/RUNTIME_POLICY_v1.md`.

## Build a release bundle

```bash
make release
```

The generated `dist/mosaic-tokenizer-<version>-<platform>.tar.gz` contains the CLI, libraries, public header, exact model/Unicode packs, English/Hindi/Japanese reference language packs, reference detector pack, Unicode-17 security pack, Unicode-16 normalization pack, runtime fingerprint manifest, checksums, and release/API documentation.

## Rust status

Stable Rust remains the intended primary implementation language. The local Windows workspace is pinned to Rust 1.97.1 and passes rustfmt, strict Clippy, workspace tests, and the `mosaic-core` no-default-features gate. Bounded nightly Miri coverage on `mosaic-core` and `mosaic-pack` also passes on the current Windows host via `tools/validate_miri.py`. A bounded nightly fuzz smoke across the four bundled harnesses also passes via `tools/validate_fuzz.py`. Full cargo-fuzz campaigns, macOS, ARM64, and race-detector support-matrix jobs remain separate stable-generation gates.

## Current project boundary

The tokenizer itself has a mature native byte/Unicode/model/language-pack execution path, but product stability is intentionally evidence-gated under `docs/VERSIONING_POLICY.md`. The broader universal processing platform continues under the converged milestone model, and no post-tournament product wedge is selected by preference alone.

See `docs/implementation/STATUS.md`, `docs/implementation/COMPATIBILITY_0.6.md`, and the latest release qualification evidence for the exact implemented/not-implemented boundary.

The working project name remains provisional.
