# Mosaic Tokenizer 0.13.0 Qualification

- GCC strict native build and complete regression suite pass.
- ASan/UBSan rich TokenDocument projection test passes.
- Clang CMake/CTest: 13/13 tests pass.
- Clang static analyzer reports no warnings after explicit normalization failure-path ownership hardening.
- Existing TokenDocument model/grapheme semantics remain unchanged.
- Security + normalization projections are opt-in and immutable.
- Capability discovery reflects optional packs actually attached to the tokenizer.
