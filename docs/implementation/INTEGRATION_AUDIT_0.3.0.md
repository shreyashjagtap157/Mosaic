# Integration Audit 0.3.0

## Result

**Direction: aligned with the converged Mosaic tokenizer architecture.**

The stable execution path is now one integrated system:

`exact bytes -> validated model + Unicode -> optional language packs -> optional detector -> deterministic projection -> exact decode / mapped grapheme view`.

## Architectural checks

- Bytes remain authoritative: PASS.
- Mandatory 256-byte fallback remains intact: PASS.
- Unicode remains a mapped view: PASS.
- Language packs cannot add model IDs: PASS.
- Detector is advisory, declarative, and non-native: PASS.
- Low confidence/unavailable pack falls back to base: PASS.
- Pack attachment changes fingerprint deterministically: PASS.
- Language attachment order does not change final fingerprint: PASS.
- Detector malformed input fails before live tokenizer mutation: PASS.
- Reference/optimized static tokenization semantics remain unchanged: PASS.

## Performance review

Detector matching uses a 257-entry first-byte index. On the 2 MiB per-language conformance workload, warmed median auto-routing overhead versus explicitly selected specialization was approximately 8-22% on this host, below the 50% v0.3 regression ceiling. This is a mechanism benchmark only.

## Known limitations

- document-level detector only;
- one detector per tokenizer;
- exact-byte reference features rather than trained statistical profiles;
- one winning specialization in auto mode, not span-level multi-language composition;
- dense per-language model adjustment vectors are retained for fast auto selection and should be revisited for very large vocabularies/catalogs.

No compiler/search/IDE/security consumer branch has been smuggled into the tokenizer before the Wedge Tournament.
