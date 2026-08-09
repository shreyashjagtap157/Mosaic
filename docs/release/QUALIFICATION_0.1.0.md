# Mosaic Tokenizer 0.1.0 Qualification Evidence

Date: 2026-08-07  
Scope: native Linux x86-64 stable tokenizer core

## Result

**PASS** for the declared native 0.1.0 release surface.

## Host

- Linux 3e1ce0151015 6.18.35 #1 SMP Mon Jul 27 18:07:50 UTC 2026 x86_64 GNU/Linux
- gcc (Debian 14.2.0-19) 14.2.0
- clang version 17.0.0 (https://github.com/swiftlang/llvm-project.git 10999b6d034fe318f3d56c83bddb6572593a8bb0)
- Python: 3.13.5

## Deterministic artifacts

- model pack SHA-256: `95595a2bd59ad126983544a6e0de03b8118b9a1274b8a46e1f4048ab2c0cd397`
- Unicode 17 pack SHA-256: `23318e7f1fcb2848208d6560d7b3a0e08e6ff042099dea7f5f10b42127c7a27f`
- integrated tokenizer fingerprint: `09733bb749978998571deb05df6587cf42be2514f9662df2d4fb08e8538ef32a`
- Linux release archive SHA-256: `962e6c97aef715ad64cd21d5f52d11fc20d13d4aca9503ae6156884c0a01b961`

## Passed gates

- deterministic empty/M2/model/Unicode pack regeneration;
- 15 malformed M2 pack classes rejected;
- 12 malformed model-pack classes rejected;
- 9 malformed Unicode-pack classes rejected;
- 4 canonical equal-cost path-order vectors;
- exact tokenizer-manifest identity test;
- 1,008 Python reference/indexed model round trips;
- Unicode 17 table counts: 1,386 GCB ranges, 473 InCB ranges, 156 Extended_Pictographic ranges;
- 17 crafted + 20,000 property-heavy Unicode grapheme cases matched the pinned Unicode-17 `regex` oracle;
- 7 malformed UTF-8 cases preserved exact bytes as opaque units;
- GCC build with `-Wall -Wextra -Wpedantic -Werror`;
- Clang build with the same strict warning floor;
- GCC `-fanalyzer` completed without diagnostics under the release source;
- ASan + UBSan long-lived API stress: 2,000 randomized exact round trips, 100 stream/full differentials, 250 edit/full differentials;
- static and shared C client tests;
- C++ header/client test;
- independent C/Python tokenization differential: 264 cases;
- independent C/Python Unicode differential: 521 cases;
- public C ABI `ctypes` test: 1,004 arbitrary-byte cases, 200 randomized stream chunkings, 250 randomized edits;
- clean CMake build and 4/4 CTest tests locally;
- deterministic release packaging reproduced byte-for-byte on repeat packaging;
- packaged release independently extracted and exercised using bundled CLI, packs, checksums, and a freshly compiled external static client.

## Performance regression gate

10 MiB mixed English/Hindi/CJK fixture using the reference model pack:

- observed qualification throughput: **55.6 MiB/s**;
- qualification floor: **20 MiB/s**;
- observed max RSS: **51.6 MiB**;
- RSS ceiling: **128 MiB**;
- CLI binary: approximately **44 KiB** before pack data;
- shared library: approximately **35 KiB** before pack data.

These are host-specific engineering measurements, not general public benchmark claims against other tokenizers.

## Not qualified on this host

- Rust compile/rustfmt/Clippy/test/no_std/Miri/fuzz gates, because `rustc` and `cargo` are absent and outbound DNS is blocked;
- native macOS/Windows execution, which is declared in CMake and GitHub CI but cannot be executed locally here;
- M3 comparison against semantics-equivalent tiktoken/Hugging Face baselines;
- production-scale multilingual vocabulary quality;
- language-pack/plugin composition and post-M4 platform branches.

The stable label therefore applies to the **declared native 0.1 tokenizer core API and semantics**, not the unfinished research/platform roadmap.
