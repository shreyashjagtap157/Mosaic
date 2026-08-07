# ADR-008: Canonical Path Total Ordering

Status: accepted for M2  
Date: 2026-08-07

## Decision

Canonical path selection uses signed `i32` edge costs and checked `i64` accumulated costs. Floating-point arithmetic is forbidden in canonical runtime selection.

After hard legality constraints have been applied, two legal complete paths are ordered by:

1. lower total accumulated cost;
2. fewer emitted tokens;
3. at the first differing edge, longer source-byte span;
4. smaller namespace ID;
5. smaller stable token ID;
6. lexicographically smaller canonical edge key `(start, end, namespace, kind, token_id, source_pack_hash)`.

The ordering is defined over legal paths covering the same source interval. Candidate generation must reject conflicting duplicate edges where the same canonical edge key is assigned different canonical costs; such a lattice is malformed rather than something the tie-breaker is expected to rationalize.

## Verification

`fixtures/conformance/path-order-v1.toml` contains adversarial equal-cost cases. Both `mosaic-reference` and `mosaic-engine` have independent comparator implementations and must agree with the fixture/oracle.
