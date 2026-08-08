# Changelog

## 0.10.0 — 2026-08-08

Stable exact incremental Viterbi document release.

- canonical token-cache prefix reuse after edits with a proven safe restart boundary;
- transactional edit application and exact source-copy behavior;
- 1,000 randomized edit/full differentials and language-specialized coverage;
- explicit reprocessed/reused byte metrics;
- raw-BPE incremental rejection pending a separate merge-semantics proof;
- near-end 10 MiB locality/performance release gate;
- backward-compatible C API advanced to 0.8.0 while tokenization semantics remain version 2.

## 0.9.0 — 2026-08-08

Stable bounded-processing release.

- exact online weighted-Viterbi streaming with survivor-prefix commitment before EOF;
- caller-defined unresolved-source memory ceiling and explicit resource-limit behavior;
- callback/visitor Unicode-security evidence without findings-array materialization;
- adversarial no-prefix fixture plus arbitrary-byte/chunk differential and sanitizer coverage;
- bounded-processing 10 MiB throughput/RSS regression gates;
- backward-compatible C API advanced to 0.7.0 while tokenization semantics remain version 2.

## 0.8.0 — 2026-08-08

Stable source-mapped normalization shadow-view release.

- self-contained Unicode-16 normalization pack generated offline from ICU 76.1;
- exact source-mapped NFD, NFC, NFKD, NFKC, and NFKC-casefold views;
- algorithmic Hangul decomposition/composition and canonical combining-mark reordering;
- malformed UTF-8 preserved as opaque normalization barriers;
- 10,000 independent ICU differential cases and 11 malformed normalization fixtures;
- generator composition-pair defect found and fixed before release by differential testing;
- backward-compatible C API surface advanced to 0.6.0 while tokenization semantics remain version 2.

## 0.7.0 — 2026-08-08

Stable Unicode-17 script/security evidence release.

- standalone declarative Unicode-17 security pack;
- all 176 Script values and exact byte-mapped script spans;
- bidi-control, default-ignorable, noncharacter, deprecated-character evidence;
- deterministic mixed-script evidence with Common/Inherited/Unknown excluded from script counting;
- security-aware tokenizer fingerprint and high-level attachment API;
- 11 malformed security-pack regression fixtures plus ASan/UBSan coverage;
- backward-compatible C API surface advanced to 0.5.0.

## 0.6.0 — 2026-08-08

Stable raw-BPE compatibility release.

- model packs can declare weighted-Viterbi or raw-byte BPE execution;
- byte fallback validation is surface-based, allowing arbitrary existing token IDs;
- `.tiktoken` mergeable-rank import and exact eligible export;
- independent raw-BPE differential oracle and sanitizer coverage;
- unsupported algorithms, duplicate surfaces, and duplicate BPE ranks fail closed;
- full tiktoken regex/special-token pipeline is intentionally not claimed by this profile.

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
