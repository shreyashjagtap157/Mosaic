# Mosaic Read-Only Registry HTTP Transport v1

This transport publishes an existing local Mosaic registry without changing canonical registry semantics. The local SQLite metadata and content-addressed object store remain authoritative; HTTP is distribution only.

## Endpoints

- `GET /health/live`
- `GET /v1/catalog`
- `GET /v1/objects/sha256/<lowercase-sha256>`

The catalog contains canonical pack metadata plus the local registry catalog hash. Object URLs are immutable content identities. Before serving an object the server recomputes its SHA-256; corrupt CAS state fails closed.

The client helper/CLI verifies the complete downloaded object against the requested SHA-256 before atomically replacing the destination path.

## Security

The transport supports optional bearer authentication and TLS 1.2+ server wrapping. Object response caching is safe because the URL itself contains the immutable SHA-256 identity. Catalog responses use `no-store` because the catalog evolves when new immutable entries are installed.

Remote upload, delete, mutable aliases, and server-side dependency resolution are intentionally absent from v1. Those require authorization/audit/governance semantics stronger than a file distribution endpoint. Canonical execution continues to depend on exact resolved hashes, never on network response order or a remote `latest` name.
