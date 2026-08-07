# ADR-011: C ABI opacity

Status: accepted contract, implementation deferred until core types stabilize  
Date: 2026-08-07

The C ABI exposes opaque handles, status codes, and explicit views. Internal Rust structure layouts are never ABI. No public C structure embeds Rust-owned pointers whose representation or lifetime is implicit. ABI freezing occurs only for a public release milestone, not merely because an experimental function happens to exist.
