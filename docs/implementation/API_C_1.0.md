# Mosaic C API 1.0.0

The native C ABI is independently versioned from the Mosaic product release. The current product candidate `0.1.0.5` continues to expose frozen C ABI `1.0.0`.

The authoritative public surface is `native/include/mosaic.h`; the machine-readable frozen declaration/type/constant/export baseline is `abi/stable-contract-v1.json` and is validated by `tools/abi_contract.py`.

A product patch does not imply a C ABI patch. The C ABI version changes only through the compatibility process described in `docs/COMPATIBILITY_POLICY.md`.

Canonical tokenizer semantics are independently versioned and currently remain version `2`.
