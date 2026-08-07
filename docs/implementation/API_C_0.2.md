# Mosaic C API 0.2

Use `native/include/mosaic.h`.

The high-level entrypoint is the opaque `mosaic_tokenizer` handle:

```c
mosaic_tokenizer *tokenizer = NULL;
mosaic_status s = mosaic_tokenizer_load_files(
    "model-v2.mpack",
    "unicode17-v1.mpack",
    &tokenizer
);
```

Optional language packs are then attached explicitly:

```c
mosaic_tokenizer_add_language_file(tokenizer, "language/en-v1.mpack");
mosaic_tokenizer_add_language_file(tokenizer, "language/hi-v1.mpack");
```

The tokenizer copies and validates loaded pack bytes. A language pack changes deterministic segmentation costs over vocabulary surfaces but cannot introduce token IDs outside the model pack. Adding packs recomputes the tokenizer fingerprint; the fingerprint is canonical and independent of pack attachment order.

### Core operations

- `mosaic_tokenizer_encode`
- `mosaic_tokenizer_encode_tokens`
- `mosaic_tokenizer_decode`
- `mosaic_tokenizer_grapheme_ranges`
- `mosaic_tokenizer_stream_create`
- `mosaic_tokenizer_document_create`
- `mosaic_tokenizer_fingerprint`
- `mosaic_tokenizer_add_language_memory`
- `mosaic_tokenizer_add_language_file`
- `mosaic_tokenizer_language_count`
- `mosaic_tokenizer_language_tag`

Low-level `mosaic_model` and `mosaic_unicode` handles remain available when specialization or the combined facade is unnecessary.

### Ownership

- Opaque handles are caller-owned after successful creation.
- Returned arrays are released with `mosaic_free`.
- Pack byte buffers passed to `*_load_memory` or `*_add_language_memory` are copied; callers may release their buffers after return.
- Streams and editable documents created from the high-level tokenizer snapshot the current model specialization. Later changes to the parent tokenizer cannot silently alter an existing child object's semantics.

### Language-pack conflicts

Version 0.2 accepts at most one loaded pack for a given language tag. A duplicate tag returns `MOSAIC_ERROR_CONFLICT`. This intentionally rejects ambiguous stacking until a future pack-resolution policy defines deterministic precedence/merging.

### Streaming and editable documents

0.2 streaming is semantically exact and buffers until EOF. Editable documents preserve exact bytes and currently retokenize the complete source on encode. Those are correctness baselines; bounded streaming and local incremental recomputation must remain bit-equivalent to them.

### ABI note

The C API version is 0.2.0. Version 0.x remains pre-1.0: additions are expected, but ownership and exact-byte semantics are treated as compatibility commitments.
