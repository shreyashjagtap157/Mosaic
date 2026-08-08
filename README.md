# Mosaic-µ

Mosaic-µ is a universal tokenization and token-native processing project built around one rule: **source bytes remain authoritative, and every higher-level interpretation must map back to them exactly**.

The project is evidence-first. Exact arbitrary-byte behavior, deterministic pack execution, Unicode conformance, reference/optimized differential testing, and explicit release gates come before ecosystem breadth or speculative optimization.

## Stable tokenizer: 0.6.0

Mosaic Tokenizer 0.6.0 is a stable native tokenizer, deterministic pack-authoring, and raw-BPE compatibility release with:

- exact arbitrary-byte encode/decode;
- mandatory 256-byte fallback, therefore no unknown source bytes;
- deterministic integer-cost Viterbi segmentation;
- exact byte spans for emitted model tokens;
- deterministic, validated, self-contained model packs;
- pinned Unicode 17.0 grapheme segmentation with malformed UTF-8 preserved as opaque bytes;
- one integrated `mosaic_tokenizer` handle for model + Unicode + optional language specialization;
- **external composable language packs** loaded from memory or files;
- **external detector packs** for deterministic document-level automatic language routing;
- fail-soft routing: ambiguity, low confidence, or an unavailable detected language uses the base model;
- pack-independent representability: removing every language pack never makes source bytes unencodable;
- language-pack projection performed once at attachment time, leaving the encode hot path with one indexed adjustment per candidate;
- deterministic order-independent tokenizer fingerprint for the exact set of loaded language packs;
- reference English, Hindi, and Japanese specialization packs demonstrating mixed-language composition;
- static/shared C libraries and C++-compatible public header;
- streaming/full semantic equivalence and editable-document/full-tokenization equivalence;
- deterministic release packaging;
- supported deterministic `mosaic-author` CLI for model/language/detector packs;
- deterministic compression-first baseline vocabulary training with bounded candidate growth;
- raw-byte BPE model execution and deterministic `.tiktoken` mergeable-rank import/export;
- byte fallback is surface-based, so existing model token IDs need not equal byte values;
- GCC + Clang qualification, ASan + UBSan, malformed-pack tests, independent Python oracles, and C/C++ client tests.

The 0.1.0 and 0.2.0 releases remain preserved by Git tags. Version 0.4.0 extended automatic document routing consistently to one-shot, stream-at-EOF, and editable-document APIs. Version 0.5.0 added supported deterministic pack authoring and baseline corpus-to-model training. Version 0.6.0 adds an exact raw/single-piece BPE compatibility profile and `.tiktoken` rank-file interchange without changing the stable 0.4 C ABI.

This is a stable **tokenizer** release, not a claim that the complete future token-native platform is finished. Constrained-Unigram/BPE training quality optimization, production detector/language training, span-level mixed-language routing, bounded-memory streaming, local incremental retokenization, rich compiler/search/IDE projections, SIMD vocabulary matching, and the Wedge Tournament remain later measured work.

## Repository layout

```text
native/                 stable native C runtime, public header, Make/CMake builds
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
```

Qualification covers deterministic fixture regeneration, native C/C++ clients, sanitizers, malformed model/Unicode/language/detector packs, Python/native differentials, Unicode 17 grapheme conformance, stream/full equality, edit/full equality, language-pack composition/order independence, detector fail-soft routing, and performance/RSS regression floors.

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
```

Auto mode applies a specialization only when the detector confidence gate passes and the exact language pack is loaded. Otherwise it uses the base model.

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

## Build a release bundle

```bash
make release
```

The generated `dist/mosaic-tokenizer-0.6.0-<platform>.tar.gz` contains the CLI, libraries, public header, exact model/Unicode packs, English/Hindi/Japanese reference language packs, reference detector pack, runtime fingerprint manifest, checksums, and release/API documentation.

## Rust status

Stable Rust remains the intended primary implementation language. Rust source exists under `crates/`, but the environment that produced the native releases has no `rustc`/`cargo` and no outbound DNS. Consequently the Rust implementation is **not falsely labeled qualified**. Repository CI is configured to compile, Clippy-check, test, and no-std-check it on Rust-capable runners once the GitHub repository receives these commits.

## Current project boundary

The tokenizer itself now has a stable native byte/Unicode/model/language-pack execution path. The broader universal processing platform continues under the converged milestone model, and no post-tournament product wedge is selected by preference alone.

See `docs/implementation/STATUS.md`, `docs/implementation/COMPATIBILITY_0.6.md`, and the latest release qualification evidence for the exact implemented/not-implemented boundary.

The working project name remains provisional.
