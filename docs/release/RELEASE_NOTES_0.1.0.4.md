# Mosaic 0.1.0.4

Mosaic 0.1.0.4 is a compatibility-preserving candidate patch that closes the Windows C++ smoke-client build defect discovered during real native qualification and hardens product-version synchronization.

## C++ consumer portability

The C++ smoke client now uses the C++11-compatible two-argument `static_assert` form and is built with an explicit strict C++11 contract across both native build topologies:

- CMake: `CXX_STANDARD 11`;
- CMake: `CXX_STANDARD_REQUIRED YES`;
- CMake: `CXX_EXTENSIONS NO`;
- Makefile: default `CXXFLAGS` include `-std=c++11`.

This avoids requiring C++17 merely for the one-argument `static_assert` syntax and prevents compiler-default language modes from weakening the compatibility test. The native Mosaic implementation and frozen C ABI are otherwise unchanged.

## Qualification

- Windows x86-64, Clang 19.1.5, CMake 4.4.0, Ninja 1.13.2, dependency-minimal `core-release`: **22/22 CTest PASS**.
- Linux x86-64, Clang 17.0.0, CMake 3.31.6, Ninja 1.12.1, dependency-minimal `core-release`: **22/22 CTest PASS**.
- Linux x86-64, Clang 17.0.0, `full-release` with available ICU/trust dependencies: **24/24 CTest PASS**.

The Windows 22-test count is intentional. `core-release` sets `CMAKE_DISABLE_FIND_PACKAGE_ICU=ON` and `MOSAIC_BUILD_TRUST=OFF`; therefore `normalization` and `trust` are not registered. The Linux `full-release` run registers and passes both, accounting exactly for 24 tests.

## Release-engineering hardening

`tools/set_version.py` now validates and updates the current product-version references in:

- native CMake metadata and public header;
- Python package/runtime metadata;
- first-party CLI version output;
- C API validation;
- README current-release headings/sentences;
- product versioning policy;
- C ABI implementation documentation;
- implementation status;
- threat-model status.

Historical release notes, legacy-tag mappings, and examples remain immutable rather than being globally rewritten.

## Compatibility

- Product release: 0.1.0.4.
- Native C ABI: 1.0.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.
