# Packed Model Projection Format v1

Status: normative for Mosaic 0.16.x.

`MSTKPK01` is a canonical cold-storage representation of the model-token projection. It does not replace source bytes or the in-memory hot representation.

## Layout

The 160-byte little-endian header contains format version, token count, exact source byte length, fixed token-ID bit width, byte lengths of the ULEB128 length stream and packed-ID stream, source SHA-256, tokenizer fingerprint, and a canonical content SHA-256. Bytes 152-159 are reserved zero.

The payload contains:

1. one canonical unsigned LEB128 byte length per token;
2. token IDs packed at the minimum fixed bit width needed by that projection, least-significant bit first within the bitstream.

The content SHA-256 is computed over the complete record with bytes 120-151 treated as zero. It therefore binds metadata and payload, not merely payload bytes.

## Validation

Readers MUST reject unknown flags/reserved bytes, unsupported bit widths, count/size arithmetic overflow, non-canonical or zero ULEB lengths, a length sum different from the declared source length, non-zero unused ID padding bits, trailing/truncated data, or a content-hash mismatch.

Decoded token spans are reconstructed by prefix-summing the exact byte lengths. Serialization does not modify token IDs or segmentation.
