# Mosaic Tokenizer 0.17.0 Qualification

Release qualification requires:

- complete inherited native GCC test suite;
- ASan/UBSan suite;
- 8-thread cache stress with 40,000 immediate put/get validations;
- bounded LRU eviction/replacement/remove/clear tests;
- independent Clang CMake/CTest matrix;
- Clang static analysis for the cache module and native runtime;
- deterministic release packaging and clean-extraction consumer validation.

The cache is non-semantic: misses, evictions, and clears may affect latency only.
