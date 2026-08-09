# Mosaic 0.1.1.0 Qualification

Status: **locally qualified enterprise deployment candidate; external platform gates remain open.**

## Current-source native qualification

- fresh dependency-minimal CMake core profile: **22/22 PASS**;
- fresh full CMake profile with ICU normalization and Ed25519 trust: **24/24 PASS**;
- Makefile strict C/C++ build and native suite: **PASS**;
- ASan+UBSan public API, malformed packs, language, detector, raw BPE, security, normalization, online stream, incremental, checkpoint resync, TokenDocument, lexer, semantic/sub-byte, block storage, cache, cache backend, runtime policy, executor, observability, reliability and trust: **PASS**;
- stable C API validator: **1004 round trips, 200 stream chunkings, 250 edit/full differentials PASS**;
- frozen C ABI 1.0.0 contract: **PASS**;
- stable binary-format/tokenizer-semantics contract: **PASS**;
- native/Python runtime version identity: **0.1.1.0 PASS**.

## Enterprise deployment surfaces

`mosaicd` integration:

- bearer authentication: PASS;
- exact arbitrary-byte encode/decode: PASS;
- detector and security delegation: PASS;
- HTTP request/decode resource limits: PASS;
- bounded concurrency saturation returns HTTP 503 and increments a dedicated counter: PASS;
- JSON metrics: PASS;
- Prometheus text metrics: PASS;
- sealed tokenizer identity/version endpoint: PASS.

Read-only registry HTTP transport:

- bearer authentication: PASS;
- canonical catalog hash: PASS;
- immutable SHA-256 object fetch: PASS;
- client complete SHA-256 verification: PASS;
- missing immutable object rejection: PASS;
- corrupt local CAS object fails closed instead of being served: PASS.

## Repository/release engineering

- workflow YAML parses after PR/nightly/release service/registry gate wiring: PASS;
- four-part version synchronizer: PASS;
- deterministic source checksum inventory: PASS;
- repository structure validator: PASS;
- clean-tree Linux x86-64 package build: **PASS** (`git_dirty=false`);
- packaged CLI/packs/release manifest/checksums: **PASS**;
- packaged SPDX SBOM and provenance: **PASS**;
- packaged authoring/compatibility/trust/registry/external static-C consumer validation: **PASS**;
- packaged `mosaicd --help` and `mosaic-registry-http --help`: **PASS**;
- release archive SHA-256: `4c25558b6f19b20d3566098edbede1c5cafc163920b780829bc74161257b86ba`;
- Python wheel SHA-256: `bd03e7cd10562aab7f3bfeb0d3ce1d44961c5cbf462468a488e55cc96165ba9d`.

## External/open gates

The following are not fabricated as local passes and continue to block stable-generation `1`:

- current-source Windows native/service/registry execution;
- current-source macOS execution;
- ARM64 platform matrix;
- .NET/other SDK work planned for later integration minors;
- Rust/Miri/fuzz/race-detector gates where the required toolchain/job is not executed in this local environment;
- final multi-platform release-candidate benchmark and supply-chain signing campaign.
