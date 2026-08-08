# Release Engineering v1

Mosaic distributions are deterministic tar.gz archives with normalized timestamps/ownership. A release contains SHA-256 checksums, a release manifest, an SPDX 2.3 JSON SBOM, and an in-toto/SLSA-shaped provenance statement. The provenance records the source checksum-manifest identity and every staged artifact digest.

Cross-platform core qualification deliberately disables optional OpenSSL/ICU-dependent features so the tokenizer core proves it has no hidden dependency on them. Full Linux qualification enables all available optional trust and Unicode differential tests.

Canonical pack identity remains content-addressed and independent of the release bundle. Release provenance authenticates what was built; pack trust authenticates publisher/content relationships. These are separate security layers.
