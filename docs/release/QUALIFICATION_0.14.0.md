# Mosaic Tokenizer 0.14.0 Qualification

Release qualification requires all inherited 0.13 gates plus lexer-specific gates.

## Lexer gates

- deterministic reference lexer pack rebuilds;
- C/Python/Rust/JSON profile smoke tests;
- exact source partition for every lexical result;
- longest-prefix delimiter regression (Python triple quotes);
- nested-comment regression (Rust);
- explicit unterminated-string error span (JSON);
- tokenizer fingerprint changes when lexer identity changes;
- lexical TokenDocument remains usable after parent tokenizer destruction;
- nine malformed lexer payload classes fail closed;
- standalone and integrated lexer CLI smoke;
- packaged static external consumer can request lexical TokenDocument output.

## Inherited gates

- exact arbitrary-byte model round trips;
- Unicode conformance and normalization differential;
- security evidence;
- language/detector behavior;
- bounded online streaming;
- incremental and checkpoint-resynchronizing documents;
- rich TokenDocument projections;
- GCC strict warnings and ASan/UBSan;
- Clang CMake/CTest;
- clean-extraction package validation.
