# Mosaic Tokenizer 0.3.0

Mosaic Tokenizer 0.3.0 adds deterministic external detector packs and document-level automatic language routing to the stable 0.2 tokenizer.

## Added

- declarative `MSDT` detector-pack format;
- first-byte-indexed detector execution;
- one detector pack attached to the integrated tokenizer;
- fail-soft `mosaic_detection` results with score and margin;
- `mosaic_tokenizer_encode_auto` and token-span equivalent;
- standalone detector API;
- detector-aware tokenizer fingerprinting;
- CLI `detect`, `fingerprint-auto`, `analyze-auto`, and `roundtrip-auto`;
- 14 malformed detector-pack regression classes;
- ASan/UBSan detector client and CMake/CTest coverage;
- explicit-vs-auto routing benchmark.

## Routing guarantees

Detection never affects source representability. If confidence is insufficient or the detected language pack is unavailable, auto mode uses the base model. The detector cannot introduce token IDs or mutate source bytes.

## Reference-only data warning

The bundled English/Hindi/Japanese detector profiles are tiny conformance fixtures. They demonstrate the architecture and are not production language-identification models.
