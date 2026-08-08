# Mosaic C API 0.5 — Tokenizer 0.7

The 0.7 release keeps all earlier C ABI entry points and adds Unicode-17 security/script evidence. The C API version is `0.5.0`; this is a backward-compatible surface extension.

## Ownership

All opaque handles are owned by Mosaic and released with their matching `*_free` function. Buffers returned by Mosaic are released with `mosaic_free()`.

## Security pack

- `mosaic_security_load_memory` / `mosaic_security_load_file`
- `mosaic_security_script_ranges`
- `mosaic_security_script_name`
- `mosaic_security_scan`
- `mosaic_security_free`

Script ranges always use original byte coordinates. Invalid UTF-8 is preserved as opaque script ID `0` and is never converted to replacement characters.

Findings are evidence only:

- `MOSAIC_SECURITY_BIDI_CONTROL`
- `MOSAIC_SECURITY_DEFAULT_IGNORABLE`
- `MOSAIC_SECURITY_NONCHARACTER`
- `MOSAIC_SECURITY_DEPRECATED`
- `MOSAIC_SECURITY_MIXED_SCRIPT`

Mixed-script detection counts significant Script values only. `Common`, `Inherited`, and `Unknown` do not create a mixed-script condition. The most frequent significant script is primary, with stable script ID as a deterministic tie-break; source spans in other significant scripts are reported.

## Integrated tokenizer

- `mosaic_tokenizer_set_security_memory` / `mosaic_tokenizer_set_security_file`
- `mosaic_tokenizer_security_loaded`
- `mosaic_tokenizer_security_scan`

A loaded security pack participates in the tokenizer fingerprint through a typed `SECURITY` domain plus the exact pack hash. Tokenizers without a security pack retain their 0.6 semantics-v2 fingerprints.
