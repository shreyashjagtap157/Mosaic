# Mosaic 0.26 Performance Hardening

## Change retained

The validated first-byte vocabulary index already proves every candidate in a bucket shares the input byte at the current source position. The production Viterbi loop therefore skips comparing byte zero. One-byte fallback candidates require no `memcmp`; longer candidates compare only the remaining suffix.

This is semantics-preserving because malformed first-byte indexes are rejected during pack validation.

## Same-host A/B

Sealed v0.25 binary versus the 0.26 candidate, same model pack and deterministic 10 MiB mixed-language fixture, warmed then 11 timed runs:

- v0.25 median: 57.1 MiB/s
- v0.26 candidate median: 59.3 MiB/s
- observed improvement: about 3.9%

These are host-local engineering measurements, not universal public throughput claims.

## Rejected optimization

A load-time expanded vocabulary metadata table was prototyped. It measured about 56.7 MiB/s on the same workload, slower than the compact pack-backed representation. It was fully reverted. Mosaic keeps optimizations only when measurements justify their complexity.

## Correctness contract

MOS-REQ-023 remains binding: optimized output must equal reference output. Existing arbitrary-byte, streaming/full, incremental/full, language-pack, detector, Unicode, TokenDocument, serialization, and binding differential suites remain release gates.
