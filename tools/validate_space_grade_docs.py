#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL: missing {label}: {needle}")


def main() -> int:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    plan = (ROOT / "docs/implementation/SPACE_GRADE_PLAN.md").read_text(encoding="utf-8")
    status = (ROOT / "docs/implementation/STATUS.md").read_text(encoding="utf-8")
    support = (ROOT / "docs/implementation/SUPPORT_MATRIX.md").read_text(encoding="utf-8")
    roadmap = (ROOT / "docs/implementation/CURRENT_VS_ROADMAP.md").read_text(encoding="utf-8")
    guide = (ROOT / "docs/implementation/INTEGRATION_GUIDE.md").read_text(encoding="utf-8")
    examples = (ROOT / "examples/integration/README.md").read_text(encoding="utf-8")

    require(readme, "docs/implementation/SUPPORT_MATRIX.md", "README support-matrix link")
    require(readme, "docs/implementation/CURRENT_VS_ROADMAP.md", "README roadmap link")
    require(readme, "docs/implementation/INTEGRATION_GUIDE.md", "README integration-guide link")
    require(plan, "current-versus-roadmap capability tables", "space-grade plan current-vs-roadmap deliverable")
    require(status, "current support boundary is summarized", "status support boundary note")
    require(support, "Windows desktop installer", "support matrix desktop row")
    require(support, "Low-memory desktop usage", "support matrix resource guidance")
    require(roadmap, "Stable-generation `1.0.0.0`", "roadmap stable-generation row")
    require(roadmap, "GPU acceleration", "roadmap research row")
    require(guide, "Desktop integration", "integration guide desktop section")
    require(guide, "Agent and service embedding", "integration guide agent/service section")
    require(examples, "Windows desktop package demonstrates", "examples desktop section")

    print("OK: space-grade docs are internally linked and boundary-consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
