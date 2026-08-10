#!/usr/bin/env python3
from __future__ import annotations
import json, os, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
AUTHOR=Path(os.environ.get('MOSAIC_AUTHOR', str(ROOT/'tools/mosaic_author.py')))
CLI=Path(os.environ.get('MOSAIC_TOKENIZER', str(ROOT/'build/mosaic-tokenizer')))
UNICODE=ROOT/'fixtures/packs/unicode17-v1.mpack'

def argv(*args):
    items=[str(x) for x in args]
    if items and items[0].lower().endswith('.py'):
        return [sys.executable, *items]
    return items

def run(*args):
    return subprocess.check_output(argv(*args), cwd=ROOT, text=True).strip()

def must_fail(*args):
    proc=subprocess.run(argv(*args),cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    assert proc.returncode!=0, f"expected failure: {args}"
    assert "Traceback" not in proc.stderr, proc.stderr
    return proc

def main():
    with tempfile.TemporaryDirectory() as td:
        d=Path(td)
        corpus_a=d/'a.txt'; corpus_b=d/'b.txt'
        corpus_a.write_text(('tokenizer tokenizer hello world\n'+'नमस्ते दुनिया नमस्ते दुनिया\n')*30,encoding='utf-8')
        corpus_b.write_text(('こんにちは世界 こんにちは世界 tokenizer\n')*30,encoding='utf-8')
        model1=d/'model1.mpack';model2=d/'model2.mpack';report=d/'report.json'
        run(AUTHOR,'train-model',corpus_a,corpus_b,'-o',model1,'--report',report,'--vocab-size','320','--max-piece-bytes','48','--min-frequency','2')
        # Corpus argument order is not semantic.
        run(AUTHOR,'train-model',corpus_b,corpus_a,'-o',model2,'--vocab-size','320','--max-piece-bytes','48','--min-frequency','2')
        assert model1.read_bytes()==model2.read_bytes()
        sample=d/'sample.bin';sample.write_bytes(b'\x00tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode()+b'\xff')
        out=run(CLI,'roundtrip',model1,sample); assert 'OK' in out
        info=json.loads(run(AUTHOR,'inspect',model1));assert info['canonical_hash_valid'] and any(x['kind']==4 for x in info['sections'])
        assert len(json.loads(report.read_text(encoding="utf-8"))['pieces'])<=64

        model_cfg=d/'model.json';explicit=d/'explicit.mpack'
        model_cfg.write_text(json.dumps({'byte_cost':1000,'pieces':[{'text':'tokenizer','cost':100},{'text':'नमस्ते दुनिया','cost':90}]},ensure_ascii=False), encoding="utf-8")
        run(AUTHOR,'model',model_cfg,explicit);assert 'OK' in run(CLI,'roundtrip',explicit,sample)

        lang_cfg=d/'lang.json';lang=d/'en.mpack'
        lang_cfg.write_text(json.dumps({'language':'en','adjustments':[{'text':'tokenizer','delta':-50}]}), encoding="utf-8")
        run(AUTHOR,'language',lang_cfg,lang)
        det_cfg=d/'det.json';det=d/'det.mpack'
        det_cfg.write_text(json.dumps({'min_margin':10,'profiles':{'en':{'min_score':50,'features':[{'text':'tokenizer','weight':100}]}}}), encoding="utf-8")
        run(AUTHOR,'detector',det_cfg,det)
        # Both custom packs must be accepted by the integrated runtime.
        fp=run(CLI,'fingerprint-auto',explicit,UNICODE,det,lang);assert len(fp)==64
        en=d/'en.txt';en.write_bytes(b'tokenizer')
        auto=run(CLI,'roundtrip-auto',explicit,UNICODE,det,en,lang);assert 'route=en' in auto
        span_auto=run(CLI,'analyze-span-auto',explicit,UNICODE,det,sample,lang,ROOT/'fixtures/packs/language/hi-v1.mpack',ROOT/'fixtures/packs/language/ja-v1.mpack')
        assert 'spans=' in span_auto and 'span start=' in span_auto and 'route=en' in span_auto and 'route=none' in span_auto

        # Authoring is fail-closed on invalid configs and malformed containers.
        bad=d/'bad.json';badout=d/'bad.mpack'
        bad.write_text(json.dumps({'pieces':[{'text':'x','id':256},{'text':'y','id':256}]}), encoding="utf-8")
        must_fail(AUTHOR,'model',bad,badout)
        bad.write_text(json.dumps({'pieces':[{'hex':'00','cost':1}]}), encoding="utf-8")
        must_fail(AUTHOR,'model',bad,badout)
        bad.write_text(json.dumps({'language':'bad tag!','adjustments':[]}), encoding="utf-8")
        must_fail(AUTHOR,'language',bad,badout)
        bad.write_text(json.dumps({'profiles':{'en':{'features':[{'text':'x','weight':0}]}}}), encoding="utf-8")
        must_fail(AUTHOR,'detector',bad,badout)
        must_fail(AUTHOR,'train-model',d/'missing.txt','-o',badout)
        must_fail(AUTHOR,'train-model',corpus_a,'-o',badout,'--vocab-size','255')
        # Force candidate-map exhaustion with a deliberately tiny bound below parser's accepted floor via API behavior.
        must_fail(AUTHOR,'train-model',corpus_a,'-o',badout,'--max-candidates','9999')
        malformed=d/'malformed.mpack';malformed.write_bytes(b'MOSPACK\0'+b'\x00'*8)
        must_fail(AUTHOR,'inspect',malformed)
    print('OK: deterministic authoring/runtime integration plus fail-closed invalid-config and malformed-pack handling')
    return 0
if __name__=='__main__':raise SystemExit(main())
