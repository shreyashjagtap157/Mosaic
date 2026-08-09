# Mosaic 0.1.1.0

Mosaic 0.1.1.0 is an additive enterprise deployment minor on top of the qualified 0.1.0.5 native runtime line.

## Added

- `mosaicd`: bounded HTTP service over the same native tokenizer runtime, with health/readiness/version, encode/decode, language detection/auto-routing, security scanning, JSON metrics, Prometheus metrics, optional bearer authentication, optional TLS, request-size/decode-count/concurrency limits, and sealed startup configuration.
- read-only Mosaic registry HTTP transport with authenticated/TLS-capable catalog distribution and immutable SHA-256 object fetch.
- client-side complete SHA-256 verification before downloaded registry objects are accepted.
- server-side CAS re-hashing before object delivery so corrupt local registry state fails closed.
- CI and packaged-release validation for the new service and registry transport tools.

## Compatibility

This release does not change native C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics version 2, or existing pack/binary-format contracts. Embedded users can continue to use the native/Python interfaces without deploying either HTTP service.

## Scope

Remote registry mutation, distributed dependency solving, OAuth/OIDC, gRPC/HTTP2, and cancellation of already executing native calls are not claimed in service/transport v1. These require stronger authorization/scheduler contracts and remain future additive work.
