#!/usr/bin/env python3
"""Generate a deterministic SPDX 2.3 JSON SBOM for a staged Mosaic distribution."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

REPO_BASE = "https://github.com/shreyashjagtap157/Mosaic"


def digest(path: Path, algorithm: str) -> str:
    h = hashlib.new(algorithm)
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stage", type=Path)
    ap.add_argument("--version", required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    root = a.stage.resolve()
    out = a.output.resolve()

    files = []
    relationships = []
    verification_inputs: list[str] = []
    file_index = 0
    for p in sorted(root.rglob("*")):
        if not p.is_file() or p.resolve() == out:
            continue
        rel = p.relative_to(root).as_posix()
        sid = f"SPDXRef-File-{file_index}"
        file_index += 1
        sha1 = digest(p, "sha1")
        sha256 = digest(p, "sha256")
        verification_inputs.append(sha1)
        files.append(
            {
                "SPDXID": sid,
                "fileName": "./" + rel,
                "checksums": [
                    {"algorithm": "SHA1", "checksumValue": sha1},
                    {"algorithm": "SHA256", "checksumValue": sha256},
                ],
                "licenseConcluded": "NOASSERTION",
                "licenseInfoInFiles": ["NOASSERTION"],
                "copyrightText": "NOASSERTION",
            }
        )
        relationships.append(
            {
                "spdxElementId": "SPDXRef-Package-Mosaic",
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": sid,
            }
        )

    package_verification_code = hashlib.sha1(
        "".join(sorted(verification_inputs)).encode("ascii")
    ).hexdigest()
    namespace = f"{REPO_BASE}/spdx/{a.version}/{root.name}"
    doc = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"Mosaic-Tokenizer-{a.version}-SBOM",
        "documentNamespace": namespace,
        "creationInfo": {
            "created": "1980-01-01T00:00:00Z",
            "creators": ["Tool: mosaic-generate-sbom/2"],
        },
        "packages": [
            {
                "name": "Mosaic Tokenizer",
                "SPDXID": "SPDXRef-Package-Mosaic",
                "versionInfo": a.version,
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "packageVerificationCode": {
                    "packageVerificationCodeValue": package_verification_code
                },
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "copyrightText": "NOASSERTION",
                "homepage": REPO_BASE,
            }
        ],
        "files": files,
        "relationships": relationships,
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
