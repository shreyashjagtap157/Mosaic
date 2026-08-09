#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, sys, tempfile, threading, urllib.error, urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from mosaic_registry import Registry
from mosaic_registry_http import RemoteConfig,build_server,fetch_catalog,fetch_object

def main():
    with tempfile.TemporaryDirectory() as td:
        root=Path(td); reg=Registry(root/'registry'); reg.init(); model=ROOT/'fixtures/packs/model-v2.mpack'; uni=ROOT/'fixtures/packs/unicode17-v1.mpack'
        rm=reg.install(model,'org.mosaic','model','1.0.0'); ru=reg.install(uni,'org.mosaic','unicode','17.0.0')
        srv=build_server(reg,RemoteConfig(port=0,bearer_token='secret',max_object_bytes=1<<20)); th=threading.Thread(target=srv.serve_forever,daemon=True);th.start()
        base=f'http://127.0.0.1:{srv.server_address[1]}'
        try:
            try: fetch_catalog(base)
            except urllib.error.HTTPError as exc:
                if exc.code!=401:raise
            else: raise SystemExit('remote catalog authentication not enforced')
            cat=fetch_catalog(base,bearer_token='secret')
            if cat['catalog_sha256']!=reg.catalog_hash() or len(cat['packs'])!=2:raise SystemExit('catalog identity mismatch')
            for row in (rm,ru):
                out=root/f'{row.sha256}.mpack';fetch_object(base,row.sha256,out,bearer_token='secret',max_bytes=1<<20)
                if hashlib.sha256(out.read_bytes()).hexdigest()!=row.sha256:raise SystemExit('download identity mismatch')
            bad=root/'bad.mpack'
            try: fetch_object(base,'0'*64,bad,bearer_token='secret')
            except urllib.error.HTTPError as exc:
                if exc.code!=404:raise
            else: raise SystemExit('missing immutable object did not fail closed')
            # Corrupt CAS object must never be served.
            p=reg.object_path(rm.sha256); original=p.read_bytes();p.write_bytes(b'corrupt')
            try: fetch_object(base,rm.sha256,bad,bearer_token='secret')
            except urllib.error.HTTPError as exc:
                if exc.code!=500:raise
            else: raise SystemExit('corrupt CAS object escaped remote validation')
            p.write_bytes(original)
        finally:srv.shutdown();srv.server_close();th.join(timeout=5)
    print('OK registry-http auth=PASS catalog=PASS immutable-fetch=PASS sha256=PASS corrupt-CAS=FAIL-CLOSED')
    return 0
if __name__=='__main__':raise SystemExit(main())
