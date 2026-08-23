# Mosaic Integration Guide

Date: 2026-08-23

## Purpose

This guide collects the supported ways to embed Mosaic into another agent, application, or desktop workflow without reading internal implementation files.

## Common integration rules

- Load packs first, then configure runtime limits, then seal the tokenizer before sharing it across threads.
- Treat returned buffers as Mosaic-owned until released with `mosaic_free()` or the wrapper equivalent.
- Prefer streaming APIs when you do not need a complete materialized result.
- Treat `MOSAIC_ERROR_RESOURCE_LIMIT` as a normal bounded-environment outcome, not as an exception to hide.
- Keep pack identities, runtime limits, and deployment configuration explicit.

## C embedding

Use the native C ABI when you want the smallest, most direct integration point.

Recommended flow:

1. Load the model and Unicode packs.
2. Attach optional language, detector, security, normalization, or lexer packs as needed.
3. Apply runtime limits appropriate to the host.
4. Seal the tokenizer before using it across worker threads.
5. Release returned arrays with `mosaic_free()`.

Example:

- [examples/integration/low_memory_embed.c](../../examples/integration/low_memory_embed.c)

## Python embedding

Use the Python binding when the host application already runs in Python or when you want a thin scripting layer over the same native runtime.

Recommended flow:

1. Create the tokenizer from the pack files and the native library.
2. Configure any optional packs.
3. Set low-memory limits when targeting constrained desktops.
4. Seal the tokenizer before sharing it with worker code.
5. Prefer the wrapper context managers so ownership stays clear.

Example:

- [examples/integration/low_memory_embed.py](../../examples/integration/low_memory_embed.py)

## Desktop integration

Use the desktop app when you want an end-user Windows workflow rather than a library integration.

The Windows installer produced by `tools/package_windows_app.ps1` installs:

- the desktop compressor app;
- the desktop self-test tool;
- the tokenizer CLI;
- the comparison harness;
- the shared runtime binaries;
- the public header;
- the reference pack set;
- the release docs.

Recommended flow:

1. Install the Windows package.
2. Launch the desktop compressor from the Start Menu or desktop shortcut.
3. Use the self-test tool for a non-interactive roundtrip check.
4. Use the CLI if you need automation from scripts or agents.

## Agent and service embedding

For another agent, service, or automation pipeline:

- prefer the C ABI for a stable low-level integration contract;
- keep tokenizer lifecycle outside request handlers when possible;
- preconfigure packs and seal the tokenizer during startup;
- use the batch executor for independent inputs and the streaming APIs for progressive work;
- use `mosaicd --print-config` or `GET /v1/config` when you need the resolved service profile for automation or host-adaptive launch decisions;
- use `GET /openapi.json` when you want a compact machine-readable service contract for launchers or agents;
- surface resource-limit responses as explicit, user-visible operational outcomes.

## What not to assume

- Do not assume packs can add hidden token IDs.
- Do not assume unsupported operations will silently degrade.
- Do not assume unsealed tokenizers are safe to share across threads.
- Do not assume desktop workflows replace the native integration APIs.
