# Mosaic 0.1.3.4

Mosaic 0.1.3.4 is a compatibility-preserving candidate patch that gives the desktop archive workflow real mode behavior instead of a cosmetic dropdown.

## Fixed

- add-and-skip now skips writing when the destination archive already exists;
- update-newer now compares source and archive timestamps and skips when the archive is already newer;
- add-and-replace continues to overwrite the current destination archive;
- the desktop archive mode control now changes the archive flow instead of doing nothing.

## Compatibility

- Product release: 0.1.3.4.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.

