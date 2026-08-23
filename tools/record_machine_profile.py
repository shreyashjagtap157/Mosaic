#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "benches" / "low_memory_4gb.runs" / "machine_profile.toml"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def run_text(command: list[str]) -> str:
    return subprocess.check_output(command, text=True).strip()


def windows_cpu() -> str:
    try:
        return run_text(["wmic", "cpu", "get", "Name", "/value"]).split("=", 1)[1].strip()
    except Exception:
        return platform.processor() or "unknown"


def windows_memory_bytes() -> int:
    try:
        text = run_text(["wmic", "ComputerSystem", "get", "TotalPhysicalMemory", "/value"])
        return int(text.split("=", 1)[1].strip())
    except Exception:
        return 0


def linux_cpu() -> str:
    for key in ("/proc/cpuinfo", "/etc/os-release"):
        try:
            text = Path(key).read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue
        if "model name" in text:
            for line in text.splitlines():
                if line.lower().startswith("model name"):
                    return line.split(":", 1)[1].strip()
    return platform.processor() or "unknown"


def linux_memory_bytes() -> int:
    try:
        for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("MemTotal:"):
                parts = line.split()
                return int(parts[1]) * 1024
    except Exception:
        pass
    return 0


def profile() -> dict[str, str]:
    system = platform.system()
    release = platform.release()
    version = platform.version()
    machine = platform.machine()
    cpu = platform.processor() or "unknown"
    memory = 0
    if system == "Windows":
        cpu = windows_cpu()
        memory = windows_memory_bytes()
    elif system == "Linux":
        cpu = linux_cpu()
        memory = linux_memory_bytes()

    return {
        "manufacturer_model": os.environ.get("MOSAIC_MACHINE_MODEL", "unknown"),
        "cpu_model": cpu,
        "microcode": os.environ.get("MOSAIC_MICROCODE", "unknown"),
        "sockets": os.environ.get("MOSAIC_SOCKETS", "unknown"),
        "physical_cores": str(os.cpu_count() or 0),
        "logical_cores": str(os.cpu_count() or 0),
        "isa_extensions": os.environ.get("MOSAIC_ISA_EXTENSIONS", ""),
        "memory_bytes": str(memory),
        "storage_device_model": os.environ.get("MOSAIC_STORAGE_MODEL", "unknown"),
        "os_kernel_build": f"{system} {release} {version}",
        "power_governor": os.environ.get("MOSAIC_POWER_GOVERNOR", "unknown"),
        "virtualization": os.environ.get("MOSAIC_VIRTUALIZATION", "unknown"),
        "compiler": os.environ.get("CC", "unknown"),
        "linker": os.environ.get("LD", os.environ.get("LINK", "unknown")),
        "rust_toolchain": os.environ.get("RUSTUP_TOOLCHAIN", "unknown"),
        "build_flags": os.environ.get("MOSAIC_BUILD_FLAGS", "unknown"),
        "thread_affinity": os.environ.get("MOSAIC_THREAD_AFFINITY", "unknown"),
        "throttling_notes": os.environ.get("MOSAIC_THROTTLING_NOTES", "none"),
    }


def write_toml(path: Path, data: dict[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["[machine]"]
    for key, value in data.items():
        if value.isdigit():
            lines.append(f"{key} = {value}")
        else:
            lines.append(f'{key} = "{value.replace(chr(34), chr(92)+chr(34))}"')
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Record a benchmark machine profile for constrained runs.")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    out = Path(args.output)
    write_toml(out, profile())
    print(f"OK: wrote machine profile to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
