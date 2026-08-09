# Qualification — Mosaic Tokenizer 0.6.0

0.6 inherits every 0.5 execution/authoring gate and adds the first exact existing-tokenizer compatibility profile.

## New qualification gates

- deterministic raw-BPE compatibility fixture regeneration;
- 608-case independent Python/native raw-BPE differential over arbitrary bytes;
- deliberately permuted single-byte token IDs to prove byte fallback is surface-based rather than ID-number-based;
- `.tiktoken` rank file import → Mosaic pack → export byte identity;
- native C public-API and ASan/UBSan raw-BPE smoke;
- invalid model algorithm and duplicate-BPE-rank fail-closed checks;
- Clang compile plus independent shared-library ABI/BPE differential;
- Clang static analyzer on the native runtime;
- clean-extraction package `.tiktoken` import and native encode smoke;
- tokenizer semantics-version/fingerprint regression test.

## Compatibility boundary

The 0.6 compatibility claim is **raw/single-piece BPE only**. It models the BPE merge-rank operation over one byte sequence. It does not claim complete ordinary-text compatibility with a tiktoken `Encoding`, whose complete behavior also includes encoding-specific pre-tokenization and special-token policy.

## Reproducibility fix found during release qualification

The initial 0.6 release candidate incorrectly retained the older tokenizer fingerprint domain even though raw-BPE execution added a new canonical model algorithm. Qualification rejected that candidate.

0.6 now separates three versions:

- release version: `0.6.0`;
- C ABI version: `0.4.0`;
- canonical tokenizer semantics version: `2`.

`mosaic_tokenizer_semantics_version()` exposes the canonical semantics version. The tokenizer fingerprint includes a `semantics-v2` domain and therefore changes whenever the semantic-runtime version changes, even when the exact model/Unicode pack bytes remain unchanged.

## Local qualified result

- GCC/ASan/UBSan complete native suite: **PASS**;
- independent Clang shared-library ABI and raw-BPE differential: **PASS**;
- Clang static analyzer: **PASS**;
- 1,004 arbitrary-byte C ABI cases: **PASS**;
- 608 independent raw-BPE compatibility cases: **PASS**;
- deterministic authoring/runtime integration: **PASS**;
- Unicode 17 generated/reference differential and malformed-pack suite: **PASS**;
- language/detector composition and malformed-pack suites: **PASS**;
- 10 MiB default Viterbi round-trip samples: `0.17 / 0.17 / 0.17 s`;
- median throughput: **58.8 MiB/s**;
- peak measured RSS: **51.6 MiB**;
- native CLI size: **66,640 bytes**;
- clean-extraction package validation: **PASS**.

## Sealed identities

- tokenizer semantics version: `2`;
- base tokenizer fingerprint: `b2676d5b09dcafe993880590d63f20c9bf1f777a22ef6fde952c8d4dcc4348a5`;
- reference three-language fingerprint: `8732f45005620b85293f418d70d708ba93694d97cec2867dd5c41cc364e44b31`;
- detector + reference-language fingerprint: `ae09203a99f9014ac933401e97db9fefcd4e1ef9919402c6c7c02bb098704e32`;
- Linux x86-64 release archive SHA-256: `1a53e67f0617463fa9ca2fabc779c1dee79ea8c1803eb992623a73fed23f1692`.

## Known compatibility-mode limitation

The raw/single-piece BPE implementation is exact but not yet memory-optimal for very large unsplit pieces. A 1 MiB adversarial unsplit piece measured roughly 67 MiB RSS during development. This does not affect the default weighted-Viterbi path. Full pre-tokenized/streaming BPE work must reduce this before large-piece raw BPE is promoted as the normal bulk-text path.
