# Mosaic Archive Product Plan

Status: draft
Date: 2026-08-23

## Goal

Build Mosaic archive functionality as one integrated subsystem that is usable from:

- native libraries;
- CLI tooling;
- desktop apps;
- the Windows installer;
- external agents and host applications.

## Integration rule

One core archive engine must back every surface.

That means:

- the GUI must not contain private archive logic;
- the CLI must not diverge from the desktop feature set;
- the installer must only package already-verified binaries and assets;
- bindings must call the same contract the desktop app uses.

## Suggested delivery order

### Phase 1: Core contract

- define the archive format versioning rules;
- define deterministic metadata and compression profile semantics;
- define add/remove/test/repair behavior;
- define error taxonomy and ownership rules;
- define the command and API surface shared by all frontends.

### Phase 2: Engine implementation

- implement create/extract paths;
- implement archive listing and verification;
- implement member add/remove/update;
- implement repair logic only for cases that are provably reconstructable;
- implement SFX generation mode.

### Phase 3: Product surfaces

- add CLI commands;
- add desktop UI screens and actions;
- add context menu or shell integration where appropriate;
- wire bindings and automation helpers;
- expose the same capabilities in docs and examples.

### Phase 4: Qualification

- round-trip tests;
- malformed archive tests;
- low-memory tests;
- editing and repair regression tests;
- installer packaging validation;
- end-to-end desktop smoke tests.

## Implementation constraints

- prefer streaming over buffering;
- keep memory bounded on 4 GB systems;
- use explicit progress and cancellation hooks;
- preserve determinism across runs;
- avoid feature islands that exist only in one frontend.

## Current status

This plan is not yet implemented. It establishes the required single-system structure before code is added.

