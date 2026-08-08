# Exact Online Viterbi Streaming in Mosaic 0.9

## Contract

`mosaic_online_stream` incrementally accepts source bytes for deterministic Viterbi model packs and emits only token IDs that are already provably identical to the corresponding prefix of complete-buffer tokenization. The implementation bounds unresolved source bytes by a caller-provided `max_pending_bytes`.

Raw-BPE compatibility packs deliberately return `MOSAIC_ERROR_UNSUPPORTED`; their merge-rank semantics require a separate online proof and are not approximated with Viterbi behavior.

## Why fixed look-behind is insufficient

A future token can begin within the last `M-1` bytes, where `M` is maximum token surface length, but the best path to those frontier positions can have different earlier token boundaries. Committing merely `N-(M-1)` bytes can therefore change canonical Viterbi segmentation.

## Survivor-prefix rule

For the unresolved buffer of length `N`:

1. Run the same canonical integer Viterbi implementation used by complete-buffer tokenization.
2. Let the future-sensitive frontier contain byte boundaries `[N-(M-1), N]`, clamped at zero.
3. The canonical predecessor of every byte boundary forms a backpointer tree rooted at byte boundary 0.
4. Compute the deepest common ancestor of the canonical paths to every frontier boundary.
5. Only the token path from 0 to that ancestor is committed and emitted.
6. Remove exactly the committed source bytes and repeat on later input.
7. At EOF, commit the complete remaining canonical path.

Any possible future token that crosses the current right edge must begin within the frontier. Therefore any future complete canonical path must extend one of those canonical frontier prefixes. Their deepest common ancestor is the longest token-boundary prefix shared by every such continuation.

The existing buffered `mosaic_stream` remains the semantic oracle.

## Resource limit

If no prefix can be proven while pending bytes reach `max_pending_bytes`, `mosaic_online_stream_push` returns `MOSAIC_ERROR_RESOURCE_LIMIT`. `out_consumed` reports exactly how many caller bytes were accepted and `out_ids` may contain canonical prefixes committed before the limit. The runtime never exceeds the source-byte ceiling and never guesses a token boundary.

The deterministic adversarial fixture `online-adversarial-v1.mpack` forces this path and is part of release qualification.

## 0.9 qualification

- 500 randomized arbitrary-byte streams with randomized chunk boundaries are differential-tested against one-shot IDs.
- Explicit language cost-specialization is snapshot-tested.
- Raw-BPE rejection is tested.
- Resource exhaustion is tested with a three-byte pending cap.
- The 10 MiB bounded-processing gate requires at least 20 MiB/s, no more than 64 MiB RSS in the benchmark harness, and strict adherence to the configured pending-byte cap.
