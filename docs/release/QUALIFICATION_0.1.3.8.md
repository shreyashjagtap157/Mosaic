# Mosaic 0.1.3.8 Qualification

Status: candidate patch qualification complete for the desktop archive-edit integrity and workflow fixes.

## Verification

- canonical four-part version synchronization, including Inno Setup and WiX metadata: **PASS**;
- strict-warning Windows x86-64 Release build: **PASS**;
- complete native Windows CTest suite, 25/25 tests: **PASS**;
- desktop standalone and integrated self-test round-trips against the arbitrary-byte golden fixture: **PASS**;
- focused archive-edit regression covering recursive selection, collision-safe output naming, path traversal rejection, strict payload consumption, and decompressed-length rejection: **PASS**.

## Notes

This candidate patch does not change the native C ABI, tokenizer semantics, or frozen pack contracts. It does not change the documented external stable-generation gates.
