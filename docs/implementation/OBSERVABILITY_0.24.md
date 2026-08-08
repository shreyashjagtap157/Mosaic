# Mosaic Observability Contract v1

Release: 0.24.0

Mosaic exposes a synchronous, privacy-preserving observer on the high-level `mosaic_tokenizer` facade. The observer is operational metadata only and does not change semantic fingerprint or runtime deployment identity.

## Configuration

Configure `mosaic_observer_config` before sealing the tokenizer. Configuration becomes immutable after `mosaic_tokenizer_seal()` and is copied into tokenizer snapshots used by child stream/document objects.

Event classes are independently selectable: success, failure, and resource rejection. A non-zero event mask requires a callback.

## Event contents

`mosaic_event` contains only:

- monotonically increasing per-tokenizer event sequence;
- operation and event kind;
- `mosaic_status`;
- input/output unit counts;
- applicable resource ceiling for a rejected request;
- exact tokenizer semantic fingerprint.

It never contains source bytes, token surfaces, normalized text, identifiers, language contents, file paths, or application payloads.

Operations covered by the v1 high-level contract are encode, decode, detection, grapheme segmentation, Unicode security, normalization, lexing, and TokenDocument construction.

## Concurrency and callback rules

Callbacks are synchronous and may run concurrently when a sealed tokenizer is shared through the parallel executor. Callback implementations therefore MUST be thread-safe and SHOULD enqueue/export telemetry rather than block the tokenizer worker.

Mosaic suppresses recursive observer delivery when a callback re-enters the same tokenizer on the same thread. The nested operation still executes and runtime metrics still account for it. Callbacks SHOULD avoid re-entering the same tokenizer because doing useful work in the callback extends request latency.

The callback context must remain valid for the lifetime of the tokenizer and all tokenizer snapshots that inherited the observer configuration.

## Identity

Observer configuration is intentionally excluded from both `mosaic_tokenizer_fingerprint()` and `mosaic_tokenizer_runtime_identity()`. Turning telemetry on or changing its destination does not change canonical tokenization or cache identity.
