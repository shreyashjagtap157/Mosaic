# Mosaic Tokenizer 0.19.0 Qualification

## Result

Qualified locally for the native Linux build.

## Gates

- full GCC strict-warning native suite: pass;
- ASan + UBSan + LeakSanitizer inherited suite: pass;
- detector auto stream/document lifetime regression: pass after child-policy fix;
- runtime policy smoke: 8 threads x 1000 encodes, exactly 8000 metric-counted calls, no failures;
- independent Clang CMake/CTest: 19/19 pass;
- Clang static analyzer: `mosaic.c` and `mosaic_cache.c` no diagnostics;
- high-level token ceilings enforced before output reconstruction/allocation;
- high-level decode byte ceiling enforced during preflight before output allocation;
- release package clean-extraction consumer: pass.

## Qualification defect fixed

`mosaic_tokenizer_document_create` initially failed to propagate `max_input_bytes` into the child document's edit ceiling. The zero-initialized ceiling rejected the first edit and caused the detector smoke process to exit before freeing owned state. LeakSanitizer caught the leaked allocations. The creation path now snapshots the parent ceiling explicitly.

## Release artifact

- archive: `mosaic-tokenizer-0.19.0-linux-x86_64.tar.gz`
- SHA-256: `6ef54c7f5a85ad7e7f2eb1f418324668ad1881cf13c3444819f0b36279c588fc`
