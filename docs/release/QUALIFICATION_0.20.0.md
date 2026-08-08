# Mosaic Tokenizer 0.20.0 Qualification

- inherited GCC + ASan/UBSan/LeakSanitizer suite: pass;
- Ed25519 trust sanitizer test: pass;
- structurally validated signed pack: pass;
- unknown publisher: rejected;
- modified pack: rejected by identity mismatch;
- modified signature: rejected;
- malformed reserved metadata/record length: rejected;
- publisher revocation: rejected with explicit revoked status;
- trust-store key ceiling: enforced;
- independent Clang CMake/CTest: 20/20 pass;
- Clang trust-module static analyzer: no diagnostics;
- clean release-package trust consumer: pass.

## Release artifact

- archive: `mosaic-tokenizer-0.20.0-linux-x86_64.tar.gz`
- SHA-256: `4257fd51ba0971390462d19c0c7c7fe266ed0924aaf15b527d3ed78d0db55e97`
