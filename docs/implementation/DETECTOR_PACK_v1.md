# Detector Pack v1

Detector packs are declarative Mosaic packs with one outer section of kind `6`. The section begins with `MSDT`, version 1.

## Purpose

A detector pack provides document-level language evidence. It does not tokenize, normalize, add vocabulary entries, or make any source representable. Its output is advisory routing metadata.

## Section layout

The 48-byte little-endian detector header contains:

- magic `MSDT`;
- format version and zero flags;
- profile count and feature count;
- fixed profile/feature record widths, currently 16 bytes each;
- offsets of profile table, feature table, 257-entry first-byte index, and blob;
- blob length;
- maximum feature byte length;
- minimum winning-score margin.

Each profile record stores a tag location/length and non-negative minimum score. Profile tags are canonical ASCII BCP47-style strings and are sorted lexicographically.

Each feature record stores a non-empty byte-surface location/length, profile index, and strictly positive signed-32-bit weight. Features are sorted by first byte, byte surface, then profile index. The first-byte table bounds candidate features for every input byte.

## Detection semantics

For each input byte position, candidate features in the matching first-byte bucket are compared. Every matching feature adds its positive weight to its profile's checked signed-64-bit score. Overlapping matches are permitted and deterministic.

A profile is reported only when:

1. its score meets its profile minimum; and
2. best score minus second-best score meets the pack minimum margin.

Ties therefore fail soft. A failed confidence gate yields `matched = 0` and does not select a language specialization.

## Resource limits

The v1 native runtime rejects detector packs with more than 256 profiles or 65,536 features, tags over 63 bytes, negative minimum scores/margins, non-positive feature weights, malformed indexes, noncanonical ordering, nonzero reserved fields, or out-of-bounds data.

## Reference pack

`fixtures/packs/detector/reference-v1.mpack` has three tiny profiles (`en`, `hi`, `ja`) and seven exact-byte features. It exists to prove pack mechanics, deterministic routing, fail-soft ambiguity, malformed-pack handling, and release integration. It is not evidence of production language-identification accuracy.
