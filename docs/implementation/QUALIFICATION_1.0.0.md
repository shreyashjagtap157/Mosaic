# Mosaic 1.0.0 Qualification

Date: 2026-08-08

## Locally executed release gates

- native C ABI metadata: 1.0.0 PASS;
- optional trust ABI metadata: 1.0.0 PASS;
- canonical tokenizer semantics remains version 2 PASS;
- frozen public declaration/type/constant/export contract: PASS;
- stable binary-format/registry/semantics contract: PASS;
- strict GCC native suite: PASS;
- GCC ASan/UBSan inherited suite: PASS;
- independent Clang CMake/CTest: 24/24 PASS;
- Clang static analyzer core/cache/executor/trust: PASS;
- Python binding source tests: 4/4 PASS;
- 250,000-iteration deterministic reliability soak repeated twice: PASS;
- soak replay digest: `8bec191c6e5dd904`, identical to pre-1.0 baseline;
- soak included 10,870 online-stream, 3,522 incremental, 1,909 cold-document, 498 lifecycle, and 200 executor-batch checks per run;
- registry concurrent install/corruption/exact repair/lock/audit/GC chaos: PASS;
- source structural validators: PASS;
- release archive and Python wheel reproducibility: finalized by clean release build below;
- SBOM/provenance/checksum and clean-extraction external-consumer validation: finalized by clean release build below.

## Explicit external gates not executable in this sandbox

- Windows native CMake/CTest execution;
- macOS native CMake/CTest execution;
- stable Rust workspace build/clippy/tests;
- Miri;
- cargo-fuzz campaigns;
- ThreadSanitizer/platform race detector where supported;
- additional architecture/hardware runners not present locally.

These gates remain declared in CI. They are not represented as passed. The packaged 1.0 production implementation is the locally qualified native C runtime; Rust remains an independent conformance/reference implementation until its CI executes.
