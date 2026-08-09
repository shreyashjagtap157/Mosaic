# Mosaic 0.25.0 Qualification

- source-tree Python binding unittest: PASS, Python 3.13
- arbitrary bytes exact round trip: PASS
- source-span partition: PASS
- rich TokenDocument cold round trip: PASS
- detection/security/normalization/lexing wrappers: PASS
- sealed runtime/resource errors: PASS
- 200-item bounded parallel batch: PASS
- observer callback + exception containment: PASS
- 50 create/close/double-close lifecycle loops + GC: PASS
- deterministic wheel rebuild: PASS, byte-identical SHA-256
- clean wheel installation target: PASS
- packaged wheel using packaged `libmosaic`: PASS
