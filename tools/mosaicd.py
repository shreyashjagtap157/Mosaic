#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import hmac
import json
import os
import ssl
import sys
import threading
import time
from dataclasses import asdict, dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
PYTHON_BINDING = ROOT / "bindings" / "python"
if PYTHON_BINDING.exists():
    sys.path.insert(0, str(PYTHON_BINDING))
else:
    wheels = sorted((ROOT / "python").glob("mosaic_tokenizer-*.whl")) if (ROOT / "python").exists() else []
    if len(wheels) == 1:
        sys.path.insert(0, str(wheels[0]))

from mosaic import MosaicError, Tokenizer, __version__ as SDK_VERSION  # noqa: E402


@dataclass(frozen=True)
class ServiceConfig:
    host: str = "127.0.0.1"
    port: int = 8787
    max_request_bytes: int = 8 << 20
    max_decode_ids: int = 4_000_000
    max_concurrency: int = 32
    socket_timeout_seconds: float = 30.0
    bearer_token: str | None = None


class ServiceState:
    def __init__(self, tokenizer: Tokenizer, config: ServiceConfig):
        self.tokenizer = tokenizer
        self.config = config
        self.admission = threading.BoundedSemaphore(config.max_concurrency)
        self.native_lock = threading.RLock()
        self.metrics_lock = threading.Lock()
        self.started = time.monotonic()
        self.requests = 0
        self.failures = 0
        self.busy_rejections = 0
        self.auth_rejections = 0
        self.bytes_in = 0
        self.bytes_out = 0

    def record(self, *, failed: bool = False, input_bytes: int = 0, output_bytes: int = 0) -> None:
        with self.metrics_lock:
            self.requests += 1
            self.failures += int(failed)
            self.bytes_in += int(input_bytes)
            self.bytes_out += int(output_bytes)

    def snapshot(self) -> dict[str, Any]:
        with self.metrics_lock:
            service = {
                "requests": self.requests,
                "failures": self.failures,
                "busy_rejections": self.busy_rejections,
                "auth_rejections": self.auth_rejections,
                "bytes_in": self.bytes_in,
                "bytes_out": self.bytes_out,
                "uptime_seconds": int(time.monotonic() - self.started),
                "max_concurrency": self.config.max_concurrency,
                "max_request_bytes": self.config.max_request_bytes,
            }
        native = asdict(self.tokenizer.metrics)
        return {"service": service, "native": native}

    def prometheus(self) -> bytes:
        snapshot = self.snapshot()
        lines = [
            "# HELP mosaic_service_requests_total HTTP requests admitted by mosaicd.",
            "# TYPE mosaic_service_requests_total counter",
            f"mosaic_service_requests_total {snapshot['service']['requests']}",
            "# HELP mosaic_service_failures_total HTTP requests that completed with an error.",
            "# TYPE mosaic_service_failures_total counter",
            f"mosaic_service_failures_total {snapshot['service']['failures']}",
            "# HELP mosaic_service_busy_rejections_total Requests rejected by the concurrency admission limit.",
            "# TYPE mosaic_service_busy_rejections_total counter",
            f"mosaic_service_busy_rejections_total {snapshot['service']['busy_rejections']}",
            "# HELP mosaic_service_auth_rejections_total Requests rejected by bearer authentication.",
            "# TYPE mosaic_service_auth_rejections_total counter",
            f"mosaic_service_auth_rejections_total {snapshot['service']['auth_rejections']}",
            "# HELP mosaic_service_bytes_in_total JSON request bytes admitted by the service.",
            "# TYPE mosaic_service_bytes_in_total counter",
            f"mosaic_service_bytes_in_total {snapshot['service']['bytes_in']}",
            "# HELP mosaic_service_bytes_out_total JSON response bytes produced by operations.",
            "# TYPE mosaic_service_bytes_out_total counter",
            f"mosaic_service_bytes_out_total {snapshot['service']['bytes_out']}",
            "# HELP mosaic_service_uptime_seconds Process uptime in seconds.",
            "# TYPE mosaic_service_uptime_seconds gauge",
            f"mosaic_service_uptime_seconds {snapshot['service']['uptime_seconds']}",
        ]
        for name, value in snapshot["native"].items():
            metric = "mosaic_native_" + name + ("_total" if name not in {""} else "")
            lines.extend([f"# TYPE {metric} counter", f"{metric} {value}"])
        return ("\n".join(lines) + "\n").encode("ascii")


class MosaicHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, handler, state: ServiceState):
        super().__init__(address, handler)
        self.state = state


class Handler(BaseHTTPRequestHandler):
    server_version = "mosaicd/1"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: object) -> None:
        # Deliberately omit request bodies and authorization material.
        sys.stderr.write("mosaicd: %s - %s\n" % (self.address_string(), fmt % args))

    @property
    def state(self) -> ServiceState:
        return self.server.state  # type: ignore[attr-defined]

    def setup(self) -> None:
        super().setup()
        self.connection.settimeout(self.state.config.socket_timeout_seconds)

    def _authorized(self) -> bool:
        expected = self.state.config.bearer_token
        if expected is None:
            return True
        auth = self.headers.get("Authorization", "")
        supplied = auth[7:] if auth.startswith("Bearer ") else ""
        ok = hmac.compare_digest(supplied.encode("utf-8"), expected.encode("utf-8"))
        if not ok:
            with self.state.metrics_lock:
                self.state.auth_rejections += 1
        return ok

    def _send_json(self, status: int, value: Any) -> None:
        payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(payload)

    def _send_text(self, status: int, content_type: str, payload: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(payload)

    def _error(self, status: int, code: str, message: str) -> None:
        self._send_json(status, {"error": {"code": code, "message": message}})

    def _read_json(self) -> tuple[dict[str, Any] | None, int]:
        raw_len = self.headers.get("Content-Length")
        if raw_len is None:
            self._error(HTTPStatus.LENGTH_REQUIRED, "length_required", "Content-Length is required")
            return None, 0
        try:
            length = int(raw_len)
        except ValueError:
            self._error(HTTPStatus.BAD_REQUEST, "invalid_length", "invalid Content-Length")
            return None, 0
        if length < 0 or length > self.state.config.max_request_bytes:
            self._error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "request_too_large", "request exceeds configured byte limit")
            return None, 0
        body = self.rfile.read(length)
        if len(body) != length:
            self._error(HTTPStatus.BAD_REQUEST, "short_body", "request body was truncated")
            return None, len(body)
        try:
            value = json.loads(body)
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._error(HTTPStatus.BAD_REQUEST, "invalid_json", "request must be valid UTF-8 JSON")
            return None, len(body)
        if not isinstance(value, dict):
            self._error(HTTPStatus.BAD_REQUEST, "invalid_json_shape", "top-level JSON value must be an object")
            return None, len(body)
        return value, len(body)

    @staticmethod
    def _decode_bytes(value: Any) -> bytes:
        if not isinstance(value, str):
            raise ValueError("data_base64 must be a string")
        try:
            return base64.b64decode(value, validate=True)
        except Exception as exc:
            raise ValueError("data_base64 is not valid base64") from exc

    def do_GET(self) -> None:
        if self.path == "/health/live":
            self._send_json(HTTPStatus.OK, {"status": "live"})
            return
        if self.path == "/health/ready":
            self._send_json(HTTPStatus.OK, {"status": "ready", "native_version": self.state.tokenizer.native_version})
            return
        if not self._authorized():
            self._error(HTTPStatus.UNAUTHORIZED, "unauthorized", "valid bearer token required")
            return
        if self.path == "/v1/version":
            t = self.state.tokenizer
            self._send_json(HTTPStatus.OK, {
                "service_api": 1,
                "sdk_version": SDK_VERSION,
                "native_version": t.native_version,
                "fingerprint_sha256": t.fingerprint.hex(),
                "runtime_identity_sha256": t.runtime_identity.hex(),
                "capabilities": t.capabilities,
                "sealed": t.sealed,
            })
            return
        if self.path == "/metrics":
            self._send_text(HTTPStatus.OK, "text/plain; version=0.0.4; charset=utf-8", self.state.prometheus())
            return
        if self.path == "/v1/metrics":
            self._send_json(HTTPStatus.OK, self.state.snapshot())
            return
        self._error(HTTPStatus.NOT_FOUND, "not_found", "endpoint not found")

    def do_POST(self) -> None:
        if not self._authorized():
            self._error(HTTPStatus.UNAUTHORIZED, "unauthorized", "valid bearer token required")
            return
        if not self.state.admission.acquire(blocking=False):
            with self.state.metrics_lock:
                self.state.busy_rejections += 1
            self._error(HTTPStatus.SERVICE_UNAVAILABLE, "busy", "service concurrency limit reached")
            return
        body_len = 0
        output_len = 0
        failed = False
        try:
            req, body_len = self._read_json()
            if req is None:
                failed = True
                return
            try:
                with self.state.native_lock:
                    response = self._dispatch(req)
                output_len = len(json.dumps(response, separators=(",", ":")))
                self._send_json(HTTPStatus.OK, response)
            except ValueError as exc:
                failed = True
                self._error(HTTPStatus.BAD_REQUEST, "invalid_argument", str(exc))
            except MosaicError as exc:
                failed = True
                status = HTTPStatus.REQUEST_ENTITY_TOO_LARGE if exc.status == 9 else HTTPStatus.UNPROCESSABLE_ENTITY
                self._error(status, "mosaic_error", str(exc))
            except Exception:
                failed = True
                self._error(HTTPStatus.INTERNAL_SERVER_ERROR, "internal_error", "internal service error")
        finally:
            self.state.record(failed=failed, input_bytes=body_len, output_bytes=output_len)
            self.state.admission.release()

    def _dispatch(self, req: dict[str, Any]) -> dict[str, Any]:
        t = self.state.tokenizer
        if self.path == "/v1/encode":
            data = self._decode_bytes(req.get("data_base64"))
            return {"ids": list(t.encode(data))}
        if self.path == "/v1/decode":
            ids = req.get("ids")
            if not isinstance(ids, list) or len(ids) > self.state.config.max_decode_ids:
                raise ValueError("ids must be an array within the configured token-count limit")
            if any(not isinstance(x, int) or isinstance(x, bool) or x < 0 or x > 0xFFFFFFFF for x in ids):
                raise ValueError("ids must contain unsigned 32-bit integers")
            return {"data_base64": base64.b64encode(t.decode(ids)).decode("ascii")}
        if self.path == "/v1/detect":
            data = self._decode_bytes(req.get("data_base64"))
            return {"detection": asdict(t.detect(data))}
        if self.path == "/v1/encode-auto":
            data = self._decode_bytes(req.get("data_base64"))
            ids, detection = t.encode_auto(data)
            return {"ids": list(ids), "detection": asdict(detection)}
        if self.path == "/v1/security":
            data = self._decode_bytes(req.get("data_base64"))
            return {"findings": [asdict(x) for x in t.security_scan(data)]}
        raise ValueError("unknown operation endpoint")


def build_server(tokenizer: Tokenizer, config: ServiceConfig) -> MosaicHTTPServer:
    if config.max_request_bytes <= 0 or config.max_decode_ids <= 0 or config.max_concurrency <= 0:
        raise ValueError("service limits must be positive")
    state = ServiceState(tokenizer, config)
    return MosaicHTTPServer((config.host, config.port), Handler, state)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="mosaicd", description="Bounded Mosaic HTTP service over the native runtime")
    p.add_argument("--model", required=True, type=Path)
    p.add_argument("--unicode", required=True, type=Path)
    p.add_argument("--language", action="append", type=Path, default=[])
    p.add_argument("--detector", type=Path)
    p.add_argument("--security", type=Path)
    p.add_argument("--normalization", type=Path)
    p.add_argument("--library", type=Path)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8787)
    p.add_argument("--max-request-bytes", type=int, default=8 << 20)
    p.add_argument("--max-decode-ids", type=int, default=4_000_000)
    p.add_argument("--max-concurrency", type=int, default=32)
    p.add_argument("--socket-timeout", type=float, default=30.0)
    p.add_argument("--bearer-token", default=os.environ.get("MOSAICD_BEARER_TOKEN"))
    p.add_argument("--tls-cert", type=Path)
    p.add_argument("--tls-key", type=Path)
    return p.parse_args()


def main() -> int:
    a = parse_args()
    if (a.tls_cert is None) != (a.tls_key is None):
        raise SystemExit("mosaicd: --tls-cert and --tls-key must be supplied together")
    library = a.library
    if library is None:
        candidates = []
        for name in ("libmosaic.so", "libmosaic.dylib", "mosaic.dll"):
            candidate = ROOT / "lib" / name
            if candidate.exists(): candidates.append(candidate)
        if len(candidates) == 1: library = candidates[0]
    with Tokenizer(a.model, a.unicode, library_path=library) as t:
        for language in a.language:
            t.add_language(language)
        if a.detector:
            t.set_detector(a.detector)
        if a.security:
            t.set_security(a.security)
        if a.normalization:
            t.set_normalization(a.normalization)
        t.seal()
        cfg = ServiceConfig(a.host, a.port, a.max_request_bytes, a.max_decode_ids, a.max_concurrency, a.socket_timeout, a.bearer_token)
        server = build_server(t, cfg)
        if a.tls_cert:
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ctx.minimum_version = ssl.TLSVersion.TLSv1_2
            ctx.load_cert_chain(a.tls_cert, a.tls_key)
            server.socket = ctx.wrap_socket(server.socket, server_side=True)
        host, port = server.server_address[:2]
        print(json.dumps({"event": "listening", "host": host, "port": port, "tls": bool(a.tls_cert), "native_version": t.native_version}, sort_keys=True), flush=True)
        try:
            server.serve_forever(poll_interval=0.25)
        except KeyboardInterrupt:
            pass
        finally:
            server.shutdown()
            server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
