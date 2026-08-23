from __future__ import annotations

import argparse
import base64
import json
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any, Iterable


class MosaicdClientError(RuntimeError):
    pass


@dataclass(frozen=True)
class MosaicdResponse:
    status: int
    data: dict[str, Any]


class MosaicdClient:
    def __init__(self, base_url: str, *, bearer_token: str | None = None, timeout: float = 5.0):
        self.base_url = base_url.rstrip("/")
        self.bearer_token = bearer_token
        self.timeout = timeout

    def _request(self, path: str, *, method: str = "GET", value: dict[str, Any] | None = None) -> MosaicdResponse:
        body = None if value is None else json.dumps(value, separators=(",", ":")).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        if self.bearer_token is not None:
            headers["Authorization"] = f"Bearer {self.bearer_token}"
        req = urllib.request.Request(self.base_url + path, data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                return MosaicdResponse(r.status, json.loads(r.read()))
        except urllib.error.HTTPError as exc:
            return MosaicdResponse(exc.code, json.loads(exc.read()))
        except Exception as exc:
            raise MosaicdClientError(str(exc)) from exc

    def openapi(self) -> dict[str, Any]:
        return self._request("/openapi.json").data

    def version(self) -> dict[str, Any]:
        return self._request("/v1/version").data

    def config(self) -> dict[str, Any]:
        return self._request("/v1/config").data

    def metrics(self) -> dict[str, Any]:
        return self._request("/v1/metrics").data

    def encode(self, data: bytes) -> dict[str, Any]:
        return self._request("/v1/encode", method="POST", value={"data_base64": base64.b64encode(data).decode("ascii")}).data

    def decode(self, ids: Iterable[int]) -> dict[str, Any]:
        return self._request("/v1/decode", method="POST", value={"ids": list(ids)}).data

    def detect(self, data: bytes) -> dict[str, Any]:
        return self._request("/v1/detect", method="POST", value={"data_base64": base64.b64encode(data).decode("ascii")}).data

    def security(self, data: bytes) -> dict[str, Any]:
        return self._request("/v1/security", method="POST", value={"data_base64": base64.b64encode(data).decode("ascii")}).data

    def encode_batch(self, items: Iterable[bytes]) -> dict[str, Any]:
        payload = {"items_base64": [base64.b64encode(item).decode("ascii") for item in items]}
        return self._request("/v1/encode-batch", method="POST", value=payload).data

    def create_stream(self) -> dict[str, Any]:
        return self._request("/v1/streams", method="POST", value={}).data

    def push_stream(self, session_id: str, data: bytes) -> dict[str, Any]:
        return self._request(f"/v1/streams/{session_id}/push", method="POST", value={"data_base64": base64.b64encode(data).decode("ascii")}).data

    def finish_stream(self, session_id: str) -> dict[str, Any]:
        return self._request(f"/v1/streams/{session_id}/finish", method="POST", value={}).data

    def cancel_stream(self, session_id: str) -> dict[str, Any]:
        return self._request(f"/v1/streams/{session_id}", method="DELETE").data


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="mosaicd_client", description="Tiny reusable HTTP client for the Mosaic service")
    parser.add_argument("--base-url", required=True, help="Base URL of the mosaicd service")
    parser.add_argument("--bearer-token", help="Bearer token for authenticated endpoints")
    parser.add_argument("--openapi", action="store_true", help="Fetch and print the service OpenAPI document")
    args = parser.parse_args(argv)
    client = MosaicdClient(args.base_url, bearer_token=args.bearer_token)
    if args.openapi:
        print(json.dumps(client.openapi(), sort_keys=True))
        return 0
    parser.print_help(sys.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
