# Mosaic Tokenizer 0.15.0 Qualification

- inherited 0.14 suite green;
- semantic-only TokenDocument does not expose lexical projection unless requested;
- identifier/number/string component vectors map to exact source ranges;
- semantic snapshot is immutable and independent of caller/tokenizer lifetime;
- MSB0/LSB0 nibble vectors pass;
- cross-byte extraction and out-of-range rejection pass;
- ASan/UBSan coverage includes semantic and sub-byte paths;
- packaged external C client exercises both APIs.

- Clang static analyzer completed cleanly after fixing a NULL-source sub-byte dereference path found during qualification;
- explicit NULL-source regression vector is retained in `semantic_subbyte_smoke.c`;
- final packaged archive SHA-256: `e712d969f54e405aac778525189823ae7d769e015d580d8224178182997fb3f9`.
