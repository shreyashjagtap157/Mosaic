# Mosaic Tokenizer 0.21.0

Enterprise content-addressed pack registry release.

- Adds `mosaic-registry`, an offline SQLite/WAL control-plane tool.
- Adds atomic immutable SHA-256 object installs.
- Adds cryptographically derived verified/unverified trust state.
- Adds deterministic bounded SemVer resolution into exact hash-pinned lockfiles.
- Rejects mutable `latest`/`*` canonical requirements.
- Adds registry audit, lock verification, and unreferenced-object GC.
- Adds concurrent-install and corruption conformance tests.
