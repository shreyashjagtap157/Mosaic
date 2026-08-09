# Mosaic Tokenizer 0.23.0

Mosaic 0.23.0 adds bounded reusable parallel batch execution for enterprise preprocessing and serving.

- fixed worker pool and bounded FIFO task queue;
- concurrent batch submissions with input-order-preserving results;
- sealed-tokenizer requirement for immutable shared semantics;
- per-batch item and byte resource ceilings;
- per-item error isolation and executor metrics;
- exact decode/order checks under forced queue pressure and concurrent callers;
- sanitizer, Clang, static-analysis, and clean-package qualification.

Single-document canonical tokenization is unchanged. The executor parallelizes independent inputs and delegates each item to the existing high-level tokenizer path.
