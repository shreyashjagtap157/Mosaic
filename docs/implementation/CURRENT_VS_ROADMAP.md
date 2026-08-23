# Mosaic Current vs Roadmap

Date: 2026-08-23

## Purpose

This table separates current shipped capability from planned or research-stage work so the product boundary is honest and easy to read.

## Current

| Area | Shipped capability |
| --- | --- |
| Native runtime | Exact arbitrary-byte tokenization, Unicode/source mapping, streaming and incremental APIs, TokenDocument projections, bounded parallel execution, cache and registry control-plane support, trust verification, and observability |
| Python binding | Native ABI-backed wrapper with ownership-safe helpers, low-memory support, and integration examples |
| Windows desktop delivery | Installer packaging for the desktop compressor, self-test, tokenizer CLI, comparison harness, shared runtime binaries, packs, and release docs |
| Low-resource operation | Explicit low-memory defaults, streaming-first guidance, bounded queues, and resource-limit handling |
| Embedding guidance | Unified integration guide for C, Python, desktop, agent, and service consumers |
| Archive product planning | Single-system archive product spec and implementation plan now defined; engine work remains to be built |

## Roadmap

| Area | Roadmap status |
| --- | --- |
| Stable-generation `1.0.0.0` | Not yet claimed; external gates remain open |
| macOS qualification | External runner work remains required |
| ARM64 / non-x86-64 qualification | External runner work remains required |
| Full cargo-fuzz campaigns | External runner work remains required |
| ThreadSanitizer / platform race detectors | External runner work remains required where supported |
| Adaptive byte-patch LLMs | Research only |
| GPU acceleration | Research only |
| Learned statistical span classifiers | Research only |
| Very large community pack catalogs | Research only |
| Scanner VM | Research only |
| Archive engine and desktop archive manager | Planned new subsystem; not yet implemented |

## Reading rule

- If an item is in the current column, it is part of the shipped product boundary.
- If an item is in the roadmap column, it is not a current release claim.
