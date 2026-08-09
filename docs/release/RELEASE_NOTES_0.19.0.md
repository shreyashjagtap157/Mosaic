# Mosaic Tokenizer 0.19.0

Enterprise runtime-policy and immutable serving release.

- Adds explicit source/output/TokenDocument resource ceilings.
- Adds allocation-aware high-level encode/decode enforcement.
- Adds immutable tokenizer sealing for shared serving runtimes.
- Adds semantic fingerprint vs deployment runtime-identity separation.
- Adds lock-free operational counters for calls, bytes, tokens, failures, and resource rejections.
- Propagates source ceilings into buffered streams, editable documents, incremental documents, and resynchronizing documents.
- Adds an eight-thread sealed-runtime concurrency conformance test.

A lifecycle regression found during qualification caused auto editable documents to inherit a zero byte ceiling; LeakSanitizer exposed the resulting early-return leak in the test. The child now snapshots the configured parent ceiling and the detector/document regression remains in the sanitizer suite.
