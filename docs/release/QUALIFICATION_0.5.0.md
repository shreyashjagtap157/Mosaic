# Qualification — Mosaic Tokenizer 0.5.0

0.5 inherits every 0.4 execution qualification gate and adds:

- Python syntax/CLI validation for `mosaic-author`;
- deterministic explicit model compilation;
- mandatory byte-fallback validation through the native runtime;
- deterministic corpus training with reversed corpus argument order producing byte-identical packs;
- exact arbitrary-byte round-trip using an authored model;
- explicit language-pack compilation and native attachment;
- detector-pack compilation and integrated auto routing;
- pack inspection/canonical-hash validation;
- candidate-map resource-bound failure policy;
- clean-extraction release test that uses the packaged authoring tool to train a model and the packaged native CLI to round-trip it.

## Local qualified result

- GCC/ASan/UBSan native suite: PASS;
- independent Clang shared-library ABI differential: PASS;
- Unicode 17 20,000-case property-heavy oracle: PASS;
- deterministic authoring/runtime integration: PASS;
- 10 MiB base round-trip median throughput: **58.8 MiB/s**;
- peak RSS: **51.6 MiB**;
- clean-extraction package authoring/runtime smoke: PASS;
- Clang static analyzer on native runtime: PASS after explicit backpointer-sentinel hardening;
- Linux x86-64 archive SHA-256: `5e513993e9fbe79abbfbed21e8a54823f350a9c38dc74f87f74d5232673868a0`.
