# Integration Audit — Mosaic Tokenizer 0.4.0

Date: 2026-08-08

## Result

The tokenizer is integrated as one native system through the `mosaic_tokenizer` facade. Model, Unicode, language, detector, one-shot, stream-at-EOF, editable-document, CLI, static/shared library, Make/CMake, deterministic pack, and release-package paths use the same runtime semantics.

## Direction check

The implementation remains aligned with the converged design:

- source truth and offsets are bytes;
- mandatory byte fallback makes every byte sequence representable;
- Unicode 17 graphemes are a derived view and malformed UTF-8 remains exact;
- language packs only alter costs for model surfaces already present in the model vocabulary;
- detector routing is advisory and fails to the base model on ambiguity/unavailable specialization;
- auto streams/documents snapshot exact pack configuration and are independent of parent lifetime;
- editable auto documents re-detect from current bytes after edits;
- deterministic integer Viterbi remains the canonical segmentation path;
- no ordinary pack contains native executable code.

## Deliberately unoptimized reference behavior

0.4 stream sessions may buffer the full source until EOF. Editable documents may retokenize fully after edits. Both are intentional semantic baselines. Bounded-memory/local incremental replacements are allowed only behind exact differential equivalence.

## Integration defects found/fixed before this release

1. Auto one-shot routing existed while streams/documents did not carry auto semantics. Added explicit auto constructors/finish/encode operations.
2. Child objects could not simply borrow parent pack state because the C ABI cannot enforce parent lifetimes. Child objects now own a validated tokenizer snapshot.
3. Release tooling still encoded C API 0.3.0. Package metadata and ABI validators now bind 0.4.0.

## Stable boundary

0.4 is a complete stable execution runtime for arbitrary-byte fixed-vocabulary tokenization with Unicode views, explicit language specialization, fail-soft document routing, stream-at-EOF equivalence, and editable-document equivalence. Pack authoring/training is the next usability layer and is intentionally not counted as part of this release.
