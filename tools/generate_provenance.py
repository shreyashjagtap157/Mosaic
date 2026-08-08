#!/usr/bin/env python3
"""Generate deterministic, verifiable SLSA-shaped provenance for a staged Mosaic distribution."""
from __future__ import annotations
import argparse, hashlib, json, platform, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO_URI = "git+https://github.com/shreyashjagtap157/Mosaic.git"


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def command_line(cmd: list[str]) -> str:
    try:
        return subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT).splitlines()[0].strip()
    except Exception:
        return "unavailable"


def git_text(*args: str) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return "unavailable"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stage", type=Path)
    ap.add_argument("--version", required=True)
    ap.add_argument("--source-checksums", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    stage = a.stage.resolve()
    out = a.output.resolve()

    artifacts: dict[str, str] = {}
    for p in sorted(stage.rglob("*")):
        if p.is_file() and p.resolve() != out:
            artifacts[p.relative_to(stage).as_posix()] = sha(p)

    revision = git_text("rev-parse", "HEAD")
    dirty = bool(git_text("status", "--porcelain"))
    source_manifest_digest = sha(a.source_checksums)
    resolved_dependencies = [
        {
            "uri": f"{REPO_URI}@{revision}",
            "digest": {"sha1": revision} if len(revision) == 40 else {},
        },
        {
            "uri": "file:ARTIFACT_CHECKSUMS.sha256",
            "digest": {"sha256": source_manifest_digest},
        },
    ]

    statement = {
        "_type": "https://in-toto.io/Statement/v1",
        "subject": [
            {"name": k, "digest": {"sha256": v}} for k, v in sorted(artifacts.items())
        ],
        "predicateType": "https://slsa.dev/provenance/v1",
        "predicate": {
            "buildDefinition": {
                "buildType": "https://github.com/shreyashjagtap157/Mosaic/blob/main/docs/implementation/RELEASE_ENGINEERING_v1.md",
                "externalParameters": {
                    "version": a.version,
                    "sourceChecksumsSha256": source_manifest_digest,
                },
                "internalParameters": {
                    "gitRevision": revision,
                    "gitDirty": dirty,
                    "platform": platform.system().lower() + "-" + platform.machine().lower(),
                    "cc": command_line(["cc", "--version"]),
                    "python": command_line(["python3", "--version"]),
                },
                "resolvedDependencies": resolved_dependencies,
            },
            "runDetails": {
                "builder": {"id": "https://github.com/shreyashjagtap157/Mosaic/tools/build_release.py"},
                "metadata": {"invocationId": f"mosaic-{a.version}-{stage.name}"},
                "byproducts": [],
            },
        },
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(statement, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
