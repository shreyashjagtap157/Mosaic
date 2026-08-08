# Mosaic Tokenizer 0.9.0 Release Notes

Mosaic 0.9 adds bounded-processing APIs without changing canonical tokenization semantics.

## New

- exact online Viterbi streaming with survivor-prefix commitment;
- caller-defined unresolved-source byte ceiling and explicit `MOSAIC_ERROR_RESOURCE_LIMIT`;
- exact consumed-byte accounting when a stream reaches its resource ceiling;
- explicit rejection of raw-BPE models by the Viterbi online API rather than semantic approximation;
- callback/visitor Unicode-security scanning that avoids materializing large finding arrays;
- deterministic adversarial online-stream fixture and sanitizer coverage;
- bounded-processing performance/RSS qualification.

## Semantics

Canonical tokenization semantics remain version 2. The public C API advances to 0.7.0.

The existing buffered stream and full-edit document implementations remain supported as semantic reference paths. Genuine local incremental retokenization is the next tokenizer-core phase.
