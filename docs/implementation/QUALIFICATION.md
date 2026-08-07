# Qualification Workflow

Mosaic deliberately separates **implementation present** from **milestone qualified**.

## Local independent gates

Run:

```bash
python3 tools/qualify.py --python-only
```

This verifies deterministic fixtures, the independent pack oracle, malicious-pack rejection, path-order vectors, TokenizerManifest identity, static repository constraints, and structural documentation. It does **not** qualify Rust implementation semantics.

## Stable-Rust local gates

With stable Rust installed:

```bash
python3 tools/qualify.py
```

This adds rustfmt, Clippy with warnings denied, workspace tests, and the no-default-features `mosaic-core` build.

## Nightly gates

Scheduled CI adds:

- Miri on trusted-core/pack crates;
- cargo-fuzz smoke campaigns;
- Linux/Windows/macOS canonical fixture rebuilds;
- broader differential suites as milestones add them.

## Release qualification

A public release candidate must pass every applicable stable, nightly, cross-platform, conformance, fuzz, ABI, benchmark, and provenance gate for the capabilities it claims. A checked box in a Markdown file is not evidence unless the corresponding CI artifact identifies the exact commit and test inputs.
