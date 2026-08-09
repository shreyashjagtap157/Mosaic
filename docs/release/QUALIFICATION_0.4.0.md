# Qualification — Mosaic Tokenizer 0.4.0

## Required gates

- deterministic empty/M2/model-v1/model-v2/language/detector/Unicode pack regeneration;
- malformed model, Unicode, language, and detector pack rejection;
- GCC `-Wall -Wextra -Wpedantic -Werror` build;
- Clang equivalent build and ABI differential;
- ASan + UBSan API/malformed/language/detector clients;
- C and C++ consumer builds;
- Python/native model and Unicode differential tests;
- 20,000 Unicode 17 property-heavy grapheme cases;
- language specialization/order-independent fingerprint tests;
- detector fail-soft routing and routing-overhead tests;
- auto stream parent-lifetime, reset, and detection tests;
- auto editable-document parent-lifetime and edit re-detection tests;
- 10 MiB base round-trip throughput >= 20 MiB/s and RSS <= 128 MiB;
- deterministic release package validation from clean extraction.

## Qualification status

The repository test matrix and independent GCC/Clang ABI gates are green on the local Linux x86-64 environment.

Measured release gate on the sealed candidate:

- 10 MiB round-trip median throughput: **58.8 MiB/s** (3 warmed samples);
- maximum measured RSS: **51.6 MiB**;
- native CLI size: approximately **61 KiB**;
- packaged Linux x86-64 archive SHA-256: `ca05b74948285f1bd906c99599c8954e7291b4b37801d44169bfaaaf893d49b3`;
- clean-extraction CLI/fingerprint/checksum/language/detector/external-static-client validation: **PASS**.
