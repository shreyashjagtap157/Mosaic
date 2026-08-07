# ADR-003: Source Ownership and Versioning

Status: Accepted for M1 interface design  
Date: 2026-08-07

## Decision

`mosaic-core` exposes source access through byte-oriented traits. Initial providers are borrowed immutable contiguous bytes and, behind `alloc`, owned immutable bytes. Ropes, piece tables, streams, and memory maps arrive through additional providers without changing canonical coordinates.

A source participating in persistent/cached processing has a logical identity and version outside the raw byte-access trait. Hashing is layered above the minimal `no_std` core.

## Ownership rules

- Rust borrowed sources use compile-time lifetimes.
- Owned sources own their backing bytes.
- Future C FFI defaults to copying unless the caller explicitly provides an external-source lifetime/release callback contract.
- Source mutation never occurs behind an immutable source handle; edits create/version a document abstraction later.
