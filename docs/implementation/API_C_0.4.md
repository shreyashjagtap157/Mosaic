# Mosaic C API 0.4

The primary public handle is `mosaic_tokenizer`, which owns a model pack, Unicode pack, zero or more language packs, and optionally one detector pack.

## Automatic streaming

```c
mosaic_stream *stream = NULL;
mosaic_tokenizer_stream_create_auto(tokenizer, &stream);
mosaic_stream_push(stream, chunk_a, chunk_a_len);
mosaic_stream_push(stream, chunk_b, chunk_b_len);

uint32_t *ids = NULL;
size_t count = 0;
mosaic_detection detection = {0};
mosaic_stream_finish_auto(stream, &ids, &count, &detection);
```

The stream snapshots the complete tokenizer at creation. It remains valid after the parent tokenizer is freed. `mosaic_stream_reset()` permits a new document to be pushed and re-detected.

## Automatic editable documents

```c
mosaic_document *document = NULL;
mosaic_tokenizer_document_create_auto(tokenizer, input, input_len, &document);

mosaic_detection detection = {0};
uint32_t *ids = NULL;
size_t count = 0;
mosaic_document_encode_auto(document, &ids, &count, &detection);
```

After `mosaic_document_apply_edit`, the next `mosaic_document_encode_auto` detects against the exact current bytes and chooses the applicable loaded language pack or the base model.

## Ownership

- tokenizer/model/Unicode/detector/language pack bytes supplied through memory APIs are copied;
- tokenizer-created streams/documents own independent configuration snapshots;
- arrays returned by Mosaic are released with `mosaic_free`;
- opaque handles are released by their corresponding `*_free` operation.

## Current implementation boundary

Streaming/full and editable-document/full equivalence are correctness contracts. In 0.4 the reference stream may buffer until EOF and documents may fully retokenize after edits. Optimized bounded/local implementations are permitted only when they produce identical canonical results.

The C API version is 0.4.0.
