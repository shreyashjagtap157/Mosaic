# Content-Addressed Pack Registry v1

Mosaic 0.21 adds an offline/control-plane registry. Registry/database/network behavior is deliberately excluded from the hot tokenizer runtime.

## Storage

- SQLite metadata database with WAL and transactional identity updates.
- Immutable SHA-256 object store under `objects/sha256/<prefix>/<hash>`.
- Atomic temp-file + fsync + rename installation.
- Orphan objects are harmless and removable with `gc`.

## Identity

Publisher, logical name, and numeric SemVer are human coordination metadata. Exact pack bytes are identified by SHA-256. An existing publisher/name/version tuple cannot be rebound to different bytes.

## Trust

Callers cannot self-declare a pack verified. Signed install requires both an Ed25519 public key and `MSSIGV01` detached signature. Successful cryptographic verification records `trust_status=verified` and the exact publisher key ID. Unsigned packs remain `unverified`. `--require-signature` fails closed.

## Resolution

Authoring requirements may use exact/comparison/caret numeric SemVer constraints. `latest`, `*`, and empty mutable aliases are rejected. Resolution produces a canonical JSON lock containing exact content hashes and publisher key IDs. Canonical execution consumes the lock, not the unresolved requirement.

## Operations

- `init`
- `install`
- `list`
- `resolve`
- `verify-lock`
- `audit`
- `gc`

Lockfiles include a catalog snapshot hash and their own canonical lock hash. Verification recomputes object identities before success.
