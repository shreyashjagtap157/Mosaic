# Mosaic Content Cache v1

Mosaic 0.17 introduces a thread-safe, bounded in-memory LRU content cache for enterprise-scale reuse of immutable processing-block projections.

## Identity

`mosaic_processing_block_cache_key` hashes the domain `MOSAIC-CACHE-KEY-v1`, an already content-addressed processing-block identity, projection namespace, and projection schema version. Block ordinals are deliberately excluded so unchanged content remains reusable after movement.

## Resource policy

The cache is configured with explicit maximum entry count, aggregate payload bytes, and per-value bytes. Defaults are 4,096 entries, 256 MiB aggregate payload, and 16 MiB per value. Values larger than the configured per-value or aggregate ceiling fail with `MOSAIC_ERROR_RESOURCE_LIMIT`; the implementation never silently exceeds policy.

## Concurrency and ownership

All operations are serialized with a C11 mutex. `put` and `get` copy payloads so caller lifetimes never cross the cache boundary. `get` returns caller-owned memory released by `mosaic_free`. The cache may be accessed concurrently from multiple threads; `mosaic_cache_free` requires the caller to have stopped all concurrent users.

## Eviction and metrics

LRU eviction runs after insertion/replacement until both entry and byte limits hold. Metrics distinguish hits, misses, puts, replacements, capacity evictions, explicit removes, cache clears, cleared entries, live entries/bytes, and peak payload bytes.

## Backend boundary

This cache is a process-local hot cache, not the persistent enterprise cache. Future persistent/distributed backends consume the same 32-byte content keys and must verify serialized values before returning them. Cache hits are optimizations only and never change canonical tokenization semantics.
