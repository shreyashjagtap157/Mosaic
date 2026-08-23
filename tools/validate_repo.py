#!/usr/bin/env python3
"""M0 structural validator usable before the Rust toolchain is available."""

from __future__ import annotations

import hashlib
import struct
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED = [
    "Cargo.toml",
    "rust-toolchain.toml",
    "README.md",
    "docs/spec/MOSAIC_SPECIFICATION_v0.1.md",
    "docs/spec/CONVERGENCE_ADDENDUM.md",
    "docs/implementation/IMPLEMENTATION_PLAN.md",
    "docs/implementation/M0_PLAN.md",
    "docs/implementation/CI_TOPOLOGY.md",
    "docs/implementation/BENCHMARK_MANIFEST_TEMPLATE.toml",
    "docs/implementation/REFERENCE_HARDWARE.md",
    "benches/low_memory_4gb.toml",
    "docs/implementation/WEDGE_TOURNAMENT.md",
    "docs/implementation/PACK_FIXTURE.md",
    "docs/implementation/REPOSITORY_LAYOUT.md",
    "docs/implementation/STATUS.md",
    "docs/VERSIONING_POLICY.md",
    "docs/COMPATIBILITY_POLICY.md",
    "docs/DEPRECATION_POLICY.md",
    "VERSION",
    "docs/implementation/M0_IMPLEMENTATION_REPORT.md",
    "docs/adr/ADR-001-byte-coordinate-system.md",
    "docs/adr/ADR-002-canonical-byte-leaves.md",
    "docs/adr/ADR-003-source-ownership-versioning.md",
    "docs/adr/ADR-004-pack-binary-container.md",
    "docs/adr/ADR-005-pack-exact-identity.md",
    "docs/adr/ADR-006-dependency-resolution-lock-graph.md",
    "docs/adr/ADR-007-canonical-cost-representation.md",
    "docs/adr/ADR-008-canonical-path-total-order.md",
    "docs/adr/ADR-009-unicode-version-pinning.md",
    "docs/adr/ADR-010-tokenizer-manifest-identity.md",
    "docs/adr/ADR-011-c-abi-opacity.md",
    "docs/adr/ADR-012-ffi-ownership-lifetimes.md",
    "docs/adr/ADR-013-ffi-panic-policy.md",
    "docs/adr/ADR-014-resource-policy-schemas.md",
    "docs/adr/ADR-015-routing-priority.md",
    "fixtures/packs/empty-v0.mpack",
    "fixtures/packs/empty-v0.expected.toml",
    "fixtures/packs/m2-v1.mpack",
    "fixtures/packs/m2-v1.expected.toml",
    "fixtures/conformance/path-order-v1.toml",
    "fixtures/conformance/tokenizer-manifest-v1.toml",
    "docs/implementation/PACK_FORMAT_v1.md",
    "docs/implementation/M2_PLAN.md",
    "docs/implementation/M1_IMPLEMENTATION_REPORT.md",
    "docs/implementation/M2_IMPLEMENTATION_REPORT.md",
    "docs/implementation/M2_QUALIFICATION_CHECKLIST.md",
    "docs/implementation/M2_QUALIFICATION_EVIDENCE.md",
    "docs/implementation/QUALIFICATION.md",
    "tools/qualify.py",
    "tools/validate_manifest_identity.py",
    "tools/validate_rust_structure.py",
]


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def check_files() -> None:
    missing = [path for path in REQUIRED if not (ROOT / path).is_file()]
    if missing:
        fail("missing required files: " + ", ".join(missing))


def check_toml() -> None:
    for path in [
        ROOT / "Cargo.toml",
        ROOT / "rust-toolchain.toml",
        ROOT / "docs/implementation/BENCHMARK_MANIFEST_TEMPLATE.toml",
        ROOT / "benches/low_memory_4gb.toml",
        ROOT / "fixtures/packs/m2-v1.expected.toml",
        ROOT / "fixtures/conformance/path-order-v1.toml",
        ROOT / "fixtures/conformance/tokenizer-manifest-v1.toml",
    ]:
        with path.open("rb") as handle:
            tomllib.load(handle)

    for path in (ROOT / "crates").glob("*/Cargo.toml"):
        with path.open("rb") as handle:
            tomllib.load(handle)


def check_fixture() -> None:
    data = (ROOT / "fixtures/packs/empty-v0.mpack").read_bytes()
    if len(data) != 32:
        fail(f"empty fixture must be 32 bytes, got {len(data)}")
    if data[:8] != b"MOSPACK\x00":
        fail("empty fixture magic mismatch")
    major, minor, header_len, flags = struct.unpack_from("<HHHH", data, 8)
    file_len = struct.unpack_from("<Q", data, 16)[0]
    section_count = struct.unpack_from("<I", data, 24)[0]
    reserved = struct.unpack_from("<I", data, 28)[0]
    expected = (0, 1, 32, 1, 32, 0, 0)
    actual = (major, minor, header_len, flags, file_len, section_count, reserved)
    if actual != expected:
        fail(f"empty fixture fields mismatch: {actual!r}")

    subprocess.run(
        [sys.executable, str(ROOT / "tools/generate_empty_pack.py"), "--check"],
        check=True,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
    )


def check_m2_fixture() -> None:
    for command in [
        [sys.executable, str(ROOT / "tools/build_m2_fixture.py"), "--check"],
        [sys.executable, str(ROOT / "tools/generate_m2_malformed.py"), "--check"],
        [sys.executable, str(ROOT / "tools/validate_m2_fixture.py")],
        [sys.executable, str(ROOT / "tools/validate_path_order.py")],
        [sys.executable, str(ROOT / "tools/validate_manifest_identity.py")],
        [sys.executable, str(ROOT / "tools/validate_rust_structure.py")],
    ]:
        subprocess.run(command, check=True, cwd=ROOT, stdout=subprocess.DEVNULL)
    data = bytearray((ROOT / "fixtures/packs/m2-v1.mpack").read_bytes())
    if data[:8] != b"MOSPACK\x00" or len(data) < 96:
        fail("M2 fixture header mismatch")
    declared = bytes(data[48:80])
    data[48:80] = bytes(32)
    if hashlib.sha256(data).digest() != declared:
        fail("M2 fixture canonical content hash mismatch")


def check_convergence_requirements() -> None:
    text = (ROOT / "docs/spec/CONVERGENCE_ADDENDUM.md").read_text(encoding="utf-8")
    for req in ["MOS-REQ-021", "MOS-REQ-022", "MOS-REQ-023", "MOS-REQ-024", "MOS-REQ-025"]:
        if req not in text:
            fail(f"missing converged requirement {req}")


def check_product_versioning() -> None:
    for command in [
        [sys.executable, str(ROOT / "tools/set_version.py"), "--check"],
        [sys.executable, str(ROOT / "tools/generate_artifact_checksums.py"), "--check"],
        [sys.executable, str(ROOT / "tools/validate_release_matrix.py")],
        [sys.executable, str(ROOT / "tools/validate_open_gates.py")],
    ]:
        subprocess.run(command, check=True, cwd=ROOT, stdout=subprocess.DEVNULL)


def main() -> int:
    check_files()
    check_toml()
    check_fixture()
    check_m2_fixture()
    check_convergence_requirements()
    check_product_versioning()
    subprocess.run(
        [sys.executable, str(ROOT / "tools/generate_artifact_checksums.py"), "--check"],
        check=True,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
    )
    digest = hashlib.sha256((ROOT / "fixtures/packs/empty-v0.mpack").read_bytes()).hexdigest()
    print("OK: M0 repository structure and four-part product versioning validated")
    print(f"OK: empty fixture SHA-256 {digest}")
    print("NOTE: Rust compile/test gates were not executed by this validator")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
