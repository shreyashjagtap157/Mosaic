# Changelog

## 0.5.0 — 2026-08-08

Stable deterministic pack-authoring and baseline-training release.

- supported `mosaic-author` CLI shipped in release bundles;
- deterministic explicit model compilation with mandatory 256-byte fallback;
- deterministic compression-first corpus training with bounded candidate growth;
- corpus-order-independent model builds and machine-readable training reports;
- supported language-specialization and detector-pack compilers;
- pack inspection and clean-extraction author/runtime integration tests;
- native C API remains compatible with 0.4.0.

## 0.4.0 — 2026-08-08

Stable automatic stream/document routing release.

- automatic routing streams snapshot the complete tokenizer and detect at EOF;
- automatic editable documents re-detect after every edit;
- child stream/document objects remain valid after parent-tokenizer destruction;
- stream reset and edit-driven language transitions are conformance-tested;
- C API advanced to 0.4.0 without removing 0.3 operations.


## 0.3.0 — 2026-08-07

Stable detector-pack and automatic document-routing release.

- declarative first-byte-indexed detector packs;
- fail-soft score/margin routing;
- auto encode/token-span APIs;
- detector-aware tokenizer fingerprint;
- detector CLI and release-bundle integration;
- 14 malformed detector fixtures and sanitizer coverage.

## 0.2.0 — 2026-08-07

Stable external language-pack release.

- composable declarative language specialization;
- attach-time vocabulary-cost projection;
- order-independent pack-set fingerprinting;
- English/Hindi/Japanese reference packs;
- specialized stream/document snapshots;
- malformed-language and sanitizer qualification;
- mixed-language conformance benchmark.

## 0.1.0 — 2026-08-07

First stable native tokenizer-core release.

- exact arbitrary-byte static tokenization;
- deterministic model and Unicode 17 packs;
- Unicode grapheme spans and invalid-byte preservation;
- stable C ABI and C++-compatible header;
- integrated tokenizer facade;
- stream/full and edit/full semantic equivalence;
- ASan/UBSan and malformed-pack qualification;
- deterministic release packaging.
