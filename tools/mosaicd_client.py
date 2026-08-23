from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PYTHON_BINDING = ROOT / "bindings" / "python"
if PYTHON_BINDING.exists():
    sys.path.insert(0, str(PYTHON_BINDING))
else:
    wheels = sorted((ROOT / "python").glob("mosaic_tokenizer-*.whl")) if (ROOT / "python").exists() else []
    if len(wheels) == 1:
        sys.path.insert(0, str(wheels[0]))

from mosaic.service import MosaicdClient, MosaicdClientError, main


if __name__ == "__main__":
    raise SystemExit(main())
