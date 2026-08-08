# Support and Compatibility Policy

## Pre-1.0

The newest stable 0.x release is the supported development line. Public C ABI additions are additive where possible, but pre-1.0 format/API changes may occur with migration notes.

## 1.0 and later

The 1.x line will freeze canonical byte semantics, pack/manifest identity rules, TokenDocument serialization v1, canonical path ordering, C ABI ownership rules, and compatibility policy. Breaking changes require a new major version.

## Enterprise deployment

Production deployments should pin exact release artifacts and pack content hashes, use resolved lockfiles, enable resource policies before sealing tokenizer instances, and retain qualification/provenance artifacts alongside deployed binaries.
