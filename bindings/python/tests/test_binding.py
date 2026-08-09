from __future__ import annotations

import gc
import os
import threading
import unittest
from pathlib import Path

from mosaic import (
    BatchExecutor, MosaicError, NORMALIZE_NFC, OBSERVE_FAILURE, OBSERVE_RESOURCE, OBSERVE_SUCCESS, TokenDocument,
    Tokenizer, TOKEN_DOCUMENT_GRAPHEMES, TOKEN_DOCUMENT_LEXICAL, TOKEN_DOCUMENT_MODEL,
    TOKEN_DOCUMENT_NORMALIZATION, TOKEN_DOCUMENT_SECURITY, TOKEN_DOCUMENT_SEMANTIC,
)

ROOT = Path(__file__).resolve().parents[3]
LIB = Path(os.environ.get("MOSAIC_LIBRARY", ROOT / "build/libmosaic.so"))
PACK = ROOT / "fixtures/packs"


def tokenizer() -> Tokenizer:
    t = Tokenizer(PACK / "model-v2.mpack", PACK / "unicode17-v1.mpack", library_path=LIB)
    t.add_language(PACK / "language/en-v1.mpack")
    t.set_detector(PACK / "detector/reference-v1.mpack")
    t.set_security(PACK / "security17-v1.mpack")
    t.set_normalization(PACK / "normalization16-v1.mpack")
    t.set_lexer(PACK / "lexer/c-v1.mpack")
    return t


class BindingTests(unittest.TestCase):
    def test_exact_arbitrary_bytes_and_spans(self):
        with tokenizer() as t:
            data = bytes(range(256)) * 4
            ids = t.encode(data)
            self.assertEqual(t.decode(ids), data)
            tokens = t.encode_tokens(data)
            cursor = 0
            for token in tokens:
                self.assertEqual(token.start, cursor); self.assertGreater(token.length, 0); cursor += token.length
            self.assertEqual(cursor, len(data)); self.assertEqual(len(t.fingerprint), 32); self.assertEqual(len(t.runtime_identity), 32)

    def test_integrated_views_and_cold_document(self):
        with tokenizer() as t:
            ids, detection = t.encode_auto(b"tokenizer")
            self.assertEqual(ids, (271,)); self.assertEqual(detection.language, "en"); self.assertTrue(detection.available)
            self.assertEqual(sum(r.length for r in t.graphemes("e\N{COMBINING ACUTE ACCENT}".encode())), 3)
            self.assertIsInstance(t.security_scan(b"hello"), tuple)
            self.assertEqual(t.normalize(b"e\xcc\x81", NORMALIZE_NFC).data, "é".encode())
            lex = t.lex(b"int x=1;"); self.assertTrue(lex); self.assertEqual(sum(x.length for x in lex), 8)
            flags = TOKEN_DOCUMENT_MODEL | TOKEN_DOCUMENT_GRAPHEMES | TOKEN_DOCUMENT_SECURITY | TOKEN_DOCUMENT_NORMALIZATION | TOKEN_DOCUMENT_LEXICAL | TOKEN_DOCUMENT_SEMANTIC
            with t.token_document(b"int x=1;", flags, NORMALIZE_NFC) as doc:
                self.assertEqual(doc.source, b"int x=1;"); self.assertTrue(doc.model_tokens); record = doc.serialize(); self.assertGreater(len(record), len(doc.source))
            with TokenDocument.deserialize(record, library_path=LIB) as restored:
                self.assertEqual(restored.source, b"int x=1;"); self.assertEqual(restored.serialize(), record)

    def test_sealing_limits_observer_and_parallel(self):
        with tokenizer() as t:
            before = t.runtime_identity
            events=[]; lock=threading.Lock()
            def observed(event):
                with lock: events.append(event)
            t.set_observer(observed, OBSERVE_SUCCESS | OBSERVE_FAILURE | OBSERVE_RESOURCE)
            self.assertEqual(before, t.runtime_identity)
            t.set_limits(max_input_bytes=16, max_output_tokens=16, max_token_document_bytes=64).seal()
            self.assertTrue(t.sealed)
            with self.assertRaises(MosaicError): t.set_limits(max_input_bytes=32,max_output_tokens=32,max_token_document_bytes=64)
            with self.assertRaises(MosaicError) as cm: t.encode(b"x"*17)
            self.assertEqual(cm.exception.status, 9)
            with BatchExecutor(worker_count=4, queue_capacity=3, max_batch_items=256, max_total_input_bytes=4096, library_path=LIB) as ex:
                results=ex.encode(t,[b"hello"]*200)
                self.assertEqual(len(results),200); self.assertTrue(all(r.status==0 and r.ids for r in results)); self.assertEqual(ex.metrics["items"],200)
            self.assertGreaterEqual(len(events),201); self.assertIsNone(t.observer_exception)

    def test_error_and_lifecycle(self):
        for _ in range(50):
            t=Tokenizer(PACK/"model-v2.mpack",PACK/"unicode17-v1.mpack",library_path=LIB); self.assertEqual(t.decode(t.encode(b"abc")),b"abc"); t.close(); t.close()
        gc.collect()
        with tokenizer() as t:
            with self.assertRaises(MosaicError) as cm:t.decode([0xFFFFFFFF])
            self.assertEqual(cm.exception.status,6)


if __name__ == "__main__": unittest.main()
