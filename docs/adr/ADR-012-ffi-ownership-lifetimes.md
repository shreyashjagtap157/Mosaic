# ADR-012: FFI ownership and lifetimes

Status: accepted contract  
Date: 2026-08-07

Every FFI input/output is documented as COPIED, BORROWED, TRANSFERRED, or SHARED. The simplest source-construction API copies. Zero-copy external sources require an explicit retain/release or release-callback contract so Mosaic cannot outlive caller memory silently. Rust-native borrowing remains compile-time checked.
