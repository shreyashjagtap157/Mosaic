# Mosaic Tokenizer 0.10.0 Release Notes

Mosaic 0.10 adds an exact incremental weighted-Viterbi document API.

- cached canonical token sequence with safe-prefix reuse after edits;
- transactional edits: a failed retokenization leaves the previous document untouched;
- explicit byte-accounting metrics for reprocessed and reused source prefixes;
- 1,000 randomized edit/full differentials plus explicit language-specialization coverage;
- raw-BPE rejection rather than unproven incremental merge behavior;
- 10 MiB near-end edit benchmark and performance gate.

Canonical tokenization semantics remain version 2. The C API advances to 0.8.0.

This release does not yet claim forward resynchronization. Middle-of-file edits still retokenize from the safe restart boundary to EOF. Checkpoint/block resynchronization remains required for the later universal-platform milestone.
