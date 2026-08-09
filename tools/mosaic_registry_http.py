#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import ssl
import sys
import threading
import urllib.error
import urllib.request
from dataclasses import asdict, dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mosaic_registry import Registry, canon  # noqa: E402


@dataclass(frozen=True)
class RemoteConfig:
    host: str = "127.0.0.1"
    port: int = 8788
    bearer_token: str | None = None
    max_object_bytes: int = 1 << 30


class RegistryServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, registry: Registry, config: RemoteConfig):
        super().__init__(address, Handler)
        self.registry = registry
        self.config = config


class Handler(BaseHTTPRequestHandler):
    server_version = "mosaic-registry-http/1"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: object) -> None:
        sys.stderr.write("mosaic-registry-http: %s - %s\n" % (self.address_string(), fmt % args))

    def _authorized(self) -> bool:
        expected = self.server.config.bearer_token  # type: ignore[attr-defined]
        if expected is None:
            return True
        auth = self.headers.get("Authorization", "")
        supplied = auth[7:] if auth.startswith("Bearer ") else ""
        return hmac.compare_digest(supplied.encode(), expected.encode())

    def _json(self, status: int, value) -> None:
        data = canon(value)
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers(); self.wfile.write(data)

    def do_GET(self) -> None:
        if self.path == "/health/live":
            self._json(200, {"status": "live"}); return
        if not self._authorized():
            self._json(401, {"error": "unauthorized"}); return
        registry = self.server.registry  # type: ignore[attr-defined]
        if self.path == "/v1/catalog":
            rows = [asdict(x) for x in sorted(registry.rows(), key=lambda r:(r.publisher,r.name,r.version,r.sha256))]
            self._json(200, {"schema": 1, "catalog_sha256": registry.catalog_hash(), "packs": rows}); return
        prefix = "/v1/objects/sha256/"
        if self.path.startswith(prefix):
            digest = self.path[len(prefix):]
            if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
                self._json(400, {"error": "invalid_sha256"}); return
            path = registry.object_path(digest)
            if not path.exists() or not path.is_file():
                self._json(404, {"error": "not_found"}); return
            size = path.stat().st_size
            if size > self.server.config.max_object_bytes:  # type: ignore[attr-defined]
                self._json(413, {"error": "object_too_large"}); return
            # Re-hash before serving so corrupt CAS state cannot escape unnoticed.
            h = hashlib.sha256()
            with path.open("rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
            if h.hexdigest() != digest:
                self._json(500, {"error": "object_corrupt"}); return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size))
            self.send_header("ETag", f'"sha256:{digest}"')
            self.send_header("Cache-Control", "public,max-age=31536000,immutable")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            with path.open("rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    self.wfile.write(chunk)
            return
        self._json(404, {"error": "not_found"})


def build_server(registry: Registry, config: RemoteConfig) -> RegistryServer:
    if config.max_object_bytes <= 0:
        raise ValueError("max_object_bytes must be positive")
    registry.init()
    return RegistryServer((config.host, config.port), registry, config)


def fetch_catalog(base_url: str, *, bearer_token: str | None = None, timeout: float = 30.0) -> dict:
    req = urllib.request.Request(base_url.rstrip("/") + "/v1/catalog")
    if bearer_token: req.add_header("Authorization", "Bearer " + bearer_token)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        value = json.loads(r.read())
    if value.get("schema") != 1 or not isinstance(value.get("packs"), list):
        raise ValueError("remote catalog schema invalid")
    canonical_rows = [dict(x) for x in value["packs"]]
    # Registry catalog hash is over the canonical pack rows only.
    expected = hashlib.sha256(canon({"schema": 1, "packs": canonical_rows})).hexdigest()
    if value.get("catalog_sha256") != expected:
        raise ValueError("remote catalog hash mismatch")
    return value


def fetch_object(base_url: str, digest: str, output: Path, *, bearer_token: str | None = None, max_bytes: int = 1 << 30, timeout: float = 30.0) -> Path:
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise ValueError("digest must be lowercase SHA-256")
    req = urllib.request.Request(base_url.rstrip("/") + "/v1/objects/sha256/" + digest)
    if bearer_token: req.add_header("Authorization", "Bearer " + bearer_token)
    h = hashlib.sha256(); total = 0
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_name(output.name + ".partial")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r, temp.open("wb") as f:
            while True:
                chunk = r.read(1 << 20)
                if not chunk: break
                total += len(chunk)
                if total > max_bytes: raise ValueError("remote object exceeds client byte limit")
                h.update(chunk); f.write(chunk)
        if h.hexdigest() != digest: raise ValueError("remote object SHA-256 mismatch")
        temp.replace(output)
        return output
    finally:
        temp.unlink(missing_ok=True)


def main() -> int:
    p = argparse.ArgumentParser(prog="mosaic-registry-http")
    sub = p.add_subparsers(dest="cmd", required=True)
    serve = sub.add_parser("serve")
    serve.add_argument("registry", type=Path); serve.add_argument("--host", default="127.0.0.1"); serve.add_argument("--port", type=int, default=8788)
    serve.add_argument("--bearer-token"); serve.add_argument("--max-object-bytes", type=int, default=1 << 30); serve.add_argument("--tls-cert", type=Path); serve.add_argument("--tls-key", type=Path)
    cat = sub.add_parser("catalog"); cat.add_argument("url"); cat.add_argument("--bearer-token")
    get = sub.add_parser("fetch"); get.add_argument("url"); get.add_argument("sha256"); get.add_argument("output", type=Path); get.add_argument("--bearer-token"); get.add_argument("--max-bytes", type=int, default=1 << 30)
    a = p.parse_args()
    if a.cmd == "catalog": print(json.dumps(fetch_catalog(a.url, bearer_token=a.bearer_token), indent=2, sort_keys=True)); return 0
    if a.cmd == "fetch": fetch_object(a.url, a.sha256, a.output, bearer_token=a.bearer_token, max_bytes=a.max_bytes); print(json.dumps({"output": str(a.output), "sha256": a.sha256}, sort_keys=True)); return 0
    if (a.tls_cert is None) != (a.tls_key is None): raise SystemExit("TLS certificate and key must be supplied together")
    registry = Registry(a.registry)
    server = build_server(registry, RemoteConfig(a.host, a.port, a.bearer_token, a.max_object_bytes))
    if a.tls_cert:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.minimum_version = ssl.TLSVersion.TLSv1_2; ctx.load_cert_chain(a.tls_cert, a.tls_key); server.socket = ctx.wrap_socket(server.socket, server_side=True)
    print(json.dumps({"event":"listening","host":server.server_address[0],"port":server.server_address[1],"tls":bool(a.tls_cert)}, sort_keys=True), flush=True)
    try: server.serve_forever(poll_interval=.25)
    except KeyboardInterrupt: pass
    finally: server.shutdown(); server.server_close()
    return 0

if __name__ == "__main__":
    try: raise SystemExit(main())
    except (ValueError, OSError, urllib.error.URLError, json.JSONDecodeError) as exc:
        print(f"mosaic-registry-http: error: {exc}", file=sys.stderr); raise SystemExit(2)
