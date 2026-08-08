# Implementation Status

Date: 2026-08-08

## Current release

**Mosaic Tokenizer 0.8.0: qualified mapped-normalization release.**

The stable native tokenizer now provides exact arbitrary-byte model tokenization, deterministic Viterbi and raw-BPE execution, Unicode-17 grapheme views, composable language packs, fail-soft document routing, deterministic authoring/training tools, raw `.tiktoken` rank interchange, a separately versioned Unicode-17 script/security evidence pack, and a separately pinned Unicode-16 normalization shadow-view pack. Security and normalization evidence never mutate source bytes or model IDs.

## Architectural direction audit

The implementation remains aligned with the converged design:

- source bytes remain authoritative and every byte remains representable without unknown tokens;
- canonical runtime decisions use deterministic integer semantics;
- Unicode/language/security/normalization information is derived and byte-mapped;
- language packs specialize costs over existing model surfaces and cannot add hidden model IDs;
- automatic detection is advisory and fail-soft;
- pack generation is offline while the runtime only validates/executes packs;
- tokenizer fingerprints bind semantic runtime identity and exact pack hashes;
- optimized paths remain differential-tested against independent/reference behavior.

The rejected 0.2 per-candidate language-pack lookup and rejected 0.7 per-byte security scratch-array designs are retained in release evidence as examples of performance gates changing implementation choices before release.

## Milestone state

- **M0 engineering substrate:** implemented.
- **M1 exact byte/source semantics:** native implementation complete; Rust qualification pending external Rust-capable CI.
- **M2 deterministic pack executor:** implemented and independently exercised natively.
- **M3 Unicode/static tokenizer substrate:** stable native implementation with weighted Viterbi and raw-BPE compatibility profile.
- **Pack specialization:** external language and detector packs implemented.
- **Authoring:** deterministic model/language/detector authoring and baseline corpus training implemented.
- **Unicode security evidence:** Unicode-17 script/property pack implemented.
- **Mapped normalization:** Unicode-16 NFD/NFC/NFKD/NFKC/NFKC-casefold pack and exact source provenance implemented and qualified.
- **M4 Wedge Tournament:** not yet run and not silently bypassed.

## Remaining tokenizer work before the broader platform

1. richer existing-tokenizer compatibility profiles including pre-tokenization semantics;
2. bounded-memory streaming while preserving exact EOF equality;
3. genuine local incremental retokenization equivalent to full processing;
4. callback/visitor security evidence for bounded output memory;
5. production-scale vocabulary/language/detector training and quality evaluation;
6. Wedge Tournament after the common tokenizer benchmark substrate is representative.

After that, the broader Mosaic platform still requires TokenDocument/Token IR, consumer projections/capability negotiation, structured/compiler profiles, semantic enrichment, sub-byte views, multiscale blocks, caching/packed serialization, trust/signature/registry infrastructure, cross-platform qualification, bindings, reliability campaigns, and a 1.0 compatibility freeze.

## Environment limitations

This host still lacks `rustc`, `cargo`, GitHub CLI, and outbound DNS. Native GCC/Clang builds are locally qualified. The configured Git remote remains `https://github.com/shreyashjagtap157/Mosaic.git`, but pushes cannot succeed from this sandbox.
