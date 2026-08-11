# External Qualification Gates

Date: 2026-08-11

This checklist records the qualification work that must be completed on external CI runners or hardware before Mosaic can claim the next stable generation. The local repository already provides the commands and workflow hooks below; the actual evidence must come from the named runners.

## Required external gates

| Gate | Runner / matrix | Canonical command(s) | Evidence |
| --- | --- | --- | --- |
| macOS native CI execution | `macos-latest` in `.github/workflows/release-qualification.yml` | `python tools/validate_release_readiness.py --build-release` plus the native cross-platform workflow steps | Job logs and uploaded artifacts from the macOS run |
| full cargo-fuzz jobs | Ubuntu nightly fuzz job in `.github/workflows/release-qualification.yml` or a dedicated long-run fuzz workflow | `cargo fuzz run fuzz_source_bytes`, `cargo fuzz run fuzz_pack_header`, `cargo fuzz run fuzz_pack_v1`, `cargo fuzz run fuzz_dfa` with production campaign durations | Fuzz logs, crash corpus state, and retained artifacts from the full campaign |
| ThreadSanitizer or platform-specific race detectors | Supported runners/toolchains in CI | the supported race-detector build/test invocation for the platform | Sanitizer logs and any captured race reports |
| non-x86-64/ARM64 qualification required by the final support matrix | Additional supported architecture runners | the same release and qualification commands on the target architecture | Job logs and artifacts from the architecture-specific runner |

## Local surrogates

Local runs can reproduce the command shape but do not substitute for the external evidence above:

- `python tools/validate_release_readiness.py`
- `python tools/validate_release_readiness.py --skip-miri --skip-package`
- `make release-readiness`
- `make release-readiness-fast`

These commands are useful for preflight and incremental development, but the external gates remain open until the corresponding CI jobs prove them on the target runner classes.
