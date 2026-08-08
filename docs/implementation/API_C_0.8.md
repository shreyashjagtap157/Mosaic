# Mosaic C API 0.6 — Tokenizer 0.8

Mosaic Tokenizer 0.8 keeps every 0.7 C ABI entry point and adds version-pinned normalization shadow views. The C API version is `0.6.0`; the canonical tokenization semantics version remains `2` because normalization is an optional derived view and does not alter encode/decode when it is absent.

## Ownership

All opaque handles are owned by Mosaic and released with their matching `*_free` function. Buffers returned by Mosaic are released with `mosaic_free()`. A `mosaic_normalized_view` owns its output byte buffer, unit array, and source-span arena and is released with `mosaic_normalized_view_free()`.

## Normalization pack

- `mosaic_normalization_load_memory` / `mosaic_normalization_load_file`
- `mosaic_normalization_unicode_version`
- `mosaic_normalize`
- `mosaic_normalized_view_free`
- `mosaic_normalization_free`

The bundled normalization pack is explicitly Unicode `16.0.0`, generated offline with ICU 76.1. The deployed Mosaic library does not link ICU. Unicode-17 grapheme/security packs remain separately versioned and are not conflated with the normalization data.

Supported mapped views:

- `MOSAIC_NORMALIZE_PRESERVE`
- `MOSAIC_NORMALIZE_NFD`
- `MOSAIC_NORMALIZE_NFC`
- `MOSAIC_NORMALIZE_NFKD`
- `MOSAIC_NORMALIZE_NFKC`
- `MOSAIC_NORMALIZE_NFKC_CASEFOLD`

Normalization never mutates the authoritative source. Each normalized output scalar has one or more exact source byte spans in `mosaic_normalized_view.source_spans`. Decomposition can map several output units to the same source span; composition can map one output unit to several contributing source spans. Canonical reordering preserves each mark's original exact source span. Invalid UTF-8 bytes are opaque normalization barriers and map to themselves byte-for-byte.

## Integrated tokenizer

- `mosaic_tokenizer_set_normalization_memory` / `mosaic_tokenizer_set_normalization_file`
- `mosaic_tokenizer_normalization_loaded`
- `mosaic_tokenizer_normalize`

A loaded normalization pack participates in the tokenizer fingerprint through the typed `NORMALIZATION` domain and exact pack hash. Tokenizers without a normalization pack retain their existing semantics-v2 fingerprints.

## Security API retained

The 0.7 Unicode-17 script/security API remains available unchanged. Security findings and normalization views are independent derived evidence layers and neither one changes model token IDs.
