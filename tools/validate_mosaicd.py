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
        server = build_server(t, ServiceConfig(port=0, bearer_token="secret", max_request_bytes=1024, max_decode_ids=4096, max_concurrency=4))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = f"http://127.0.0.1:{server.server_address[1]}"
            status, live = request(base + "/health/live")
            if status != 200 or live != {"status": "live"}:
                raise SystemExit("liveness endpoint failed")
            status, _ = request(base + "/v1/version")
            if status != 401:
                raise SystemExit("auth was not enforced")
            status, ver = request(base + "/v1/version", token="secret")
            if status != 200 or ver["native_version"] != t.native_version or ver["sealed"] is not True:
                raise SystemExit("version/identity endpoint failed")
            samples = [b"hello world", "नमस्ते 日本".encode(), bytes([0, 1, 0xFF, 0x80, 10])]
            for data in samples:
                b64 = base64.b64encode(data).decode()
                status, enc = request(base + "/v1/encode", method="POST", value={"data_base64": b64}, token="secret")
                if status != 200 or tuple(enc["ids"]) != t.encode(data):
                    raise SystemExit("encode mismatch")
                status, dec = request(base + "/v1/decode", method="POST", value={"ids": enc["ids"]}, token="secret")
                if status != 200 or base64.b64decode(dec["data_base64"]) != data:
                    raise SystemExit("decode mismatch")
            text = "this is an english sentence".encode()
            status, det = request(base + "/v1/detect", method="POST", value={"data_base64": base64.b64encode(text).decode()}, token="secret")
            if status != 200 or det["detection"]["language"] != t.detect(text).language:
                raise SystemExit("detector mismatch")
            status, sec = request(base + "/v1/security", method="POST", value={"data_base64": base64.b64encode("abc\u202Edef".encode()).decode()}, token="secret")
            if status != 200 or not isinstance(sec["findings"], list):
                raise SystemExit("security endpoint failed")
            status, _ = request(base + "/v1/encode", method="POST", value={"data_base64": base64.b64encode(b"x" * 900).decode()}, token="secret")
            if status != 413:
                raise SystemExit("request-size limit did not fail closed")
            status, _ = request(base + "/v1/decode", method="POST", value={"ids": [-1]}, token="secret")
            if status != 400:
                raise SystemExit("invalid token id did not fail closed")
            status, metrics = request(base + "/v1/metrics", token="secret")
            if status != 200 or metrics["service"]["requests"] < 8 or "native" not in metrics:
                raise SystemExit("metrics endpoint failed")
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=5)
    print("OK mosaicd auth=PASS roundtrip=PASS arbitrary-bytes=PASS detect=PASS security=PASS limits=PASS metrics=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
