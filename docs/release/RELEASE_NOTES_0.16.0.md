# Mosaic Tokenizer 0.16.0

Enterprise multiscale storage release.

- token-aligned adaptive KiB processing blocks and MiB macroblocks;
- tokenizer-bound content identities suitable for cache keys;
- explicit block/macroblock resource ceilings and oversized-token signaling;
- canonical `MSTKPK01` compact model-projection serialization;
- fixed-bit token IDs, ULEB128 token lengths, whole-record SHA-256 integrity;
- forged authenticated malformed-record conformance tests.
