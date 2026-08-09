# Mosaic 0.1.2.0 Qualification

Status: local Linux x86-64 qualification complete; external platform/stable-generation gates remain open.

## Qualified source

Product version: `0.1.2.0`.

This candidate adds enterprise service batching, resumable native online-stream sessions, and the Python `OnlineStream` wrapper while preserving tokenizer semantics version 2, C ABI 1.0.0, trust ABI 1.0.0, and the existing binary pack/serialization contracts.

## Fresh CMake qualification

Fresh build directories were configured and built from the committed `0.1.2.0` source.

- dependency-minimal `core-release`: **22/22 CTest PASS**;
- full profile with available ICU/trust dependencies: **24/24 CTest PASS**;
- C++ API smoke reports product version `0.1.2.0`;
- stable C API differential validator: **PASS** — 1,004 byte round trips, 200 online-stream chunkings, and 250 incremental-edit/full-retokenization differentials;
- frozen C ABI contract: **PASS**;
- frozen binary-format contract: **PASS**.

## Enterprise application surfaces

All tests below used the freshly rebuilt `0.1.2.0` full native library.

- Python native-version identity and arbitrary-byte round trip: **PASS**;
- Python `OnlineStream` arbitrary-chunk full/stream equivalence: **PASS**;
- `mosaicd` bearer-authenticated encode/decode, arbitrary-byte, detector, security, request-limit, saturation, JSON metrics, and Prometheus metrics gates: **PASS**;
- native-backed `POST /v1/encode-batch` ordered output equivalence: **PASS**;
- service batch item and decoded-byte ceilings: **PASS**;
- resumable stream create/push/finish exact equivalence to full encode: **PASS**;
- native pending-byte limit/partial-consumption handling: **PASS**;
- stream cancellation, missing-session rejection, and zero leaked active sessions: **PASS**;
- immutable authenticated registry HTTP catalog/object transport with client/server SHA-256 verification: **PASS**.

## Strict Makefile and sanitizer qualification

The native Makefile tree was cleaned and rebuilt at product version `0.1.2.0`. Static/shared C API and strict C++11 smoke clients report `0.1.2.0`.

The native Makefile suite completed successfully, including ASan+UBSan coverage for:

- public API and malformed packs;
- language routing and detector;
- raw BPE;
- Unicode security and ICU normalization differential;
- bounded online streaming;
- incremental edits and checkpoint resynchronization;
- TokenDocument core/rich/serialization projections;
- lexer and semantic/sub-byte projections;
- block storage;
- content cache and cache backend;
- runtime policy and bounded executor;
- observability and reliability;
- optional Ed25519 trust/revocation.

Observed non-sanitized stress/differential gates include 500 arbitrary online-stream chunkings, 1,000 incremental edits, 500 resynchronizing edits, 8,000 threaded runtime-policy encodes, and 1,000 reliability iterations.

## Source identity

- canonical product-version synchronization: **PASS**;
- repository validator: **PASS**;
- deterministic tracked-source checksum inventory: **PASS**;
- tracked release artifacts before packaging evidence update: **453**.

## Packaging

Clean-tree archive/wheel/SBOM/provenance qualification: **PASS**.

- package validator reports `git_dirty=False`;
- Linux x86-64 archive: `mosaic-tokenizer-0.1.2.0-linux-x86_64.tar.gz`;
- archive SHA-256: `e581e3c220f9e31839c72959c8d844fcda9eb778de912f40270e18302f9339d6`;
- Python wheel: `mosaic_tokenizer-0.1.2.0-py3-none-any.whl`;
- wheel SHA-256: `10a6c939ab0644744afc230a4a0c88ca1c39cf0066a50d52084d8a3c54ec6986`;
- packaged CLI, packs, manifest, checksum inventory, SPDX SBOM, provenance, authoring, compatibility, trust signing/verification, registry lock/audit, and external static C consumer: **PASS**.

The enterprise service and registry tools are shipped in the relocatable package layout validated previously on this branch.

## External/open gates

The following are not claimed as passes by this local Linux qualification and continue to block stability generation `1`:

- current-source Windows native/service/package execution;
- current-source macOS execution;
- Linux/Windows ARM64 and Apple Silicon qualification;
- Android/iOS/WASM deployment qualification;
- Rust workspace/MSRV/Miri/cargo-fuzz gates for this reconstructed branch where not re-executed here;
- ThreadSanitizer/race-detector campaign;
- long-running multi-platform soak/fault-injection campaign;
- final cross-tokenizer benchmark campaign on frozen release hardware/corpora;
- final release signing and remaining stable-generation governance gates.

No `v0.1.2.0` tag and no `1.0.0.0` stability claim are justified until the applicable external gates are completed.
