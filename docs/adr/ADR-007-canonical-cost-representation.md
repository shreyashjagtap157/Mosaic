# ADR-007: Canonical Cost Representation

Status: Accepted and represented in M2 source; qualification pending  
Date: 2026-08-07

## Decision

- canonical edge cost: signed `i32`;
- dynamic-programming/path accumulator: checked signed `i64`;
- floating-point arithmetic is forbidden in canonical runtime path comparison;
- overflow is an explicit error, never wrapping or saturation;
- training and quantization algorithms remain versioned build semantics;
- final canonical path tie ordering is specified by MOS-REQ-022.

## Performance note

M3 benchmarks checked arithmetic. A verifier-proven optimized path may remove redundant checks only if it remains MOS-REQ-023 differential-equivalent to the checked reference implementation and the safety proof is reviewable.
