# Mosaic 1.0.0

Mosaic 1.0.0 is the first stable enterprise release of the universal tokenization and token-native processing platform.

## Stable contracts

- native C ABI 1.0.0;
- optional trust ABI 1.0.0;
- canonical tokenizer semantics version 2;
- MOSPACK v1;
- TokenDocument `MSTIRD01` v1;
- packed-model `MSTKPK01` v1;
- cache-record `MSCACHR1` v1;
- signature-record `MSSIGV01` v1;
- registry schema 1 and exact-hash canonical lock identity.

The 1.x compatibility/deprecation policy now applies. Existing frozen declarations and binary records cannot be silently mutated in a 1.x release.

## Enterprise baseline

1.0 includes exact arbitrary-byte tokenization, Unicode/source mapping, deterministic packs and authoring, language specialization/routing, streaming and incremental processing, TokenDocument projections and persistence, declarative lexical/semantic views, multiscale storage, bounded caches and batch execution, trust/registry control plane, runtime resource policy, observability, Python binding, reproducible release supply chain, and deterministic reliability/chaos qualification.

## Qualification note

The production artifact is the native C runtime. Linux GCC/Clang, sanitizer, static-analysis, differential, reliability, package and reproducibility gates are executed locally. Windows/macOS remain explicit external gates, while Rust Miri and bounded fuzz smoke are now locally exercised through `tools/validate_miri.py` and `tools/validate_fuzz.py`. Full cross-platform Rust qualification still depends on external CI runners.
