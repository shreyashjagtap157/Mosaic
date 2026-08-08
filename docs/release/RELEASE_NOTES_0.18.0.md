# Mosaic Tokenizer 0.18.0

Authenticated cache-backend integration release.

- Adds the `MSCACHR1` corruption-evident cache-record format.
- Adds a backend-neutral get/put/remove callback contract suitable for databases, distributed KV stores, and object storage.
- Verifies key binding, payload SHA-256, whole-record SHA-256, exact record length, flags, version, and reserved bytes before returning cached values.
- Adds `MOSAIC_ERROR_INTEGRITY` and cache-backend capability discovery.
- Adds hostile/corrupt backend conformance tests and ASan/UBSan coverage.

Canonical tokenizer output remains independent of caching.
