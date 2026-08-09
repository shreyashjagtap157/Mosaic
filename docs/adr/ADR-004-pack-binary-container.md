# ADR-004: Pack Binary Container

Status: Pack v1 M2 baseline accepted; qualification pending Rust CI  
Date: 2026-08-07

## Decision

Mosaic uses a custom sectioned binary pack container designed for checked parsing and memory mapping.

Production constraints:

- fixed magic and explicit format version;
- little-endian canonical integers;
- explicit offsets/lengths/counts;
- no raw pointers;
- no native Rust/C object serialization;
- canonical section ordering;
- structurally validated before execution;
- signatures never bypass validation.

M0 defines only a 32-byte non-executable fixture header to seed tests. M2 may evolve the format under a new format version rather than pretending the test header is already the eternal storage covenant.
