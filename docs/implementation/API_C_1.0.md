# Mosaic C API 1.1.0

The native C ABI is independently versioned from the Mosaic product release. The current product candidate `0.1.3.5` exposes frozen C ABI `1.1.0`.

The authoritative public surface is `native/include/mosaic.h`; the machine-readable frozen declaration/type/constant/export baseline is `abi/stable-contract-v1.json` and is validated by `tools/abi_contract.py`.

This is an additive ABI update over `1.0.0`: existing declarations remain present, and `mosaic_span_route`, `MOSAIC_CAP_SPAN_ROUTING`, `mosaic_tokenizer_detect_spans`, and `mosaic_tokenizer_encode_span_auto` are added for deterministic mixed-language span routing.

A product patch does not imply a C ABI patch. The C ABI version changes only through the compatibility process described in `docs/COMPATIBILITY_POLICY.md`.

Canonical tokenizer semantics are independently versioned and currently remain version `2`.

## Span Routing

`mosaic_tokenizer_detect_spans()` returns a Mosaic-owned `mosaic_span_route` array. Routes use byte offsets, cover the input exactly, and carry an ordinary `mosaic_detection`; neutral punctuation and whitespace spans are represented as unmatched base-model spans.

`mosaic_tokenizer_encode_span_auto()` uses the same routes to encode each detected span with its exact loaded language pack when available, otherwise with the base model. The returned token IDs remain exactly decodable to the original input bytes.
