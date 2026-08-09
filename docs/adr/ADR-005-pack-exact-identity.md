# ADR-005: Pack Exact Identity

Status: Accepted and implemented for M2 SHA-256 profile; qualification pending  
Date: 2026-08-07

## Context

A human-readable version identifies a release line, but it is not sufficient to prove that two runtimes consumed identical bytes. Mosaic caching, deterministic execution, benchmark evidence, and dependency locking require exact content identity.

## Decision

A resolved pack identity contains:

- publisher;
- logical name;
- semantic version;
- pack-format version;
- exact cryptographic content hash.

The content hash is exact identity. Semantic version coordinates humans and compatibility policy. Mutable names such as `latest` never identify canonical execution.

Pack v1 declares its hash algorithm in the fixed header. The first executable M2 profile uses SHA-256 so the fixture can be independently rebuilt and verified using standard tooling in the current environment. The hash is calculated over the entire file with the 32-byte hash field itself replaced by zero.

BLAKE3 remains a planned high-performance content/cache profile. Changing hash algorithms never weakens the rule that canonical identity includes the algorithm and exact digest.

## Bootstrap-crypto caveat

The M2 Rust source contains a small safe/no-std SHA-256 implementation to avoid making pack parsing dependent on a hosted runtime during the bootstrap. It is not granted production trust merely because the algorithm is familiar. Before a stable security-sensitive release it must be differential-tested against an independently maintained implementation and audited or replaced with a vetted implementation. Known empty, `abc`, split-update, and million-`a` vectors are already written.

## Consequences

Two byte-distinct packs claiming the same publisher/name/version are ambiguous and rejected under canonical resolution rather than selected by filesystem/registry order.
