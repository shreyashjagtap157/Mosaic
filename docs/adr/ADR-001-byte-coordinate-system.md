# ADR-001: Byte Coordinate System

Status: Accepted for implementation  
Date: 2026-08-07

## Context

Mosaic must represent arbitrary bytes, valid/invalid UTF-8, binary data, compiler offsets, streams, and memory-mapped files without making Unicode interpretation authoritative.

## Decision

All canonical source coordinates are unsigned byte offsets and byte lengths. Public/core range types use 64-bit coordinates. Derived Unicode, lexical, linguistic, model, bit, and nibble structures map to byte ranges.

## Consequences

- Exact binary coverage is natural.
- UTF-8 and Unicode versions cannot redefine source identity.
- Very large sources remain addressable.
- Human character indexes require derived mapping APIs rather than pretending they are byte indexes.
