# Mosaic Python Binding 0.25

The supported Python package is a thin `ctypes` wrapper over the stable Mosaic C ABI. It contains no tokenizer algorithm or pack interpretation logic.

## Library resolution

Resolution order is explicit `library_path`, `MOSAIC_LIBRARY`, a native library adjacent to the packaged release tree, then the platform library search path. Production services SHOULD pass an absolute library path or control `MOSAIC_LIBRARY`.

## Ownership

All buffers returned by Mosaic are copied into immutable Python values before `mosaic_free()`. Opaque native handles are owned by Python context-manager objects with idempotent `close()` and GC fallback.

## Surface

0.25 exposes integrated tokenizer pack attachment, exact encode/decode and token spans, automatic language routing, grapheme/security/normalization/lexer views, TokenDocument creation and cold serialization, runtime limits/sealing/metrics, bounded batch execution, and the metadata-only observer.

Observer callbacks are retained strongly for the tokenizer lifetime. Exceptions are caught inside the `ctypes` callback and exposed through `Tokenizer.observer_exception`; they are never allowed to unwind through C.

## Distribution

The binding builds as a deterministic pure-Python wheel. The wheel intentionally does not bundle a second native runtime. The Mosaic release tarball ships the wheel beside the qualified `libmosaic` artifact, keeping one native implementation and one ABI.
