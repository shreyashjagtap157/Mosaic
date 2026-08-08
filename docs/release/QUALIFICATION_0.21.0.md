# Mosaic Tokenizer 0.21.0 Qualification

- registry schema creation/WAL: pass;
- 32 concurrent idempotent signed installs: pass;
- publisher/name/version immutability conflict: rejected;
- unsigned install under signature-required policy: rejected;
- signed trust state derived cryptographically: pass;
- deterministic SemVer resolution: pass;
- mutable `latest` constraint: rejected;
- lock hash/object verification: pass;
- injected object corruption: detected by lock verification and audit;
- restored object audit: pass;
- unreferenced-object GC: pass;
- canonical lock byte reproducibility: pass;
- inherited native tokenizer/trust behavior: unchanged from fully qualified 0.20; changed control-plane surface tested independently;
- clean package registry/trust/lock flow: pass.

## Release artifact

- archive: `mosaic-tokenizer-0.21.0-linux-x86_64.tar.gz`
- SHA-256: `a5e854805a0f9d2d00bfda9b5001a9fe47184903828da3b11b2a5a1e6c29d5ae`
