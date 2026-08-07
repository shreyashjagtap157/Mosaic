# Implementation Status

Date: 2026-08-07

## Stable release state

**Mosaic Tokenizer 0.3.0 native tokenizer: STABLE NATIVE RELEASE QUALIFIED LOCALLY.**

Version 0.3 adds declarative detector packs and fail-soft automatic document routing to the qualified byte/Unicode/model/language core. A high-level tokenizer owns one model pack, one Unicode pack, zero or more language-specialization packs, and optionally one detector pack.

### Implemented tokenizer path

- exact arbitrary bytes and complete 256-byte fallback;
- deterministic static integer-cost Viterbi;
- exact token byte spans;
- Unicode 17 grapheme view with malformed-byte preservation;
- model pack and Unicode pack validation;
- external language packs loaded from files or memory;
- English/Hindi/Japanese reference language packs;
- mixed-pack composition;
- duplicate-tag rejection;
- order-independent pack-set fingerprint;
- attach-time projection of pack costs onto model vocabulary;
- language-specialized streams and editable-document snapshots;
- deterministic detector packs and document-level automatic routing;
- fallback to base model on ambiguous, low-confidence, or unavailable language results;
- CLI, C ABI, static/shared libraries, CMake and Make builds;
- deterministic release bundle.

## Architectural direction audit

The implementation remains aligned with the converged design. Language packs do not define representability and cannot introduce model IDs. They only specialize deterministic costs over model surfaces. Bytes remain authoritative, Unicode remains a mapped interpretation, and attaching/removing specialization never changes whether source data can be represented exactly.

The 0.2 hot-path language lookup defect remains fixed through attach-time vocabulary projection. The 0.3 detector adds a first-byte index and is benchmarked separately against explicit specialization.

## Milestones

- **M0:** implemented.
- **M1:** semantic/native behavior implemented; Rust qualification pending external CI.
- **M2:** implemented and independently exercised natively; Rust qualification pending external CI.
- **M3:** stable native Unicode/static tokenizer substrate implemented. Production-scale vocabulary training and semantics-equivalent external baseline benchmarking remain research gates.
- **M4 Wedge Tournament:** not run and not silently bypassed.

## Remaining tokenizer work

The next tokenizer-specific capabilities, before broader consumer projections, are:

1. production-scale vocabulary/language/detector training and evaluation;
2. span-level mixed-language routing only if document-level evidence proves insufficient;
3. bounded-memory streaming that remains exactly equal at EOF;
4. local incremental retokenization equivalent to full processing;
5. hot matcher/SIMD work only after profiling;
6. Wedge Tournament once the benchmark substrate is representative enough.

Compiler/search/IDE/security branches remain outside the stable tokenizer surface until evidence selects a product wedge.

## Environment limitation

This host still lacks `rustc`, `cargo`, GitHub CLI, and outbound DNS. Native GCC/Clang builds are locally qualified. Rust and cross-platform jobs are present in repository CI but cannot execute here until the commits reach a connected GitHub environment.
