# Mosaic C API 0.3

Use `native/include/mosaic.h`. The stable high-level handle remains `mosaic_tokenizer`.

```c
mosaic_tokenizer *tokenizer = NULL;
mosaic_tokenizer_load_files("model-v2.mpack", "unicode17-v1.mpack", &tokenizer);
mosaic_tokenizer_add_language_file(tokenizer, "language/en-v1.mpack");
mosaic_tokenizer_set_detector_file(tokenizer, "detector/reference-v1.mpack");
```

## Exactness contract

The detector is advisory. `mosaic_tokenizer_encode_auto()` first obtains a document-level detection result and uses that language pack only when both `matched` and `available` are true. Low-confidence results, ties, and detected-but-unavailable languages use the unmodified base model. Every path remains exactly decodable because the detector never defines representability and language packs cannot introduce model token IDs.

```c
mosaic_detection detection = {0};
uint32_t *ids = NULL;
size_t count = 0;
mosaic_status s = mosaic_tokenizer_encode_auto(
    tokenizer, input, input_len, &ids, &count, &detection);
```

`mosaic_detection` contains:

- `matched`: detector confidence gate passed;
- `available`: an exact matching language pack is loaded in this tokenizer;
- `score`: winning detector score;
- `margin`: winning score minus second-best score;
- `language[64]`: BCP47-style language tag when matched.

## Detector operations

Standalone detector handles are available for applications that only want routing information:

- `mosaic_detector_load_memory`
- `mosaic_detector_load_file`
- `mosaic_detector_detect`
- `mosaic_detector_free`

Integrated tokenizer operations:

- `mosaic_tokenizer_set_detector_memory`
- `mosaic_tokenizer_set_detector_file`
- `mosaic_tokenizer_detector_loaded`
- `mosaic_tokenizer_detect_language`
- `mosaic_tokenizer_encode_auto`
- `mosaic_tokenizer_encode_tokens_auto`

Version 0.3 accepts one detector pack per tokenizer. Attempting to attach a second detector returns `MOSAIC_ERROR_CONFLICT` rather than silently changing routing policy.

## Existing operations

0.3 retains the 0.2 model/Unicode/language APIs, exact decode, token spans, grapheme spans, streaming baseline, editable-document baseline, static/shared libraries, and opaque-handle ownership model.

Pack byte buffers passed to `*_load_memory`, `*_add_language_memory`, or `*_set_detector_memory` are copied and validated. Returned arrays are released with `mosaic_free()`.

## Fingerprint semantics

The tokenizer SHA-256 fingerprint binds:

1. runtime semantic version;
2. exact model-pack bytes;
3. exact Unicode-pack bytes;
4. canonical order-independent set of exact language-pack hashes;
5. exact detector-pack hash, when attached.

Thus attaching the same detector/language pack set in a different order gives the same fingerprint, while changing the detector changes the fingerprint.

## Current routing scope

0.3 implements document-level routing. The reference detector pack uses exact byte features plus per-language minimum score and a global minimum margin. It is a conformance fixture, not a production-quality language classifier. Span-level mixed-language routing and statistical detector training are later work.

## ABI note

The C API version is 0.3.0. Version 0.x is pre-1.0, but exact-byte behavior, explicit ownership, and fail-soft routing are compatibility commitments.
