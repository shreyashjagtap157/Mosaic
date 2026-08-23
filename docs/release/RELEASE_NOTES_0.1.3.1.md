# Mosaic 0.1.3.1

Mosaic 0.1.3.1 is a compatibility-preserving candidate patch that corrects the Windows desktop installer launch paths and keeps the release artifact hygiene aligned with the current package version.

## Fixed

- Windows installer shortcuts and post-install launch action now target the staged `bin\` layout instead of a missing app-root executable path;
- the Windows packaging script now prunes older `MosaicCompressorSetup-*.exe` files before producing the next test installer, so only the latest build remains in `dist/windows`.

## Compatibility

- Product release: 0.1.3.1.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.

