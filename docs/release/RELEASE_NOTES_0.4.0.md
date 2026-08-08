# Mosaic Tokenizer 0.4.0

Mosaic Tokenizer 0.4.0 makes automatic routing semantics consistent across one-shot encoding, streaming sessions, and editable documents.

## Added

- `mosaic_tokenizer_stream_create_auto`;
- `mosaic_stream_finish_auto` with the exact document-level detection used at EOF;
- `mosaic_tokenizer_document_create_auto`;
- `mosaic_document_encode_auto` with re-detection after edits;
- full tokenizer configuration snapshots for stream/document child objects;
- parent-lifetime independence tests for auto streams/documents;
- edit-driven language re-detection conformance tests.

## Semantic guarantees

Streams and documents created from a high-level `mosaic_tokenizer` own a validated snapshot of its model, Unicode, language, and detector packs. Mutating or freeing the parent cannot change the child object's behavior.

The 0.4 streaming implementation remains an EOF-equivalent reference path and may buffer input. Editable documents may retokenize fully after edits. These choices preserve correctness while bounded-memory streaming and local incremental retokenization remain performance optimizations that must match the same canonical output.

## Compatibility

All 0.3 one-shot, model, Unicode, language-pack, detector, decode, token-span, and grapheme APIs remain available. The C API version is 0.4.0.
