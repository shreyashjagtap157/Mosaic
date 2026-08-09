# Mosaic C API 0.10.0 (Tokenizer 0.12)

0.12 keeps prior ABI behavior and adds the opaque `mosaic_token_document` Core IR snapshot.

Key additions:

```c
mosaic_tokenizer_token_document_create(...);
mosaic_tokenizer_token_document_create_auto(...);
mosaic_token_document_get_info(...);
mosaic_token_document_copy_source(...);
mosaic_token_document_model_tokens(...);
mosaic_token_document_graphemes(...);
mosaic_token_document_free(...);
```

Model/grapheme projection construction is controlled by `MOSAIC_TOKEN_DOCUMENT_MODEL` and `MOSAIC_TOKEN_DOCUMENT_GRAPHEMES`. All returned coordinates are original byte offsets. Canonical tokenizer semantics remain version 2.
