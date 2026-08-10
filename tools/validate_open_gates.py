#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXPECTED = [
    "macOS native CI execution;",
    "full cargo-fuzz jobs;",
    "ThreadSanitizer or platform-specific race detectors where supported;",
    "non-x86-64/ARM64 qualification required by the final support matrix.",
]


def extract(text: str) -> list[str]:
    in_block = False
    gates: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped == "## Stable-generation qualification gates still open":
            in_block = True
            continue
        if in_block:
            if stripped.startswith("## "):
                break
            if stripped.startswith("- "):
                gates.append(stripped[2:])
    return gates


def main() -> int:
    status = (ROOT / "docs/implementation/STATUS.md").read_text(encoding="utf-8")
    qualification = (ROOT / "docs/implementation/QUALIFICATION_1.0.0.md").read_text(encoding="utf-8")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")

    status_gates = extract(status)
    if status_gates != EXPECTED:
        raise SystemExit(f"FAIL: STATUS.md open gates mismatch: {status_gates!r}")

    for gate in [
        "macOS native CMake/CTest execution;",
        "stable Rust workspace build/clippy/tests;",
        "ThreadSanitizer/platform race detector where supported;",
    ]:
        if gate not in qualification:
            raise SystemExit(f"FAIL: QUALIFICATION_1.0.0.md missing gate: {gate}")

    if "macOS/Miri/fuzz/race-detector and remaining support-matrix gates must complete" not in readme:
        raise SystemExit("FAIL: README candidate boundary no longer matches the open-gates summary")

    print("OK: open release gates stay aligned across STATUS, qualification, and README")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
