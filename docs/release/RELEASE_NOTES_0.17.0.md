# Mosaic Tokenizer 0.17.0

Enterprise content-cache release.

- Adds thread-safe bounded in-memory LRU caching keyed by processing-block content identity plus projection namespace/schema.
- Adds explicit entry, aggregate-byte, and per-value resource ceilings.
- Adds cache metrics for hits, misses, puts, replacements, evictions, removes, clears, live usage, and peak usage.
- Adds `MOSAIC_ERROR_NOT_FOUND` for cache misses.
- Starts native-runtime modularization with `mosaic_cache.c` as an independent translation unit.
- Extends GCC/ASan/UBSan and independent Clang/CTest qualification with concurrent cache stress.

Canonical tokenization semantics are unchanged.
