# Mosaic 0.1.3.8

Mosaic 0.1.3.8 is a compatibility-preserving desktop archive-edit integrity and workflow patch.

## Fixed

- archive inspection and testing now validate compressed-payload checksums and reject malformed size fields before decompression;
- archive payload parsing now rejects unsafe paths, invalid entry types, directory data, impossible counts and lengths, decompressed-length mismatches, and trailing data;
- removing a selected folder also removes its descendants, while true list multi-selection supports removing multiple members in one operation;
- archive removal now writes a collision-free edited copy, preserves the original archive, and refreshes the browser against the new copy;
- stale archive-list selections cannot be applied after the active archive path changes;
- the Windows comparison test now uses an explicit build-local output directory.

## Release Engineering

- the canonical version synchronizer now covers Inno Setup and WiX package metadata;
- a native archive-edit regression test covers the fixed safety and workflow semantics.

## Compatibility

- Product release: 0.1.3.8.
- Native C ABI: 1.1.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- Existing valid Mosaic archive containers remain readable; corrupt archives with invalid checksums or malformed payloads now fail closed.
