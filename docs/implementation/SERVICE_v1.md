# Mosaic Service API v1 (`mosaicd`)

`mosaicd` is the optional HTTP deployment surface for Mosaic. It is intentionally a thin service over the same Python binding and native `libmosaic` runtime used by embedded applications. It does not implement tokenization, language detection, or security policy independently.

## Deployment model

- one sealed `Tokenizer` is loaded at process startup;
- language, detector, security, and normalization packs are attached before sealing;
- request concurrency is bounded by a non-blocking semaphore;
- native calls are serialized through the service state lock because the current public tokenizer contract does not promise concurrent calls on one tokenizer handle;
- multiple service processes may be used when process-level parallelism is required;
- request bodies are never written to service logs by default.
- `GET /v1/version` includes a `service_profile` object so callers can detect whether the service is running in low-memory mode and what ceilings were selected.
- `GET /v1/config` returns the active service profile and concrete ceilings in a machine-readable form for embedders and ops tooling.
- `mosaicd --print-config` writes the same resolved configuration to stdout and exits without starting the server.
- `GET /openapi.json` publishes a machine-readable service schema with the core request, response, and error shapes for launchers, agents, and ops tooling.

## Endpoints

Unauthenticated operational endpoints:

- `GET /health/live`
- `GET /health/ready`
- `GET /openapi.json`

Authenticated API endpoints when a bearer token is configured:

- `GET /v1/version`
- `GET /v1/config`
- `GET /v1/metrics`
- `POST /v1/encode`
- `POST /v1/encode-batch`
- `POST /v1/decode`
- `POST /v1/detect`
- `POST /v1/encode-auto`
- `POST /v1/security`
- `POST /v1/streams` (create resumable native online stream)
- `POST /v1/streams/<id>/push`
- `POST /v1/streams/<id>/finish`
- `DELETE /v1/streams/<id>` (cancel)

Binary input and output use canonical Base64 strings in JSON. Token IDs are unsigned 32-bit JSON integers.

## Security and resource boundaries

The service enforces:

- maximum HTTP request bytes;
- maximum decoded-token array length;
- maximum batch item count and decoded batch bytes;
- bounded native batch-executor workers/queue;
- maximum active resumable stream sessions;
- maximum unresolved bytes per online stream and idle-session expiry;
- bounded concurrent admitted requests;
- an explicit `--low-memory` mode that lowers service, batch, and stream ceilings for constrained desktops;
- socket timeout;
- optional constant-time bearer-token comparison;
- optional TLS server mode with TLS 1.2 minimum;
- `Cache-Control: no-store` and `X-Content-Type-Options: nosniff` responses;
- sealed native tokenizer configuration after startup;
- native Mosaic runtime limits in addition to HTTP-level limits.

A concurrency admission failure returns HTTP 503. Invalid requests return 400-class errors. Native Mosaic resource rejection is mapped to HTTP 413; other native semantic failures map to HTTP 422.

## Non-goals of v1

Service API v1 supports cancellation of idle/resumable stream sessions, but does not claim interruption of an already executing native call, distributed scheduling, tenant-specific tokenizer mutation, OAuth/OIDC identity, or HTTP/2/gRPC. Those require a stronger service runtime rather than being approximated in this minimal deterministic deployment surface.
