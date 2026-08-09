# Mosaic Tokenizer 0.27.0

Mosaic 0.27.0 is the enterprise portability and software-supply-chain release. Canonical tokenization semantics remain version 2 and the public C API remains backward-compatible.

## Highlights

- Repository-root CMake superbuild with `core-release` and `full-release` presets.
- Dependency-minimal core configuration does not require OpenSSL or ICU.
- Internal thread abstraction uses Windows `_beginthreadex`/Win32 synchronization or POSIX pthreads without exposing platform types in the ABI.
- Cross-platform core CI targets Linux, macOS, and Windows; deep Linux qualification retains sanitizers, ICU differentials, trust, Python, package validation, and supply-chain checks.
- Deterministic SPDX 2.3 SBOM, in-toto/SLSA-shaped provenance, staged SHA-256 inventory, and source checksum-manifest binding.
- Official release builds reject dirty Git trees; explicitly permitted preflight builds record their dirty state.
- SECURITY.md, SUPPORT.md, and release-engineering policy are packaged with the distribution.
- Release version metadata is mechanically synchronized and validated.

## Compatibility

No tokenizer semantic, pack-format, TokenDocument-format, or C ABI break is introduced by this release.

## External qualification remaining

Linux qualification is performed locally. Windows and macOS execution remains a CI gate to be exercised on their native runners.
