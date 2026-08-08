#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO_BASE = "https://github.com/shreyashjagtap157/Mosaic"


def digest(p: Path, algorithm: str = "sha256") -> str:
    h = hashlib.new(algorithm)
    with p.open("rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def git_text(*args: str) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return "unavailable"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stage", type=Path)
    ap.add_argument("--source-checksums", type=Path, required=True)
    a = ap.parse_args()
    stage = a.stage.resolve()
    sbom = json.loads((stage / "share/mosaic/sbom.spdx.json").read_text())
    prov = json.loads((stage / "share/mosaic/provenance.intoto.json").read_text())
    manifest = json.loads((stage / "share/mosaic/release-manifest.json").read_text())

    assert sbom["spdxVersion"] == "SPDX-2.3"
    assert sbom["documentNamespace"].startswith(REPO_BASE + "/spdx/")
    package = sbom["packages"][0]
    assert package["SPDXID"] == "SPDXRef-Package-Mosaic"
    verification_sha1s: list[str] = []
    for f in sbom["files"]:
        rel = f["fileName"].removeprefix("./")
        p = stage / rel
        assert p.is_file(), rel
        checks = {x["algorithm"]: x["checksumValue"] for x in f["checksums"]}
        assert digest(p, "sha1") == checks["SHA1"], rel
        assert digest(p) == checks["SHA256"], rel
        verification_sha1s.append(checks["SHA1"])
    pvc = hashlib.sha1("".join(sorted(verification_sha1s)).encode("ascii")).hexdigest()
    assert package["packageVerificationCode"]["packageVerificationCodeValue"] == pvc

    assert prov["_type"] == "https://in-toto.io/Statement/v1"
    assert prov["predicateType"] == "https://slsa.dev/provenance/v1"
    subjects = {x["name"]: x["digest"]["sha256"] for x in prov["subject"]}
    for rel, expected in subjects.items():
        p = stage / rel
        assert p.is_file(), rel
        assert digest(p) == expected, rel
    build = prov["predicate"]["buildDefinition"]
    ext = build["externalParameters"]
    assert ext["version"] == manifest["version"]
    source_digest = digest(a.source_checksums)
    assert ext["sourceChecksumsSha256"] == source_digest
    deps = build["resolvedDependencies"]
    assert any(d.get("digest", {}).get("sha256") == source_digest for d in deps)
    params = build["internalParameters"]
    current_revision = git_text("rev-parse", "HEAD")
    if current_revision != "unavailable":
        assert params["gitRevision"] == current_revision
    assert bool(params["gitDirty"]) == bool(git_text("status", "--porcelain"))

    sums: dict[str, str] = {}
    for line in (stage / "SHA256SUMS").read_text().splitlines():
        expected, rel = line.split("  ", 1)
        if rel in sums:
            raise AssertionError(f"duplicate checksum path: {rel}")
        sums[rel] = expected
    for rel, expected in sums.items():
        assert digest(stage / rel) == expected, rel

    expected_stage = f"mosaic-tokenizer-{manifest['version']}-{manifest['platform']}"
    assert stage.name == expected_stage, (stage.name, expected_stage)
    print(
        f"OK supply-chain sbom_files={len(sbom['files'])} provenance_subjects={len(subjects)} "
        f"checksums={len(sums)} git_dirty={params['gitDirty']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
