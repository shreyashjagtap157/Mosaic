# Mosaic Tokenizer 0.1.0

Status: stable native tokenizer core release

This release freezes the first usable Mosaic tokenizer core. It is intentionally narrower than the eventual universal token-processing platform: it proves exact arbitrary-byte tokenization, deterministic static vocabulary segmentation, pinned Unicode 17 grapheme views, validated deterministic packs, stable C ABI, streaming-at-EOF equivalence, and editable-document semantic equivalence.

## Included

- exact arbitrary-byte encode/decode with mandatory 256-byte fallback;
- deterministic integer-cost Viterbi segmentation;
- token IDs plus exact byte spans and costs;
- Unicode 17 extended grapheme cluster spans;
- invalid UTF-8 preserved as opaque one-byte units rather than replaced;
- validated deterministic model and Unicode packs;
- integrated `mosaic_tokenizer` handle and deterministic runtime fingerprint;
- stable C 0.1 ABI with static/shared libraries;
- C++-compatible header;
- CLI for validation, encoding, binary ID serialization, decoding, grapheme inspection, integrated analysis, and fingerprinting;
- streaming API whose 0.1 implementation buffers until EOF but is semantically identical to one-shot tokenization;
- editable-document API whose 0.1 implementation retokenizes fully after edits but is semantically identical to fresh tokenization.

## Explicitly not claimed complete

- bounded-memory streaming;
- local incremental retokenization;
- script/language/locale/domain pack composition;
- compiler/IDE/search projections;
- scanner VM;
- SIMD token matching;
- full rich Token IR;
- Rust production-runtime qualification;
- Wedge Tournament / post-M4 product selection.

Those remain platform milestones and must preserve 0.1 canonical semantics where applicable.

## Release qualification

The native runtime is compiled with both GCC and Clang. The qualification suite includes deterministic pack regeneration, malformed-pack rejection, Python/reference differentials, Unicode 17 property-heavy grapheme comparison, ASan + UBSan, static/shared C clients, C++ header compatibility, randomized stream/full equality, randomized edit/full equality, and a 10 MiB throughput/RSS regression gate.

The Rust workspace remains source-only on the build host because `rustc`/`cargo` are unavailable and outbound DNS is blocked. It is therefore not falsely represented as a qualified release runtime.
