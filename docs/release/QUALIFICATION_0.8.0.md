# Mosaic Tokenizer 0.8.0 Qualification Evidence

Status: **qualified locally on the native Linux x86-64 reference host.**

Required gates:

- deterministic rebuild of all pack fixtures, including normalization16;
- 10,000 randomized ICU-76/Unicode-16 normalization comparisons across NFD/NFC/NFKD/NFKC/NFKC-casefold;
- exact normalization provenance structural invariants and malformed UTF-8 barriers;
- 11 malformed normalization packs rejected under ASan/UBSan;
- complete existing tokenizer, Unicode17, language, detector, security, raw-BPE and authoring regressions;
- GCC and Clang builds with warnings as errors;
- Clang normalization smoke linked to the independently built Mosaic library;
- CMake/CTest including ICU-backed normalization differential test;
- static analysis and 10 MiB base tokenizer throughput/RSS regression floor;
- deterministic clean-extraction distribution validation, including an external C consumer that attaches normalization.

Observed pack identity:

- normalization pack size: 273,080 bytes
- normalization pack SHA-256: `78848429c7a6e97e87466222b104e004188df6d710587641b5dda50fdbe6dac5`
- Unicode data: 16.0.0
- ICU generator: 76.1
- CCC ranges: 393
- canonical decomposition mappings: 2,081
- compatibility decomposition mappings: 5,913
- NFKC-casefold mappings: 10,554
- canonical composition pairs: 961

The deployed Mosaic runtime has no ICU dependency; ICU is used only by the offline generator and independent conformance executable.

## Observed native release regression

- 10 MiB mixed-language base tokenizer: 0.17 / 0.17 / 0.17 s after warm-up
- median throughput: 58.8 MiB/s
- maximum observed RSS: 52,968 KiB (51.7 MiB)
- native CLI size: 89,176 bytes
- language specialization controlled fixture: 27.3% fewer tokens, warmed median overhead approximately -0.6% in the measured run
- automatic detector routing: worst warmed median overhead 17.5% versus explicit specialization in the measured run
- release archive SHA-256: `aae71c1579aa5c395c562e71bbf8b964002ff56d244301b1819afb9a40d1fa36`
- normalization-aware reference tokenizer fingerprint: `8deaf6cf8ddc47e842e399ff027b61a6e20d97571b53d59ec21ddf13266f2d6d`

GCC/ASan/UBSan, Clang, Clang static analysis, CMake Release/CTest, and clean-extraction package validation all completed without a normative failure. The stable Rust implementation remains unqualified on this host because `rustc`/`cargo` are unavailable.
