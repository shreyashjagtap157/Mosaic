#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "benches" / "low_memory_4gb.runs" / "latest.toml"
BENCH = ROOT / "build" / "preset-core-release" / "native" / "mosaic-low-memory-bench.exe"
MODEL = ROOT / "fixtures" / "packs" / "model-v2.mpack"
UNICODE = ROOT / "fixtures" / "packs" / "unicode17-v1.mpack"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def git_rev() -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()


def parse_output(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in text.strip().split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = value
    required = ["bytes", "ids", "elapsed", "peak_rss", "worker_count", "queue_capacity", "max_batch_items"]
    missing = [key for key in required if key not in fields]
    if missing:
        fail("missing benchmark fields: " + ", ".join(missing))
    return fields


def as_int(value: str, field: str) -> int:
    try:
        return int(value)
    except ValueError as exc:
        raise SystemExit(f"FAIL: invalid integer for {field}: {value!r}") from exc


def as_float(value: str, field: str) -> float:
    try:
        return float(value)
    except ValueError as exc:
        raise SystemExit(f"FAIL: invalid float for {field}: {value!r}") from exc


def write_manifest(out: Path, fields: dict[str, str]) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    now = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    text = f"""schema_version = 1
benchmark_family = "B3"
run_id = "low-memory-{now}"
run_timestamp_utc = "{now}"

[hardware]
profile_id = "low-memory-4gb"
cpu = "record actual CPU model"
physical_cores = 0
logical_cores = 0
memory_bytes = 4294967296
isa_extensions = []

[software]
os = "record actual OS"
kernel = "record actual kernel"
rustc = "n/a"
linker = "n/a"
commit = "{git_rev()}"

[build]
profile = "release"
lto = "thin"
codegen_units = 1
features = []

[input]
corpus_id = "fixtures/packs/model-v2.mpack + fixtures/packs/unicode17-v1.mpack"
content_hash = "record corpus hash"
bytes = {as_int(fields['bytes'], 'bytes')}
language_tags = []

[tokenizer]
manifest_hash = "record tokenizer manifest hash"
projection = "low-memory native C ABI"
output_density = "{fields['ids']}"
warm_state = true
threads = {as_int(fields['worker_count'], 'worker_count')}

[baseline]
name = "mosaic-low-memory-bench"
version = "local"
semantic_equivalence_notes = "exact encode/decode plus bounded batch execution under low-memory defaults"

[measurements]
wall_time_ns = {int(round(as_float(fields['elapsed'], 'elapsed') * 1_000_000_000))}
cpu_time_ns = 0
peak_rss_bytes = {as_int(fields['peak_rss'], 'peak_rss')}
allocations = 0
input_bytes_per_second = {as_float(fields['bytes'], 'bytes') / max(as_float(fields['elapsed'], 'elapsed'), 1e-9):.6f}
output_tokens = {as_int(fields['ids'], 'ids')}
"""
    out.write_text(text, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run and record the low-memory benchmark profile.")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    if not BENCH.is_file():
        fail(f"missing benchmark executable: {BENCH}")
    if not MODEL.is_file() or not UNICODE.is_file():
        fail("missing benchmark inputs")
    raw = subprocess.check_output([str(BENCH), str(MODEL), str(UNICODE)], cwd=ROOT, text=True).strip()
    fields = parse_output(raw)
    out = Path(args.output)
    write_manifest(out, fields)
    print(raw)
    print(f"OK: wrote low-memory run manifest to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
