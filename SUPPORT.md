# Support and Compatibility Policy

## Stable 1.x line

Mosaic 1.0.0 establishes the supported stable major line. Public C/trust ABI items and stable binary formats are governed by `docs/COMPATIBILITY_POLICY_1.0.md` and mechanically checked by the ABI/format contract tools. Breaking changes require a new major version except for narrowly documented critical-security rejection changes.

The newest 1.x patch/minor release is the preferred production target. Security fixes are backported when practical according to severity and compatibility risk; exact supported versions are stated in security advisories.

## Pre-1.0 releases

0.x releases remain preserved by Git tags for reproducibility but are development history, not the recommended enterprise deployment line after 1.0.

## Enterprise deployment

Production deployments should pin exact release artifacts and pack content hashes, use resolved lockfiles, enable resource policies before sealing tokenizer instances, retain SBOM/provenance/qualification artifacts alongside deployed binaries, and exercise the platform-specific CI gates for their supported operating systems and architectures.

## Implementation status

The qualified 1.0 production implementation is the native C runtime and C ABI. The Rust workspace remains an independent safe reference/conformance implementation until its Rust CI/toolchain gates are executed; it is not part of the 1.0 packaged runtime artifact.
