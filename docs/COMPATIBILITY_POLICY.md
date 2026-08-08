# Mosaic Compatibility Policy

Status: frozen-contract policy for the current `0.1.x.x` candidate line and the future `1.x.x.x` stable generation.

Product release numbering follows `docs/VERSIONING_POLICY.md`. The current product remains pre-stable until the stable-generation qualification gate is satisfied. This does **not** unfreeze contracts that were deliberately frozen during the enterprise candidate work.

## Compatibility dimensions

Mosaic versions several independent contracts. A product release number alone is not a semantic identity.

1. **Canonical tokenizer semantics**: identical source bytes, exact packs and options produce the same canonical projection while the tokenizer-semantics version is unchanged.
2. **C ABI**: functions, public enums/structures/constants, ownership rules and status meanings exposed by `mosaic.h`.
3. **Trust ABI**: the optional `mosaic_trust.h` surface.
4. **Pack container**: MOSPACK v1 structural container.
5. **Cold TokenDocument**: `MSTIRD01` version 1.
6. **Packed model projection**: `MSTKPK01` version 1.
7. **Cache record**: `MSCACHR1` version 1.
8. **Detached pack signature**: `MSSIGV01` version 1.
9. **Registry schema/lock contract**: schema 1 and exact-hash canonical lock identity.

## Frozen candidate guarantees

For the current `0.1.x.x` candidate line, and unless an explicitly documented compatibility/security exception is invoked:

- frozen public functions are not removed or signature-changed;
- frozen public structs/enums/constants are not layout- or value-changed;
- additive functions/types may be introduced only through an intentional product minor/major release, not a patch;
- canonical tokenizer output does not change without a tokenizer-semantics-version change and an explicitly documented migration;
- a reader continues to accept every frozen record/pack profile it previously accepted unless rejecting it is required to fix a security vulnerability;
- exact content hashes remain the reproducibility identity; mutable registry aliases never become canonical identities;
- Python bindings remain a thin wrapper over the same native semantics.

When Mosaic graduates to `1.0.0.0`, these frozen guarantees become the stable-generation baseline unless the graduation release explicitly documents a stricter contract. Graduation itself does not require an ABI/format reset.

## What is not guaranteed

- identical performance across releases or hardware;
- byte-identical internal memory layout of opaque handles;
- deterministic wall-clock timing or thread scheduling;
- acceptance of malformed/noncanonical records previously accepted accidentally;
- stability of experimental APIs explicitly documented outside the frozen headers.

## Additive evolution

New APIs are allowed only when they do not mutate a frozen declaration. Public configuration structures intended for future extension must use an existing `struct_size`/version discipline where provided. A new required field may not be inserted into a frozen public structure in a patch release.

## Security exceptions

Security may require refusing a previously accepted pack/record/key/profile. Such a change must:

1. preserve source exactness and memory safety;
2. receive a security advisory identifier;
3. document the affected contract and versions;
4. provide an upgrade/re-authoring path when feasible;
5. never silently reinterpret the same accepted bytes under the same semantic identity.

## Machine enforcement

`abi/stable-contract-v1.json` and `tools/abi_contract.py` enforce the frozen C declarations/types/constants/exports. `abi/format-contract-v1.json` and `tools/validate_format_contract.py` enforce stable binary-format/semantics identifiers. CI treats drift as release-blocking and requires an explicit compatibility process.
