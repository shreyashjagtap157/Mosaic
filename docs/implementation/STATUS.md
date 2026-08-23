# Implementation Status

Date: 2026-08-10

## Current enterprise candidate

**0.1.3.4 — universal tokenization and token-native processing platform candidate.**

Mosaic uses the four-part product version `S.M.N.P` documented in `docs/VERSIONING_POLICY.md`. Stability generation `0` remains deliberate: real Windows qualification exposed portability defects, and `0.1.0.4` closes the C++ smoke-client compatibility defect with an explicit strict-C++11 consumer contract. The dependency-minimal Windows Clang preset and the pinned Rust 1.97.1 workspace gate now pass, but the remaining declared stable-generation gates must complete before the project is permitted to claim `1.0.0.0` stable.

The qualified production implementation is the native C runtime on the platforms for which evidence exists. The supported release bundle contains the CLI, static/shared C libraries, optional Ed25519 trust library, public headers, deterministic authoring and registry tools, reference packs, Python wheel, SBOM, provenance and checksum inventory. The current support boundary is summarized in `docs/implementation/SUPPORT_MATRIX.md`.

## Implemented platform surface

- exact arbitrary-byte encode/decode and source spans with mandatory byte fallback;
- deterministic checked integer Viterbi and BPE/tiktoken compatibility;
- generated Unicode grapheme/security/normalization views with exact source mapping;
- external language, detector, security, normalization and lexer packs;
- deterministic document-level and mixed-language span-level detector routing;
- native CLI span-aware auto analysis for document and span routing;
- deterministic authoring/training baseline and content-addressed pack registry/lockfiles;
- exact online streaming, transactional incrementality and checkpoint suffix resynchronization;
- immutable TokenDocument/Core IR with model, grapheme, security, normalization, lexical and semantic projections;
- identifier/number/string semantic enrichment and generic sub-byte views;
- KiB token-aligned blocks, MiB macroblocks and compact authenticated model projection;
- canonical authenticated cold TokenDocument serialization with resource-bounded hostile-input validation;
- bounded concurrent LRU cache and authenticated external-cache backend protocol;
- immutable runtime resource policies, deployment identity, lock-free metrics and privacy-preserving observability;
- bounded reusable parallel executor with deterministic result order and backpressure;
- optional Ed25519 publisher trust and revocation;
- Python binding over the same native ABI;
- cross-platform root CMake build topology and Windows/POSIX threading abstraction;
- deterministic SBOM/provenance/checksum release artifacts;
- replayable reliability/chaos campaign including registry corruption/repair;
- frozen C/trust ABI and binary-format contracts independent of product-version maturity.

## Qualification completed on available hosts

- Windows x86-64 Clang 19.1.5 dependency-minimal `core-release`: 22/22 CTest PASS; optional ICU normalization and trust tests intentionally excluded by preset (`CMAKE_DISABLE_FIND_PACKAGE_ICU=ON`, `MOSAIC_BUILD_TRUST=OFF`);
- Windows x86-64 Rust 1.97.1: rustfmt check PASS, strict Clippy PASS, workspace tests PASS, `mosaic-core` no-default-features PASS;
- Linux x86-64 Clang 17.0.0 dependency-minimal `core-release`: 22/22 CTest PASS;
- Linux x86-64 Clang 17.0.0 `full-release` with available ICU/trust dependencies: 24/24 CTest PASS;
- strict GCC builds and full ASan/UBSan inherited native suite;
- independent Clang CMake/CTest suite;
- Clang static analysis of core/cache/executor/trust modules;
- arbitrary-byte, streaming/full, incremental/full, resync, reference/optimized and Unicode differential tests;
- hostile/malformed pack, TokenDocument, cache-record and trust corpora;
- deterministic 250,000-iteration reliability soak repeated with identical replay digest;
- deterministic release archives and Python wheels;
- targeted nightly Miri on `mosaic-core` and `mosaic-pack`;
- bounded Windows nightly cargo-fuzz smoke across `fuzz_source_bytes`, `fuzz_pack_header`, `fuzz_pack_v1`, and `fuzz_dfa` with the matching ASan runtime path;
- a faster local readiness path that can defer Miri while still running repo, version, checksum, gate, and fuzz-smoke validation;
- clean-extraction external C consumer and control-plane package tests;
- ABI/export and stable-format contract checks, including Windows PE/COFF export validation for the native DLL;
- clean Git provenance and SPDX/in-toto release metadata.

## Stable-generation qualification gates still open

The following gates are not fabricated as passes and keep the first product-version component at `0` until completed:

- macOS native CI execution;
- full cargo-fuzz jobs;
- ThreadSanitizer or platform-specific race detectors where supported;
- non-x86-64/ARM64 qualification required by the final support matrix.

A failure during this candidate period increments only the fourth product-version component when the fix is compatibility-preserving, e.g. `0.1.0.3` → `0.1.0.4`.

## Research beyond stabilization

- native adaptive byte-patch LLM architecture;
- GPU tokenizer/offline corpus acceleration;
- span-level learned language identification;
- very large community language/profile catalogs;
- optional scanner VM if real compiler profiles prove declarative lexing insufficient;
- additional managed-language bindings and distributed registry adapters.

## Archive subsystem direction

Mosaic's archive-product work is now defined as a single integrated subsystem rather than disconnected desktop features. The current repository state only establishes the product boundary, not the engine itself.

Planned archive capabilities:

- archive creation and extraction;
- add/remove/update archive entries;
- compression profiles from `store` through `ultra`;
- integrity testing and bounded repair;
- SFX or equivalent launchable packaging;
- desktop UI, CLI, bindings, and installer backed by the same core engine.

This direction is documented in `docs/spec/ARCHIVE_PRODUCT_SPEC.md` and `docs/implementation/ARCHIVE_PRODUCT_PLAN.md`.
