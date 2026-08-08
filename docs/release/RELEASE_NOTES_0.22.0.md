# Mosaic Tokenizer 0.22.0

Mosaic 0.22.0 adds canonical cold serialization for immutable TokenDocument snapshots.

## Added

- Endian-defined `MSTIRD01` TokenDocument record with explicit column directory.
- Whole-record SHA-256 plus independent exact-source SHA-256.
- Persistence for model, grapheme, Unicode-security, mapped-normalization, lexical, and semantic projections.
- Internal lexical dependency storage for semantic-only documents without exposing an unrequested lexical projection.
- `mosaic_token_ir_limits` and bounded deserialization API for record/source/projection item ceilings.
- Public serialization capability bit.
- Native, sanitizer, Clang, static-analysis, authenticated-malformed-record, and clean-package conformance.

## Compatibility

The C API remains additive. Canonical tokenizer semantics remain version 2; cold serialization does not change tokenization output. Format version 1 is explicitly versioned independently of the C ABI.
