from __future__ import annotations

from pathlib import Path

from mosaic import NORMALIZE_NFC, BatchExecutor, Tokenizer


ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "fixtures" / "packs"
LIB = ROOT / "build" / "preset-core-release" / "native" / "mosaic.dll"


def main() -> int:
    tokenizer = Tokenizer(PACK / "model-v2.mpack", PACK / "unicode17-v1.mpack", library_path=LIB)
    tokenizer.add_language(PACK / "language" / "en-v1.mpack")
    tokenizer.add_language(PACK / "language" / "hi-v1.mpack")
    tokenizer.add_language(PACK / "language" / "ja-v1.mpack")
    tokenizer.set_detector(PACK / "detector" / "reference-v1.mpack")
    tokenizer.set_low_memory_limits()
    tokenizer.seal()

    sample = "tokenizer नमस्ते दुनिया こんにちは世界".encode("utf-8")
    ids, detection = tokenizer.encode_auto(sample)
    assert tokenizer.decode(ids) == sample

    stream = tokenizer.online_stream(max_pending_bytes=64 * 1024)
    consumed, committed = stream.push(sample[:8])
    assert consumed <= 8
    _ = committed
    committed += stream.finish()

    assert tokenizer.normalize(b"e\xcc\x81", NORMALIZE_NFC).data == "é".encode("utf-8")

    with BatchExecutor.low_memory(library_path=LIB) as executor:
        results = executor.encode(tokenizer, [b"hello", b"world"])
        assert len(results) == 2
        assert all(result.status == 0 for result in results)

    print(f"route={detection.language} ids={len(ids)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
