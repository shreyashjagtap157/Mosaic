# Mosaic 0.1.3.6

Mosaic 0.1.3.6 is a compatibility-preserving candidate patch that continues the desktop app's file/folder selection and archive-workflow cleanup, and now keeps the suggested destination archive path aligned with the selected source until the user manually overrides it.

## Fixed

- source selection now keeps the source type in sync with the actual selected path;
- the destination archive field auto-fills when a source is chosen and the destination is still blank;
- the auto-generated destination now stays in sync with later source changes until the user manually picks a custom output path;
- the archive inspector/browser and test actions remain integrated into the desktop workflow;
- the Windows package continues to prune older installer executables so only the latest build remains in `dist/windows`.

## Compatibility

- Product release: 0.1.3.6.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.
