#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import json
import sys
import threading
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "bindings" / "python"))

from mosaic import Tokenizer  # noqa: E402
from mosaicd_client import MosaicdClient  # noqa: E402
from mosaicd import ServiceConfig, build_server  # noqa: E402


def request(url: str, *, method="GET", value=None, token=None):
    body = None if value is None else json.dumps(value).encode()
    headers = {"Content-Type": "application/json"}
    if token is not None:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())


def run_print_config(model: Path, unicode: Path, library: Path, *, low_memory: bool = False) -> dict:
    import subprocess
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        command = [sys.executable, str(ROOT / "tools" / "mosaicd.py"), "--model", str(model), "--unicode", str(unicode), "--library", str(library), "--print-config"]
        if low_memory:
            command.insert(-1, "--low-memory")
        raw = subprocess.check_output(command, cwd=ROOT, text=True)
    return json.loads(raw)


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--build-dir", type=Path, default=ROOT / "build" / "preset-core-release")
    a = p.parse_args()
    native = a.build_dir / "native"
    libs = sorted(native.glob("libmosaic.so")) + sorted(native.glob("mosaic.dll")) + sorted(native.glob("libmosaic.dylib"))
    if len(libs) != 1:
        raise SystemExit(f"expected exactly one native library in {native}, found {len(libs)}")
    model = ROOT / "fixtures/packs/model-v2.mpack"
    unicode = ROOT / "fixtures/packs/unicode17-v1.mpack"
    detector = ROOT / "fixtures/packs/detector/reference-v1.mpack"
    security = ROOT / "fixtures/packs/security17-v1.mpack"
    languages = [ROOT / f"fixtures/packs/language/{x}-v1.mpack" for x in ("en", "hi", "ja")]
    with Tokenizer(model, unicode, library_path=libs[0]) as t:
        for lang in languages:
            t.add_language(lang)
        t.set_detector(detector).set_security(security).seal()
        printed = run_print_config(model, unicode, libs[0])
        if printed["service_profile"]["low_memory"] is not False or printed["service_profile"]["max_concurrency"] != 32:
            raise SystemExit("print-config default profile failed")
        low = ServiceConfig.low_memory()
        if (low.low_memory_mode, low.executor_workers, low.executor_queue, low.max_batch_items, low.max_stream_sessions) != (True, 1, 8, 256, 32):
            raise SystemExit("low-memory service preset changed unexpectedly")
        low_printed = run_print_config(model, unicode, libs[0], low_memory=True)
        if low_printed["service_profile"]["low_memory"] is not True or low_printed["service_profile"]["max_concurrency"] != 4:
            raise SystemExit("print-config low-memory profile failed")
        server = build_server(t, ServiceConfig(port=0, bearer_token="secret", max_request_bytes=1024, max_decode_ids=4096, max_concurrency=1, max_batch_items=8, max_batch_bytes=512, executor_workers=2, executor_queue=8, max_stream_sessions=2, stream_pending_bytes=128, stream_idle_seconds=30.0))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = f"http://127.0.0.1:{server.server_address[1]}"
            client = MosaicdClient(base, bearer_token="secret")
            status, live = request(base + "/health/live")
            if status != 200 or live != {"status": "live"}:
                raise SystemExit("liveness endpoint failed")
            status, schema = request(base + "/openapi.json")
            schemas = schema.get("components", {}).get("schemas", {})
            if status != 200 or schema.get("openapi") != "3.1.0" or "/v1/config" not in schema.get("paths", {}) or "ServiceProfile" not in schemas or "EncodeResponse" not in schemas or "StreamPushResponse" not in schemas or "MetricsResponse" not in schemas or "ErrorResponse" not in schemas:
                raise SystemExit("openapi document failed")
            status, _ = request(base + "/v1/version")
            if status != 401:
                raise SystemExit("auth was not enforced")
            status, ver = request(base + "/v1/version", token="secret")
            if status != 200 or ver["native_version"] != t.native_version or ver["sealed"] is not True or ver["service_profile"]["low_memory"] is not False or ver["service_profile"]["max_request_bytes"] != server.state.config.max_request_bytes:
                raise SystemExit("version/identity endpoint failed")
            status, config = request(base + "/v1/config", token="secret")
            if status != 200 or config["service_profile"]["low_memory"] is not False or config["service_profile"]["max_concurrency"] != server.state.config.max_concurrency:
                raise SystemExit("service config endpoint failed")
            status, metrics = request(base + "/v1/metrics", token="secret")
            if status != 200 or metrics["service"]["low_memory"] is not False or metrics["executor"]["batches"] != 0 or metrics["native"]["encode_calls"] != 0:
                raise SystemExit("default service profile metadata missing")
            client_schema = client.openapi()
            if client_schema["components"]["schemas"]["MetricsResponse"]["required"] != ["service", "native", "executor"]:
                raise SystemExit("client openapi fetch failed")
            if client.version()["service_profile"]["max_decode_ids"] != server.state.config.max_decode_ids:
                raise SystemExit("client version fetch failed")
            if client.config()["service_profile"]["stream_idle_seconds"] != server.state.config.stream_idle_seconds:
                raise SystemExit("client config fetch failed")
            samples = [b"hello world", "नमस्ते 日本".encode(), bytes([0, 1, 0xFF, 0x80, 10])]
            for data in samples:
                b64 = base64.b64encode(data).decode()
                status, enc = request(base + "/v1/encode", method="POST", value={"data_base64": b64}, token="secret")
                if status != 200 or tuple(enc["ids"]) != t.encode(data):
                    raise SystemExit("encode mismatch")
                status, dec = request(base + "/v1/decode", method="POST", value={"ids": enc["ids"]}, token="secret")
                if status != 200 or base64.b64decode(dec["data_base64"]) != data:
                    raise SystemExit("decode mismatch")
                if tuple(client.encode(data)["ids"]) != tuple(enc["ids"]):
                    raise SystemExit("client encode mismatch")
                if base64.b64decode(client.decode(enc["ids"])["data_base64"]) != data:
                    raise SystemExit("client decode mismatch")
            batch = [b"batch one", "नमस्ते".encode(), bytes([0, 255, 1]), b""]
            status, batched = request(base + "/v1/encode-batch", method="POST", value={"items_base64":[base64.b64encode(x).decode() for x in batch]}, token="secret")
            if status != 200 or len(batched.get("results", [])) != len(batch):
                raise SystemExit("batch encode endpoint failed")
            for item, result in zip(batch, batched["results"]):
                if result["status"] != 0 or tuple(result["ids"]) != t.encode(item):
                    raise SystemExit("batch encode ordering/result mismatch")
            client_batch = client.encode_batch(batch)
            if len(client_batch.get("results", [])) != len(batch):
                raise SystemExit("client batch encode failed")
            streamed = (b"streaming mosaic " * 20) + bytes([0, 255, 128, 1])
            status, created = request(base + "/v1/streams", method="POST", value={}, token="secret")
            if status != 200 or not created.get("session_id"):
                raise SystemExit("stream session creation failed")
            sid = created["session_id"]; stream_ids=[]
            for i in range(0, len(streamed), 31):
                remaining = streamed[i:i+31]
                while remaining:
                    status, pushed = request(base + f"/v1/streams/{sid}/push", method="POST", value={"data_base64":base64.b64encode(remaining).decode()}, token="secret")
                    if status != 200 or pushed["consumed"] <= 0:
                        raise SystemExit("stream push failed to make progress")
                    if pushed["pending_bytes"] > server.state.config.stream_pending_bytes:
                        raise SystemExit("stream pending-byte bound exceeded")
                    stream_ids.extend(pushed["ids"]); remaining=remaining[pushed["consumed"]:]
            status, finished = request(base + f"/v1/streams/{sid}/finish", method="POST", value={}, token="secret")
            if status != 200 or not finished.get("finished"):
                raise SystemExit("stream finish failed")
            stream_ids.extend(finished["ids"])
            if tuple(stream_ids) != t.encode(streamed):
                raise SystemExit("service stream/full tokenization mismatch")
            client_created = client.create_stream()
            client_sid = client_created["session_id"]
            client_push = client.push_stream(client_sid, streamed[:32])
            if client_push["consumed"] <= 0:
                raise SystemExit("client stream push failed")
            if not client.finish_stream(client_sid).get("finished"):
                raise SystemExit("client stream finish failed")
            # Cancellation is explicit and subsequent use fails closed.
            status, created = request(base + "/v1/streams", method="POST", value={}, token="secret"); cancel_id=created["session_id"]
            status, cancelled = request(base + f"/v1/streams/{cancel_id}", method="DELETE", token="secret")
            if status != 200 or cancelled != {"cancelled": True}: raise SystemExit("stream cancellation failed")
            status, _ = request(base + f"/v1/streams/{cancel_id}/finish", method="POST", value={}, token="secret")
            if status != 404: raise SystemExit("cancelled stream remained addressable")
            text = "this is an english sentence".encode()
            status, det = request(base + "/v1/detect", method="POST", value={"data_base64": base64.b64encode(text).decode()}, token="secret")
            if status != 200 or det["detection"]["language"] != t.detect(text).language:
                raise SystemExit("detector mismatch")
            if client.detect(text)["detection"]["language"] != t.detect(text).language:
                raise SystemExit("client detect mismatch")
            status, sec = request(base + "/v1/security", method="POST", value={"data_base64": base64.b64encode("abc\u202Edef".encode()).decode()}, token="secret")
            if status != 200 or not isinstance(sec["findings"], list):
                raise SystemExit("security endpoint failed")
            if not isinstance(client.security("abc\u202Edef".encode())["findings"], list):
                raise SystemExit("client security failed")
            status, _ = request(base + "/v1/encode", method="POST", value={"data_base64": base64.b64encode(b"x" * 900).decode()}, token="secret")
            if status != 413:
                raise SystemExit("request-size limit did not fail closed")
            status, _ = request(base + "/v1/decode", method="POST", value={"ids": [-1]}, token="secret")
            if status != 400:
                raise SystemExit("invalid token id did not fail closed")
            status, _ = request(base + "/v1/encode-batch", method="POST", value={"items_base64":[""] * (server.state.config.max_batch_items + 1)}, token="secret")
            if status != 400:
                raise SystemExit("batch item limit did not fail closed")
            too_big = base64.b64encode(b"x" * (server.state.config.max_batch_bytes + 1)).decode()
            # This request can exceed the HTTP body limit first; both are fail-closed 4xx outcomes.
            status, _ = request(base + "/v1/encode-batch", method="POST", value={"items_base64":[too_big]}, token="secret")
            if status not in (400, 413):
                raise SystemExit("batch byte limit did not fail closed")
            # Saturation is fail-closed and separately counted.
            import time
            for _ in range(100):
                if server.state.admission.acquire(blocking=False):
                    break
                time.sleep(0.005)
            else:
                raise SystemExit("could not reserve admission permit for saturation test")
            try:
                status, busy = request(base + "/v1/encode", method="POST", value={"data_base64": ""}, token="secret")
                if status != 503 or busy.get("error", {}).get("code") != "busy":
                    raise SystemExit("concurrency saturation did not fail closed")
            finally:
                server.state.admission.release()
            status, metrics = request(base + "/v1/metrics", token="secret")
            if status != 200 or metrics["service"]["requests"] < 8 or metrics["service"]["busy_rejections"] < 1 or metrics["service"]["stream_sessions"] != 0 or metrics["service"]["low_memory"] is not False or "native" not in metrics or "executor" not in metrics or "batches" not in metrics["executor"] or "encode_calls" not in metrics["native"]:
                raise SystemExit("metrics endpoint failed")
            low_server = build_server(t, ServiceConfig.low_memory())
            if low_server.state.config.low_memory_mode is not True or low_server.state.snapshot()["service"]["low_memory"] is not True:
                raise SystemExit("low-memory service profile did not propagate")
            if low_server.state.tokenizer.sealed is False:
                raise SystemExit("low-memory service server did not preserve tokenizer state")
            low_config = low_server.state.snapshot()["service"]["low_memory"]
            if low_config is not True:
                raise SystemExit("low-memory config snapshot missing")
            low_server.server_close()
            req = urllib.request.Request(base + "/metrics", headers={"Authorization":"Bearer secret"})
            with urllib.request.urlopen(req, timeout=5) as r:
                text = r.read().decode("ascii")
            for metric in ("mosaic_service_requests_total", "mosaic_service_busy_rejections_total", "mosaic_native_encode_calls_total"):
                if metric not in text: raise SystemExit(f"Prometheus metric missing: {metric}")
        finally:
            server.shutdown(); server.server_close(); server.state.close(); thread.join(timeout=5)
    print("OK mosaicd auth=PASS roundtrip=PASS arbitrary-bytes=PASS detect=PASS security=PASS limits=PASS saturation=PASS metrics-json=PASS prometheus=PASS batch=PASS resumable-stream=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
