#!/usr/bin/env python3
"""Synchronize and validate Mosaic four-part product release metadata."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOSAIC_VERSION_RE = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$"
)

FILES = {
    "cmake": ROOT / "native/CMakeLists.txt",
    "header": ROOT / "native/include/mosaic.h",
    "author": ROOT / "tools/mosaic_author.py",
    "registry": ROOT / "tools/mosaic_registry.py",
    "pyinit": ROOT / "bindings/python/mosaic/__init__.py",
    "pyproject": ROOT / "bindings/python/pyproject.toml",
    "capi_test": ROOT / "tools/validate_c_api.py",
    "readme": ROOT / "README.md",
    "version_policy": ROOT / "docs/VERSIONING_POLICY.md",
    "capi_doc": ROOT / "docs/implementation/API_C_1.0.md",
    "status": ROOT / "docs/implementation/STATUS.md",
    "threat_model": ROOT / "docs/security/THREAT_MODEL.md",
    "inno_setup": ROOT / "packaging/windows/MosaicDesktop.iss",
    "wix": ROOT / "packaging/windows/MosaicDesktop.wxs",
}


def parse_version(version: str) -> tuple[int, int, int, int]:
    match = MOSAIC_VERSION_RE.fullmatch(version)
    if not match:
        raise ValueError(
            "Mosaic product version must be numeric S.M.N.P "
            "(stability-generation.major.minor.patch)"
        )
    return tuple(int(part) for part in match.groups())


def release_label(version: str) -> str:
    stability, _, _, _ = parse_version(version)
    return "Candidate tokenizer" if stability == 0 else "Stable tokenizer"


def release_sentence(version: str) -> str:
    stability, _, _, _ = parse_version(version)
    state = "current pre-stable enterprise candidate" if stability == 0 else "current stable enterprise"
    return f"Mosaic Tokenizer {version} is the {state} tokenizer"


def platform_sentence(version: str) -> str:
    return f"Mosaic `{version}` is the current **universal tokenization and token-native processing platform candidate**."


def expected_occurrences(version: str) -> dict[str, str]:
    label = release_label(version)
    sentence = release_sentence(version)
    return {
        "cmake": f"project(mosaic VERSION {version} LANGUAGES C CXX)",
        "header": f'#define MOSAIC_RELEASE_VERSION "{version}"',
        "author": f'version="mosaic-author {version}"',
        "registry": f"version='mosaic-registry {version}'",
        "pyinit": f'__version__ = "{version}"',
        "pyproject": f'version = "{version}"',
        "capi_test": f"assert lib.mosaic_version_string()==b'{version}'",
        "readme": f"## {label}: {version}",
        "readme_sentence": sentence,
        "readme_platform": platform_sentence(version),
        "version_policy": f"The current cumulative candidate is `{version}`.",
        "capi_doc": f"The native C ABI is independently versioned from the Mosaic product release. The current product candidate `{version}` exposes frozen C ABI `1.1.0`.",
        "status": f"**{version} — universal tokenization and token-native processing platform candidate.**",
        "threat_model": f"Status: frozen enterprise candidate baseline; current product release {version}.",
        "inno_setup": f'#define AppVersion "{version}"',
        "wix": f'Version="{version}"',
    }


def check(version: str) -> list[str]:
    problems: list[str] = []
    try:
        parse_version(version)
    except ValueError as exc:
        return [str(exc)]
    for key, needle in expected_occurrences(version).items():
        path = FILES["readme"] if key in {"readme_sentence", "readme_platform"} else FILES[key]
        text = path.read_text(encoding="utf-8")
        if needle not in text:
            problems.append(f"{path.relative_to(ROOT)} missing {needle!r}")
    if (ROOT / "VERSION").read_text(encoding="utf-8").strip() != version:
        problems.append("VERSION does not match")
    return problems


def replace_regex(path: Path, pattern: str, replacement: str) -> None:
    text = path.read_text(encoding="utf-8")
    new, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"could not update {path.relative_to(ROOT)} using {pattern!r}")
    path.write_text(new, encoding="utf-8")


def set_version(version: str) -> None:
    try:
        parse_version(version)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc

    (ROOT / "VERSION").write_text(version + "\n", encoding="utf-8")
    replace_regex(
        FILES["cmake"],
        r"project\(mosaic VERSION [^ ]+ LANGUAGES C CXX\)",
        f"project(mosaic VERSION {version} LANGUAGES C CXX)",
    )
    replace_regex(
        FILES["header"],
        r'#define MOSAIC_RELEASE_VERSION "[^"]+"',
        f'#define MOSAIC_RELEASE_VERSION "{version}"',
    )
    replace_regex(FILES["author"], r'version="mosaic-author [^"]+"', f'version="mosaic-author {version}"')
    replace_regex(FILES["registry"], r"version='mosaic-registry [^']+'", f"version='mosaic-registry {version}'")
    replace_regex(FILES["pyinit"], r'__version__ = "[^"]+"', f'__version__ = "{version}"')
    replace_regex(FILES["pyproject"], r'^version = "[^"]+"$', f'version = "{version}"')
    replace_regex(
        FILES["capi_test"],
        r"assert lib\.mosaic_version_string\(\)==b'[^']+'",
        f"assert lib.mosaic_version_string()==b'{version}'",
    )
    replace_regex(
        FILES["readme"],
        r'^## (?:Stable|Candidate) tokenizer: .+$',
        f"## {release_label(version)}: {version}",
    )
    replace_regex(
        FILES["readme"],
        r'^Mosaic Tokenizer [0-9]+(?:\.[0-9]+){2,3} is the (?:current stable enterprise|current pre-stable enterprise candidate) tokenizer',
        release_sentence(version),
    )
    replace_regex(
        FILES["readme"],
        r'^Mosaic `[0-9]+(?:\.[0-9]+){2,3}` is the current \*\*universal tokenization and token-native processing platform candidate\*\*\.',
        platform_sentence(version),
    )
    replace_regex(
        FILES["version_policy"],
        r'^The current cumulative candidate is `[0-9]+(?:\.[0-9]+){2,3}`\.$',
        f"The current cumulative candidate is `{version}`.",
    )
    replace_regex(
        FILES["capi_doc"],
        r'^The native C ABI is independently versioned from the Mosaic product release\. The current product candidate `[0-9]+(?:\.[0-9]+){2,3}` (?:continues to expose|exposes) frozen C ABI `(?:1\.0\.0|1\.1\.0)`\.$',
        f"The native C ABI is independently versioned from the Mosaic product release. The current product candidate `{version}` exposes frozen C ABI `1.1.0`.",
    )
    replace_regex(
        FILES["status"],
        r'^\*\*[0-9]+(?:\.[0-9]+){2,3} — universal tokenization and token-native processing platform candidate\.\*\*$',
        f"**{version} — universal tokenization and token-native processing platform candidate.**",
    )
    replace_regex(
        FILES["threat_model"],
        r'^Status: frozen enterprise candidate baseline; current product release [0-9]+(?:\.[0-9]+){2,3}\.$',
        f"Status: frozen enterprise candidate baseline; current product release {version}.",
    )
    replace_regex(FILES["inno_setup"], r'^#define AppVersion "[^"]+"$', f'#define AppVersion "{version}"')
    replace_regex(FILES["wix"], r'^    Version="[^"]+"$', f'    Version="{version}"')


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("version", nargs="?")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    if args.version and args.check:
        raise SystemExit("use either VERSION or --check")
    if args.version:
        set_version(args.version)
        version = args.version
    else:
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    problems = check(version)
    if problems:
        for problem in problems:
            print("FAIL:", problem)
        return 1
    print(f"OK: Mosaic release version synchronized at {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
