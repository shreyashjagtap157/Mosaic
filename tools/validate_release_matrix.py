#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL: missing {label}: {needle}")


def main() -> int:
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    workflow = (ROOT / ".github/workflows/release-qualification.yml").read_text(encoding="utf-8")
    doc = (ROOT / "docs/implementation/RELEASE_ENGINEERING_v1.md").read_text(encoding="utf-8")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")

    require(makefile, "release-readiness:", "Makefile target")
    require(makefile, "tools/validate_release_readiness.py --skip-package", "Makefile readiness command")
    require(makefile, "tools/validate_release_readiness.py --build-release --archive dist/mosaic-tokenizer-$(VERSION)-linux-x86_64.tar.gz", "Makefile release command")

    require(workflow, "python tools/validate_release_readiness.py --build-release", "release workflow readiness command")

    require(doc, "consolidated local entrypoint is `tools/validate_release_readiness.py`", "release engineering doc")
    require(doc, "release-qualification workflow now calls the same entrypoint", "release engineering workflow note")

    require(readme, "make release-readiness", "README release-readiness target")
    require(readme, "python tools/validate_release_readiness.py", "README readiness command")

    print("OK: release matrix entries are aligned across Makefile, workflow, README, and release engineering docs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
