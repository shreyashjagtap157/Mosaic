# Mosaic Tokenizer 0.10.0 Qualification

Required gates include all 0.9 regressions plus:

- 1,000 randomized arbitrary-byte edit sequences comparing the incremental document to fresh whole-buffer token IDs after every edit;
- exact source-copy verification;
- explicit language-specialized incremental differential;
- raw-BPE unsupported-path contract;
- ASan/UBSan incremental client;
- Clang independent incremental client;
- near-end 10 MiB edit: no more than 2% source reprocessed and at least 2x faster than fresh whole-buffer tokenization on the reference host.

Reference-host development result: 1.000% source reprocessed for the 99%-position edit and approximately 4.6x speedup; uniformly random edit positions averaged about 50.4% reprocessing, accurately reflecting the current restart-to-EOF design.

## Sealed reference-host evidence

- 1,000 randomized edit/full ID differentials passed under normal and ASan/UBSan builds;
- uniformly random edit positions reprocessed 50.4% of source on average;
- 10 MiB edit at the 99% position reprocessed 104,895 bytes (1.000%), reused 10,380,865 prefix bytes, and measured approximately 4.47x faster than fresh whole-buffer tokenization;
- Clang static analyzer passed after incremental transaction control flow was made explicitly non-null;
- CMake Release includes the incremental client;
- clean-extraction package external C client exercised the incremental API;
- release archive SHA-256: `760abea190251c9204b13cc264e2b720e66965941a584463038fceaf5c118092`.
