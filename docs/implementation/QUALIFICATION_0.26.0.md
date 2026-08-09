# Mosaic 0.26.0 Qualification

Date: 2026-08-08

- retained optimization: validated first-byte bucket comparison elision: PASS
- sealed-v0.25 vs v0.26 same-host 10 MiB A/B: 57.1 -> 59.3 MiB/s median, about +3.9%: PASS
- rejected expanded hot-vocabulary experiment: measured regression (~56.7 MiB/s), fully reverted
- GCC strict build: PASS
- GCC ASan/UBSan inherited native suite: PASS
- checkpoint resynchronization sanitizer run (100 edits): PASS
- independent Clang CMake/CTest: 23/23 PASS
- Clang static analyzer: core/cache/executor PASS
- stable C API arbitrary-byte + stream/edit differential: PASS (1004/200/250 cases)
- Python binding source tests: PASS
- deterministic Python wheel build: PASS
- clean-extraction release validation: PASS
- package SHA-256: `95928ee69ad592353e4244697c8df5b7ca97d011e65f51695543bfcde81eb33b`

The optimization changes no pack format, public ABI layout, canonical cost ordering, or token identity. MOS-REQ-023 remains binding.
