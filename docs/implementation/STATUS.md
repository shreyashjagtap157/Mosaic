# Implementation Status

Date: 2026-08-08

## Stable enterprise release

**1.0.1 — universal tokenization and token-native processing platform.**

The qualified production implementation is the native C runtime. The supported release bundle contains the CLI, static/shared C libraries, optional Ed25519 trust library, public headers, deterministic authoring and registry tools, reference packs, Python wheel, SBOM, provenance and checksum inventory.

## Implemented platform surface

- exact arbitrary-byte encode/decode and source spans with mandatory byte fallback;
- deterministic checked integer Viterbi and BPE/tiktoken compatibility;
- generated Unicode grapheme/security/normalization views with exact source mapping;
- external language, detector, security, normalization and lexer packs;
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
- frozen 1.x C/trust ABI and binary-format contracts.

## Local qualification completed

- strict GCC builds and full ASan/UBSan inherited native suite;
- independent Clang CMake/CTest suite;
- Clang static analysis of core/cache/executor/trust modules;
- arbitrary-byte, streaming/full, incremental/full, resync, reference/optimized and Unicode differential tests;
- hostile/malformed pack, TokenDocument, cache-record and trust corpora;
- deterministic 250,000-iteration reliability soak repeated with identical replay digest;
- deterministic release archives and Python wheels;
- clean-extraction external C consumer and control-plane package tests;
- ABI/export and stable-format contract checks;
- clean Git provenance and SPDX/in-toto release metadata.

## External qualification gates

The current Linux qualification environment cannot execute these gates and does not fabricate them:

- Windows and macOS native CI execution;
- stable Rust workspace build/clippy/tests;
- Miri and cargo-fuzz jobs;
- ThreadSanitizer or platform-specific race detectors where supported;
- non-x86-64/ARM64 hardware qualification beyond available CI runners.

These gates remain declared in CI and should be executed when the repository is run on GitHub/your future Windows environment. A failure is a 1.0.x defect to fix without changing frozen canonical semantics unless a major-version process is explicitly invoked.

## Post-1.0 research, not 1.0 blockers

- native adaptive byte-patch LLM architecture;
- GPU tokenizer/offline corpus acceleration;
- span-level learned language identification;
- very large community language/profile catalogs;
- optional scanner VM if real compiler profiles prove declarative lexing insufficient;
- additional managed-language bindings and distributed registry adapters.
