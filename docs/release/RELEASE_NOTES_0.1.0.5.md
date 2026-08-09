# Mosaic 0.1.0.5

Mosaic 0.1.0.5 is a compatibility-preserving performance/correctness remediation candidate. It addresses the execution bottlenecks found during controlled comparison of the 0.1.0.4 native engine while preserving the frozen C/trust ABIs, tokenizer semantics, source mappings, and pack formats.

## Weighted Viterbi

The production weighted tokenizer now computes optimal paths backward from the end of the source. With suffix state already fixed, equal-cost/equal-token-count ties are resolved by the first token locally, preserving the canonical lexicographic total order without reconstructing complete prefix paths or allocating per comparison. The intentionally simple reference/path-order implementations remain independent correctness oracles.

Validated packs are also compiled into an immutable native-endian runtime vocabulary index after structural validation, removing repeated packed-field decoding from hot loops.

## Raw BPE

The raw-BPE path now uses direct byte and two-byte pair indices, hashed merged-surface lookup, compact node state/generation tracking, and exact segmentation at vocabulary-impossible byte-pair boundaries. A boundary is split only when no vocabulary surface can contain that adjacent byte pair, so no legal final token can cross it. Direct token reconstruction removes an intermediate output-index allocation.

On the same virtualized Linux host and 10 MiB stress input used for the pre-remediation comparison, elapsed time changed from 12.25 s to 0.89 s and peak RSS from 680,768 KiB to 52,872 KiB while token count and total cost remained identical (6,183,905 tokens; cost 896,128,726). These are internal qualification measurements, not a cross-product public performance claim.

## Streaming

Online Viterbi retains explicit forward prefix-tree proof state because safe prefix commitment requires lowest-common-prefix information across frontier paths. Ring traversal now uses explicit circular indices instead of hot-loop modulo division, stores winning depth during DP, and avoids duplicate output-count traversals.

The 10 MiB bounded-processing gate improved from 11.8 MiB/s on the 0.1.0.4 baseline to 33.3 MiB/s in the remediated tree while retaining `max_pending=54` bytes.

## Incremental and resynchronizing documents

Near-end incremental editing now tokenizes and commits only the restart suffix. Existing source/token allocations are retained when capacity permits, and mutation is committed only after suffix tokenization and capacity reservation succeed. The same 10 MiB benchmark improved from 0.735682 s to 0.035444 s incremental update time while still reprocessing approximately 1.000% of the source.

Checkpoint resynchronization now materializes only the changed token interval and commits it into retained token storage with overlap-safe suffix movement. On the same middle-edit benchmark, update time changed from 0.280519 s to 0.058445 s, speedup versus full retokenization rose from 4.23x to 11.67x, reprocessed bytes remained 0.625%, and observed peak RSS fell from 154.3 MiB to 136.1 MiB.

## Compatibility

- Product candidate: 0.1.0.5.
- Native C ABI: 1.0.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and frozen binary-format contracts: unchanged.

This candidate remains in stability generation 0. Windows qualification of the changed native core and the remaining stable-generation platform/Rust/fuzz/race-detector gates are still required before promotion to a stable generation.
