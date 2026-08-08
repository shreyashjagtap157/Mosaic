# Mosaic Authenticated Cache Backend v1

Mosaic 0.18 defines a backend-neutral persistence/distribution boundary. The tokenizer does not embed a database client. Applications adapt RocksDB, Redis, S3-compatible storage, shared filesystems, or proprietary stores through a strict callback table.

## Record envelope

`MSCACHR1` records bind the 32-byte semantic cache key, value length, SHA-256 of the value, and SHA-256 of the entire record with the record-hash field logically zeroed. Reserved fields must be zero. Wrong-key replay, payload modification, metadata modification, truncation, and trailing bytes fail with `MOSAIC_ERROR_INTEGRITY`.

## Backend contract

Reads use a two-call size/fill protocol. Because cache keys are immutable content identities, a backend MUST return a stable record for a key. Writes receive a fully authenticated record and SHOULD implement put-if-absent or idempotent immutable semantics. `mosaic_cache_backend_get_value` verifies the envelope before returning payload bytes.

## Enterprise integration

This deliberately avoids coupling the trusted tokenizer runtime to one storage product. A deployment can use a process-local LRU as L1 and an authenticated backend adapter as L2/L3. Backend availability changes performance only; canonical tokenization never depends on a cache hit.
