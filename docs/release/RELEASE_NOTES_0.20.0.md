# Mosaic Tokenizer 0.20.0

Optional enterprise pack-trust release.

- Adds separate `libmosaic_trust` static/shared libraries.
- Adds bounded Ed25519 publisher trust stores and SHA-256 public-key identities.
- Adds detached `MSSIGV01` pack signatures binding exact pack content identity.
- Adds publisher revocation and explicit untrusted/revoked errors.
- Adds `mosaic-author sign-pack` and `key-id` offline operations.
- Preserves structural-validation-first behavior and keeps the core `libmosaic` free of OpenSSL.
