# Mosaic 0.1.3.0

Mosaic 0.1.3.0 is a backward-compatible candidate minor release that adds deterministic mixed-language span routing to the native runtime and Python binding.

## Added

- `mosaic_span_route` with byte offsets and per-span `mosaic_detection`;
- `MOSAIC_CAP_SPAN_ROUTING`;
- `mosaic_tokenizer_detect_spans`;
- `mosaic_tokenizer_encode_span_auto`;
- native `mosaic-tokenizer analyze-span-auto` span-routing analysis output;
- Python `SpanRoute`, `Tokenizer.detect_spans()`, and `Tokenizer.encode_span_auto()`;
- native and Python regression coverage for gapless en/hi/ja mixed-language routing and exact decode preservation.

## Compatibility

C ABI advances additively from 1.0.0 to 1.1.0. Existing C ABI 1.0.0 declarations remain present, trust ABI 1.0.0 remains unchanged, tokenizer semantics remain version 2, and native pack/serialization formats remain unchanged.
