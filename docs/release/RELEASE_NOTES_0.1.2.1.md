# Mosaic 0.1.2.1

Mosaic 0.1.2.1 is a compatibility-preserving Rust-substrate correctness patch over the 0.1.2.0 enterprise streaming candidate.

## Fixed

- register and export the existing Unicode 17 pack view from `mosaic-pack`;
- add the Unicode section kind, Unicode range resource ceiling, and Unicode-specific pack validation errors required by `mosaic-unicode`;
- fix incremental SHA-256 updates so a partially filled block is retained across calls instead of being overwritten by the next update;
- add regression coverage for split-update and million-update SHA-256 vectors;
- add Unicode 17 fixture regression coverage for combining-mark/flag grapheme spans and exact invalid-UTF-8 byte preservation.

## Compatibility

C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics version 2, native pack formats, service APIs, registry transport, and 0.1.2.0 application behavior remain unchanged.
