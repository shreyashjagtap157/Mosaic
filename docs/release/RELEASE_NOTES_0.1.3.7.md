# Mosaic 0.1.3.7

Mosaic 0.1.3.7 is a compatibility-preserving candidate patch that improves the archive subsystem by preserving source timestamps through archive creation and extraction, while keeping the desktop browser aligned with the selected source and destination paths.

## Fixed

- archive entries now carry source last-write timestamps when the format supports them;
- extracted files and folders now restore those timestamps on Windows when the filesystem allows it;
- the archive browser now shows the preserved modified timestamp alongside path, type, size, and ratio;
- the existing desktop source/destination and archive test flows remain intact.

## Compatibility

- Product release: 0.1.3.7.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged except for the archive container version used by the desktop archive subsystem.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.
