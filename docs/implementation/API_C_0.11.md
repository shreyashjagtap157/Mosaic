# Mosaic C API 0.9.0 (Tokenizer 0.11)

The 0.11 tokenizer keeps all prior C API behavior and adds `mosaic_resync_document` as an opaque exact incremental handle for weighted-Viterbi models.

Key additions:

```c
mosaic_resync_document_create(...);
mosaic_tokenizer_resync_document_create(...);
mosaic_resync_document_apply_edit(...);
mosaic_resync_document_encode(...);
mosaic_resync_document_copy_bytes(...);
mosaic_resync_document_last_reprocessed_bytes(...);
mosaic_resync_document_last_reused_prefix_bytes(...);
mosaic_resync_document_last_reused_suffix_bytes(...);
mosaic_resync_document_last_resynchronized(...);
mosaic_resync_document_free(...);
```

`checkpoint_bytes` controls checkpoint spacing and `max_pending_bytes` bounds the online Viterbi unresolved-source state. The constructor rejects raw-BPE packs with `MOSAIC_ERROR_UNSUPPORTED`. All output handles are nulled before failure and all edits are transactional.

Canonical tokenization semantics remain version 2.
