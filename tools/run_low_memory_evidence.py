#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "benches" / "low_memory_4gb.runs"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture the constrained desktop machine profile and benchmark record.")
    parser.add_argument("--output-dir", default=str(DEFAULT_DIR))
    args = parser.parse_args()
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    machine = out_dir / "machine_profile.toml"
    run = out_dir / "latest.toml"

    subprocess.run([sys.executable, "tools/record_machine_profile.py", "--output", str(machine)], cwd=ROOT, check=True)
    subprocess.run([sys.executable, "tools/run_low_memory_profile.py", "--output", str(run)], cwd=ROOT, check=True)
    subprocess.run([sys.executable, "tools/validate_low_memory_profile.py"], cwd=ROOT, check=True)

    print(f"OK: wrote low-memory evidence bundle to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
