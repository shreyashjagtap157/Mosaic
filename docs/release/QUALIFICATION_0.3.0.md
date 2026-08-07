# Mosaic Tokenizer 0.3.0 Qualification

Date: 2026-08-07
Platform qualified locally: Linux x86-64

## Release identity

- Model v2 SHA-256: `5295e581cb87faa4c289fb5590c178cb73b56ec7946938fe8f815ceca2b2dffb`
- Unicode 17 pack SHA-256: `23318e7f1fcb2848208d6560d7b3a0e08e6ff042099dea7f5f10b42127c7a27f`
- Detector v1 SHA-256: `20dd4456afb5d0c50c9d033449f324189a09d961af89e2e21363730acafa81a9`
- English language pack SHA-256: `75179427e58eeade21cfa70d5ea1af127a497a849f55d6cb6e915a421c104551`
- Hindi language pack SHA-256: `309fdd7db255d9dd5ba704c095805e7049a08fae0f9bb8655edaab100b142e9e`
- Japanese language pack SHA-256: `7abcebfa000d09583190edd33bb6833245fa79c2c0977ba2743d83f3e83aa9e3`
- Base tokenizer fingerprint: `9e542a728d94b98fed083d61247a554e22a0f4dceb23f39c7106a41f4d1341ba`
- Three-language fingerprint: `425a2646174b21fec0cabeb8df4dad43769a6cae3c020ffc7e54d1961117b248`
- Detector + three-language fingerprint: `054cda43c4eaa31975599ce7eb471eb9460a749aae2b60530466d559b86081a6`
- Release archive SHA-256: `51b795e2a302b9a17fcc049bef05989942b815441436e91ec28b9a9c8681e90d`

## Correctness and hardening gates

Passed locally:

- deterministic pack regeneration including detector pack;
- 1008 Python/reference tokenizer differentials;
- Unicode 17: 17 crafted + 20,000 property-heavy grapheme cases;
- exact malformed UTF-8 preservation;
- 12 malformed vocabulary, 9 Unicode, 10 language, and 14 detector pack classes rejected;
- stable C ABI: 1004 arbitrary-byte round trips;
- 200 random stream/full equivalence cases;
- 250 edit/full equivalence sequences;
- language-pack order independence and duplicate rejection;
- detector en/hi/ja routing, ambiguity fallback, unavailable-pack fallback, and attachment-order fingerprint independence;
- GCC ASan + UBSan clients;
- GCC and Clang strict-warning builds;
- Clang shared-library ABI differentials;
- GCC `-fanalyzer` with no diagnostics;
- fresh CMake build and CTest 6/6;
- extracted release archive CLI/fingerprint/checksum/auto-routing tests;
- external C client compiled against the extracted static library.

## Performance gates

Base 10 MiB mixed-byte round-trip after one warm-up, median of three measured runs:

- median elapsed: `0.170 s`;
- throughput: `58.8 MiB/s`;
- peak observed RSS: `51.6 MiB`;
- CLI size: `61,968 bytes`;
- shared library size: `48,440 bytes`;
- static library size: `42,806 bytes`.

Controlled language-specialization fixture:

- base tokens: `703,329`;
- specialized tokens: `511,517`;
- reduction: `27.3%`;
- latest warmed median specialization overhead: `3.3%`.

Document-level detector routing versus explicitly selected specialization on 2 MiB per-language fixtures:

- English overhead: `8.5%`;
- Hindi overhead: `16.5%`;
- Japanese overhead: `12.0%`;
- worst measured median overhead: `16.5%`.

These language/detector numbers are conformance-mechanism measurements, not production multilingual quality claims.

## Environment limitation

Rust qualification and remote GitHub Actions cannot run in the local sandbox because `rustc`, `cargo`, GitHub CLI, outbound DNS, and the GitHub connector are unavailable here. The stable native release is qualified with independent GCC/Clang implementations and CI is retained for connected environments.
