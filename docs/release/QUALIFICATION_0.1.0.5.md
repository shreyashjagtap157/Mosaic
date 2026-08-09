# Mosaic 0.1.0.5 Qualification Evidence

Date: 2026-08-08
Scope: performance-correctness remediation on the independent Linux mirror; Windows requalification pending operator application

## Current result

**PASS on the exercised Linux native functional, differential, performance-gate, and ASan/UBSan profiles.** The release is not yet tagged because the changed native core still requires Windows requalification and the stable-generation gates remain open.

## Source lineage

- baseline release: `v0.1.0.4` / `b155327`;
- performance implementation commit: `7f4797a` (`perf(core): eliminate tokenizer hot-path scaling bottlenecks`);
- C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, and stable format versions unchanged.

## Native functional qualification

- Linux x86-64 Clang full-release build: PASS;
- CTest full profile: **24/24 PASS**, including normalization and trust;
- canonical path-order vectors: **4/4 PASS**;
- M3 model/reference differential: **1008 cases PASS** plus 12 malformed vocabulary fixtures rejected;
- raw `.tiktoken`/native BPE independent-oracle differential: **608 cases PASS**;
- tokenizer-manifest exact identity: PASS;
- frozen public C ABI contract: PASS;
- stable binary-format/tokenizer-semantics contract: PASS;
- `git diff --check`: PASS before implementation commit.

## Performance-gate evidence

These measurements are same-host internal qualification evidence from a virtualized Intel Xeon E5-2673 v4 environment. They are not public claims against another tokenizer implementation.

### Weighted Viterbi stress

600-entry generated model, 64 KiB stress input, identical output (`6499` tokens, total cost `11494`):

- 0.1.0.4 baseline: 7.20 s;
- remediated engine: 0.01 s observed wall-clock at `/usr/bin/time` resolution;
- 128 KiB remediated: 0.02 s;
- 256 KiB remediated: 0.04 s.

This removes the previously observed approximately quadratic scaling on the stress vocabulary. Because the post-fix 64 KiB run is near timer granularity, the ratio is not used as a precise public speedup claim.

### Raw BPE, 10 MiB

Same raw-BPE fixture/input and identical output (`6,183,905` tokens, cost `896,128,726`):

- baseline: 12.25 s, 680,768 KiB peak RSS;
- remediated: 0.89 s, 52,872 KiB peak RSS;
- observed elapsed-time improvement: approximately **13.8x**;
- observed peak-RSS reduction: approximately **92.2%**.

### Bounded online processing, 10 MiB

- baseline: 11.8 MiB/s (below the 20 MiB/s project floor);
- remediated: **33.3 MiB/s**, `max_pending=54` bytes, approximately 12.6 MiB max RSS;
- security visitor: 12.0 MiB/s, approximately 11.7 MiB max RSS;
- result: PASS.

### Near-end incremental edit, 10 MiB

- source reprocessed: 104,895 bytes (~1.000%);
- source reused: 10,380,865 bytes;
- baseline incremental time: 0.735682 s;
- remediated incremental time: 0.035444 s;
- remediated full-retokenization time: 0.607003 s;
- remediated speedup: **17.13x** versus full retokenization;
- result: PASS (project floor 2x).

### Checkpoint resynchronization, 10 MiB

- source reprocessed: 65,575 bytes (~0.625%);
- reusable prefix: 5,242,841 bytes;
- reusable suffix: 5,177,380 bytes;
- baseline incremental time: 0.280519 s;
- remediated incremental time: 0.058445 s;
- remediated full-retokenization time: 0.682054 s;
- remediated speedup: **11.67x**;
- remediated max RSS: ~136.1 MiB;
- result: PASS (project floor 3x).

## ASan + UBSan qualification

The sanitizer build uses `-fsanitize=address,undefined`, leak detection, abort-on-error, and halt-on-UB. The following independently executed targets passed without an ASan/UBSan report:

- public API stress/round-trip/stream/edit smoke;
- raw BPE public API;
- online Viterbi: 500 arbitrary-byte/chunk differentials plus language snapshot;
- incremental Viterbi: 1,000 edit cases;
- checkpoint resync: 100 edit cases;
- malformed pack corpus;
- Unicode 17 security;
- Unicode 16 normalization: 10,000 ICU differential cases;
- language packs;
- language detector and auto routing;
- TokenDocument core/rich/serialization paths;
- declarative lexer and semantic/sub-byte views;
- block storage and packed representation;
- cache and authenticated cache backend;
- sealed runtime policy;
- bounded executor;
- observability;
- reliability (500 sanitizer iterations);
- Ed25519 trust, tamper, revocation, unknown-publisher and resource gates.

## Static-analysis status

Whole-file Clang static analysis of the expanded `mosaic.c` was attempted twice in this environment and exceeded the per-command execution limit without emitting a diagnostic. It is therefore recorded as **TIMEOUT / unresolved**, not PASS. Strict GCC/Clang warning-as-error compilation and the ASan/UBSan matrix above pass. The analyzer must be rerun on an unconstrained runner before final release qualification.

## Release-package preflight

- Python wheel built: `mosaic_tokenizer-0.1.0.5-py3-none-any.whl`, SHA-256 `5d1839597d058b588bb592c192287c8d26cc01ab17e7a653e9d93b3a7a6435fd`;
- Linux x86-64 release archive built: `mosaic-tokenizer-0.1.0.5-linux-x86_64.tar.gz`, SHA-256 `6dcc2268b9631ea290c0e125e3bb6f8b444c904200674d31826d068594c466cd`;
- SPDX SBOM and in-toto provenance generated;
- staged supply-chain validation: PASS (`sbom_files=47`, `provenance_subjects=48`, `checksums=49`);
- the monolithic clean-package validator advanced through packaged Python/CLI, fingerprints, authoring, raw-BPE compatibility, static-client, trust signing/verification, registry lock/audit, and checksum stages, but the wrapper process exceeded the execution window before its final success line. It remains recorded as **TIMEOUT / final wrapper completion unobserved**, not PASS.

## Still required before tagging/finalizing 0.1.0.5

- apply the code/release-prep patch to the Windows repository and rerun the Windows Clang core profile;
- rerun the public C API validator against the Windows 0.1.0.5 DLL;
- record Windows benchmark deltas where meaningful;
- rerun unconstrained Clang static analysis;
- regenerate release/source checksums after all release metadata is frozen;
- package and validate the final candidate artifacts.

The broader stability-generation gates (macOS, production Rust qualification, Miri, fuzz campaigns, race-detector/support-matrix work) remain open and are not represented as passes.
