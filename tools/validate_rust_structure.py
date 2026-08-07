#!/usr/bin/env python3
"""Static structural checks that do not pretend to replace rustc/clippy."""
from __future__ import annotations

import re
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CRATES = ROOT / "crates"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def main() -> int:
    workspace = tomllib.loads((ROOT / "Cargo.toml").read_text())
    members = workspace["workspace"]["members"]
    for member in members:
        if not (ROOT / member / "Cargo.toml").is_file():
            fail(f"workspace member missing Cargo.toml: {member}")

    for crate in ["mosaic-core", "mosaic-ir", "mosaic-pack"]:
        lib = CRATES / crate / "src" / "lib.rs"
        text = lib.read_text()
        if "#![forbid(unsafe_code)]" not in text:
            fail(f"{crate} does not forbid unsafe code")

    for path in CRATES.rglob("*.rs"):
        text = path.read_text()
        if re.search(r"\bunsafe\b", text) and "unsafe_code" not in text:
            fail(f"unexpected unsafe token in {path.relative_to(ROOT)}")
        if "unimplemented!" in text or "todo!" in text:
            fail(f"placeholder implementation macro in {path.relative_to(ROOT)}")

    engine = tomllib.loads((CRATES / "mosaic-engine" / "Cargo.toml").read_text())
    if "mosaic-reference" in engine.get("dependencies", {}):
        fail("mosaic-engine production dependencies must not include mosaic-reference")
    if "mosaic-reference" not in engine.get("dev-dependencies", {}):
        fail("mosaic-engine must keep mosaic-reference as a differential-test dependency")

    pack_lib = (CRATES / "mosaic-pack" / "src" / "lib.rs").read_text()
    for module in ["dfa", "execution_manifest", "hash", "lock", "manifest", "v1"]:
        if f"mod {module};" not in pack_lib:
            fail(f"mosaic-pack missing module declaration: {module}")

    print(f"OK: {len(members)} workspace members structurally present")
    print("OK: reference implementation is not a production dependency of mosaic-engine")
    print("NOTE: this is not a Rust parser/compiler and does not replace cargo qualification")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
