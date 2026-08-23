from __future__ import annotations

import ctypes as C
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Optional, Sequence

from ._ffi import (
    CBatchInput, CBatchResult, CDetection, CDocumentToken, CEvent, CExecutorConfig, CExecutorMetrics,
    CLexToken, CNormalizedView, CObserverConfig, CRange, CRuntimeLimits, CRuntimeMetrics, CSecurityFinding,
    CSpanRoute, CToken, CTokenDocumentInfo, CTokenDocumentOptions, CTokenizerCapabilities, ObserverCallback, load_library,
)
from .service import MosaicdClient, MosaicdClientError

__version__ = "0.1.3.8"

OK = 0
ERROR_RESOURCE_LIMIT = 9
OBSERVE_SUCCESS = 1 << 0
OBSERVE_FAILURE = 1 << 1
OBSERVE_RESOURCE = 1 << 2
TOKEN_DOCUMENT_MODEL = 1 << 0
TOKEN_DOCUMENT_GRAPHEMES = 1 << 1
TOKEN_DOCUMENT_SECURITY = 1 << 2
TOKEN_DOCUMENT_NORMALIZATION = 1 << 3
TOKEN_DOCUMENT_LEXICAL = 1 << 4
TOKEN_DOCUMENT_SEMANTIC = 1 << 5
NORMALIZE_PRESERVE = 0
NORMALIZE_NFD = 1
NORMALIZE_NFC = 2
NORMALIZE_NFKD = 3
NORMALIZE_NFKC = 4
NORMALIZE_NFKC_CASEFOLD = 5


class MosaicError(RuntimeError):
    def __init__(self, status: int, message: str):
        self.status = int(status)
        super().__init__(f"{message}: {status}")


@dataclass(frozen=True)
class Token:
    id: int
    start: int
    length: int
    cost: int = 0


@dataclass(frozen=True)
class Range:
    start: int
    length: int


@dataclass(frozen=True)
class Detection:
    matched: bool
    available: bool
    score: int
    margin: int
    language: str


@dataclass(frozen=True)
class SpanRoute:
    start: int
    length: int
    detection: Detection


@dataclass(frozen=True)
class SecurityFinding:
    kind: int
    script_id: int
    start: int
    length: int


@dataclass(frozen=True)
class LexToken:
    kind: int
    flags: int
    start: int
    length: int


@dataclass(frozen=True)
class NormalizedView:
    data: bytes
    source_spans: tuple[Range, ...]


@dataclass(frozen=True)
class RuntimeLimits:
    max_input_bytes: int
    max_output_tokens: int
    max_token_document_bytes: int


@dataclass(frozen=True)
class RuntimeMetrics:
    encode_calls: int
    decode_calls: int
    bytes_in: int
    bytes_out: int
    tokens_out: int
    failures: int
    resource_rejections: int


@dataclass(frozen=True)
class Event:
    kind: int
    operation: int
    status: int
    sequence: int
    input_units: int
    output_units: int
    resource_limit: int
    tokenizer_fingerprint: bytes


@dataclass(frozen=True)
class BatchResult:
    status: int
    ids: tuple[int, ...]


def _path(path: str | Path) -> bytes:
    return str(Path(path)).encode("utf-8")


def _buffer(data: bytes):
    if not data:
        return None, C.POINTER(C.c_uint8)()
    owner = (C.c_uint8 * len(data)).from_buffer_copy(data)
    return owner, C.cast(owner, C.POINTER(C.c_uint8))


def _detection(value: CDetection) -> Detection:
    return Detection(
        bool(value.matched), bool(value.available), int(value.score), int(value.margin),
        bytes(value.language).split(b'\0', 1)[0].decode(),
    )


class _LibraryBound:
    def __init__(self, lib):
        self._lib = lib

    def _error(self, status: int, operation: str) -> MosaicError:
        raw = self._lib.mosaic_status_string(int(status))
        detail = raw.decode("utf-8", "replace") if raw else f"status {status}"
        return MosaicError(status, f"{operation} failed ({detail})")

    def _check(self, status: int, operation: str) -> None:
        if status != OK:
            raise self._error(status, operation)


class TokenDocument(_LibraryBound):
    def __init__(self, lib, handle):
        super().__init__(lib); self._handle = C.c_void_p(handle.value if isinstance(handle, C.c_void_p) else handle)

    def close(self) -> None:
        if self._handle and self._handle.value:
            self._lib.mosaic_token_document_free(self._handle); self._handle = C.c_void_p()

    def __enter__(self): return self
    def __exit__(self, *_): self.close()
    def __del__(self):
        try: self.close()
        except Exception: pass

    @property
    def info(self):
        info = CTokenDocumentInfo(); self._check(self._lib.mosaic_token_document_get_info(self._handle, C.byref(info)), "TokenDocument.info")
        return {
            "flags": int(info.flags), "source_length": int(info.source_length), "model_token_count": int(info.model_token_count),
            "grapheme_count": int(info.grapheme_count), "security_finding_count": int(info.security_finding_count),
            "normalized_byte_length": int(info.normalized_byte_length), "normalized_unit_count": int(info.normalized_unit_count),
            "lexical_token_count": int(info.lexical_token_count), "semantic_component_count": int(info.semantic_component_count),
            "source_sha256": bytes(info.source_sha256), "tokenizer_fingerprint": bytes(info.tokenizer_fingerprint_sha256),
        }

    @property
    def source(self) -> bytes:
        out = C.POINTER(C.c_uint8)(); n = C.c_size_t()
        self._check(self._lib.mosaic_token_document_copy_source(self._handle, C.byref(out), C.byref(n)), "TokenDocument.source")
        try: return C.string_at(out, n.value) if n.value else b""
        finally: self._lib.mosaic_free(out)

    @property
    def model_tokens(self) -> tuple[Token, ...]:
        out = C.POINTER(CDocumentToken)(); n = C.c_size_t()
        self._check(self._lib.mosaic_token_document_model_tokens(self._handle, C.byref(out), C.byref(n)), "TokenDocument.model_tokens")
        try: return tuple(Token(int(out[i].id), int(out[i].start), int(out[i].length)) for i in range(n.value))
        finally: self._lib.mosaic_free(out)

    def serialize(self) -> bytes:
        out = C.POINTER(C.c_uint8)(); n = C.c_size_t()
        self._check(self._lib.mosaic_token_document_serialize(self._handle, C.byref(out), C.byref(n)), "TokenDocument.serialize")
        try: return C.string_at(out, n.value) if n.value else b""
        finally: self._lib.mosaic_free(out)

    @classmethod
    def deserialize(cls, record: bytes, *, library_path=None) -> "TokenDocument":
        lib = load_library(library_path); owner, ptr = _buffer(record); handle = C.c_void_p()
        status = lib.mosaic_token_document_deserialize(ptr, len(record), C.byref(handle)); del owner
        if status != OK:
            raw = lib.mosaic_status_string(status); raise MosaicError(status, f"TokenDocument.deserialize failed ({raw.decode() if raw else status})")
        return cls(lib, handle)


class OnlineStream(_LibraryBound):
    def __init__(self, lib, handle):
        super().__init__(lib)
        self._handle = C.c_void_p(handle.value if isinstance(handle, C.c_void_p) else handle)
        self._finished = False

    def close(self) -> None:
        if self._handle and self._handle.value:
            self._lib.mosaic_online_stream_free(self._handle)
            self._handle = C.c_void_p()

    def __enter__(self): return self
    def __exit__(self, *_): self.close()
    def __del__(self):
        try: self.close()
        except Exception: pass

    @property
    def pending_bytes(self) -> int:
        if not self._handle or not self._handle.value: return 0
        return int(self._lib.mosaic_online_stream_pending_bytes(self._handle))

    def push(self, data: bytes) -> tuple[int, tuple[int, ...]]:
        if self._finished: raise RuntimeError("online stream is already finished")
        raw=bytes(data); owner,ptr=_buffer(raw); consumed=C.c_size_t(); out=C.POINTER(C.c_uint32)(); n=C.c_size_t()
        status=self._lib.mosaic_online_stream_push(self._handle,ptr,len(raw),C.byref(consumed),C.byref(out),C.byref(n)); del owner
        try: ids=tuple(int(out[i]) for i in range(n.value))
        finally: self._lib.mosaic_free(out)
        if status not in (OK, ERROR_RESOURCE_LIMIT): raise self._error(status,"online_stream.push")
        return int(consumed.value),ids

    def finish(self) -> tuple[int, ...]:
        if self._finished: return ()
        out=C.POINTER(C.c_uint32)(); n=C.c_size_t(); status=self._lib.mosaic_online_stream_finish(self._handle,C.byref(out),C.byref(n))
        self._check(status,"online_stream.finish"); self._finished=True
        try: return tuple(int(out[i]) for i in range(n.value))
        finally: self._lib.mosaic_free(out)


class Tokenizer(_LibraryBound):
    def __init__(self, model_path: str | Path, unicode_path: str | Path, *, library_path=None):
        lib = load_library(library_path); super().__init__(lib); handle = C.c_void_p()
        self._check(lib.mosaic_tokenizer_load_files(_path(model_path), _path(unicode_path), C.byref(handle)), "Tokenizer.load")
        self._handle = handle; self._observer_callback = None; self._observer_exception: Optional[BaseException] = None; self._observer_lock = threading.Lock()

    @property
    def native_version(self) -> str: return self._lib.mosaic_version_string().decode()
    def close(self) -> None:
        if self._handle and self._handle.value:
            self._lib.mosaic_tokenizer_free(self._handle); self._handle = C.c_void_p(); self._observer_callback = None
    def __enter__(self): return self
    def __exit__(self, *_): self.close()
    def __del__(self):
        try: self.close()
        except Exception: pass

    def _pack(self, fn_name: str, path: str | Path) -> "Tokenizer":
        self._check(getattr(self._lib, fn_name)(self._handle, _path(path)), fn_name); return self
    def add_language(self, path): return self._pack("mosaic_tokenizer_add_language_file", path)
    def set_detector(self, path): return self._pack("mosaic_tokenizer_set_detector_file", path)
    def set_security(self, path): return self._pack("mosaic_tokenizer_set_security_file", path)
    def set_normalization(self, path): return self._pack("mosaic_tokenizer_set_normalization_file", path)
    def set_lexer(self, path): return self._pack("mosaic_tokenizer_set_lexer_file", path)

    @property
    def fingerprint(self) -> bytes:
        out=(C.c_uint8*32)(); self._check(self._lib.mosaic_tokenizer_fingerprint(self._handle,out),"fingerprint"); return bytes(out)
    @property
    def runtime_identity(self) -> bytes:
        out=(C.c_uint8*32)(); self._check(self._lib.mosaic_tokenizer_runtime_identity(self._handle,out),"runtime_identity"); return bytes(out)
    @property
    def capabilities(self) -> int:
        c=CTokenizerCapabilities(C.sizeof(CTokenizerCapabilities),0,0); self._check(self._lib.mosaic_tokenizer_get_capabilities(self._handle,C.byref(c)),"capabilities"); return int(c.available)
    @property
    def sealed(self) -> bool: return bool(self._lib.mosaic_tokenizer_is_sealed(self._handle))
    def set_limits(self, *, max_input_bytes: int, max_output_tokens: int, max_token_document_bytes: int) -> "Tokenizer":
        limits=CRuntimeLimits(); self._lib.mosaic_runtime_limits_default(C.byref(limits)); limits.max_input_bytes=max_input_bytes; limits.max_output_tokens=max_output_tokens; limits.max_token_document_bytes=max_token_document_bytes
        self._check(self._lib.mosaic_tokenizer_set_runtime_limits(self._handle,C.byref(limits)),"set_limits"); return self
    @property
    def limits(self) -> RuntimeLimits:
        c=CRuntimeLimits(); c.struct_size=C.sizeof(c); self._check(self._lib.mosaic_tokenizer_get_runtime_limits(self._handle,C.byref(c)),"limits"); return RuntimeLimits(int(c.max_input_bytes),int(c.max_output_tokens),int(c.max_token_document_bytes))
    def set_low_memory_limits(self) -> "Tokenizer":
        limits = CRuntimeLimits()
        self._lib.mosaic_runtime_limits_low_memory_default(C.byref(limits))
        self._check(self._lib.mosaic_tokenizer_set_runtime_limits(self._handle, C.byref(limits)), "set_low_memory_limits")
        return self
    def seal(self) -> "Tokenizer": self._check(self._lib.mosaic_tokenizer_seal(self._handle),"seal"); return self
    @property
    def metrics(self) -> RuntimeMetrics:
        c=CRuntimeMetrics(); self._check(self._lib.mosaic_tokenizer_get_metrics(self._handle,C.byref(c)),"metrics"); return RuntimeMetrics(*(int(getattr(c,n)) for n,_ in c._fields_))

    def encode(self, data: bytes) -> tuple[int, ...]:
        owner,ptr=_buffer(bytes(data)); out=C.POINTER(C.c_uint32)(); n=C.c_size_t(); status=self._lib.mosaic_tokenizer_encode(self._handle,ptr,len(data),C.byref(out),C.byref(n)); del owner
        self._check(status,"encode")
        try:return tuple(int(out[i]) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def encode_tokens(self, data: bytes) -> tuple[Token, ...]:
        owner,ptr=_buffer(bytes(data)); out=C.POINTER(CToken)(); n=C.c_size_t(); status=self._lib.mosaic_tokenizer_encode_tokens(self._handle,ptr,len(data),C.byref(out),C.byref(n)); del owner; self._check(status,"encode_tokens")
        try:return tuple(Token(int(out[i].id),int(out[i].start),int(out[i].length),int(out[i].cost)) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def decode(self, ids: Sequence[int]) -> bytes:
        arr=(C.c_uint32*len(ids))(*(int(x) for x in ids)) if ids else None; ptr=C.cast(arr,C.POINTER(C.c_uint32)) if arr is not None else C.POINTER(C.c_uint32)(); out=C.POINTER(C.c_uint8)(); n=C.c_size_t(); self._check(self._lib.mosaic_tokenizer_decode(self._handle,ptr,len(ids),C.byref(out),C.byref(n)),"decode")
        try:return C.string_at(out,n.value) if n.value else b""
        finally:self._lib.mosaic_free(out)
    def online_stream(self, max_pending_bytes: int = 1 << 20) -> OnlineStream:
        if max_pending_bytes <= 0: raise ValueError("max_pending_bytes must be positive")
        handle=C.c_void_p(); self._check(self._lib.mosaic_tokenizer_online_stream_create(self._handle,max_pending_bytes,C.byref(handle)),"online_stream_create"); return OnlineStream(self._lib,handle)
    def detect(self,data:bytes)->Detection:
        owner,ptr=_buffer(bytes(data)); d=CDetection(); status=self._lib.mosaic_tokenizer_detect_language(self._handle,ptr,len(data),C.byref(d));del owner;self._check(status,"detect");return Detection(bool(d.matched),bool(d.available),int(d.score),int(d.margin),bytes(d.language).split(b'\0',1)[0].decode())
    def detect_spans(self,data:bytes)->tuple[SpanRoute,...]:
        owner,ptr=_buffer(bytes(data));out=C.POINTER(CSpanRoute)();n=C.c_size_t();status=self._lib.mosaic_tokenizer_detect_spans(self._handle,ptr,len(data),C.byref(out),C.byref(n));del owner;self._check(status,"detect_spans")
        try:return tuple(SpanRoute(int(out[i].start),int(out[i].length),_detection(out[i].detection)) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def encode_auto(self,data:bytes):
        owner,ptr=_buffer(bytes(data));out=C.POINTER(C.c_uint32)();n=C.c_size_t();d=CDetection();status=self._lib.mosaic_tokenizer_encode_auto(self._handle,ptr,len(data),C.byref(out),C.byref(n),C.byref(d));del owner;self._check(status,"encode_auto")
        try:ids=tuple(int(out[i]) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
        return ids,_detection(d)
    def encode_span_auto(self,data:bytes):
        owner,ptr=_buffer(bytes(data));ids_out=C.POINTER(C.c_uint32)();ids_n=C.c_size_t();routes_out=C.POINTER(CSpanRoute)();routes_n=C.c_size_t();status=self._lib.mosaic_tokenizer_encode_span_auto(self._handle,ptr,len(data),C.byref(ids_out),C.byref(ids_n),C.byref(routes_out),C.byref(routes_n));del owner;self._check(status,"encode_span_auto")
        try:
            ids=tuple(int(ids_out[i]) for i in range(ids_n.value))
            routes=tuple(SpanRoute(int(routes_out[i].start),int(routes_out[i].length),_detection(routes_out[i].detection)) for i in range(routes_n.value))
            return ids,routes
        finally:
            self._lib.mosaic_free(ids_out); self._lib.mosaic_free(routes_out)
    def graphemes(self,data:bytes)->tuple[Range,...]:
        owner,ptr=_buffer(bytes(data));out=C.POINTER(CRange)();n=C.c_size_t();status=self._lib.mosaic_tokenizer_grapheme_ranges(self._handle,ptr,len(data),C.byref(out),C.byref(n));del owner;self._check(status,"graphemes")
        try:return tuple(Range(int(out[i].start),int(out[i].length)) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def security_scan(self,data:bytes)->tuple[SecurityFinding,...]:
        owner,ptr=_buffer(bytes(data));out=C.POINTER(CSecurityFinding)();n=C.c_size_t();status=self._lib.mosaic_tokenizer_security_scan(self._handle,ptr,len(data),C.byref(out),C.byref(n));del owner;self._check(status,"security_scan")
        try:return tuple(SecurityFinding(int(out[i].kind),int(out[i].script_id),int(out[i].start),int(out[i].length)) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def normalize(self,data:bytes,mode:int=NORMALIZE_NFC)->NormalizedView:
        owner,ptr=_buffer(bytes(data));view=CNormalizedView();status=self._lib.mosaic_tokenizer_normalize(self._handle,mode,ptr,len(data),C.byref(view));del owner;self._check(status,"normalize")
        try:
            b=C.string_at(view.bytes,view.byte_length) if view.byte_length else b""; spans=tuple(Range(int(view.source_spans[i].start),int(view.source_spans[i].length)) for i in range(view.source_span_count)); return NormalizedView(b,spans)
        finally:self._lib.mosaic_normalized_view_free(C.byref(view))
    def lex(self,data:bytes)->tuple[LexToken,...]:
        owner,ptr=_buffer(bytes(data));out=C.POINTER(CLexToken)();n=C.c_size_t();status=self._lib.mosaic_tokenizer_lex(self._handle,ptr,len(data),C.byref(out),C.byref(n));del owner;self._check(status,"lex")
        try:return tuple(LexToken(int(out[i].kind),int(out[i].flags),int(out[i].start),int(out[i].length)) for i in range(n.value))
        finally:self._lib.mosaic_free(out)
    def token_document(self,data:bytes,flags:int=TOKEN_DOCUMENT_MODEL|TOKEN_DOCUMENT_GRAPHEMES,normalization_mode:int=NORMALIZE_PRESERVE)->TokenDocument:
        owner,ptr=_buffer(bytes(data));opts=CTokenDocumentOptions(C.sizeof(CTokenDocumentOptions),flags,normalization_mode,0);handle=C.c_void_p();status=self._lib.mosaic_tokenizer_token_document_create_ex(self._handle,ptr,len(data),C.byref(opts),C.byref(handle));del owner;self._check(status,"token_document");return TokenDocument(self._lib,handle)
    def set_observer(self, callback: Optional[Callable[[Event],None]], mask:int=OBSERVE_SUCCESS|OBSERVE_FAILURE|OBSERVE_RESOURCE)->"Tokenizer":
        if callback is None:
            cb=ObserverCallback(); cfg=CObserverConfig(C.sizeof(CObserverConfig),0,0,0,cb,None); self._check(self._lib.mosaic_tokenizer_set_observer(self._handle,C.byref(cfg)),"set_observer"); self._observer_callback=None; return self
        def trampoline(_ctx,evp):
            try:
                e=evp.contents; callback(Event(int(e.kind),int(e.operation),int(e.status),int(e.sequence),int(e.input_units),int(e.output_units),int(e.resource_limit),bytes(e.tokenizer_fingerprint_sha256)))
            except BaseException as exc:
                with self._observer_lock:
                    if self._observer_exception is None:self._observer_exception=exc
        native=ObserverCallback(trampoline); cfg=CObserverConfig(C.sizeof(CObserverConfig),0,mask,0,native,None); self._check(self._lib.mosaic_tokenizer_set_observer(self._handle,C.byref(cfg)),"set_observer"); self._observer_callback=native; return self
    @property
    def observer_exception(self)->Optional[BaseException]:
        with self._observer_lock:return self._observer_exception


class BatchExecutor(_LibraryBound):
    def __init__(self, *, worker_count:int=4, queue_capacity:int=1024, max_batch_items:int=65536, max_total_input_bytes:int=1<<30, library_path=None):
        lib=load_library(library_path);super().__init__(lib);cfg=CExecutorConfig();lib.mosaic_executor_config_default(C.byref(cfg));cfg.worker_count=worker_count;cfg.queue_capacity=queue_capacity;cfg.max_batch_items=max_batch_items;cfg.max_total_input_bytes=max_total_input_bytes;h=C.c_void_p();self._check(lib.mosaic_executor_create(C.byref(cfg),C.byref(h)),"executor_create");self._handle=h
    @classmethod
    def low_memory(cls, *, library_path=None) -> "BatchExecutor":
        lib = load_library(library_path)
        cfg = CExecutorConfig()
        lib.mosaic_executor_config_low_memory_default(C.byref(cfg))
        handle = C.c_void_p()
        inst = cls.__new__(cls)
        _LibraryBound.__init__(inst, lib)
        inst._check(lib.mosaic_executor_create(C.byref(cfg), C.byref(handle)), "executor_create")
        inst._handle = handle
        return inst
    def close(self):
        if self._handle and self._handle.value:self._lib.mosaic_executor_free(self._handle);self._handle=C.c_void_p()
    def __enter__(self):return self
    def __exit__(self,*_):self.close()
    def __del__(self):
        try:self.close()
        except Exception:pass
    def encode(self,tokenizer:Tokenizer,items:Iterable[bytes])->tuple[BatchResult,...]:
        data=[bytes(x) for x in items];owners=[];arr=(CBatchInput*len(data))()
        for i,b in enumerate(data):
            owner,ptr=_buffer(b);owners.append(owner);arr[i]=CBatchInput(ptr,len(b))
        out=C.POINTER(CBatchResult)();self._check(self._lib.mosaic_executor_encode_batch(self._handle,tokenizer._handle,arr,len(data),C.byref(out)),"executor_encode_batch")
        try:return tuple(BatchResult(int(out[i].status),tuple(int(out[i].ids[j]) for j in range(out[i].count)) if out[i].status==OK else ()) for i in range(len(data)))
        finally:self._lib.mosaic_batch_results_free(out,len(data));del owners
    @property
    def metrics(self):
        m=CExecutorMetrics();self._check(self._lib.mosaic_executor_get_metrics(self._handle,C.byref(m)),"executor_metrics");return {n:int(getattr(m,n)) for n,_ in m._fields_}


__all__ = [
    "Tokenizer", "TokenDocument", "OnlineStream", "BatchExecutor", "MosaicError", "Token", "Range", "Detection", "SecurityFinding",
    "SpanRoute", "LexToken", "NormalizedView", "RuntimeLimits", "RuntimeMetrics", "Event", "BatchResult",
    "TOKEN_DOCUMENT_MODEL", "TOKEN_DOCUMENT_GRAPHEMES", "TOKEN_DOCUMENT_SECURITY", "TOKEN_DOCUMENT_NORMALIZATION",
    "TOKEN_DOCUMENT_LEXICAL", "TOKEN_DOCUMENT_SEMANTIC", "NORMALIZE_PRESERVE", "NORMALIZE_NFD", "NORMALIZE_NFC",
    "NORMALIZE_NFKD", "NORMALIZE_NFKC", "NORMALIZE_NFKC_CASEFOLD", "OBSERVE_SUCCESS", "OBSERVE_FAILURE", "OBSERVE_RESOURCE",
    "MosaicdClient", "MosaicdClientError",
]
