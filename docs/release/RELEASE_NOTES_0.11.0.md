# Mosaic Tokenizer 0.11.0

0.11.0 adds exact forward checkpoint resynchronization for weighted-Viterbi editable documents while retaining the 0.10 safe-prefix engine as a simpler exact fallback/reference.

A corrected and compact implementation passed randomized full-tokenization differentials, ASan/UBSan, GCC/Clang qualification, static analysis, and the existing Unicode/language/detector/security/normalization/online-stream regression suites. An early release candidate exposed a use-after-free in post-transaction metrics; the permanent sanitizer reproducer caught it before release. The resync token cache was then compacted from public 24-byte token records to an internal 8-byte record after a 10 MiB memory benchmark exposed unnecessary metadata overhead.

Raw-BPE checkpoint restart remains explicitly unsupported. No canonical tokenization semantics changed; semantics version remains 2.
