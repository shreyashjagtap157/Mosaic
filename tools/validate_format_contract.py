#!/usr/bin/env python3
from __future__ import annotations
import json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
contract=json.loads((ROOT/'abi/format-contract-v1.json').read_text(encoding="utf-8"))
core=(ROOT/'native/src/mosaic.c').read_text(encoding="utf-8");cache=(ROOT/'native/src/mosaic_cache.c').read_text(encoding="utf-8");trust=(ROOT/'native/src/mosaic_trust.c').read_text(encoding="utf-8");registry=(ROOT/'tools/mosaic_registry.py').read_text(encoding="utf-8")
errors=[]
def need(ok,msg):
    if not ok:errors.append(msg)
p=contract['pack_container'];need('memcmp(data, "MOSPACK\\0", 8)' in core,'MOSPACK magic drift');need('rd16(data + 12) != 96' in core,'MOSPACK header-size check drift')
t=contract['token_document'];need('#define MOSAIC_TOKEN_IR_MAGIC "MSTIRD01"' in core,'TokenDocument magic drift');need(f'#define MOSAIC_TOKEN_IR_VERSION {t["version"]}u' in core,'TokenDocument version drift');need(f'#define MOSAIC_TOKEN_IR_HEADER {t["header_bytes"]}u' in core,'TokenDocument header drift');need(f'#define MOSAIC_TOKEN_IR_SECTIONS {t["sections"]}u' in core,'TokenDocument section-count drift')
pm=contract['packed_model'];need("{'M','S','T','K','P','K','0','1'}" in core,'packed-model magic drift');need(f'#define MOSAIC_PACKED_MODEL_HEADER {pm["header_bytes"]}u' in core,'packed-model header drift');need('wr16(out+8,1u)' in core,'packed-model version drift')
cr=contract['cache_record'];need("{'M','S','C','A','C','H','R','1'}" in cache,'cache-record magic drift');need(f'#define MOSAIC_CACHE_RECORD_HEADER {cr["header_bytes"]}u' in cache,'cache-record header drift');need('cache_wr16(record + 8u, 1u)' in cache,'cache-record version drift')
sig=contract['signature_record'];need('#define TRUST_MAGIC "MSSIGV01"' in trust,'signature magic drift');need(f'#define TRUST_VERSION {sig["version"]}u' in trust,'signature version drift');need(f'#define TRUST_HEADER_SIZE {sig["header_bytes"]}u' in trust,'signature header drift');need('#define TRUST_ALGORITHM_ED25519 1u' in trust,'signature algorithm drift')
need(f'SCHEMA={contract["registry"]["schema"]}' in registry,'registry schema drift')
need(f'return {contract["tokenizer_semantics"]}u;' in core,'tokenizer semantics drift')
if errors:
 print('\n'.join('FAIL: '+x for x in errors));raise SystemExit(1)
print('OK: stable binary-format and tokenizer-semantics contract matches implementation')
