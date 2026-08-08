#!/usr/bin/env python3
"""Synchronize and validate Mosaic release version metadata."""
from __future__ import annotations
import argparse, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:[-+][0-9A-Za-z.-]+)?$")

FILES = {
    "cmake": ROOT / "native/CMakeLists.txt",
    "header": ROOT / "native/include/mosaic.h",
    "author": ROOT / "tools/mosaic_author.py",
    "registry": ROOT / "tools/mosaic_registry.py",
    "pyinit": ROOT / "bindings/python/mosaic/__init__.py",
    "pyproject": ROOT / "bindings/python/pyproject.toml",
    "capi_test": ROOT / "tools/validate_c_api.py",
    "readme": ROOT / "README.md",
}


def expected_occurrences(version: str) -> dict[str, str]:
    return {
        "cmake": f"project(mosaic VERSION {version} LANGUAGES C CXX)",
        "header": f'#define MOSAIC_RELEASE_VERSION "{version}"',
        "author": f"version=\"mosaic-author {version}\"",
        "registry": f"version='mosaic-registry {version}'",
        "pyinit": f'__version__ = "{version}"',
        "pyproject": f'version = "{version}"',
        "capi_test": f"assert lib.mosaic_version_string()==b'{version}'",
        "readme": f"## Stable tokenizer: {version}",
    }


def check(version: str) -> list[str]:
    problems = []
    for key, needle in expected_occurrences(version).items():
        text = FILES[key].read_text(encoding="utf-8")
        if needle not in text:
            problems.append(f"{FILES[key].relative_to(ROOT)} missing {needle!r}")
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
    if not SEMVER.match(version):
        raise SystemExit(f"invalid SemVer: {version}")
    (ROOT / "VERSION").write_text(version + "\n", encoding="utf-8")
    replace_regex(FILES["cmake"], r"project\(mosaic VERSION [^ ]+ LANGUAGES C CXX\)", f"project(mosaic VERSION {version} LANGUAGES C CXX)")
    replace_regex(FILES["header"], r'#define MOSAIC_RELEASE_VERSION "[^"]+"', f'#define MOSAIC_RELEASE_VERSION "{version}"')
    replace_regex(FILES["author"], r'version="mosaic-author [^"]+"', f'version="mosaic-author {version}"')
    replace_regex(FILES["registry"], r"version='mosaic-registry [^']+'", f"version='mosaic-registry {version}'")
    replace_regex(FILES["pyinit"], r'__version__ = "[^"]+"', f'__version__ = "{version}"')
    replace_regex(FILES["pyproject"], r'^version = "[^"]+"$', f'version = "{version}"')
    replace_regex(FILES["capi_test"], r"assert lib\.mosaic_version_string\(\)==b'[^']+'", f"assert lib.mosaic_version_string()==b'{version}'")
    replace_regex(FILES["readme"], r'^## Stable tokenizer: .+$', f'## Stable tokenizer: {version}')
    replace_regex(FILES["readme"], r'^Mosaic Tokenizer [0-9]+\.[0-9]+\.[0-9]+ is the current stable enterprise tokenizer', f'Mosaic Tokenizer {version} is the current stable enterprise tokenizer')


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
    print(f"OK: release version synchronized at {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
