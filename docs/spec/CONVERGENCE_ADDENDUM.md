# Mosaic-µ Convergence Addendum

Status: implementation-binding addendum to architecture baseline v0.1  
Date: 2026-08-07

This document records decisions converged after the v0.1 architecture baseline. Where this addendum conflicts with the roadmap or implementation details in the earlier specification, this addendum controls implementation until merged into a future normative specification revision.

## 1. Development principle

Mosaic development is evidence-first. Invariants, reproducibility, exactness, pack safety, and reference semantics freeze before performance machinery or ecosystem breadth. Product investment after the common substrate is selected empirically through the Wedge Tournament rather than by architectural intuition alone.

## 2. Canonical source and leaves

**MOS-REQ-021 [MUST] Canonical leaves are the source bytes themselves.** For a source of `N` bytes, the conceptual canonical partition contains `N` leaves, with leaf `i` covering byte range `[i, i+1)`. Implementations MAY represent this partition implicitly and MUST NOT allocate one object per source byte merely to materialize the invariant. Optional Unicode, language, domain, lexer, model, detector, locale, security, or other packs SHALL NOT change canonical leaf boundaries.

Derived consumers reference byte coordinates using `ByteRange { start, length }`, not canonical-leaf indexes. Unicode scalars, graphemes, words, morphemes, lexemes, model tokens, bit fields, and nibble fields are derived structures over byte ranges.

## 3. Canonical path determinism

**MOS-REQ-022 [MUST] Canonical path selection uses integer costs and a specification-defined total ordering.** Canonical runtime path selection SHALL NOT depend on floating-point arithmetic.

For legal paths, comparison order is:

1. lower total checked integer cost;
2. fewer emitted projection tokens;
3. at the first differing source position, the path whose next token spans more source bytes;
4. smaller canonical namespace ID;
5. smaller stable token ID;
6. lexicographically smaller canonical edge key `(start, end, namespace, kind, token_id, source_pack_hash)`.

Mandatory and forbidden boundaries are legality constraints. Statistical and semantic preferences belong in canonical integer costs rather than a separate post-cost tie-break.

The v1 runtime cost contract is `i32` edge cost plus checked `i64` path accumulation. Cost-training/quantization semantics remain versioned build metadata.

## 4. Reference semantics

**MOS-REQ-023 [MUST] Optimized execution SHALL equal designated reference semantics.** Given identical source bytes, resolved manifest, projection, seed where applicable, and resource policy, SIMD, parallel, cached, incremental, streaming, expanded-table, packed-storage, and other optimized paths must produce canonical output identical to the designated reference implementation.

The reference implementation is intentionally simple, scalar, single-threaded, and clarity-oriented. It is not a performance target.

## 5. Reproducible reference packs

**MOS-REQ-024 [MUST] Canonical reference-pack construction is byte reproducible.** Given identical canonical inputs, build manifest, builder version, dependency lock graph, and declared seed, canonical pack payload bytes SHALL be identical across supported build platforms.

Canonical payloads must not contain uncontrolled timestamps, host paths, unordered map traversal results, architecture-native serialization, or unrecorded randomness.

## 6. Resolved manifests

**MOS-REQ-025 [MUST] Canonical processing requires an exact hash-resolved manifest.** Authoring requirements MAY contain bounded semantic-version ranges. Version ranges, aliases, mutable registry tags, or `latest` SHALL be resolved before canonical execution. Every executed dependency is identified by exact content hash in a resolved lock graph.

`latest` MAY exist as an interactive installation convenience, but it never participates in canonical execution identity.

## 7. First-class TokenizerManifest

Canonical deterministic output is defined by:

```text
CanonicalResult = F(SourceBytes, TokenizerManifest, ProjectionRequest)
```

A `TokenizerManifest` records at minimum:

- manifest schema and runtime-semantics version;
- canonical-leaf version;
- exact resolved pack hashes;
- normalization view;
- routing policy;
- selector/cost/tie-break versions;
- typed-control protocol;
- resource-policy profile;
- dependency-lock hash.

Runtime binary version alone is not a sufficient tokenizer identity.

## 8. Pack requirements versus resolved identity

Authoring dependency constraints and canonical identity are separate concepts.

- `PackRequirement`: human/tool-authored logical dependency with bounded version constraints.
- `ResolvedPackIdentity`: publisher, logical name, semantic version, format version, and exact content hash.
- `ResolvedManifest`: complete acyclic graph of exact `ResolvedPackIdentity` nodes.

Ambiguous same-publisher/name/version content is rejected rather than selected by filesystem or registry ordering.

## 9. Milestones instead of premature SemVer planning

Internal development uses milestones:

- M0 Engineering substrate
- M1 Exact source substrate
- M2 Deterministic pack executor
- M3 Unicode + static tokenizer
- M4 Wedge Tournament
- M5A/M5B/M5C/M5H Chosen product investment
- M6 Incrementality/streaming hardening
- M7 Platform expansion
- M8 Security/performance/storage hardening

External SemVer describes released compatibility and is assigned when externally consumed artifacts warrant it.

## 10. Wedge Tournament

M4 tests three primary candidates in parallel using a frozen M3 substrate:

- multilingual LLM preprocessing;
- compiler + LLM developer tooling;
- high-assurance token-processing SDK.

Each wedge has fatal qualification gates and wedge-class-specific metrics. Passing wedges are compared on a Pareto frontier; a weighted composite is a tie-break only. `NONE` is a valid outcome. Hybrid selection is allowed only under explicit engineering-cost and combined-value gates.

The tournament is time-boxed and produces `WEDGE-EVAL-001` with hypotheses, hashes, baselines, measurements, failures, engineering effort, and selection rationale.

## 11. Pack quality

Pack quality gates are purpose-specific. A blanket token-count improvement is not sufficient for script, detector, security, or linguistic packs. A pack declares a `PackQualityContract` and may include an informative `QualityDeclaration`; registry/community certification is independent evidence and is not replaced by self-declared metrics.
