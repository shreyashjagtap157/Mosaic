# ADR-013: FFI panic and unwind policy

Status: accepted contract  
Date: 2026-08-07

No panic may unwind across the C ABI. Release FFI artifacts use `panic = "abort"`; recoverable input, validation, resource, and execution failures use explicit status/error values rather than panic. Development-only Rust builds may retain unwinding when useful for diagnosis, but that behavior is not part of the C contract.
