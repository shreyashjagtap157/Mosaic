# Mosaic Archive Product Specification

Status: draft
Date: 2026-08-23

## Purpose

This specification defines the archive-oriented product line that Mosaic can grow into while remaining one coherent system.

The goal is not to copy another archiver's internals. The goal is to define a native Mosaic archive subsystem that is:

- deterministic;
- embeddable;
- installable on Windows;
- usable from CLI, desktop UI, automation, and agents;
- bounded in memory and CPU use;
- honest about supported capabilities.

## Product boundary

The archive subsystem is a single vertical product stack with shared core logic:

- core codec and archive format library;
- native C ABI and optional language bindings;
- command-line tools;
- desktop application;
- Windows installer and file associations;
- documentation, qualification, and release artifacts.

No layer may silently diverge from the others. If a feature exists in the desktop UI, it must be reachable from the CLI or service contract as well, unless the feature is purely presentation-related.

## Required capabilities

### 1. Archive creation and extraction

- create archives from files and directories;
- extract archives to a target directory;
- preserve paths, timestamps, and selected metadata where supported;
- stream input and output instead of loading whole archives unnecessarily;
- fail closed on malformed or ambiguous inputs.

### 2. Archive editing

- add files to an existing archive;
- remove files from an existing archive;
- update entries in place when the format permits it, otherwise rewrite safely;
- keep archive identity deterministic after edit operations.

### 3. Compression profiles

- `store`;
- `low`;
- `med`;
- `high`;
- `xhigh`;
- `max`;
- `ultra`.

Profiles must be mapped to explicit, documented internal settings. They may not be cosmetic labels.

### 4. Integrity and repair

- test archive integrity;
- surface damaged members precisely;
- repair only when the format can prove a safe reconstruction path;
- never claim a repair succeeded without evidence.

### 5. SFX and distribution

- generate self-extracting packages or equivalent launchable bundles;
- keep SFX generation as a first-class format mode rather than a separate tool path;
- support installer integration for desktop distribution.

### 6. Desktop usability

- archive browser with file list, sizes, ratios, timestamps, and status;
- context actions for extract, test, add, remove, and repair;
- drag-and-drop support when feasible;
- clear error reporting;
- keyboard-friendly operation.

### 7. Integration and automation

- CLI and service API must expose the same core capabilities;
- a machine-readable contract must describe archive operations;
- other agents and apps must be able to call the same core engine without GUI dependence;
- ownership and lifetime rules must be explicit for foreign runtimes.

### 8. Windows packaging

- installer builds from the same staged application tree;
- installer includes desktop app, CLI, shared runtime, docs, and validation tools;
- optional shell integration must be explicit and removable;
- file associations must be reversible.

## Quality requirements

- deterministic behavior for the same inputs and configuration;
- bounded memory growth;
- graceful low-resource failure modes;
- reproducible release artifacts;
- portable architecture that does not assume GPU availability;
- clear versioning and upgrade behavior;
- integration as a single system rather than disconnected utilities.

## Non-goals for the first archive release

- claiming full compatibility with every third-party archive format feature;
- advertising unsupported repair guarantees;
- silent proprietary format cloning;
- GPU dependence;
- unbounded background indexing;
- hidden network services.

## Acceptance criteria

This specification is satisfied when:

- the archive subsystem is implemented once and shared by CLI, desktop, installer, and bindings;
- the product can create, extract, add, remove, test, and repair according to the documented format contract;
- compression profiles are stable and documented;
- SFX generation is part of the release boundary;
- the Windows installer ships the archive desktop product coherently;
- qualification evidence exists for the supported paths.

