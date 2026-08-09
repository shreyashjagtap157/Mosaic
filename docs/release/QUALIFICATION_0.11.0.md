# Mosaic Tokenizer 0.11.0 Qualification

Required release gates:

- all pre-0.11 deterministic pack, Unicode, normalization, language, detector, security, online-stream, compatibility, and incremental regressions;
- 500 randomized resync edit/full ID differentials in the normal native suite;
- ASan/UBSan resync transaction stress;
- explicit-language resync snapshot;
- raw-BPE unsupported/NULL-handle contract;
- Clang independent resync client;
- Release-mode CMake/CTest resync client;
- Clang static analyzer on the native runtime;
- clean-extraction external C client using the resync API;
- 10 MiB middle-edit benchmark: <2% reprocessed source, >=3x speedup, combined comparison RSS <=256 MiB.

Reference-candidate benchmark before final packaging: 65,575 bytes reprocessed of 10 MiB (0.625%), approximately 8.1x update speedup, approximately 154 MiB combined benchmark peak RSS including the full-tokenization oracle and duplicate ID arrays.
