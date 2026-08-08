# Mosaic 0.29 ABI and Format Freeze

0.29 is the release-candidate contract phase. It intentionally reduces feature velocity.

## Frozen candidate surface

- 167 public core C functions;
- 9 optional trust C functions;
- normalized signatures of all frozen functions;
- all currently public non-opaque struct/enum definitions and stable numeric constants;
- MOSPACK v1;
- `MSTIRD01` TokenDocument v1;
- `MSTKPK01` packed model v1;
- `MSCACHR1` cache record v1;
- `MSSIGV01` Ed25519 signature record v1;
- registry schema 1;
- tokenizer canonical semantics version 2.

The shared libraries now use explicit API visibility declarations and hidden-by-default compilation rather than Windows auto-export of arbitrary globals.

## Enforcement

- `tools/abi_contract.py`: frozen declaration/type/constant + shared export checks;
- `tools/validate_format_contract.py`: stable binary format and semantic identifiers;
- `abi/stable-contract-v1.json`: additive ABI baseline;
- `abi/format-contract-v1.json`: binary-format baseline.

Extra APIs in a future 1.x minor release are allowed only additively; mutation/removal of a frozen item fails validation.
