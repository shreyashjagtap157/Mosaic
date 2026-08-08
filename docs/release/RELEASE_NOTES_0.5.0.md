# Mosaic Tokenizer 0.5.0

Mosaic Tokenizer 0.5.0 adds a supported deterministic pack-authoring and baseline training tool to the stable 0.4 execution runtime.

## Added

- `mosaic-author model` for explicit byte/UTF-8 vocabulary compilation;
- `mosaic-author train-model` for deterministic compression-first corpus training;
- automatic mandatory 256-byte fallback in every authored model;
- stable deterministic token-ID assignment for pieces without explicit IDs;
- corpus-order-independent training output;
- configurable vocabulary size, maximum piece bytes, minimum frequency, byte cost, and candidate-map safety limit;
- optional machine-readable training report;
- `mosaic-author language` for external specialization packs;
- `mosaic-author detector` for fail-soft detector packs;
- `mosaic-author inspect` for MOSPACK container metadata;
- bundled authoring examples and documentation;
- release-package self-test that trains a model after clean extraction and loads it through the native runtime.

## Training scope

The 0.5 trainer is a deterministic compression-first baseline, not the future constrained-Unigram research trainer. It exists so Mosaic is independently usable and reproducible today. Quality or LLM claims require controlled downstream evaluation.

## Runtime compatibility

The native C API remains API 0.4.0. Runtime/release version advances to 0.5.0.
