# Changelog

## Unreleased

- add a Windows installer proof line to the 1.0.0 qualification note so the stable-release evidence trail reflects the staged package path;
- add repo-native `make windows-package` and `make windows-package-verify` entry points for the Windows installer flow;
- add a release index validator so the current release notes and qualification reports stay discoverable and synchronized;
- add a release notes and qualification index under `docs/release/` so the current evidence trail is easier to navigate;
- add a Windows packaging qualification note so the installer evidence is recorded alongside the release docs;
- add a Windows package validator for the staged installer tree and wire it into the standard release-readiness path;
- add a release-readiness validator for the space-grade boundary docs and wire it into the standard readiness path;
- add a current-versus-roadmap capability table that cleanly separates shipped behavior from research and external qualification work;
- add a concrete support matrix that spells out the qualified platform boundary, runtime surfaces, and resource-profile guidance;
- add a single integration guide that covers native C, Python, desktop, agent, and service embedding patterns;
- expand the Windows install tree and installer packaging so the staged app now includes the main CLI, desktop self-test, comparison harness, public headers, shared/runtime binaries, release docs, and reference packs;
- add explicit low-memory native defaults for runtime limits and executor sizing;
- expose Python convenience helpers for low-resource operation and make library discovery robust against preset build-tree layouts on Windows;
- add native low-memory smoke and benchmark coverage in the build and test runners, plus tests covering the constrained profile and improve binding-path resolution for local integration use;
- add a constrained low-memory benchmark manifest example and tighten the reference-hardware policy for 4 GB-class desktops;
- add a low-memory profile validator and refresh the source-identity checksum manifest to cover the new constrained benchmark evidence path;
- add a low-memory profile runner that executes the benchmark and writes a filled constrained record into an ignored results folder;
- record the space-grade completion plan/spec in-repo so remaining work stays explicit and measurable.

## 0.1.3.0 — 2026-08-10

Mixed-language span-routing candidate minor.

- add `mosaic_span_route`, `MOSAIC_CAP_SPAN_ROUTING`, `mosaic_tokenizer_detect_spans`, and `mosaic_tokenizer_encode_span_auto`;
- expose the span-routing API through Python as `SpanRoute`, `detect_spans()`, and `encode_span_auto()`;
- add native and Python regressions for gapless en/hi/ja span routing with exact decode preservation;
- add native CLI `analyze-span-auto` smoke coverage for gapless en/hi/ja span routing with exact decode preservation;
- teach ABI validation to inspect Windows PE/COFF DLL exports through `llvm-readobj`;
- advance native C ABI additively from 1.0.0 to 1.1.0 while preserving tokenizer semantics 2, trust ABI 1.0.0, and frozen pack/serialization formats.

## 0.1.2.1 — 2026-08-10

Rust-substrate correctness patch.

- wire the existing Unicode 17 pack parser into `mosaic-pack` and enforce a bounded Unicode-range resource policy;
- fix incremental SHA-256 updates so manifest/content identities are correct across arbitrarily split input;
- add SHA-256 and Unicode exactness regressions;
- preserve C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, service/registry APIs, and frozen pack formats.

## 0.1.2.0 — 2026-08-09

Enterprise batching and resumable streaming minor.

- expose native bounded OnlineStream through Python;
- add native BatchExecutor-backed HTTP batch encoding with explicit item/byte limits;
- add resumable authenticated stream sessions with bounded pending bytes, session count, idle expiry, finish and cancel;
- preserve frozen C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, and existing pack formats.

## 0.1.1.0 — 2026-08-09

Enterprise deployment and remote distribution minor.

- add bounded `mosaicd` HTTP service over the existing native runtime;
- add optional bearer authentication and TLS plus explicit request/concurrency limits;
- add JSON and Prometheus content-free service/native metrics;
- add read-only authenticated immutable registry HTTP catalog/object distribution;
- verify CAS objects on the server and SHA-256 downloads on the client before acceptance;
- wire service and registry transport into CI and packaged-release validation;
- preserve C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, and frozen pack formats.

## 0.1.0.5 — 2026-08-08

Tokenizer hot-path scalability, bounded streaming, and incremental/resynchronizing update remediation.

- replace quadratic forward Viterbi tie reconstruction in the production batch engine with exact backward dynamic programming while preserving the independent reference/path-order oracle;
- compile validated pack vocabulary entries into immutable native-endian runtime indices so hot execution avoids repeated packed-field decoding and redundant vocabulary searches;
- rebuild raw-BPE execution around direct byte/pair indices, compact node state, vocabulary-impossible boundary segmentation, and direct output reconstruction;
- retain explicit forward prefix-tree proof state only for online safe-prefix commitment and remove duplicate streaming traversals/modulo-heavy ring indexing;
- make near-end incremental updates transactional and suffix-only, retaining source/token allocations rather than copying the complete document and token prefix;
- make checkpoint resynchronization materialize only the changed token interval and move reusable suffix tokens overlap-safely in retained storage;
- preserve C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, canonical pack formats, exact source mappings, and deterministic MOS-REQ-022 ordering.

## 0.1.0.4 — 2026-08-08

Windows C++ API-consumer compatibility and release-metadata synchronization patch.

- make the C++ API smoke assertion valid under the declared pre-C++17 consumer floor by using the two-argument `static_assert`;
- pin the C++ smoke consumer to strict C++11 in both CMake (`CXX_STANDARD 11`, required, extensions disabled) and the native Makefile (`-std=c++11`) so build-system/compiler defaults cannot silently weaken the compatibility test;
- close the dependency-minimal Windows Clang qualification path at 22/22 tests, with ICU normalization and trust tests explicitly absent because the `core-release` preset disables those optional dependencies;
- independently re-qualify Linux Clang at 22/22 for `core-release` and 24/24 for `full-release`, including normalization and trust;
- expand `tools/set_version.py` so current-version references in README, versioning policy, C ABI documentation, implementation status, and threat-model status cannot silently remain stale after a product-version bump;
- preserve native C ABI 1.0.0, optional trust ABI 1.0.0, tokenizer semantics version 2, pack versions, and frozen binary-format contracts unchanged.

## 0.1.0.3 — 2026-08-08

Four-part product-version re-baseline and stability-label correction.

- define canonical `S.M.N.P` product versions: stability generation, major release, minor release, patch;
- move the current cumulative enterprise build to pre-stable candidate `0.1.0.3`;
- reserve `1.0.0.0` for the first build that actually satisfies the declared stable cross-platform qualification gates;
- retain old three-component Git tags as immutable legacy history and document their migration mapping;
- keep C ABI 1.0.0, trust ABI 1.0.0, tokenizer semantics 2, pack SemVer, and all binary-format contracts independently versioned and unchanged;
- update release tooling, package metadata, support/security/compatibility documentation, and release evidence to use the four-part product version.

### Legacy mapping

- `v1.0.0` → conceptual `0.1.0.0`;
- `v1.0.1` → conceptual `0.1.0.1`;
- `v1.0.2` → conceptual `0.1.0.2`;
- `v1.0.3` → conceptual `0.1.0.3`.

The canonical four-part tag is created on the migration commit; legacy tags are not rewritten.

## 1.0.3 — 2026-08-08

Windows UCRT strict-warning portability patch.

- retain portable ISO C file I/O while suppressing only Microsoft UCRT secure-CRT deprecation annotations on Windows targets;
- keep all compiler warnings promoted to errors (`/W4 /WX` or `-Wall -Wextra -Wpedantic -Werror`);
- replace fixed-literal `strcpy` calls in the CLI security projection with compile-time-sized bounded copies;
- apply the Windows CRT policy consistently to native libraries, CLI, C/C++ conformance clients, and optional trust tests;
- preserve native/trust ABI 1.0.0, tokenizer semantics version 2, and all stable binary formats unchanged.

## 1.0.2 — 2026-08-08

Windows native build portability patch.

- give Windows static archives distinct basenames (`mosaic_static.lib`, `mosaic_trust_static.lib`) so they cannot collide with DLL import libraries (`mosaic.lib`, `mosaic_trust.lib`);
- add configure-time guards that fail closed if a Windows static/shared target pair regresses to the same `.lib` output name;
- preserve Linux/macOS library filenames, native/trust ABI 1.0.0, tokenizer semantics version 2, and all stable binary formats unchanged.

## 1.0.1 — 2026-08-08

Cross-platform source-integrity and Windows tooling patch.

- make repository Python tooling explicitly UTF-8 instead of depending on the host locale encoding;
- add canonical LF Git attributes while preserving binary pack/trust fixtures byte-for-byte;
- make source checksums canonical across LF/CRLF text checkouts;
- exclude Git-ignored/generated working-tree artifacts from source identity;
- preserve native/trust ABI 1.0.0, tokenizer semantics version 2, and all stable binary formats unchanged.

## 1.0.0 — 2026-08-08

First stable enterprise universal-platform release.

- freeze native C ABI 1.0.0 and optional trust ABI 1.0.0;
- retain canonical tokenizer semantics version 2 and all stable v1 binary formats;
- activate 1.x compatibility/deprecation and security-exception policies;
- publish enterprise threat model and complete implementation/external-gate status;
- preserve the 0.29 candidate function/type/export baseline as the 1.x additive ABI contract;
- require deterministic reliability, supply-chain, package, ABI and format gates for release promotion.

## 0.29.0 — 2026-08-08

Enterprise 1.0 release-candidate ABI, format, compatibility, and threat-model freeze.

- explicit cross-platform public symbol visibility instead of automatic shared-library export;
- machine-readable additive C ABI baseline covering 167 core and 9 trust functions;
- normalized frozen function signatures, public structs/enums, and stable constants;
- machine-checked MOSPACK/TokenDocument/packed-model/cache/signature/registry/semantics format contract;
- 1.x compatibility and deprecation policies;
- enterprise threat model across source bytes, packs, serialized records, caches, registry, FFI, concurrency, observers, and supply chain;
- CI release gates for ABI and binary-format drift.

## 0.28.0 — 2026-08-08

Enterprise deterministic reliability and recovery qualification release.

- replayable arbitrary-byte, online-streaming, incremental-edit, cold-TokenDocument, lifecycle, and executor reliability campaign;
- PR smoke, nightly release, and extended soak reliability tiers;
- 25,000-iteration release campaign repeated with identical semantic replay digest;
- concurrent content-addressed registry install chaos and deterministic lock verification;
- deliberate registry object corruption detection followed by exact-hash atomic repair;
- SQLite integrity/schema audit hardening and orphan-object garbage-collection verification;
- reliability campaign reports designed for seed-based failure replay rather than wall-clock anecdotes.

## 0.27.1 — 2026-08-08

Cross-platform conformance patch.

- remove direct C11 `<threads.h>` dependence from cache, runtime-policy, executor, and observability conformance tests;
- exercise the same private Win32/pthread portability shim used by the runtime;
- preserve tokenizer semantics, pack formats, and C ABI unchanged.

## 0.27.0 — 2026-08-08

Enterprise portability and release-supply-chain hardening release.

- repository-root CMake superbuild and reproducible core/full presets;
- dependency-minimal core builds without OpenSSL or ICU;
- Windows `_beginthreadex`/Win32 synchronization and POSIX pthread portability shim;
- cross-platform Linux/macOS/Windows core CI topology with Windows Clang C11 selection;
- deterministic SPDX 2.3 SBOM and SLSA-shaped in-toto provenance;
- staged SHA-256 inventory and source-checksum-manifest binding;
- official release builds refuse dirty Git trees while preflight builds record dirty provenance;
- release archive naming and supply-chain validation defects fixed;
- SECURITY/SUPPORT/release-engineering policy documents;
- mechanical release-version synchronization and validation.

## 0.26.0 — 2026-08-08

Enterprise performance-hardening release.

- eliminate redundant first-byte comparisons in validated Viterbi vocabulary buckets;
- preserve exact canonical segmentation while reducing hot-path comparison work;
- benchmark against the sealed v0.25 binary on the same 10 MiB fixture;
- reject an expanded-vocabulary metadata experiment after it measured slower than the compact pack view;
- retain the existing compact runtime representation and memory profile.

## 0.25.0 — 2026-08-08

Supported Python binding release.

- deterministic pure-Python wheel over the stable C ABI;
- ownership-safe tokenizer/TokenDocument/executor context managers;
- exact arbitrary-byte, rich-view, batch, runtime-policy, and observer APIs;
- clean-wheel lifecycle and package qualification.

## 0.24.0 — 2026-08-08

Enterprise privacy-preserving observability release.

- structured encode/decode/detect/Unicode/security/normalization/lex/TokenDocument events;
- success/failure/resource event filtering and exact semantic fingerprint correlation;
- concurrent callback qualification through the bounded executor;
- same-tokenizer observer reentrancy suppression;
- telemetry configuration excluded from semantic and deployment identity.

## 0.23.0 — 2026-08-08

Enterprise bounded parallel batch-execution release.

- reusable fixed worker pool with bounded FIFO queue;
- concurrent batch submissions and deterministic result ordering;
- sealed-tokenizer sharing contract;
- per-batch item/input ceilings and per-item failure isolation;
- executor throughput/error metrics and sanitizer concurrency qualification.

## 0.22.0 — 2026-08-08

Enterprise cold TokenDocument serialization release.

- endian-defined columnar `MSTIRD01` TokenDocument record;
- whole-record and source-identity SHA-256 validation;
- exact preservation of requested model/Unicode/security/normalization/lexical/semantic projections;
- semantic-only internal lexical dependency without public projection leakage;
- bounded record/source/projection-item deserialization policy;
- authenticated malformed-record and cross-compiler qualification.

## 0.21.0 — 2026-08-08

Enterprise content-addressed pack registry release.

- atomic SQLite/WAL + SHA-256 object registry;
- cryptographically derived trust state;
- deterministic SemVer-to-exact-hash lock resolution;
- registry audit, lock verification, and object GC;
- concurrent install/corruption qualification.

## 0.20.0 — 2026-08-08

Optional enterprise Ed25519 pack-trust release.

- separate dependency-isolated trust library;
- exact SHA-256 pack identity signatures;
- bounded publisher trust store and revocation;
- structural-validation-first authorization;
- offline pack signing/key-ID authoring.

## 0.19.0 — 2026-08-08

Enterprise immutable runtime-policy release.

- explicit input/output/TokenDocument resource ceilings;
- allocation-aware high-level encode/decode limits;
- immutable tokenizer sealing for shared serving;
- separate deployment runtime identity;
- lock-free runtime metrics and resource-rejection counters;
- stream/document/incremental policy propagation and concurrency qualification.

## 0.18.0 — 2026-08-08

Authenticated persistent/distributed cache-backend protocol.

- backend-neutral immutable record callbacks;
- whole-record and payload SHA-256 integrity;
- wrong-key replay and corrupt-backend fail-closed behavior;
- explicit integrity error and capability.

## 0.17.0 — 2026-08-08

Enterprise bounded content-cache release.

- thread-safe bounded O(1)-average in-memory LRU cache;
- explicit byte/entry ceilings and eviction semantics;
- cache hit/miss/replace/eviction/removal/peak metrics;
- projection-specific block cache keys bound to content identity.

## 0.16.0 — 2026-08-08

Enterprise multiscale block planning and compact model serialization.

- adaptive token-aligned KiB processing blocks and MiB macroblocks;
- content identities bound to tokenizer fingerprints;
- block/macroblock resource ceilings and oversized-token signaling;
- checksummed fixed-bit ID + ULEB length serialization with exact span recovery.

## 0.15.0 — 2026-08-08

Stable semantic enrichment and sub-byte view release.

- identifier, number, and string source-mapped semantic components;
- semantic TokenDocument density/capability;
- generic bounded MSB0/LSB0 sub-byte extraction;
- nibble/cross-byte sanitizer conformance.

## 0.14.0 — 2026-08-08

Stable declarative lexer and lexical TokenDocument release.

- bounded lexer pack v1 with exact lexical partitions;
- C/Python/Rust/JSON reference profiles;
- longest-prefix delimiters and optional nested block comments;
- lexical TokenDocument projection and capability bit;
- standalone/integrated lexer CLI;
- nine malformed lexer pack classes and sanitizer coverage.

## 0.13.0 — 2026-08-08

Rich TokenDocument projection and capability-negotiation release.

- optional immutable security-evidence projection;
- optional immutable mapped-normalization projection;
- tokenizer capability discovery;
- density-controlled extended TokenDocument creation;
- allocation-failure ownership path hardened under static analysis.

## 0.12.0 — 2026-08-08

Core TokenDocument / Token IR release.

- immutable exact source snapshot with source SHA-256 and tokenizer fingerprint;
- compact model-token ID/byte-length column with exact byte-span projection;
- optional Unicode grapheme projection;
- explicit density flags with no hidden construction of unrequested views;
- automatic-routing snapshot records its detection decision;
- document lifetime is independent from its parent tokenizer.

## 0.11.0 — 2026-08-08

Exact checkpoint-resynchronizing incremental Viterbi release.

- cached resumable online-Viterbi checkpoints;
- middle-edit forward state resynchronization with exact suffix reuse;
- compact 8-byte internal resync token cache;
- 500 randomized edit/full differentials plus sanitizer coverage;
- 10 MiB middle edit reprocesses about 0.625% and is over 8x faster than a fresh full tokenization on the reference host;
- raw-BPE explicitly remains unsupported by the Viterbi-specific resync API.

## 0.10.0 — 2026-08-08

Stable exact incremental Viterbi document release.

- canonical token-cache prefix reuse after edits with a proven safe restart boundary;
- transactional edit application and exact source-copy behavior;
- 1,000 randomized edit/full differentials and language-specialized coverage;
- explicit reprocessed/reused byte metrics;
- raw-BPE incremental rejection pending a separate merge-semantics proof;
- near-end 10 MiB locality/performance release gate;
- backward-compatible C API advanced to 0.8.0 while tokenization semantics remain version 2.

## 0.9.0 — 2026-08-08

Stable bounded-processing release.

- exact online weighted-Viterbi streaming with survivor-prefix commitment before EOF;
- caller-defined unresolved-source memory ceiling and explicit resource-limit behavior;
- callback/visitor Unicode-security evidence without findings-array materialization;
- adversarial no-prefix fixture plus arbitrary-byte/chunk differential and sanitizer coverage;
- bounded-processing 10 MiB throughput/RSS regression gates;
- backward-compatible C API advanced to 0.7.0 while tokenization semantics remain version 2.

## 0.8.0 — 2026-08-08

Stable source-mapped normalization shadow-view release.

- self-contained Unicode-16 normalization pack generated offline from ICU 76.1;
- exact source-mapped NFD, NFC, NFKD, NFKC, and NFKC-casefold views;
- algorithmic Hangul decomposition/composition and canonical combining-mark reordering;
- malformed UTF-8 preserved as opaque normalization barriers;
- 10,000 independent ICU differential cases and 11 malformed normalization fixtures;
- generator composition-pair defect found and fixed before release by differential testing;
- backward-compatible C API surface advanced to 0.6.0 while tokenization semantics remain version 2.

## 0.7.0 — 2026-08-08

Stable Unicode-17 script/security evidence release.

- standalone declarative Unicode-17 security pack;
- all 176 Script values and exact byte-mapped script spans;
- bidi-control, default-ignorable, noncharacter, deprecated-character evidence;
- deterministic mixed-script evidence with Common/Inherited/Unknown excluded from script counting;
- security-aware tokenizer fingerprint and high-level attachment API;
- 11 malformed security-pack regression fixtures plus ASan/UBSan coverage;
- backward-compatible C API surface advanced to 0.5.0.

## 0.6.0 — 2026-08-08

Stable raw-BPE compatibility release.

- model packs can declare weighted-Viterbi or raw-byte BPE execution;
- byte fallback validation is surface-based, allowing arbitrary existing token IDs;
- `.tiktoken` mergeable-rank import and exact eligible export;
- independent raw-BPE differential oracle and sanitizer coverage;
- unsupported algorithms, duplicate surfaces, and duplicate BPE ranks fail closed;
- full tiktoken regex/special-token pipeline is intentionally not claimed by this profile.

## 0.5.0 — 2026-08-08

Stable deterministic pack-authoring and baseline-training release.

- supported `mosaic-author` CLI shipped in release bundles;
- deterministic explicit model compilation with mandatory 256-byte fallback;
- deterministic compression-first corpus training with bounded candidate growth;
- corpus-order-independent model builds and machine-readable training reports;
- supported language-specialization and detector-pack compilers;
- pack inspection and clean-extraction author/runtime integration tests;
- native C API remains compatible with 0.4.0.

## 0.4.0 — 2026-08-08

Stable automatic stream/document routing release.

- automatic routing streams snapshot the complete tokenizer and detect at EOF;
- automatic editable documents re-detect after every edit;
- child stream/document objects remain valid after parent-tokenizer destruction;
- stream reset and edit-driven language transitions are conformance-tested;
- C API advanced to 0.4.0 without removing 0.3 operations.


## 0.3.0 — 2026-08-07

Stable detector-pack and automatic document-routing release.

- declarative first-byte-indexed detector packs;
- fail-soft score/margin routing;
- auto encode/token-span APIs;
- detector-aware tokenizer fingerprint;
- detector CLI and release-bundle integration;
- 14 malformed detector fixtures and sanitizer coverage.

## 0.2.0 — 2026-08-07

Stable external language-pack release.

- composable declarative language specialization;
- attach-time vocabulary-cost projection;
- order-independent pack-set fingerprinting;
- English/Hindi/Japanese reference packs;
- specialized stream/document snapshots;
- malformed-language and sanitizer qualification;
- mixed-language conformance benchmark.

## 0.1.0 — 2026-08-07

First stable native tokenizer-core release.

- exact arbitrary-byte static tokenization;
- deterministic model and Unicode 17 packs;
- Unicode grapheme spans and invalid-byte preservation;
- stable C ABI and C++-compatible header;
- integrated tokenizer facade;
- stream/full and edit/full semantic equivalence;
- ASan/UBSan and malformed-pack qualification;
- deterministic release packaging.
