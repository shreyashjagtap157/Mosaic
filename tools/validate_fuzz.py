#!/usr/bin/env python3
from __future__ import annotations
import argparse, os, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FUZZ_DIR = ROOT / "fuzz"
DEFAULT_TARGETS = [
    "fuzz_source_bytes",
    "fuzz_pack_header",
    "fuzz_pack_v1",
    "fuzz_dfa",
]


def runtime_directories() -> list[Path]:
    candidates: list[Path] = []
    override = os.environ.get("MOSAIC_FUZZ_RUNTIME_DIR")
    if override:
        candidates.append(Path(override))
    for entry in os.environ.get("PATH", "").split(os.pathsep):
        if entry:
            candidates.append(Path(entry))
    for root in (
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm"),
        Path(r"C:\Program Files\LLVM"),
    ):
        if not root.exists():
            continue
        for dll in root.rglob("clang_rt.asan_dynamic-x86_64.dll"):
            candidates.append(dll.parent)
    seen: set[Path] = set()
    ordered: list[Path] = []
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
        except Exception:
            resolved = candidate
        if resolved in seen:
            continue
        seen.add(resolved)
        ordered.append(resolved)
    return ordered


def compiler_directories() -> list[Path]:
    candidates: list[Path] = []
    override = os.environ.get("MOSAIC_FUZZ_CLANG_DIR")
    if override:
        candidates.append(Path(override))
    for entry in os.environ.get("PATH", "").split(os.pathsep):
        if entry:
            candidates.append(Path(entry))
    for root in (
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm"),
        Path(r"C:\Program Files\LLVM"),
    ):
        if not root.exists():
            continue
        for clang in root.rglob("clang.exe"):
            candidates.append(clang.parent)
    seen: set[Path] = set()
    ordered: list[Path] = []
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
        except Exception:
            resolved = candidate
        if resolved in seen:
            continue
        seen.add(resolved)
        ordered.append(resolved)
    return ordered


def fuzz_env() -> dict[str, str]:
    runtime = next((path for path in runtime_directories() if (path / "clang_rt.asan_dynamic-x86_64.dll").exists()), None)
    if runtime is None:
        raise SystemExit(
            "missing clang_rt.asan_dynamic-x86_64.dll; set MOSAIC_FUZZ_RUNTIME_DIR to the directory that contains it"
        )
    compiler = next((path for path in compiler_directories() if (path / "clang.exe").exists()), None)
    if compiler is None:
        raise SystemExit("missing clang.exe; set MOSAIC_FUZZ_CLANG_DIR to the directory that contains it")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(compiler), str(runtime), env.get("PATH", "")])
    return env


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--toolchain", default=os.environ.get("MOSAIC_FUZZ_TOOLCHAIN", "nightly"))
    ap.add_argument("--runs", type=int, default=1000)
    ap.add_argument("targets", nargs="*", default=DEFAULT_TARGETS)
    args = ap.parse_args()

    env = fuzz_env()
    for target in args.targets:
        subprocess.run(
            [
                "cargo",
                f"+{args.toolchain}",
                "fuzz",
                "run",
                target,
                "--",
                f"-runs={args.runs}",
            ],
            cwd=FUZZ_DIR,
            env=env,
            check=True,
        )

    print(f"OK: fuzz targets passed ({', '.join(args.targets)}) with {args.runs} runs each")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
