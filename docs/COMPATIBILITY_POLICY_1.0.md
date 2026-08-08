# Mosaic 1.x Compatibility Policy

Status: 1.0 release-candidate contract. This document becomes binding for the 1.x stable line when v1.0.0 is tagged.

## Compatibility dimensions

Mosaic versions several independent contracts. A release number alone is not a semantic identity.

1. **Canonical tokenizer semantics**: identical source bytes, exact packs and options produce the same canonical projection while the tokenizer-semantics version is unchanged.
2. **C ABI**: functions, public enums/structures/constants, ownership rules and status meanings exposed by `mosaic.h`.
3. **Trust ABI**: the optional `mosaic_trust.h` surface.
4. **Pack container**: MOSPACK v1 structural container.
5. **Cold TokenDocument**: `MSTIRD01` version 1.
6. **Packed model projection**: `MSTKPK01` version 1.
7. **Cache record**: `MSCACHR1` version 1.
8. **Detached pack signature**: `MSSIGV01` version 1.
9. **Registry schema/lock contract**: schema 1 and exact-hash canonical lock identity.

## 1.x guarantees

Within major version 1:

- frozen public functions are not removed or signature-changed;
- frozen public structs/enums/constants are not layout- or value-changed;
- additive functions/types may be introduced in minor releases;
- canonical tokenizer output does not change without a tokenizer-semantics-version change and an explicitly documented migration;
- a reader continues to accept every stable v1 record/pack profile it previously accepted unless rejecting it is required to fix a security vulnerability; such exceptions require a security advisory and migration path;
- exact content hashes remain the reproducibility identity; mutable registry aliases never become canonical identities;
- Python bindings remain a thin wrapper over the same native semantics and follow normal Python semantic-versioning compatibility within the 1.x release line.

## What is not guaranteed

- identical performance across releases or hardware;
- byte-identical internal memory layout of opaque handles;
- deterministic wall-clock timing or thread scheduling;
- acceptance of malformed/noncanonical records previously accepted accidentally;
- stability of experimental APIs explicitly documented outside the stable headers.

## Additive ABI evolution

New APIs are allowed only when they do not mutate a frozen declaration. Public configuration structures intended for future extension must use an existing `struct_size`/version discipline where provided. A new required field may not be inserted into a frozen public structure in 1.x.

## Security exceptions

Security may require refusing a previously accepted pack/record/key/profile. Such a change must:

1. preserve source exactness and memory safety;
2. receive a security advisory identifier;
3. document the affected contract and versions;
4. provide an upgrade/re-authoring path when feasible;
5. never silently reinterpret the same accepted bytes under the same semantic identity.

## Machine enforcement

`abi/stable-contract-v1.json` and `tools/abi_contract.py` enforce the frozen C declarations/types/constants/exports. `abi/format-contract-v1.json` and `tools/validate_format_contract.py` enforce the stable binary-format/semantics identifiers. CI treats drift as a release-blocking change requiring an explicit major-version process.
