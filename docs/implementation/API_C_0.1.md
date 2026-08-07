# Mosaic C API 0.1

Use `native/include/mosaic.h`.

The easiest entrypoint is the integrated opaque `mosaic_tokenizer` handle:

```c
mosaic_tokenizer *tokenizer = NULL;
mosaic_status s = mosaic_tokenizer_load_files(
    "model-v1.mpack",
    "unicode17-v1.mpack",
    &tokenizer
);
```

The tokenizer copies and validates both packs, so callers may release source pack buffers after `*_load_memory` returns.

Returned ID, byte, token, and range arrays are allocated by Mosaic and must be released with `mosaic_free()`.

Core operations:

- `mosaic_tokenizer_encode`
- `mosaic_tokenizer_encode_tokens`
- `mosaic_tokenizer_decode`
- `mosaic_tokenizer_grapheme_ranges`
- `mosaic_tokenizer_stream_create`
- `mosaic_tokenizer_document_create`
- `mosaic_tokenizer_fingerprint`

Low-level `mosaic_model` and `mosaic_unicode` handles remain available for callers that deliberately need only one projection class.

### Ownership

- Opaque handles are owned by the caller after successful creation.
- `mosaic_tokenizer_free`, `mosaic_model_free`, `mosaic_unicode_free`, `mosaic_stream_free`, and `mosaic_document_free` release handles.
- Arrays returned by encode/decode/range APIs are released with `mosaic_free`.
- Input byte arrays and paths are borrowed only for the duration of the call unless documentation explicitly states that Mosaic copies them.

### Streaming

0.1 streaming buffers all pushed input until `mosaic_stream_finish`. This deliberately establishes exact stream/full semantics before bounded-memory optimization. Pushing after finish returns `MOSAIC_ERROR_INVALID_ARGUMENT` until reset.

### Editable documents

0.1 document edits preserve exact bytes and use full retokenization on encode. Local invalidation and block resynchronization are later optimizations constrained by the existing full-equivalence contract.
