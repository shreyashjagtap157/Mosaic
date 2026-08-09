# Mosaic Tokenizer 0.2.0

0.2.0 extends the stable byte/Unicode/model tokenizer with externally attachable language-specialization packs.

## Added

- declarative `MSLG` language-pack section format;
- high-level APIs to attach language packs from memory or files;
- language tag enumeration and duplicate-tag conflict detection;
- order-independent fingerprinting of exact loaded language packs;
- attach-time projection of language cost adjustments onto the model vocabulary;
- language-aware streams and editable-document snapshots;
- English, Hindi, and Japanese reference/conformance packs;
- mixed-language CLI commands `fingerprint-languages`, `analyze-languages`, and `roundtrip-languages`;
- ten malformed language-pack regression fixtures;
- language-pack ASan/UBSan, CMake, Python/FFI, load-order, and performance tests.

## Controlled conformance result

On the included synthetic/reference mixed-language corpus, the three reference packs reduce emitted model tokens by about 27% relative to the v2 base model. This is a mechanism/conformance benchmark, **not** a claim about production multilingual quality or real LLM throughput.

## Important design property

Language packs do not create new token IDs. They specialize costs for surfaces already present in the loaded model vocabulary. Removing every language pack therefore leaves exact byte representability unchanged and avoids silently inventing embeddings for fixed-vocabulary models.

## Still outside this release

- automatic language detection/routing;
- production-scale trained language packs/vocabulary;
- bounded-memory streaming;
- local incremental retokenization;
- compiler/search/IDE/security projections;
- native scanner VM;
- SIMD vocabulary matcher;
- post-M4 product branch.
