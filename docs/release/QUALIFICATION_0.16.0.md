# Mosaic Tokenizer 0.16.0 Qualification

Qualification gates include all inherited 0.15 tests plus:

- 1 MiB token-aligned block/macroblock partition and deterministic identities;
- explicit block-count resource-limit rejection;
- oversized-token signaling without semantic splitting;
- packed model encode/inspect/decode identity;
- ASan/UBSan block and serialization paths;
- forged, rechecksummed malformed packed-record corpus;
- independent GCC and Clang/CTest builds;
- packaged external-client use of block planning and packed serialization.

Measured 1 MiB fixture: 257 blocks, 29 macroblocks; packed projection 616,674 bytes versus 6,962,976 bytes of naive public token structs (8.9%).

Independent Clang/CTest: 16/16 passed. Clang static analyzer: no diagnostics.

Release archive SHA-256: `647ca0547cb9046d6db6a7edb95f09187eb2341f555bf4115202fd3c2fc31c35`.
