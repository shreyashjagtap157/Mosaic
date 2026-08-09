# M2 Deterministic Pack Executor Plan

Status: implementation in progress  
Date: 2026-08-07

M2 turns the M0 dummy pack into the first checked executable-data container. The runtime remains an executor: build-time tools own construction, ordering, hashing, automata generation, and optimization.

## Binding format choices

- Pack magic: `MOSPACK\0`.
- v1 fixed header: 96 bytes, little endian.
- v1 section entry: 32 bytes.
- Sections are ordered, non-overlapping, aligned, and bounds checked before use.
- `content_hash` is computed with its 32-byte header field zeroed to avoid self-reference.
- Hash algorithm is format-declared. M2 fixture uses SHA-256 because it can be independently verified in the current environment. BLAKE3 remains the intended high-performance default profile later.
- Ordinary pack sections contain data only, never native pointers or native executable code.

## M2 sub-gates

1. M2.1 outer container + deterministic content identity.
2. M2.2 manifest and exact dependency lock graph.
3. M2.3 checked DFA section and scalar reference executor.
4. M2.4 `i32` edge costs, checked `i64` accumulation, total path order vectors.
5. M2.5 malicious fixture corpus and canonical-execution dependency rejection.

The M2 milestone is not complete until all five sub-gates pass under Rust CI. Python fixture validation is supporting evidence, not a substitute for Rust qualification.
