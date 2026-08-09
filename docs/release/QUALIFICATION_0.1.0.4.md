# Mosaic 0.1.0.4 Qualification Evidence

Date: 2026-08-08
Scope: Windows dependency-minimal native qualification plus independent Linux mirror qualification

## Result

**PASS** for the declared 0.1.0.4 C/C++ portability patch on the exercised native profiles.

## Windows operator qualification

Host/toolchain evidence supplied from the project working tree:

- target: `x86_64-pc-windows-msvc`;
- Clang/Clang++: 19.1.5;
- CMake: 4.4.0;
- Ninja: 1.13.2;
- preset: `core-release`;
- `CMAKE_DISABLE_FIND_PACKAGE_ICU=ON`;
- `MOSAIC_BUILD_TRUST=OFF`;
- configure: PASS;
- build: PASS;
- CTest: **22/22 PASS**;
- `git diff --check`: PASS before the portability commit.

The two tests present in a dependency-enabled 24-test profile are intentionally absent here:

1. `normalization` — registered only when ICU is found; the core preset disables ICU discovery.
2. `trust` — registered only when target `mosaic_trust_static` exists; the core preset sets `MOSAIC_BUILD_TRUST=OFF`.

## Independent Linux mirror qualification

Reconstructed from the canonical `v0.1.0.3` bundle/tag at commit `e1d3217`, then patched equivalently.

Toolchain:

- Linux x86-64;
- Clang/Clang++: 17.0.0;
- CMake: 3.31.6;
- Ninja: 1.12.1;
- Python: 3.13.5.

Results:

- `core-release` configure/build: PASS;
- `core-release` CTest: **22/22 PASS**;
- `full-release` configure/build with available ICU/trust dependencies: PASS;
- `full-release` CTest: **24/24 PASS**, including `normalization` and `trust`.
- native Makefile C++ smoke with g++ under `-std=c++11`: PASS, reported `0.1.0.4`;
- native Makefile C++ smoke with clang++ under `-std=c++11`: PASS, reported `0.1.0.4`.

## Compatibility assertions

- C++ smoke consumer is explicitly strict C++11 in both CMake and Makefile build paths.
- Native C ABI remains 1.0.0.
- Optional trust ABI remains 1.0.0.
- Tokenizer semantics remain version 2.
- No MOSPACK or frozen binary-format version is changed by this patch.

## Still-open stable-generation gates

This patch does not promote Mosaic to stability generation `1`. Still-open evidence includes:

- native macOS qualification;
- stable Rust workspace build/rustfmt/Clippy/tests and `no_std` gate;
- Miri;
- cargo-fuzz campaigns;
- ThreadSanitizer/platform race-detector qualification where supported;
- remaining architecture/support-matrix runners required for the eventual stable baseline.

No missing gate is represented as a pass.
