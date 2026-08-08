# Mosaic Tokenizer 0.9.0 Qualification

Release candidate qualification requires:

- deterministic regeneration of every canonical pack plus `online-adversarial-v1.mpack`;
- GCC and Clang strict-warning builds;
- ASan/UBSan native tests;
- 500 arbitrary-byte/chunk online-stream differentials against complete-buffer token IDs;
- explicit language-specialization stream equivalence;
- raw-BPE online rejection;
- adversarial pending-byte resource-limit behavior;
- security visitor/array finding equivalence and callback-abort behavior;
- existing Unicode 17, normalization 16, language, detector, BPE and malformed-pack regressions;
- bounded-processing benchmark over 10 MiB.

Reference-host bounded-processing measurements during development were approximately 50 MiB/s for exact online Viterbi with a 54-byte maximum unresolved frontier on the mixed fixture, and 27 MiB/s for security visitor scanning, both with under 13 MiB RSS in a harness that itself retains the 10 MiB input buffer. Final qualification records the release-candidate measurements.

## Sealed reference-host measurements

- exact online Viterbi: 50.0 MiB/s median on the 10 MiB mixed fixture; maximum unresolved source 54 bytes under a 1 MiB cap; maximum RSS 12.6 MiB in a harness retaining the 10 MiB input;
- Unicode-security visitor: 27.0 MiB/s median; maximum RSS 11.6 MiB while delivering approximately 2.34 million findings through the callback;
- base whole-buffer tokenizer regression: remains above the 20 MiB/s release floor and below 128 MiB RSS;
- language specialization controlled fixture: 27.3% fewer tokens with 8.1% median runtime overhead;
- detector auto-routing worst reference-profile median overhead: 20.6%;
- CMake Release: 9/9 tests passed;
- GCC/ASan/UBSan online and visitor tests passed;
- Clang online/security builds and Clang static analyzer passed with no diagnostics;
- clean-extraction release package passed external static-client use of online streaming and security visitor APIs.
