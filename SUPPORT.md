# Mosaic Support Policy

## Current candidate line

Mosaic product releases use `S.M.N.P` as documented in `docs/VERSIONING_POLICY.md`. The current supported development/candidate line is `0.1.0.x`.

The newest `0.1.0.x` patch is the preferred test/qualification target. It preserves the frozen native C ABI 1.0.0, optional trust ABI 1.0.0, tokenizer semantics version 2, and stable binary-format contracts unless an explicitly documented compatibility/security exception says otherwise.

The product does not claim stability generation `1` until the declared cross-platform and Rust qualification gates are complete. The first fully qualified stable product baseline will be `1.0.0.0`.

## Compatibility

Current compatibility rules are governed by `docs/COMPATIBILITY_POLICY.md`; product-version rules are governed by `docs/VERSIONING_POLICY.md`. Historical three-component tags remain preserved but are legacy nomenclature.

Security fixes are backported when practical according to severity and compatibility risk. Exact supported versions are stated in security advisories.
