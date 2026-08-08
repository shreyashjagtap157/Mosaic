# Implementation Status

Date: 2026-08-08

## Current release candidate

**0.22.0 — enterprise cold TokenDocument persistence.**

Completed through this point:

- exact arbitrary-byte tokenizer and deterministic integer segmentation;
- Unicode 17 grapheme/security plus mapped normalization;
- deterministic model/language/detector/lexer packs and authoring;
- BPE/tiktoken compatibility;
- bounded streaming, incremental and checkpoint-resynchronized editing;
- immutable TokenDocument with model, grapheme, security, normalization, lexical and semantic projections;
- sub-byte semantic views;
- KiB/MiB block planning and packed model projection;
- bounded concurrent in-memory cache and authenticated external-cache record protocol;
- immutable runtime policy, deployment identity and lock-free metrics;
- optional Ed25519 pack trust and content-addressed SQLite registry/control plane;
- canonical cold TokenDocument serialization with bounded hostile-input validation.

## Qualification

The native C implementation is continuously qualified with GCC and Clang, ASan/UBSan, malformed/adversarial corpora, independent Python oracles, static analysis, clean-extraction package consumers, and deterministic fixture regeneration. The intended Rust implementation remains blocked in this environment because no Rust toolchain is installed and outbound network access is unavailable. This does not block the qualified native release.

## Next enterprise layers

1. reusable deterministic parallel batch executor;
2. structured observability/error-detail hooks;
3. thin supported language bindings;
4. SIMD/performance hardening under reference differential;
5. long-duration reliability/chaos qualification;
6. cross-platform release matrix and supply-chain metadata;
7. 1.0 RC ABI/schema/compatibility freeze.
