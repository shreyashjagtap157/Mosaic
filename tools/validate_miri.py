#!/usr/bin/env python3
from __future__ import annotations
import argparse, os, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PACKAGES = ["mosaic-core", "mosaic-pack"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--toolchain", default=os.environ.get("MOSAIC_MIRI_TOOLCHAIN", "nightly"))
    ap.add_argument("packages", nargs="*", default=DEFAULT_PACKAGES)
    args = ap.parse_args()

    for package in args.packages:
        command = ["cargo", f"+{args.toolchain}", "miri", "test", "-p", package]
        print("+", " ".join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)

    print(f"OK: Miri passed for {', '.join(args.packages)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
