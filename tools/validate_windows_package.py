#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
STAGE = ROOT / "dist" / "windows" / "stage"
INSTALLER = ROOT / "dist" / "windows" / f"MosaicCompressorSetup-{VERSION}-x64.exe"

REQUIRED_FILES = [
    STAGE / "bin" / "mosaic-desktop.exe",
    STAGE / "bin" / "mosaic-desktop-selftest.exe",
    STAGE / "bin" / "mosaic-tokenizer.exe",
    STAGE / "bin" / "mosaic-compress-compare.exe",
    STAGE / "bin" / "mosaic.dll",
    STAGE / "include" / "mosaic.h",
    STAGE / "share" / "mosaic" / "docs" / "README.md",
    STAGE / "share" / "mosaic" / "docs" / "CHANGELOG.md",
    STAGE / "share" / "mosaic" / "docs" / "SECURITY.md",
    STAGE / "share" / "mosaic" / "docs" / "SUPPORT.md",
    STAGE / "share" / "mosaic" / "docs" / "RELEASE_ENGINEERING_v1.md",
    STAGE / "share" / "mosaic" / "packs" / "model-v2.mpack",
    STAGE / "share" / "mosaic" / "packs" / "unicode17-v1.mpack",
    STAGE / "share" / "mosaic" / "packs" / "security17-v1.mpack",
    STAGE / "share" / "mosaic" / "packs" / "normalization16-v1.mpack",
]


def main() -> int:
    if not STAGE.is_dir():
        raise SystemExit(f"FAIL: missing Windows stage tree: {STAGE}")
    missing = [path for path in REQUIRED_FILES if not path.is_file()]
    if missing:
        raise SystemExit("FAIL: missing staged Windows package files: " + ", ".join(str(p.relative_to(ROOT)) for p in missing))
    if not INSTALLER.is_file():
        raise SystemExit(f"FAIL: missing Windows installer: {INSTALLER.relative_to(ROOT)}")

    installer_name = INSTALLER.name
    expected_name = f"MosaicCompressorSetup-{VERSION}-x64.exe"
    if installer_name != expected_name:
        raise SystemExit(f"FAIL: installer filename mismatch: {installer_name} != {expected_name}")

    print("OK: Windows package stage and installer are present and complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
