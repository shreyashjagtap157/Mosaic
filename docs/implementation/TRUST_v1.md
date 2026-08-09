# Pack Trust v1

Mosaic 0.20 adds an optional trust layer without changing core pack semantics.

## Separation of concerns

1. Parse and structurally validate a pack with its normal Mosaic pack/model/unicode/etc. loader.
2. Authenticate the exact pack identity with `libmosaic_trust`.
3. Apply deployment trust/revocation policy.

A signature never bypasses structural validation.

## Signature record

`MSSIGV01` is a 160-byte little-endian detached record containing version/header metadata, Ed25519 key ID, exact SHA-256 pack identity, 64-byte signature, and zero reserved bytes.

The Ed25519 message is:

`"MOSAIC-PACK-SIGNATURE-v1" || key_id || SHA256(pack_bytes)`

`key_id = SHA256(raw_ed25519_public_key)`.

This domain-separated fixed-size message keeps signature verification independent of pack size while authenticating the exact content-addressed pack identity.

## Runtime library

`libmosaic_trust` is separate from `libmosaic` and links OpenSSL Crypto. The core tokenizer remains dependency-minimal.

The trust store is explicitly bounded by `max_keys`, supports idempotent exact key insertion, publisher lookup, revocation, and fail-closed verification.

## Errors

- unknown publisher: `MOSAIC_ERROR_UNTRUSTED`;
- invalid signature: `MOSAIC_ERROR_UNTRUSTED`;
- pack/hash mismatch or malformed signature metadata: `MOSAIC_ERROR_INTEGRITY`;
- revoked publisher: `MOSAIC_ERROR_REVOKED`;
- missing revocation target: `MOSAIC_ERROR_NOT_FOUND`.

## Authoring

`mosaic-author sign-pack PACK PRIVATE_KEY.pem OUTPUT.sig` signs a pack using an Ed25519 PEM private key. This command has an optional Python `cryptography` dependency. Private keys are never required or loaded by the runtime trust library.

The checked-in conformance public key/signature are generated from a deliberately public deterministic test seed and MUST NOT be used as production trust material.
