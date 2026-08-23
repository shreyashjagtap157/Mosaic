# Mosaic 0.1.3.3

Mosaic 0.1.3.3 is a compatibility-preserving candidate patch that improves the desktop archive picker semantics and keeps the Windows package line synchronized.

## Fixed

- source selection now returns the exact chosen file or folder path;
- source browsing remains generic rather than biasing toward `.mzc` as the only visible file type;
- output archive selection continues to populate the destination field correctly;
- installer metadata and release evidence are synchronized to the current patch line.

## Compatibility

- Product release: 0.1.3.3.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.

