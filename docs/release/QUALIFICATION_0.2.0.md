# Mosaic Tokenizer 0.2.0 Qualification Evidence

Date: 2026-08-07  
Scope: native Linux x86-64 stable tokenizer with external language-pack composition

## Result

**PASS** for the declared native 0.2.0 release surface.

## Toolchains

- GCC 14.2.0 strict `-Wall -Wextra -Wpedantic -Werror`
- Clang 17.0.0 with the same warning floor
- GCC `-fanalyzer` completed without diagnostics
- ASan + UBSan long-lived API and malformed-pack clients
- Python 3.13.5 independent/reference oracles
- CMake/CTest: 5/5 native tests on this host

## Exact release identities

- model v2 SHA-256: `5295e581cb87faa4c289fb5590c178cb73b56ec7946938fe8f815ceca2b2dffb`
- Unicode 17 pack SHA-256: `23318e7f1fcb2848208d6560d7b3a0e08e6ff042099dea7f5f10b42127c7a27f`
- English language pack: `75179427e58eeade21cfa70d5ea1af127a497a849f55d6cb6e915a421c104551`
- Hindi language pack: `309fdd7db255d9dd5ba704c095805e7049a08fae0f9bb8655edaab100b142e9e`
- Japanese language pack: `7abcebfa000d09583190edd33bb6833245fa79c2c0977ba2743d83f3e83aa9e3`
- base tokenizer fingerprint: `d491b35c9c512044036b0d1a2093253860e48d7d950ba4591446864fa1b97d0e`
- en+hi+ja canonical language-set fingerprint: `d175040a0b3194db26f60b9f11b53aa532f3b6d9a57c3e1d65443541c3a0941a`
- Linux x86-64 release archive SHA-256: `ea89dd5885de940cb4407844f0abb007685d799e1925aba2c0c7962eb387b0d7`

## Correctness and hardening gates

Passed:

- deterministic empty/M2/model-v1/model-v2/Unicode/language pack generation;
- all 256 fallback-byte IDs remain present and valid;
- 15 malformed M2 packs rejected;
- 12 malformed model packs rejected;
- 9 malformed Unicode packs rejected;
- 10 malformed language packs rejected without mutating live tokenizer state;
- canonical equal-cost path-order vectors;
- exact arbitrary-byte encode/decode;
- Python/native model and Unicode differentials;
- Unicode 17 property-heavy grapheme corpus and malformed UTF-8 preservation;
- static/shared C clients and C++ header/client;
- stream/full equality and edit/full equality;
- language-pack duplicate-tag conflict behavior;
- English/Hindi/Japanese specialization examples;
- three-pack mixed-language composition;
- language-pack attachment-order-independent output and fingerprint;
- streams/documents snapshot the attached specialization set;
- GCC and Clang build/API parity;
- ASan/UBSan language and malformed-pack coverage;
- extracted release CLI, checksums, language fingerprint, and freshly compiled external static consumer.

## Performance regression gates

### Base tokenizer

10 MiB mixed English/Hindi/CJK source, warmed before measurement:

- measured times: 0.17 s, 0.16 s, 0.17 s;
- median: **0.17 s**;
- median throughput: **58.8 MiB/s**;
- maximum observed RSS: approximately **51.6 MiB**;
- release floor: 20 MiB/s;
- RSS ceiling: 128 MiB.

The CLI is approximately 53 KiB, shared library approximately 43 KiB, and static library approximately 36 KiB before pack data on this host.

### Language specialization

Controlled 5 MiB reference fixture exercising English/Hindi/Japanese alternatives:

- base token count: 703,329;
- specialized token count: 511,517;
- controlled token-count reduction: **27.3%**;
- warmed median encoder timings across repeated runs were generally at or near base-path parity after attach-time projection.

The 27.3% value is a **conformance/mechanism benchmark on a deliberately constructed vocabulary and corpus**. It is not evidence of a 27% production multilingual improvement.

## Performance defect found and fixed before release

The first implementation searched every attached language pack for every vocabulary candidate. On a 10 MiB mixed fixture it changed the intended segmentation but took about 0.80 s versus 0.21 s without packs.

That implementation was rejected. Version 0.2 projects every language pack onto the model vocabulary once when attached. The hot path subsequently performs one indexed integer adjustment per candidate and no per-pack lookup.

## Not claimed by 0.2

- production multilingual quality;
- automatic language detection/routing;
- bounded-memory streaming;
- local incremental retokenization;
- compiler/search/IDE/security consumer projections;
- a production-scale trained vocabulary;
- semantics-equivalent tiktoken/Hugging Face benchmark;
- qualified Rust production build on this host;
- local Windows/macOS execution (CI is configured for those targets once pushed).
