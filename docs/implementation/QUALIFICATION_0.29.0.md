# Mosaic 0.29.0 Qualification

Date: 2026-08-08

- inherited strict GCC/ASan/UBSan native suite after visibility-header changes: PASS
- independent Clang CMake/CTest: 24/24 PASS
- Clang static analyzer: core/cache/executor/trust PASS
- hidden-by-default core exports: 167 declared Mosaic symbols, no unexpected globals
- hidden-by-default trust exports: 9 declared Mosaic symbols, no unexpected globals
- normalized frozen declaration/type/constant contract: PASS
- stable binary-format/tokenizer-semantics contract: PASS
- 0.28 250,000-iteration reliability soak remains the inherited operational baseline

0.29 intentionally adds no canonical tokenization feature. Its purpose is to make the 1.0 promises mechanically testable before the major-version release.
