# Mosaic Tokenizer 0.27.1

Mosaic 0.27.1 is a conformance portability patch. It removes the remaining direct C11 `<threads.h>` dependency from concurrency tests and exercises the same private Win32/pthread shim as the runtime. No tokenizer semantics, pack format, TokenDocument format, or C ABI changes are introduced.
