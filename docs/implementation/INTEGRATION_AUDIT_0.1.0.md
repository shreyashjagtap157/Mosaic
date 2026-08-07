# Mosaic Integration Audit — 0.1.0

Date: 2026-08-07

## Verdict

The implemented tokenizer core is integrated in the intended architectural direction after restructuring the repository. The release runtime now lives under `native/`; `conformance/` contains test clients rather than production implementation; top-level build commands drive deterministic fixtures, native build, sanitizers, differential oracles, and release qualification.

## Direction checks

| Architectural requirement | 0.1 status | Evidence/behavior |
|---|---|---|
| Bytes are authoritative | PASS | encode/decode round-trips arbitrary bytes; canonical source mapping uses byte spans |
| Byte fallback / no unknown source | PASS | model pack contains token IDs 0–255 as exact one-byte surfaces |
| Derived Unicode view does not mutate source | PASS | grapheme API returns byte ranges only; invalid UTF-8 is opaque |
| Deterministic integer segmentation | PASS | i32 edge costs, checked i64 path accumulation, deterministic tie ordering |
| Offline generation / runtime execution split | PASS | Python builders create deterministic packs; C runtime only validates/executes |
| Declarative packs | PASS for model + Unicode | runtime loads no executable code from packs |
| Reference/optimized equivalence | PASS for static tokenizer paths tested | Python oracle and native indexed implementation differential tests |
| Streaming/full equivalence | PASS semantically | v0.1 buffers stream until EOF intentionally |
| Incremental/full equivalence | PASS semantically | v0.1 editable document performs full retokenization intentionally |
| One integrated system | PASS for tokenizer core | `mosaic_tokenizer` binds model + Unicode packs and exposes one API |
| Stable cross-language boundary | PASS for native C ABI | C static/shared + C++ clients pass |
| Language-pack plugin architecture | NOT IMPLEMENTED | remains M5A candidate after Wedge Tournament |
| Rich universal consumer projections | NOT IMPLEMENTED | intentionally post-M4/M7 work |
| Rust production runtime | UNQUALIFIED | source exists; toolchain unavailable on current host |

## Corrections made during integration

1. Moved production native runtime out of `conformance/c` into `native/src` and public API into `native/include`.
2. Added top-level build orchestration so fixtures, runtime, tests, sanitizers, and qualification form one system.
3. Fixed sanitizer targets that previously could be skipped because cached binaries made Make consider the targets up to date.
4. Reworked native Viterbi memory from full per-position state to compact backpointers plus rolling costs, reducing a 10 MiB test from roughly 497 MiB RSS to roughly 52–57 MiB while preserving exact output.
5. Removed OpenSSL dependency by embedding a small SHA-256 implementation, leaving only libc as runtime dependency on Linux.
6. Added integrated high-level tokenizer handle, deterministic fingerprint, streaming and editable-document facade operations.
7. Independent format/oracle tests previously caught pack bounds-validation order and DFA reserved/flag validation mismatches; both remain regression cases.

## Remaining direction risks

- The C runtime is the stable reference/native release, while stable Rust remains the planned primary implementation. The two must converge through differential conformance once Rust qualification is available.
- The model fixture is intentionally small and proves mechanics, not production multilingual vocabulary quality.
- Streaming and edits satisfy semantic equivalence but not the eventual bounded/local performance goals.
- M4 must still decide the first product wedge before language, compiler, or assurance breadth receives major investment.
