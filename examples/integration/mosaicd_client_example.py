from __future__ import annotations

import argparse
import base64
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PYTHON_BINDING = ROOT / "bindings" / "python"
if PYTHON_BINDING.exists():
    sys.path.insert(0, str(PYTHON_BINDING))
else:
    wheels = sorted((ROOT / "python").glob("mosaic_tokenizer-*.whl")) if (ROOT / "python").exists() else []
    if len(wheels) == 1:
        sys.path.insert(0, str(wheels[0]))

from mosaic import MosaicdClient


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", default=os.environ.get("MOSAICD_BASE_URL", "http://127.0.0.1:8787"))
    parser.add_argument("--bearer-token", default=os.environ.get("MOSAICD_BEARER_TOKEN", "secret"))
    args = parser.parse_args()
    base_url = args.base_url
    token = args.bearer_token
    client = MosaicdClient(base_url, bearer_token=token)

    schema = client.openapi()
    version = client.version()
    config = client.config()

    sample = b"tokenizer \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87"
    encoded = client.encode(sample)
    decoded = client.decode(encoded["ids"])
    assert base64.b64decode(decoded["data_base64"]) == sample

    batch = client.encode_batch([b"hello", b"world"])
    assert len(batch["results"]) == 2

    print(f"openapi={schema['openapi']} service_api={version['service_api']} port={config['port']} batch={len(batch['results'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
