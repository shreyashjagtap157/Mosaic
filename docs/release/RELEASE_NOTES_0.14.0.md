# Mosaic Tokenizer 0.14.0

Mosaic 0.14 adds the first structured compiler/IDE projection: bounded declarative lexer packs.

## Added

- lexer pack v1 (`MSLX`) inside the canonical Mosaic pack container;
- exact lexical tokens for whitespace, newline, identifier, keyword, number, string, comment, punctuation, and explicit error spans;
- longest-prefix delimiter handling;
- optional nested block comments and `$` identifier support;
- lexer attachment to the high-level tokenizer and exact fingerprint binding;
- lexical TokenDocument density flag and snapshot semantics;
- standalone/integrated lexer CLI commands;
- reference C, Python, Rust, and JSON lexer packs;
- nine lexer-specific malformed pack fixtures;
- GCC/Clang/ASan/UBSan conformance coverage.

## Compatibility

C API version: 0.12.0. Existing 0.13 APIs remain source-compatible; lexer functionality is additive.

## Deliberate scope

This release does not add a scanner VM or parser callbacks. Lexer v1 is intentionally declarative and bounded. More expressive mechanisms must be justified by real profiles rather than designed speculatively.
