# ADR-006: Dependency Requirements and Resolved Lock Graph

Status: Accepted; exact runtime lock representation implemented in M2 baseline  
Date: 2026-08-07

## Decision

Authoring requirements MAY contain bounded semantic-version ranges. Canonical execution MUST use a fully resolved dependency closure with every executed pack pinned by exact content hash.

Interactive commands may accept conveniences such as `latest`, but persistence/build/execution resolves them immediately to exact identities. There is deliberately no binary lock-entry encoding for `latest`, `*`, or an unresolved range.

## Deterministic authoring-time resolution

A future registry resolver follows this order:

1. filter by exact logical identity;
2. filter by explicit trust/publisher policy;
3. filter by pack-format/runtime compatibility;
4. filter by declared semantic-version constraint;
5. order remaining versions deterministically;
6. resolve dependencies recursively;
7. reject cycles;
8. reject same-publisher/name/version ambiguity when content differs;
9. emit exact hash-pinned identities and a canonical lock artifact.

No filesystem iteration order, network response order, locale sort, or mutable registry tag is allowed to decide canonical identity.

## M2 runtime representation

The pack v1 lock section contains resolved direct dependencies. Each entry carries publisher/name, semantic version, pack-format version, and a nonzero exact hash. Pack-local direct dependencies compose recursively into the full execution dependency closure. The top-level `TokenizerManifest` separately hashes the exact pack set used by an execution configuration.

## Complexity bound

Duplicate logical identities are checked allocation-free in O(n²) time with a hard dependency-count limit. This is intentional for the bootstrap: predictable bounded work is preferable to introducing a heap/hash-map dependency inside the no-std pack validator. A future optimized verifier may change the algorithm only under MOS-REQ-023-equivalent validation semantics.
