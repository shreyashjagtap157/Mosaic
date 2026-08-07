# Repository Layout and Reorganization Record

## Starting state

No Mosaic source repository was available in the implementation environment. The only Mosaic artifacts present were the architecture specification in Markdown/DOCX form. Therefore this bootstrap is a **new clean repository**, not a migration of existing source code.

The Markdown specification was preserved verbatim under:

`docs/spec/MOSAIC_SPECIFICATION_v0.1.md`

Converged decisions made after that specification are recorded separately under:

`docs/spec/CONVERGENCE_ADDENDUM.md`

This prevents later maintainers from confusing historical architecture with implementation amendments.

## Dependency direction

Current M0/M1 direction:

```text
mosaic-core  <──── mosaic-ir
     ▲              ▲
     │              │
     ├──── mosaic-reference
     ├──── mosaic-engine
     │
     └──── mosaic-pack (currently independent; will use shared identity types later)
```

Target principle: lower-level crates never depend on higher-level consumer/runtime crates.

## Why these initial crates exist now

### `mosaic-core`

Contains only byte-coordinate/source invariants that must survive every future product branch. It is `no_std` and has no mandatory allocator.

### `mosaic-ir`

Contains the minimal Core IR identifiers and mappings. Rich structures are intentionally absent until consumers justify them.

### `mosaic-reference`

Correctness oracle. It may be slow and allocate. It must remain simple and independent of production optimizations.

### `mosaic-engine`

Future optimized executor. It begins with a deliberately trivial byte projection solely so differential infrastructure exists from the first implementation step.

### `mosaic-pack`

Begins with a checked parser for the M0 empty fixture. M2 expands it into the deterministic sectioned pack executor.

## Planned crates are not pre-created

The repository deliberately does **not** create empty `mosaic-unicode`, `mosaic-language`, `mosaic-lex`, `mosaic-model`, `mosaic-store`, `mosaic-ffi`, or `mosaic-cli` crates yet. They will be introduced when milestones make their dependency boundaries real. Empty architecture-shaped directories are not implementation progress.
