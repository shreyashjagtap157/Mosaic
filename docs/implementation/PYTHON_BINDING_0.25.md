# Mosaic Python Binding 0.25

The supported Python package is a thin `ctypes` wrapper over the stable Mosaic C ABI. It contains no tokenizer algorithm or pack interpretation logic.

## Library resolution

Resolution order is explicit `library_path`, `MOSAIC_LIBRARY`, a native library adjacent to the packaged release tree, then the platform library search path. Production services SHOULD pass an absolute library path or control `MOSAIC_LIBRARY`.
The loader also recognizes common preset build-tree locations on Windows so local development and CI can run against the current checkout without extra environment setup.

## Ownership

All buffers returned by Mosaic are copied into immutable Python values before `mosaic_free()`. Opaque native handles are owned by Python context-manager objects with idempotent `close()` and GC fallback.

## Surface

0.25 exposes integrated tokenizer pack attachment, exact encode/decode and token spans, automatic language routing, grapheme/security/normalization/lexer views, TokenDocument creation and cold serialization, runtime limits/sealing/metrics, bounded batch execution, and the metadata-only observer.

For constrained desktops, the binding now exposes explicit low-memory helpers: `Tokenizer.set_low_memory_limits()` and `BatchExecutor.low_memory()`. These map to the native low-resource defaults and are intended for 4 GB-class personal machines or similar bounded environments.

Observer callbacks are retained strongly for the tokenizer lifetime. Exceptions are caught inside the `ctypes` callback and exposed through `Tokenizer.observer_exception`; they are never allowed to unwind through C.

## Distribution

The binding builds as a deterministic pure-Python wheel. The wheel intentionally does not bundle a second native runtime. The Mosaic release tarball ships the wheel beside the qualified `libmosaic` artifact, keeping one native implementation and one ABI.

## Stateful online streaming

`Tokenizer.online_stream(max_pending_bytes)` returns an `OnlineStream` backed directly by `mosaic_online_stream`. `push()` returns `(consumed_bytes, committed_ids)` and therefore preserves native partial-consumption/resource-limit semantics; `finish()` emits the final canonical suffix. `pending_bytes` exposes the current unresolved-source bound. The Python layer performs no segmentation/tokenization logic of its own.
