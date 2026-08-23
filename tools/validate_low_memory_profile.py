#!/usr/bin/env python3
from __future__ import annotations

import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "benches" / "low_memory_4gb.toml"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def require(data: dict, path: str, expected) -> None:
    cur = data
    parts = path.split(".")
    for key in parts[:-1]:
        if key not in cur or not isinstance(cur[key], dict):
            fail(f"missing section for {path}")
        cur = cur[key]
    leaf = parts[-1]
    if leaf not in cur:
        fail(f"missing field {path}")
    if cur[leaf] != expected:
        fail(f"unexpected {path}: {cur[leaf]!r} != {expected!r}")


def main() -> int:
    if not MANIFEST.is_file():
        fail(f"missing constrained benchmark manifest: {MANIFEST}")
    data = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))

    require(data, "schema_version", 1)
    require(data, "benchmark_family", "B3")
    require(data, "hardware.profile_id", "low-memory-4gb")
    require(data, "hardware.memory_bytes", 4294967296)
    require(data, "tokenizer.projection", "low-memory native C ABI")
    require(data, "tokenizer.threads", 1)
    require(data, "baseline.name", "mosaic-low-memory-bench")

    hardware = data.get("hardware", {})
    for field in ("cpu", "physical_cores", "logical_cores", "memory_bytes"):
        if field not in hardware:
            fail(f"missing hardware.{field}")
    measurements = data.get("measurements", {})
    for field in ("wall_time_ns", "peak_rss_bytes", "input_bytes_per_second"):
        if field not in measurements:
            fail(f"missing measurements.{field}")

    print("OK: low-memory benchmark manifest is present and bounded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
