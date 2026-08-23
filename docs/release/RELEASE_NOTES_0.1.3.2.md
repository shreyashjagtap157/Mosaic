# Mosaic 0.1.3.2

Mosaic 0.1.3.2 is a compatibility-preserving candidate patch that fixes the Windows desktop archive picker flow and continues the modern UI/UX cleanup.

## Fixed

- source file browsing now returns the exact file or folder selected in the picker instead of leaving the source field empty;
- source browsing no longer forces the file filter to `.mzc` only;
- the output archive picker now populates the destination field correctly;
- the app can now proceed into compression/extraction once valid paths are selected.

## Compatibility

- Product release: 0.1.3.2.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.

