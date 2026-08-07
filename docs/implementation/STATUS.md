# Implementation Status

Date: 2026-08-07

## Stable release state

**Mosaic Tokenizer 0.2.0 native tokenizer: STABLE NATIVE RELEASE QUALIFIED LOCALLY.**

Version 0.2 adds a real external language-pack subsystem to the previously qualified byte/Unicode/model core. A high-level tokenizer now owns one model pack, one Unicode pack, and zero or more declarative language-specialization packs.

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
- CLI, C ABI, static/shared libraries, CMake and Make builds;
- deterministic release bundle.

## Architectural direction audit

The implementation remains aligned with the converged design. Language packs do not define representability and cannot introduce model IDs. They only specialize deterministic costs over model surfaces. Bytes remain authoritative, Unicode remains a mapped interpretation, and attaching/removing specialization never changes whether source data can be represented exactly.

A performance-direction defect was found and corrected before 0.2: per-candidate/per-pack hot-path lookups were replaced by one-time attach-time vocabulary projection.

## Milestones

- **M0:** implemented.
- **M1:** semantic/native behavior implemented; Rust qualification pending external CI.
- **M2:** implemented and independently exercised natively; Rust qualification pending external CI.
- **M3:** stable native Unicode/static tokenizer substrate implemented. Production-scale vocabulary training and semantics-equivalent external baseline benchmarking remain research gates.
- **M4 Wedge Tournament:** not run and not silently bypassed.

## Remaining tokenizer work

The next tokenizer-specific capabilities, before broader consumer projections, are:

1. automatic language routing/detector packs while keeping correctness independent of detection;
2. scalable production vocabulary/language-pack training and evaluation;
3. bounded-memory streaming that remains exactly equal at EOF;
4. local incremental retokenization equivalent to full processing;
5. hot matcher/SIMD work only after profiling;
6. Wedge Tournament once the benchmark substrate is representative enough.

Compiler/search/IDE/security branches remain outside the stable tokenizer surface until evidence selects a product wedge.

## Environment limitation

This host still lacks `rustc`, `cargo`, GitHub CLI, and outbound DNS. Native GCC/Clang builds are locally qualified. Rust and cross-platform jobs are present in repository CI but cannot execute here until the commits reach a connected GitHub environment.
