# Mosaic TokenDocument Serialization v1

Status: stable cold-format candidate for Mosaic 0.22.x  
Magic: `MSTIRD01`  
Format version: 1

## Purpose

This format persists an immutable `mosaic_token_document` without serializing native C structs or host pointers. It is intended for enterprise caches, durable preprocessing artifacts, IPC payloads, and reproducible test fixtures.

Source bytes remain authoritative. Only requested public projections are serialized, except that a semantic projection carries its lexical column as an internal dependency because semantic components reference lexical-token indexes. Deserializing such a semantic-only record does not expose the lexical projection through the public API.

## Canonical record

All integers are little-endian. The record contains a 256-byte header, nine fixed directory entries, canonical zero alignment padding, then the selected column payloads. There are no trailing bytes.

The header binds:

- TokenDocument density flags;
- normalization mode;
- exact total/source lengths;
- tokenizer-semantics version;
- source SHA-256;
- tokenizer semantic fingerprint;
- detector result metadata;
- whole-record SHA-256.

The whole-record SHA-256 is calculated with the 32-byte record-hash field zeroed. It authenticates every header field, directory entry, canonical padding byte, and payload byte against accidental or malicious modification. Source bytes also carry their independent source SHA-256 so a record with a valid whole-record hash cannot silently redefine source identity.

## Column order

1. exact source bytes;
2. model-token records (`id`, reserved, source start, source length);
3. grapheme source ranges;
4. Unicode security findings;
5. normalized bytes;
6. normalized units;
7. normalization source spans;
8. lexical tokens;
9. semantic components.

Directory order and widths are format-defined. Unknown section kinds, non-zero reserved values, noncanonical offsets/padding, malformed source partitions, invalid kinds, dangling semantic references, and authenticated trailing bytes fail closed.

## Resource policy

`mosaic_token_document_deserialize()` uses conservative default limits:

- maximum record: 1 GiB;
- maximum source: 1 GiB;
- maximum structured projection items per column: 1,000,000,000.

Enterprise callers SHOULD use `mosaic_token_document_deserialize_with_limits()` with service-specific ceilings. The limits are checked before projection allocation.

## Determinism

Serializing a valid document, deserializing it, and serializing it again MUST produce byte-identical records. Cold serialization is a representation optimization and MUST NOT alter canonical source, model, Unicode, lexical, semantic, normalization, or security semantics.

## Security boundary

The record hash is an integrity mechanism, not publisher authentication. Untrusted records must still pass structural validation and resource policy. If an artifact requires publisher authenticity, the separate Mosaic trust/signature layer should authenticate the serialized artifact or its enclosing deployment manifest.
