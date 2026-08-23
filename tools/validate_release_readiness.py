#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARCHIVE = ROOT / "dist" / f"mosaic-tokenizer-{(ROOT / 'VERSION').read_text(encoding='utf-8').strip()}-linux-x86_64.tar.gz"


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def python() -> str:
    return sys.executable


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Mosaic release-readiness checks; optionally defer Miri or packaging."
    )
    parser.add_argument("--archive", default=str(DEFAULT_ARCHIVE))
    parser.add_argument("--build-release", action="store_true")
    parser.add_argument("--skip-package", action="store_true")
    parser.add_argument("--skip-miri", action="store_true", help="skip the long Miri pass")
    args = parser.parse_args()

    run([python(), "tools/validate_repo.py"])
    run([python(), "tools/validate_release_matrix.py"])
    run([python(), "tools/validate_open_gates.py"])
    run([python(), "tools/validate_space_grade_docs.py"])
    run([python(), "tools/validate_windows_package.py"])
    run([python(), "tools/validate_low_memory_profile.py"])
    run([python(), "tools/set_version.py", "--check"])
    run([python(), "tools/generate_artifact_checksums.py", "--check"])
    if not args.skip_miri:
        run([python(), "tools/validate_miri.py"])
    run([python(), "tools/validate_fuzz.py", "--runs", "60"])

    if not args.skip_package:
        if args.build_release:
            run([python(), "tools/build_release.py", "--no-build", "--allow-dirty"])
        archive = Path(args.archive)
        if not archive.exists():
            raise SystemExit(f"missing release archive: {archive}")
        run([python(), "tools/validate_release_package.py", str(archive)])

    print("OK: release readiness checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
