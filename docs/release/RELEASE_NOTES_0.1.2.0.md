# Mosaic 0.1.2.0

Mosaic 0.1.2.0 extends the enterprise deployment surface while preserving the same native tokenizer semantics and ABI.

## Added

- Python `OnlineStream`, a thin wrapper over the native bounded online Viterbi stream (`push`, `finish`, `pending_bytes`).
- `mosaicd` `/v1/encode-batch`, backed by the native bounded `BatchExecutor` with ordered per-item results.
- configurable native executor worker/queue, batch item and decoded-byte ceilings.
- resumable `mosaicd` online-stream sessions with random opaque IDs, native pending-byte bounds, active-session ceilings, idle expiry, finish and explicit cancellation.
- executor and active-stream state in service metrics.

## Compatibility

C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics version 2, existing pack formats and all 0.1.1.0 embedded behavior remain unchanged. These are additive SDK/service capabilities.
