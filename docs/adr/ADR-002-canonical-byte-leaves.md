# ADR-002: Canonical Leaves Are Bytes

Status: Accepted for implementation  
Date: 2026-08-07

## Decision

For source length `N`, canonical leaf `i` is conceptually the byte range `[i, i+1)` for every `0 <= i < N`.

The canonical partition is logical and need not be materialized. Implementations should iterate or compute leaves from offsets. Downstream consumers reference `ByteRange { start, length }`, not leaf indexes.

Graphemes, Unicode scalars, words, morphemes, lexemes, model tokens, bit fields, and nibble fields are derived views and cannot alter this partition.

## Why

This is the simplest pack-independent, version-independent, binary-safe partition possible. UTF-8 scalar leaves were rejected because they would make text decoding part of Mosaic's fundamental source identity.
