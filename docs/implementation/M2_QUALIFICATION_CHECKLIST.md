# M2 Qualification Checklist

M2 is qualified only when every mandatory item below is green on a commit hash recorded in the milestone evidence.

- [x] Deterministic v1 fixture generator exists.
- [x] Independent Python format oracle accepts canonical fixture.
- [x] Malicious fixture generator is deterministic.
- [x] Independent oracle rejects all committed malformed fixtures.
- [x] Canonical path-order conformance vectors exist.
- [x] TokenizerManifest identity vector exists.
- [x] Reference and optimized DFA implementations are source-separated.
- [x] Resource-limit tests are written.
- [ ] Rust workspace compiles on stable toolchain.
- [ ] rustfmt check passes.
- [ ] Clippy `-D warnings` passes.
- [ ] workspace Rust tests pass.
- [ ] no-std core check passes.
- [ ] Miri checks pass.
- [ ] fuzz smoke campaigns pass without crash/UB/unbounded behavior.
- [ ] Linux/Windows/macOS fixture regeneration is byte-identical.
- [ ] cross-platform canonical Rust outputs match.

Until the unchecked items pass, status is **implemented, not qualified**.
