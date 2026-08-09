from __future__ import annotations

import ctypes as C
import ctypes.util
import os
from pathlib import Path
from typing import Optional


class CRange(C.Structure):
    _fields_ = [("start", C.c_uint64), ("length", C.c_uint64)]


class CToken(C.Structure):
    _fields_ = [("id", C.c_uint32), ("start", C.c_uint64), ("length", C.c_uint64), ("cost", C.c_int32)]


class CDocumentToken(C.Structure):
    _fields_ = [("id", C.c_uint32), ("start", C.c_uint64), ("length", C.c_uint64)]


class CSecurityFinding(C.Structure):
    _fields_ = [("kind", C.c_uint32), ("script_id", C.c_uint16), ("reserved", C.c_uint16),
                ("start", C.c_uint64), ("length", C.c_uint64)]


class CNormalizedUnit(C.Structure):
    _fields_ = [("output_start", C.c_uint64), ("output_length", C.c_uint64),
                ("source_span_index", C.c_uint32), ("source_span_count", C.c_uint32)]


class CNormalizedView(C.Structure):
    _fields_ = [("bytes", C.POINTER(C.c_uint8)), ("byte_length", C.c_size_t),
                ("units", C.POINTER(CNormalizedUnit)), ("unit_count", C.c_size_t),
                ("source_spans", C.POINTER(CRange)), ("source_span_count", C.c_size_t)]


class CLexToken(C.Structure):
    _fields_ = [("kind", C.c_uint32), ("flags", C.c_uint32), ("start", C.c_uint64), ("length", C.c_uint64)]


class CSemanticComponent(C.Structure):
    _fields_ = [("kind", C.c_uint32), ("flags", C.c_uint32), ("lexical_token_index", C.c_uint64),
                ("start", C.c_uint64), ("length", C.c_uint64)]


class CDetection(C.Structure):
    _fields_ = [("matched", C.c_uint32), ("available", C.c_uint32), ("score", C.c_int64),
                ("margin", C.c_int64), ("language", C.c_char * 64)]


class CRuntimeLimits(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("flags", C.c_uint32),
                ("max_input_bytes", C.c_uint64), ("max_output_tokens", C.c_uint64),
                ("max_token_document_bytes", C.c_uint64)]


class CRuntimeMetrics(C.Structure):
    _fields_ = [("encode_calls", C.c_uint64), ("decode_calls", C.c_uint64), ("bytes_in", C.c_uint64),
                ("bytes_out", C.c_uint64), ("tokens_out", C.c_uint64), ("failures", C.c_uint64),
                ("resource_rejections", C.c_uint64)]


class CTokenizerCapabilities(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("reserved", C.c_uint32), ("available", C.c_uint64)]


class CTokenDocumentOptions(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("flags", C.c_uint32),
                ("normalization_mode", C.c_uint32), ("reserved", C.c_uint32)]


class CTokenDocumentInfo(C.Structure):
    _fields_ = [("flags", C.c_uint32), ("source_length", C.c_uint64), ("model_token_count", C.c_uint64),
                ("grapheme_count", C.c_uint64), ("security_finding_count", C.c_uint64),
                ("normalized_byte_length", C.c_uint64), ("normalized_unit_count", C.c_uint64),
                ("lexical_token_count", C.c_uint64), ("semantic_component_count", C.c_uint64),
                ("normalization_mode", C.c_uint32), ("reserved", C.c_uint32),
                ("source_sha256", C.c_uint8 * 32), ("tokenizer_fingerprint_sha256", C.c_uint8 * 32),
                ("detection", CDetection)]


class CTokenIRLimits(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("flags", C.c_uint32), ("max_record_bytes", C.c_uint64),
                ("max_source_bytes", C.c_uint64), ("max_projection_items", C.c_uint64)]


class CBatchInput(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_uint8)), ("length", C.c_size_t)]


class CBatchResult(C.Structure):
    _fields_ = [("status", C.c_int), ("ids", C.POINTER(C.c_uint32)), ("count", C.c_size_t)]


class CExecutorConfig(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("flags", C.c_uint32), ("worker_count", C.c_uint32),
                ("queue_capacity", C.c_uint32), ("max_batch_items", C.c_uint64),
                ("max_total_input_bytes", C.c_uint64)]


class CExecutorMetrics(C.Structure):
    _fields_ = [("batches", C.c_uint64), ("items", C.c_uint64), ("succeeded_items", C.c_uint64),
                ("failed_items", C.c_uint64), ("input_bytes", C.c_uint64)]


class CEvent(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("kind", C.c_uint32), ("operation", C.c_uint32),
                ("status", C.c_uint32), ("sequence", C.c_uint64), ("input_units", C.c_uint64),
                ("output_units", C.c_uint64), ("resource_limit", C.c_uint64),
                ("tokenizer_fingerprint_sha256", C.c_uint8 * 32)]


ObserverCallback = C.CFUNCTYPE(None, C.c_void_p, C.POINTER(CEvent))


class CObserverConfig(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("flags", C.c_uint32), ("event_mask", C.c_uint32),
                ("reserved", C.c_uint32), ("callback", ObserverCallback), ("context", C.c_void_p)]


def _candidate_library_paths(explicit: Optional[os.PathLike[str] | str]):
    if explicit:
        yield str(Path(explicit).expanduser().resolve())
    env = os.environ.get("MOSAIC_LIBRARY")
    if env:
        yield str(Path(env).expanduser().resolve())
    here = Path(__file__).resolve()
    # Source/release tree candidates.
    for parent in here.parents:
        yield str(parent / "lib" / "libmosaic.so")
        yield str(parent / "lib" / "libmosaic.dylib")
        yield str(parent / "bin" / "mosaic.dll")
        yield str(parent / "build" / "libmosaic.so")
    found = ctypes.util.find_library("mosaic")
    if found:
        yield found


def load_library(explicit: Optional[os.PathLike[str] | str] = None) -> C.CDLL:
    errors: list[str] = []
    seen: set[str] = set()
    for candidate in _candidate_library_paths(explicit):
        if candidate in seen:
            continue
        seen.add(candidate)
        try:
            lib = C.CDLL(candidate)
            configure_library(lib)
            return lib
        except OSError as exc:
            errors.append(f"{candidate}: {exc}")
    detail = "; ".join(errors[-5:])
    raise OSError("unable to load libmosaic; pass library_path or set MOSAIC_LIBRARY" + (f" ({detail})" if detail else ""))


def configure_library(lib: C.CDLL) -> None:
    u8p = C.POINTER(C.c_uint8); u32p = C.POINTER(C.c_uint32)
    pp_u32 = C.POINTER(u32p); pp_u8 = C.POINTER(u8p)
    lib.mosaic_version_string.restype = C.c_char_p
    lib.mosaic_status_string.argtypes = [C.c_int]; lib.mosaic_status_string.restype = C.c_char_p
    lib.mosaic_free.argtypes = [C.c_void_p]
    lib.mosaic_tokenizer_load_files.argtypes = [C.c_char_p, C.c_char_p, C.POINTER(C.c_void_p)]; lib.mosaic_tokenizer_load_files.restype = C.c_int
    for name in ("mosaic_tokenizer_add_language_file", "mosaic_tokenizer_set_detector_file", "mosaic_tokenizer_set_security_file",
                 "mosaic_tokenizer_set_normalization_file", "mosaic_tokenizer_set_lexer_file"):
        fn = getattr(lib, name); fn.argtypes = [C.c_void_p, C.c_char_p]; fn.restype = C.c_int
    lib.mosaic_tokenizer_free.argtypes = [C.c_void_p]
    lib.mosaic_tokenizer_fingerprint.argtypes = [C.c_void_p, u8p]; lib.mosaic_tokenizer_fingerprint.restype = C.c_int
    lib.mosaic_tokenizer_runtime_identity.argtypes = [C.c_void_p, u8p]; lib.mosaic_tokenizer_runtime_identity.restype = C.c_int
    lib.mosaic_tokenizer_get_capabilities.argtypes = [C.c_void_p, C.POINTER(CTokenizerCapabilities)]; lib.mosaic_tokenizer_get_capabilities.restype = C.c_int
    lib.mosaic_runtime_limits_default.argtypes = [C.POINTER(CRuntimeLimits)]
    lib.mosaic_tokenizer_set_runtime_limits.argtypes = [C.c_void_p, C.POINTER(CRuntimeLimits)]; lib.mosaic_tokenizer_set_runtime_limits.restype = C.c_int
    lib.mosaic_tokenizer_get_runtime_limits.argtypes = [C.c_void_p, C.POINTER(CRuntimeLimits)]; lib.mosaic_tokenizer_get_runtime_limits.restype = C.c_int
    lib.mosaic_tokenizer_seal.argtypes = [C.c_void_p]; lib.mosaic_tokenizer_seal.restype = C.c_int
    lib.mosaic_tokenizer_is_sealed.argtypes = [C.c_void_p]; lib.mosaic_tokenizer_is_sealed.restype = C.c_int
    lib.mosaic_tokenizer_get_metrics.argtypes = [C.c_void_p, C.POINTER(CRuntimeMetrics)]; lib.mosaic_tokenizer_get_metrics.restype = C.c_int
    lib.mosaic_tokenizer_reset_metrics.argtypes = [C.c_void_p]; lib.mosaic_tokenizer_reset_metrics.restype = C.c_int
    lib.mosaic_tokenizer_encode.argtypes = [C.c_void_p, u8p, C.c_size_t, pp_u32, C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_encode.restype = C.c_int
    lib.mosaic_tokenizer_encode_tokens.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(C.POINTER(CToken)), C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_encode_tokens.restype = C.c_int
    lib.mosaic_tokenizer_decode.argtypes = [C.c_void_p, u32p, C.c_size_t, pp_u8, C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_decode.restype = C.c_int
    lib.mosaic_tokenizer_online_stream_create.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(C.c_void_p)]; lib.mosaic_tokenizer_online_stream_create.restype = C.c_int
    lib.mosaic_online_stream_push.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(C.c_size_t), pp_u32, C.POINTER(C.c_size_t)]; lib.mosaic_online_stream_push.restype = C.c_int
    lib.mosaic_online_stream_finish.argtypes = [C.c_void_p, pp_u32, C.POINTER(C.c_size_t)]; lib.mosaic_online_stream_finish.restype = C.c_int
    lib.mosaic_online_stream_pending_bytes.argtypes = [C.c_void_p]; lib.mosaic_online_stream_pending_bytes.restype = C.c_size_t
    lib.mosaic_online_stream_free.argtypes = [C.c_void_p]
    lib.mosaic_tokenizer_detect_language.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(CDetection)]; lib.mosaic_tokenizer_detect_language.restype = C.c_int
    lib.mosaic_tokenizer_encode_auto.argtypes = [C.c_void_p, u8p, C.c_size_t, pp_u32, C.POINTER(C.c_size_t), C.POINTER(CDetection)]; lib.mosaic_tokenizer_encode_auto.restype = C.c_int
    lib.mosaic_tokenizer_grapheme_ranges.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(C.POINTER(CRange)), C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_grapheme_ranges.restype = C.c_int
    lib.mosaic_tokenizer_security_scan.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(C.POINTER(CSecurityFinding)), C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_security_scan.restype = C.c_int
    lib.mosaic_tokenizer_normalize.argtypes = [C.c_void_p, C.c_uint32, u8p, C.c_size_t, C.POINTER(CNormalizedView)]; lib.mosaic_tokenizer_normalize.restype = C.c_int
    lib.mosaic_normalized_view_free.argtypes = [C.POINTER(CNormalizedView)]
    lib.mosaic_tokenizer_lex.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(C.POINTER(CLexToken)), C.POINTER(C.c_size_t)]; lib.mosaic_tokenizer_lex.restype = C.c_int
    lib.mosaic_tokenizer_token_document_create_ex.argtypes = [C.c_void_p, u8p, C.c_size_t, C.POINTER(CTokenDocumentOptions), C.POINTER(C.c_void_p)]; lib.mosaic_tokenizer_token_document_create_ex.restype = C.c_int
    lib.mosaic_token_document_get_info.argtypes = [C.c_void_p, C.POINTER(CTokenDocumentInfo)]; lib.mosaic_token_document_get_info.restype = C.c_int
    lib.mosaic_token_document_copy_source.argtypes = [C.c_void_p, pp_u8, C.POINTER(C.c_size_t)]; lib.mosaic_token_document_copy_source.restype = C.c_int
    lib.mosaic_token_document_model_tokens.argtypes = [C.c_void_p, C.POINTER(C.POINTER(CDocumentToken)), C.POINTER(C.c_size_t)]; lib.mosaic_token_document_model_tokens.restype = C.c_int
    lib.mosaic_token_document_serialize.argtypes = [C.c_void_p, pp_u8, C.POINTER(C.c_size_t)]; lib.mosaic_token_document_serialize.restype = C.c_int
    lib.mosaic_token_document_deserialize.argtypes = [u8p, C.c_size_t, C.POINTER(C.c_void_p)]; lib.mosaic_token_document_deserialize.restype = C.c_int
    lib.mosaic_token_document_free.argtypes = [C.c_void_p]
    lib.mosaic_executor_config_default.argtypes = [C.POINTER(CExecutorConfig)]
    lib.mosaic_executor_create.argtypes = [C.POINTER(CExecutorConfig), C.POINTER(C.c_void_p)]; lib.mosaic_executor_create.restype = C.c_int
    lib.mosaic_executor_encode_batch.argtypes = [C.c_void_p, C.c_void_p, C.POINTER(CBatchInput), C.c_size_t, C.POINTER(C.POINTER(CBatchResult))]; lib.mosaic_executor_encode_batch.restype = C.c_int
    lib.mosaic_batch_results_free.argtypes = [C.POINTER(CBatchResult), C.c_size_t]
    lib.mosaic_executor_get_metrics.argtypes = [C.c_void_p, C.POINTER(CExecutorMetrics)]; lib.mosaic_executor_get_metrics.restype = C.c_int
    lib.mosaic_executor_free.argtypes = [C.c_void_p]
    lib.mosaic_tokenizer_set_observer.argtypes = [C.c_void_p, C.POINTER(CObserverConfig)]; lib.mosaic_tokenizer_set_observer.restype = C.c_int
