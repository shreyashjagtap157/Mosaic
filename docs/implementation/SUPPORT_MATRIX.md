# Mosaic Support Matrix

Date: 2026-08-23

## Purpose

This matrix summarizes the current support boundary as reflected by repository evidence, qualification notes, and release packaging.

## Core product surfaces

| Surface | Status | Evidence |
| --- | --- | --- |
| Native C runtime | Supported and qualified on exercised hosts | `docs/implementation/STATUS.md`, `docs/implementation/QUALIFICATION_1.0.0.md` |
| Python binding | Supported and qualified on exercised Windows host | `docs/implementation/STATUS.md`, `docs/implementation/PYTHON_BINDING_0.25.md` |
| Windows desktop installer | Supported packaging path | `tools/package_windows_app.ps1`, `packaging/windows/MosaicDesktop.iss` |
| CLI and library packaging | Supported release path | `tools/build_release.py`, `docs/implementation/RELEASE_ENGINEERING_v1.md` |

## Platform boundary

| Platform / target | Status | Notes |
| --- | --- | --- |
| Windows x86-64 | Qualified for the dependency-minimal native path and desktop packaging | Windows CMake/CTest and installer packaging are exercised in repository evidence |
| Linux x86-64 | Qualified for the dependency-minimal and full native paths | See release qualification notes |
| macOS | External qualification gate | Declared but not locally exercised |
| ARM64 / non-x86-64 | External qualification gate | Declared but not locally exercised |

## Runtime and embedding surfaces

| Surface | Status | Notes |
| --- | --- | --- |
| Native C ABI | Supported | Use the integration guide and public header |
| Python wrapper | Supported | Use the integration guide and binding docs |
| Desktop consumer app | Supported on Windows through the installer | Intended for end users and quick verification |
| Agent/service embedding | Supported as a documented integration pattern | Prefer the C ABI or Python wrapper depending on host |
| `mosaicd --low-memory` | Supported and observable | Advertises constrained-desktop profile metadata and lowers service ceilings |
| `GET /openapi.json` | Supported | Machine-readable service schema for launchers, agents, and ops tooling |

## Resource profile guidance

| Profile | Status | Notes |
| --- | --- | --- |
| Low-memory desktop usage | Documented and tested | Use the low-memory defaults and streaming-first APIs |
| 4 GB-class Windows desktops | Explicitly supported by guidance | Bounded queues, sealed tokenizers, and streaming paths are documented |
| GPU dependence | Not required | No GPU is needed for the supported runtime path |

## What is not implied

- This matrix does not claim stable-generation `1.0.0.0`.
- This matrix does not claim macOS or ARM64 qualification is complete.
- This matrix does not turn research items into product commitments.
