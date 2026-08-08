# Mosaic 1.0.1

Mosaic 1.0.1 is a compatibility-preserving cross-platform source-integrity patch for the 1.x enterprise line.

## Fixed

- Repository Python tooling now opens repository text explicitly as UTF-8 rather than relying on the operating-system locale. This fixes Windows execution under non-UTF-8 locale defaults.
- Source checksum generation now canonicalizes CRLF text checkouts to LF for identity purposes and excludes ignored/generated working-tree artifacts.
- `.gitattributes` now enforces canonical LF for source text and marks binary pack/signature/key fixtures as binary.

## Compatibility

- Native C ABI: 1.0.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- Stable binary formats: unchanged.

No tokenization, Unicode, pack, TokenDocument, cache, registry, or serialization semantics changed.
