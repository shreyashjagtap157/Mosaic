# Mosaic 0.23.0 Qualification Evidence

Date: 2026-08-08

- strict GCC build and inherited runtime regression suite;
- ASan/UBSan executor test with a four-worker pool and three-slot queue;
- eight concurrent caller batches, 800 exact arbitrary-byte round trips, zero ordering mismatches;
- unsealed tokenizer rejection, per-item resource failure isolation, batch item/input ceilings;
- exact executor metrics after concurrent execution;
- 22/22 independent Clang/CTest cases;
- Clang static analysis of `mosaic_executor.c`;
- clean-extraction external static consumer using the packaged executor;
- canonical tokenizer fingerprints unchanged by parallel execution.
