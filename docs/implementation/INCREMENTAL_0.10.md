# Exact Incremental Viterbi Documents in Mosaic 0.10

## Scope

`mosaic_incremental_document` caches the canonical weighted-Viterbi token sequence and reuses a provably unaffected source/token prefix after edits. It is an additive API. The older `mosaic_document` remains the full-recompute semantic oracle.

Raw-BPE packs return `MOSAIC_ERROR_UNSUPPORTED`; their merge semantics need a separate restart/resynchronization proof.

## Safe restart proof

Let `M` be the maximum vocabulary surface length and let an edit first change source byte `E`.

1. Source bytes strictly before `E` are unchanged.
2. Any token whose bytes can be affected by the edit must start after `E-M`.
3. Select the greatest cached canonical token boundary `R <= max(0, E-(M-1))`.
4. No token affected by the edit can cross `R`.
5. The canonical Viterbi path to every unchanged byte boundary before the edit is independent of future source, so the old canonical prefix ending at `R` remains canonical.
6. Every suffix candidate after `R` shares that same prefix. Prefix cost/count and lexicographic path are therefore common constants, so retokenizing the edited suffix from `R` yields the same canonical suffix as complete-buffer processing.

The engine preserves cached tokens ending at `R`, retokenizes `[R, EOF)`, adjusts suffix byte offsets, and transactionally swaps the new source/cache only after successful tokenization.

## Guarantees

- exact IDs equal complete-buffer tokenization after every successful edit;
- exact source bytes remain copyable;
- failed edits do not partially update the live document;
- `last_reprocessed_bytes` and `last_reused_prefix_bytes` expose actual work locality;
- explicit language specialization snapshots are supported;
- raw-BPE is rejected, not approximated.

## Current limitation

0.10 does not yet perform forward checkpoint resynchronization. An edit at byte `E` therefore generally reprocesses from a safe boundary before `E` to EOF. Uniformly distributed edits naturally average about half the document. This is still genuine prefix reuse, but the later multiscale/checkpoint engine is required before Mosaic claims bounded local reprocessing for middle-of-document edits.
