# Checkpoint Resynchronization 0.11

Mosaic 0.11 adds an exact forward-resynchronizing incremental document for weighted-Viterbi model packs.

## State and proof boundary

The online Viterbi executor repeatedly commits only the token prefix common to every viable frontier path. After such a commit its future canonical behavior is fully determined by the immutable model/adjustments plus the unresolved pending source bytes. A checkpoint therefore records the consumed byte coordinate, committed byte coordinate, committed token count, and an exact copy of the pending bytes.

After an edit Mosaic restores the last checkpoint before the edit, runs the edited source forward, and compares the new pending state at shifted old checkpoint coordinates. When the pending bytes match, future Viterbi execution state is identical; Mosaic can reuse the old canonical suffix shifted by the edit delta. If no state match occurs it falls back to exact processing through EOF.

The API never guesses a boundary. Raw-BPE packs return `MOSAIC_ERROR_UNSUPPORTED` because merge-rank BPE has different restart semantics.

## Representation

The resync cache stores an internal 8-byte `{u32 token_id, u16 byte_length, u16 reserved}` record. Token starts are implicit prefix sums and costs remain in the immutable vocabulary. This replaced the initial 24-byte public-token cache discovered during the 10 MiB benchmark.

## Qualification

- 500 deterministic randomized insert/delete/replace edits compare the complete resync ID stream with fresh full tokenization after every edit.
- ASan/UBSan exercises the same transaction path with a reduced local stress count; release/CI can raise `MOSAIC_RESYNC_EDITS`.
- Explicit language specialization is covered.
- A deterministic middle edit must resynchronize and materially reuse both source sides.
- The 10 MiB benchmark requires <2% source reprocessing and >=3x speedup against fresh full tokenization.

Reference-host result at release candidate: 65,575 / 10,485,760 bytes reprocessed (0.625%), about 8.1x update speedup. The benchmark's peak RSS includes the live resync document, a simultaneous full-tokenization oracle, and both ID arrays; it is not persistent checkpoint-state size.
