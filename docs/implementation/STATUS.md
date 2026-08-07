# Implementation Status

Date: 2026-08-07

## Stable release state

**Mosaic Tokenizer 0.1.0 native core: STABLE NATIVE RELEASE QUALIFIED LOCALLY.**

The native tokenizer is integrated under `native/` and exposes one high-level tokenizer handle combining the validated static-model and Unicode-17 packs. It builds as a CLI, static library, and shared library.

Local native qualification currently demonstrates:

- exact arbitrary-byte round trip;
- complete 256-byte fallback;
- deterministic static Viterbi segmentation;
- model/native Python differential tests;
- Unicode 17 grapheme conformance against a pinned Unicode-17 oracle;
- malformed UTF-8 byte preservation;
- ASan + UBSan stress;
- deterministic malformed-pack rejection;
- C static/shared and C++ consumer tests;
- streaming/full equivalence;
- edit/full equivalence;
- GCC and Clang builds;
- GCC `-fanalyzer` clean run;
- deterministic release packaging;
- 10 MiB mixed-language benchmark within release floor.

## Milestones

### M0 Engineering substrate

Status: **implemented**.

Repository, documentation, ADRs, CI topology, benchmark contracts, deterministic fixtures, fuzz skeletons, and qualification tooling exist.

### M1 Exact source substrate

Status: **implemented semantically; Rust qualification pending CI**.

Byte coordinates, implicit one-byte leaves, exact source reads, minimal IR, source identity/versioning, and reference/engine byte projections exist in Rust source. Native 0.1 behavior additionally proves exact arbitrary-byte API semantics.

### M2 Deterministic pack executor

Status: **implemented and independently exercised natively; Rust qualification pending CI**.

Checked pack container, exact pack identity, manifest/lock semantics, deterministic costs, path-order vectors, DFA fixtures, and adversarial packs exist.

### M3 Unicode + static tokenizer

Status: **native stable core implemented; Rust source implemented substantially; benchmark-compatibility gate still incomplete**.

Implemented:

- deterministic 271-entry model fixture with all 256 byte fallback entries;
- Viterbi static tokenizer;
- Unicode 17 deterministic pack;
- grapheme semantics including GB9c, emoji ZWJ, regional-indicator behavior, and malformed-byte preservation;
- integrated C ABI/CLI;
- stable release packaging.

Still required before claiming the complete M3 research milestone:

- stable-Rust compile/Clippy/Miri/fuzz qualification on CI;
- semantics-equivalent performance comparison against strong production tokenizer baselines;
- production-scale vocabulary training rather than the conformance/reference vocabulary.

### M4 Wedge Tournament

Status: **not run**.

It remains intentionally blocked on a sufficiently representative M3 benchmark substrate. The project will not choose multilingual, compiler+LLM, or assurance investment by architectural preference alone.

## Stable native 0.1 limitations

- streaming buffers until EOF;
- document edits currently cause full retokenization;
- language/script/locale/domain packs are not yet composable runtime inputs;
- no scanner VM or compiler profiles;
- no rich universal Token IR runtime;
- no SIMD model matching;
- native release currently qualified on Linux x86-64 locally; CMake/CI declares cross-platform qualification jobs.

These limitations are explicit rather than hidden beneath the word “stable.” Stable means the declared 0.1 API/semantics are qualified, not that the entire research roadmap is finished.
