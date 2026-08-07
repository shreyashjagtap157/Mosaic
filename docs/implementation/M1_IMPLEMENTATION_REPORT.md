# M1 Exact Source Substrate Implementation Report

Date: 2026-08-07  
Status: source implementation complete; Rust qualification pending

## Implemented

- `ByteOffset`, `ByteLen`, and checked `ByteRange` using 64-bit byte coordinates.
- Borrowed immutable byte source and optional alloc-backed owned source.
- Logical `SourceIdentity` / `SourceVersion` wrapper separated from raw byte access.
- Conceptual one-byte `CanonicalLeaf` with an implicit iterator rather than mandatory per-byte allocation.
- Exact byte reads and explicit range errors.
- Minimal Core IR identifiers, source mappings, projected tokens, deterministic cost/key types.
- Independent reference and engine byte projections.
- All-256-byte committed golden fixture and generated-buffer exactness tests.

## Normative properties represented in code

- Canonical leaf `i` covers `[i, i+1)`.
- Source identity/version metadata cannot change byte semantics.
- No Unicode or pack behavior participates in canonical byte partitioning.
- Optimized byte projection has a reference oracle for MOS-REQ-023 differential qualification.

## Unresolved qualification

The artifact environment has no Rust toolchain. `cargo fmt`, `cargo clippy`, `cargo test`, no-std compilation, and Miri have not run. M1 therefore cannot be marked qualified even though its source implementation is present.
