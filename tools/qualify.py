#!/usr/bin/env python3
"""Run Mosaic qualification gates with an explicit python-only fallback."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PYTHON_GATES = [
    [sys.executable, "tools/generate_empty_pack.py", "--check"],
    [sys.executable, "tools/build_m2_fixture.py", "--check"],
    [sys.executable, "tools/generate_m2_malformed.py", "--check"],
    [sys.executable, "tools/validate_m2_fixture.py"],
    [sys.executable, "tools/validate_path_order.py"],
    [sys.executable, "tools/validate_manifest_identity.py"],
    [sys.executable, "tools/validate_rust_structure.py"],
    [sys.executable, "tools/validate_repo.py"],
]

RUST_GATES = [
    ["cargo", "fmt", "--all", "--", "--check"],
    ["cargo", "clippy", "--workspace", "--all-targets", "--", "-D", "warnings"],
    ["cargo", "test", "--workspace"],
    ["cargo", "check", "-p", "mosaic-core", "--no-default-features"],
]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--python-only",
        action="store_true",
        help="run executable non-Rust gates only; does not qualify M1/M2",
    )
    args = parser.parse_args()

    for command in PYTHON_GATES:
        run(command)

    if args.python_only:
        print("PASS: Python-independent gates completed")
        print("NOT QUALIFIED: Rust gates intentionally skipped")
        return 0

    missing = [tool for tool in ["rustc", "cargo"] if shutil.which(tool) is None]
    if missing:
        print(
            "FAIL: Rust qualification unavailable; missing " + ", ".join(missing),
            file=sys.stderr,
        )
        return 2

    for command in RUST_GATES:
        run(command)

    print("PASS: local stable-Rust qualification gates completed")
    print("NOTE: nightly/release CI still owns cross-platform gates; local validators cover Miri and fuzz smoke")
    print("NOTE: for release-readiness without the long Miri pass, use `make release-readiness-fast`")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
