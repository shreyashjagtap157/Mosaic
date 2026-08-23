# Mosaic 0.1.3.5

Mosaic 0.1.3.5 is a compatibility-preserving candidate patch that fixes the desktop app's file and folder selection workflow, restores correct path display, and hardens the archive roundtrip path used by the Windows UI and self-test.

This follow-on patch further smooths the destination workflow: when a source path is chosen and the destination field is still blank, Mosaic now auto-fills the archive path so the user can move straight to compression without an extra manual step.

## Fixed

- file and folder selection now writes the chosen path back into the app fields instead of silently losing it;
- source folder selection no longer behaves like navigation when the user confirms the dialog;
- output archive selection now updates the visible destination field correctly;
- desktop archive roundtrip verification now uses the same proven direct decompression sequence as the standalone self-test;
- the dead alternate decompression wrapper path has been removed from the desktop app.

## Compatibility

- Product release: 0.1.3.5.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.
- Pack-registry package versions remain independently versioned.

Mosaic remains in stability generation `0`; macOS, Rust, Miri/fuzzing, race-detector, and remaining support-matrix qualification are still required before `1.0.0.0`.
