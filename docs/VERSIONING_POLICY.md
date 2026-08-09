# Mosaic Product Versioning Policy

Status: canonical for Mosaic product releases beginning 2026-08-08.

Mosaic product releases use a four-part numeric version:

`S.M.N.P`

where:

- `S` — **stability generation**. `0` means pre-stable/candidate. `1` is the first fully qualified stable generation. A future increment beyond `1` is reserved for a new foundational stable generation that deliberately resets the major/minor release line.
- `M` — **major release** within the current stability generation. Increment for a substantial release that may intentionally require migration or introduce broad platform-level change. Reset `N` and `P` to zero.
- `N` — **minor release** within the current major release. Increment for backward-compatible capability additions or materially expanded supported surface. Reset `P` to zero.
- `P` — **patch**. Increment for bug fixes, security fixes, portability fixes, packaging/build fixes, documentation corrections, and other compatibility-preserving maintenance that does not intentionally change the release's major/minor product surface.

All four fields are non-negative decimal integers without leading zero padding. Product tags use `vS.M.N.P`.

## Current line

The current cumulative candidate is `0.1.0.5`.

The `0` stability generation is intentional: Linux qualification was strong, but real Windows qualification subsequently exposed portability defects. Mosaic will not claim the first stable generation merely because a feature-complete build exists. The first component advances to `1` only after the declared stable qualification gates are satisfied.

The intended graduation path is:

- `0.1.0.3`, `0.1.0.4`, ... — candidate maintenance patches while qualification continues;
- `0.1.1.0` — a backward-compatible candidate minor release, only if new product capability is intentionally added before stabilization;
- `0.2.0.0` — a candidate major release, only if a broad product change is intentionally introduced before stabilization;
- `1.0.0.0` — first fully qualified stable baseline;
- `1.1.0.0` — next stable major release;
- `1.1.1.0` — minor release within stable major `1`;
- `1.1.1.1` — patch to that minor release.

Stabilization is a deliberate re-baseline: moving from `0.x.y.z` to `1.0.0.0` does not imply the frozen C ABI, pack formats, or tokenizer semantic version must reset. Those contracts are versioned independently.

## Independent compatibility versions

The product release number is not the identity of every Mosaic contract. These remain independently versioned:

- native C ABI;
- optional trust ABI;
- tokenizer semantic version;
- MOSPACK container/profile versions;
- TokenDocument, packed-model, cache-record, signature-record, and registry schema versions;
- individual pack versions and pack dependency constraints.

A product patch may therefore change from `0.1.0.3` to `0.1.0.4` while C ABI `1.0.0`, trust ABI `1.0.0`, tokenizer semantics `2`, and all stable binary-format versions remain unchanged.

The pack registry continues to use ordinary three-component numeric SemVer for pack publisher/name/version coordination. That pack-level SemVer is deliberately separate from the four-part Mosaic product release version.

## Legacy three-component tags

Tags created before this policy are historical and are not rewritten or deleted. Rewriting them would break existing clones, evidence, provenance, and references.

The final legacy release sequence maps conceptually to the new candidate patch line as follows:

| Legacy tag | Four-part interpretation | Meaning |
|---|---:|---|
| `v1.0.0` | `0.1.0.0` | feature-complete enterprise candidate baseline, previously labeled stable |
| `v1.0.1` | `0.1.0.1` | Windows UTF-8/checksum portability patch |
| `v1.0.2` | `0.1.0.2` | Windows static/import-library collision patch |
| `v1.0.3` | `0.1.0.3` | Windows UCRT strict-warning portability patch |

The canonical four-part product line begins with the repository migration release `v0.1.0.3`; that tag contains the same cumulative implementation fixes plus the corrected versioning policy and metadata. Earlier legacy tags remain valid historical pointers but are no longer the preferred product-version nomenclature.

Older `v0.x.y` tags are development/milestone history created under the former three-component scheme. They remain immutable historical evidence and are not mechanically reinterpreted as current four-part releases.

## Patch rules

A patch release increments only `P` when the intended change is compatibility-preserving. Examples include:

- Windows/Linux/macOS portability fixes;
- compiler/toolchain compatibility fixes;
- memory-safety or correctness fixes that preserve canonical semantics;
- security hardening that does not require a declared compatibility break;
- build, package, installer, CI, SBOM, provenance, or documentation corrections;
- test corrections that do not intentionally alter product behavior.

If a fix requires intentionally changing a frozen semantic, ABI, or externally documented behavior, it must follow the compatibility/security exception process and may require a major/minor product release rather than being hidden inside `P`.

## Stable-generation gate

`S=1` is reserved for a properly qualified stable build. At minimum the release evidence must include:

1. dependency-minimal native builds and tests on Linux, Windows, and macOS;
2. the declared sanitizer/static-analysis gates on supported hosts;
3. stable Rust workspace compile, format, Clippy, tests, and `no_std` gate where declared;
4. deterministic pack/oracle checks and arbitrary-byte/reference differentials;
5. package/reproducibility/SBOM/provenance validation;
6. ABI/format contract validation;
7. deterministic reliability qualification;
8. no unresolved release-blocking defects in the supported platform matrix.

A missing gate is recorded as missing evidence, not silently treated as a pass.

## Ecosystem packaging

The canonical Mosaic product version is the four-part value in `VERSION`. First-party CMake metadata, headers, CLI tools, Python package metadata, release manifests, SBOM/provenance, release archives, and Git tags must agree with it.

If a future external package ecosystem cannot represent four numeric release components, that ecosystem must define an explicit deterministic mapping in its adapter/package metadata. The canonical `S.M.N.P` product version must still be recorded without ambiguity; it must never be silently collapsed into a different semantic meaning.
