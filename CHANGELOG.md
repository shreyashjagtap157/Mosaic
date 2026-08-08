# Changelog

## 0.20.0 — 2026-08-08

Optional enterprise Ed25519 pack-trust release.

- separate dependency-isolated trust library;
- exact SHA-256 pack identity signatures;
- bounded publisher trust store and revocation;
- structural-validation-first authorization;
- offline pack signing/key-ID authoring.

## 0.19.0 — 2026-08-08

Enterprise immutable runtime-policy release.

- explicit input/output/TokenDocument resource ceilings;
- allocation-aware high-level encode/decode limits;
- immutable tokenizer sealing for shared serving;
- separate deployment runtime identity;
- lock-free runtime metrics and resource-rejection counters;
- stream/document/incremental policy propagation and concurrency qualification.

## 0.18.0 — 2026-08-08

Authenticated persistent/distributed cache-backend protocol.

- backend-neutral immutable record callbacks;
- whole-record and payload SHA-256 integrity;
- wrong-key replay and corrupt-backend fail-closed behavior;
- explicit integrity error and capability.

## 0.17.0 — 2026-08-08

Enterprise bounded content-cache release.

- thread-safe bounded O(1)-average in-memory LRU cache;
- explicit byte/entry ceilings and eviction semantics;
- cache hit/miss/replace/eviction/removal/peak metrics;
- projection-specific block cache keys bound to content identity.

## 0.16.0 — 2026-08-08

Enterprise multiscale block planning and compact model serialization.

- adaptive token-aligned KiB processing blocks and MiB macroblocks;
- content identities bound to tokenizer fingerprints;
- block/macroblock resource ceilings and oversized-token signaling;
- checksummed fixed-bit ID + ULEB length serialization with exact span recovery.

## 0.15.0 — 2026-08-08

Stable semantic enrichment and sub-byte view release.

- identifier, number, and string source-mapped semantic components;
- semantic TokenDocument density/capability;
- generic bounded MSB0/LSB0 sub-byte extraction;
- nibble/cross-byte sanitizer conformance.

## 0.14.0 — 2026-08-08

Stable declarative lexer and lexical TokenDocument release.

- bounded lexer pack v1 with exact lexical partitions;
- C/Python/Rust/JSON reference profiles;
- longest-prefix delimiters and optional nested block comments;
- lexical TokenDocument projection and capability bit;
- standalone/integrated lexer CLI;
- nine malformed lexer pack classes and sanitizer coverage.

## 0.13.0 — 2026-08-08

Rich TokenDocument projection and capability-negotiation release.

- optional immutable security-evidence projection;
- optional immutable mapped-normalization projection;
- tokenizer capability discovery;
- density-controlled extended TokenDocument creation;
- allocation-failure ownership path hardened under static analysis.

## 0.12.0 — 2026-08-08

Core TokenDocument / Token IR release.

- immutable exact source snapshot with source SHA-256 and tokenizer fingerprint;
- compact model-token ID/byte-length column with exact byte-span projection;
- optional Unicode grapheme projection;
- explicit density flags with no hidden construction of unrequested views;
- automatic-routing snapshot records its detection decision;
- document lifetime is independent from its parent tokenizer.

## 0.11.0 — 2026-08-08

Exact checkpoint-resynchronizing incremental Viterbi release.

- cached resumable online-Viterbi checkpoints;
- middle-edit forward state resynchronization with exact suffix reuse;
- compact 8-byte internal resync token cache;
- 500 randomized edit/full differentials plus sanitizer coverage;
- 10 MiB middle edit reprocesses about 0.625% and is over 8x faster than a fresh full tokenization on the reference host;
- raw-BPE explicitly remains unsupported by the Viterbi-specific resync API.

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
