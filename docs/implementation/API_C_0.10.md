# Mosaic C API 0.10

Mosaic Tokenizer 0.10 advances the backward-compatible C API to `0.8.0`. Canonical tokenization semantics remain version 2.

## New exact incremental document API

- `mosaic_incremental_document_create`
- `mosaic_tokenizer_incremental_document_create`
- `mosaic_incremental_document_apply_edit`
- `mosaic_incremental_document_encode`
- `mosaic_incremental_document_copy_bytes`
- `mosaic_incremental_document_last_reprocessed_bytes`
- `mosaic_incremental_document_last_reused_prefix_bytes`
- `mosaic_incremental_document_free`

The new document caches canonical Viterbi tokens and reuses a proven-safe unchanged prefix. Edit application is transactional. Raw-BPE models return `MOSAIC_ERROR_UNSUPPORTED`.

The existing `mosaic_document` API remains available as the conservative full-retokenization reference implementation.
