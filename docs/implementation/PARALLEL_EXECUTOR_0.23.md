# Mosaic Parallel Batch Executor — 0.23

## Scope

Mosaic 0.23 adds reusable bounded parallel execution across independent inputs. It intentionally does **not** split one source at arbitrary byte boundaries before canonical model tokenization; doing so could change Viterbi results. Parallel batch workers call the same sealed high-level tokenizer path used by single-input execution.

## Executor contract

`mosaic_executor` owns a fixed worker pool and bounded FIFO work queue. Configuration binds:

- worker count, hard-capped at 256;
- queue capacity, hard-capped at 1,048,576 tasks;
- maximum items per submitted batch;
- maximum aggregate input bytes per batch.

A tokenizer MUST be sealed before submission. This guarantees the semantic pack set and deployment policy cannot mutate while workers share it. Tokenizer metrics remain lock-free/atomic.

Multiple caller threads may submit batches concurrently. The pool serializes queue access but processes jobs across workers. Results are always returned in original input order. Per-item failures do not invalidate successful siblings; scheduling/configuration failures are returned by the batch call itself.

The caller must not destroy an executor while a batch call is active. `mosaic_executor_free()` drains queued work and joins workers during ordinary quiescent shutdown.

## Enterprise failure semantics

- invalid/null input tuple: batch rejected before submission;
- unsealed tokenizer: `MOSAIC_ERROR_STATE`;
- batch item/byte ceiling exceeded: `MOSAIC_ERROR_RESOURCE_LIMIT` before output allocation;
- item-level tokenizer policy failure: recorded in that result only;
- queue pressure: producer waits on bounded capacity rather than allocating an unbounded queue;
- result buffers use ordinary Mosaic ownership and are released by `mosaic_batch_results_free()`.

## Qualification

The conformance test uses a four-worker executor with a three-task queue to force queue backpressure, verifies a partial resource-limit failure, and submits eight batches concurrently. It decodes every successful result back to its exact original bytes and checks executor metrics. The same test runs under ASan/UBSan and an independent Clang/CTest build.
