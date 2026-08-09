# Mosaic 0.22.0 Qualification Evidence

Date: 2026-08-08

## Passed

- strict GCC build with `-Wall -Wextra -Wpedantic -Werror`;
- complete inherited native suite;
- ASan/UBSan including TokenDocument serialization;
- 21/21 independent Clang/CTest cases;
- Clang static analysis for core, cache, and trust translation units;
- 1,004 arbitrary-byte C API cases, 200 stream chunkings, 250 edit/full differentials;
- Python/C vocabulary, Unicode, language, detector, authoring, BPE, security, and packed-model differentials;
- 15 authenticated malformed `MSTIRD01` records rejected;
- default and caller-lowered Token IR resource ceilings;
- byte-identical deserialize/reserialize canonical record;
- content-addressed registry concurrency/corruption regression;
- clean-extraction release validation and external static C consumer using serialization/deserialization.

The top-level monolithic `make test` command exceeded the conversation command timeout only after the complete native suite and early Python suites were already green. The remaining individual gates were rerun separately and passed; no failing assertion was masked by the timeout.
