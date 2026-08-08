# Mosaic C API 0.9

Mosaic Tokenizer 0.9 advances the backward-compatible C API to `0.7.0`. Canonical tokenization semantics remain version 2.

## New bounded-processing APIs

### Exact online Viterbi stream

- `mosaic_online_stream_create`
- `mosaic_tokenizer_online_stream_create`
- `mosaic_online_stream_push`
- `mosaic_online_stream_finish`
- `mosaic_online_stream_pending_bytes`
- `mosaic_online_stream_free`

This API emits only proven canonical prefixes before EOF and caps unresolved source bytes. Raw-BPE packs return `MOSAIC_ERROR_UNSUPPORTED`. An exhausted pending-byte budget returns `MOSAIC_ERROR_RESOURCE_LIMIT`, never approximate segmentation.

### Streaming security findings

- `mosaic_security_visit`
- `mosaic_tokenizer_security_visit`

The visitor API produces the same findings and ordering as the array-returning security scan without materializing the findings array. A visitor can stop processing by returning any non-`MOSAIC_OK` status; the API propagates that status and reports the number of findings successfully delivered before the callback stopped.

## Existing ABI

All 0.8 entry points remain present, including exact byte encode/decode, token spans, Unicode graphemes, language specialization, detector routing, buffered streams, editable documents, Unicode security evidence, and mapped normalization shadow views.
