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

This local checkout now pins Rust 1.97.1 in `rust-toolchain.toml`; rerun these commands after Rust source changes and regenerate `ARTIFACT_CHECKSUMS.sha256` before committing.

## Do not tag yet

Do not graduate to stable generation `1.0.0.0` until the remaining multi-platform/Miri/fuzz/race/soak gates documented in the release qualification files pass. Candidate tagging may proceed only from a clean tree with the Windows native and Rust 1.97.1 gates passing on the exact tagged source.
