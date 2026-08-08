# Compatibility Profiles — Mosaic Tokenizer 0.6

## Raw tiktoken BPE

0.6 adds model algorithm `raw-bpe` and deterministic `.tiktoken` mergeable-rank import/export.

`mosaic-author import-tiktoken INPUT.tiktoken OUTPUT.mpack` preserves every mergeable byte surface and rank/token ID. Mosaic then applies the standard raw-byte BPE merge rule to a *single byte piece*. `export-tiktoken` is allowed only when model token IDs equal BPE ranks, so the export cannot silently invent a different external encoding.

The public native runtime validates that:

- every byte value has at least one one-byte fallback surface;
- BPE surfaces are unique;
- BPE ranks are non-negative and unique;
- unsupported algorithm codes fail closed;
- source decode remains exact for arbitrary bytes.

## Exactness scope

The 0.6 compatibility claim is deliberately **raw/single-piece BPE**, equivalent to tiktoken's single-piece BPE operation when given the same mergeable ranks. Full `Encoding.encode_ordinary` also applies a Unicode regex pre-tokenizer before BPE; that pre-tokenizer is not silently approximated in 0.6.

This distinction matters because tiktoken itself defines an encoding from the regex `pat_str`, mergeable ranks, and special-token mapping, rather than the rank file alone.

## Performance caveat

The first raw-BPE executor is optimized for correctness and bounded vocabulary lookup, not giant unsplit documents. A 1 MiB adversarial single piece can require tens of MiB of transient merge state. Normal tiktoken-style operation splits text before BPE. Future bounded streaming/pre-tokenizer work must preserve the exact 0.6 raw-BPE result before replacing this implementation.
