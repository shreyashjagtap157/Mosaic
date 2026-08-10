# Codex Local Handoff — Mosaic 0.1.2.1

This repository is intended to replace the previous local Mosaic folder directly. It includes Git history and excludes generated build trees.

## First local qualification on Windows

From a Visual Studio Developer PowerShell in `D:\Project\Mosaic`:

```powershell
$env:CC = "clang"
$env:CXX = "clang++"
python tools\set_version.py --check
python tools\generate_artifact_checksums.py --check
cmake --preset core-release
cmake --build --preset core-release --parallel
ctest --test-dir build\preset-core-release --output-on-failure
```

For the exact Rust hardening gate, install/pin Rust 1.97.1, then run:

```powershell
cargo fmt --all
cargo fmt --all -- --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
cargo check -p mosaic-core --no-default-features
```

If Rust formatting changes files, review those changes, regenerate `ARTIFACT_CHECKSUMS.sha256`, rerun the gates, and commit them as the continuation of the 0.1.2.1 hardening patch.

## Do not tag yet

Do not create `v0.1.2.1` until the Rust gate above and Windows current-source native qualification pass. Stable generation `1.0.0.0` additionally requires the remaining multi-platform/Miri/fuzz/race/soak gates documented in the release qualification files.
