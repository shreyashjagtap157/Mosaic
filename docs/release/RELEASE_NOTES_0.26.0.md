# Mosaic Tokenizer 0.26.0

Mosaic 0.26.0 is a measured performance-hardening release. It removes a redundant first-byte comparison from the weighted Viterbi candidate loop. The first-byte index is validated at pack load, so candidates in a bucket already prove equality at byte zero.

A larger load-time expanded-vocabulary representation was prototyped and rejected after it benchmarked slower than the compact pack-backed representation. The release keeps the simpler representation and the measurable low-risk optimization only.

Qualification includes the complete inherited native sanitizer suite, 23/23 independent Clang tests, static analysis, arbitrary-byte C/Python differentials, and clean release-package validation.
