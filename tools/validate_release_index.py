#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "docs" / "release" / "README.md"


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL: missing {label}: {needle}")


def main() -> int:
    text = INDEX.read_text(encoding="utf-8")
    require(text, "RELEASE_NOTES_0.1.3.5.md", "current release note link")
    require(text, "QUALIFICATION_0.1.3.5.md", "current qualification link")
    require(text, "QUALIFICATION_0.1.3.5_WINDOWS_PACKAGE.md", "windows package qualification link")
    require(text, "RELEASE_NOTES_1.0.0.md", "stable release note link")
    require(text, "RELEASE_NOTES_1.0.1.md", "stable release note link")
    print("OK: release notes index points at current and stable release evidence")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
