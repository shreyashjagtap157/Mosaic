#!/usr/bin/env python3
from __future__ import annotations
import argparse,base64,hashlib,json,os,shutil,subprocess,sys,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];VERSION=(ROOT/'VERSION').read_text(encoding="utf-8").strip();BUILD_DIR=Path(os.environ.get('MOSAIC_BUILD_DIR', str(ROOT/'build')))
def launch(cmd):
    items=[str(x) for x in cmd]
    if not items:
        return items
    first=Path(items[0])
    if first.suffix.lower()=='.py':
        return [sys.executable, *items]
    try:
        head=first.read_text(encoding='utf-8', errors='ignore').splitlines()[:1]
    except Exception:
        head=[]
    if head and head[0].startswith('#!') and 'python' in head[0].lower():
        return [sys.executable, *items]
    return items
def run(cmd,cwd=None):return subprocess.check_output(launch(cmd),cwd=cwd,text=True).strip()
def cmake_cache_value(name:str)->str|None:
    cache=BUILD_DIR/'CMakeCache.txt'
    if not cache.exists():
        return None
    prefix=f'{name}:'
    for line in cache.read_text(encoding='utf-8', errors='ignore').splitlines():
        if line.startswith(prefix):
            return line.split('=',1)[1]
    return None
def compiler()->str:
    return os.environ.get('CC') or cmake_cache_value('CMAKE_C_COMPILER') or shutil.which('cc') or shutil.which('clang') or shutil.which('clang.exe') or 'cc'
def toolchain_env()->dict[str,str]:
    env=os.environ.copy()
    comp=compiler()
    comp_dir=Path(comp).parent
    if comp_dir != Path('.'):
        env['PATH']=str(comp_dir)+os.pathsep+env.get('PATH','')
    return env
def main():
    ap=argparse.ArgumentParser();ap.add_argument('archive',nargs='?',default=str(ROOT/f'dist/mosaic-tokenizer-{VERSION}-linux-x86_64.tar.gz'));a=ap.parse_args();archive=Path(a.archive).resolve()
    if not archive.exists():raise SystemExit(f'missing archive: {archive}')
    with tempfile.TemporaryDirectory() as td:
        temp=Path(td)
        with tarfile.open(archive,'r:gz') as tf:tf.extractall(temp,filter='data')
        roots=[p for p in temp.iterdir() if p.is_dir()]
        if len(roots)!=1:raise SystemExit('archive must contain exactly one root directory')
        d=roots[0];cli=d/'bin/mosaic-tokenizer';author=d/'bin/mosaic-author'; registry=d/'bin/mosaic-registry';registry_http=d/'bin/mosaic-registry-http';mosaicd=d/'bin/mosaicd';mosaicd_client=d/'bin/mosaicd_client.py';model=d/'share/mosaic/packs/model-v2.mpack';uni=d/'share/mosaic/packs/unicode17-v1.mpack';langs={t:d/f'share/mosaic/packs/language/{t}-v1.mpack' for t in ('en','hi','ja')};det=d/'share/mosaic/packs/detector/reference-v1.mpack';security=d/'share/mosaic/packs/security17-v1.mpack';normalization=d/'share/mosaic/packs/normalization16-v1.mpack';lexers={t:d/f'share/mosaic/packs/lexer/{t}-v1.mpack' for t in ('c','python','rust','json')}
        if run([cli,'--version'])!=f'mosaic-tokenizer {VERSION}':raise SystemExit('packaged CLI version mismatch')
        if run([author,'--version'])!=f'mosaic-author {VERSION}':raise SystemExit('packaged author version mismatch')
        for rel in [
            'docs/release/README.md',
            f'docs/release/QUALIFICATION_{VERSION}.md',
            f'docs/release/QUALIFICATION_{VERSION}_WINDOWS_PACKAGE.md',
            'docs/release/RELEASE_NOTES_1.0.0.md',
            'docs/release/RELEASE_NOTES_1.0.1.md',
            'examples/integration/README.md',
            'examples/integration/low_memory_embed.c',
            'examples/integration/low_memory_embed.py',
            'examples/integration/mosaicd_client_example.py',
            'examples/integration/mosaicd_client.py',
        ]:
            if not (d/rel).exists():
                raise SystemExit(f'packaged release doc missing: {rel}')
        if not mosaicd_client.exists():
            raise SystemExit('packaged mosaicd client helper missing')
        if run([sys.executable, str(mosaicd_client), '--help']) is None:
            raise SystemExit('packaged mosaicd client helper not executable')
        py_wheel=d/'python'/f'mosaic_tokenizer-{VERSION}-py3-none-any.whl'
        if not py_wheel.exists(): raise SystemExit('packaged Python wheel missing')
        py_target=temp/'python-install';py_target.mkdir()
        subprocess.run([sys.executable,'-m','pip','install','--no-deps','--no-index','--target',str(py_target),str(py_wheel)],check=True,stdout=subprocess.DEVNULL)
        py_script=temp/'python-smoke.py';py_script.write_text("from mosaic import Tokenizer, BatchExecutor\nimport sys\nroot=sys.argv[1]\nwith Tokenizer(root+'/share/mosaic/packs/model-v2.mpack',root+'/share/mosaic/packs/unicode17-v1.mpack',library_path=root+'/lib/libmosaic.so') as t:\n    data=bytes(range(256))*2\n    ids=t.encode(data)\n    assert t.decode(ids)==data\n    t.seal()\n    with BatchExecutor(worker_count=2,queue_capacity=2,max_batch_items=8,max_total_input_bytes=1024,library_path=root+'/lib/libmosaic.so') as ex:\n        rs=ex.encode(t,[b'hello',b'world'])\n        assert len(rs)==2 and all(r.status==0 and r.ids for r in rs)\n", encoding="utf-8")
        py_env=os.environ.copy();py_env['PYTHONPATH']=str(py_target);py_env['MOSAIC_LIBRARY']=str(d/'lib/libmosaic.so')
        subprocess.run([sys.executable,str(py_script),str(d)],check=True,env=py_env)
        manifest=json.loads((d/'share/mosaic/release-manifest.json').read_text(encoding="utf-8"))
        if run([cli,'fingerprint',model,uni])!=manifest['tokenizer_fingerprint_sha256']:raise SystemExit('packaged base fingerprint mismatch')
        lf=run([cli,'fingerprint-languages',model,uni,langs['ja'],langs['en'],langs['hi']])
        if lf!=manifest['reference_language_fingerprint_sha256']:raise SystemExit('packaged language fingerprint mismatch/order instability')
        af=run([cli,'fingerprint-auto',model,uni,det,langs['hi'],langs['ja'],langs['en']])
        if af!=manifest['reference_auto_fingerprint_sha256']:raise SystemExit('packaged auto fingerprint mismatch/order instability')
        sf=run([cli,'fingerprint-security',model,uni,security])
        if sf!=manifest['reference_security_fingerprint_sha256']:raise SystemExit('packaged security fingerprint mismatch')
        nf=run([cli,'fingerprint-normalization',model,uni,normalization])
        if nf!=manifest['reference_normalization_fingerprint_sha256']:raise SystemExit('packaged normalization fingerprint mismatch')
        for tag,path in lexers.items():
            lfpx=run([cli,'fingerprint-lexer',model,uni,path])
            if lfpx!=manifest['lexer_packs'][tag]['tokenizer_fingerprint_sha256']:raise SystemExit(f'packaged lexer fingerprint mismatch: {tag}')
        lexer_sample=temp/'lexer.c';lexer_sample.write_bytes(b'int main(){return 0;} // hi\n')
        lexer_out=run([cli,'lexer',lexers['c'],lexer_sample])
        if 'profile=c' not in lexer_out or 'kind=keyword' not in lexer_out or 'kind=comment' not in lexer_out:raise SystemExit('packaged lexer CLI smoke failed')
        for line in (d/'SHA256SUMS').read_text(encoding="utf-8").splitlines():
            expected,rel=line.split('  ',1);actual=hashlib.sha256((d/rel).read_bytes()).hexdigest()
            if actual!=expected:raise SystemExit(f'checksum mismatch: {rel}')
        sample=temp/'mixed.txt';sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode())
        result=run([cli,'roundtrip-languages',model,uni,sample,langs['en'],langs['hi'],langs['ja']])
        if 'languages=3' not in result:raise SystemExit('packaged language CLI smoke failed')
        auto=run([cli,'roundtrip-auto',model,uni,det,sample,langs['en'],langs['hi'],langs['ja']])
        if 'route=none' not in auto:raise SystemExit('packaged ambiguous auto-routing fallback failed')
        en_sample=temp/'english.txt';en_sample.write_bytes(b'tokenizer')
        detected=run([cli,'roundtrip-auto',model,uni,det,en_sample,langs['en'],langs['hi'],langs['ja']])
        if 'route=en' not in detected or 'tokens=1' not in detected:raise SystemExit('packaged auto-routing specialization failed')
        span_auto=run([cli,'analyze-span-auto',model,uni,det,sample,langs['en'],langs['hi'],langs['ja']])
        if 'spans=' not in span_auto or 'span start=' not in span_auto or 'route=en' not in span_auto or 'route=none' not in span_auto:raise SystemExit('packaged span-routing CLI smoke failed')
        security_sample=temp/'security.txt';security_sample.write_bytes('AЖ\u202e'.encode())
        security_out=run([cli,'security',security,security_sample])
        if 'findings=' not in security_out or 'CYRILLIC' not in security_out:raise SystemExit('packaged security CLI smoke failed')
        norm_in=temp/'norm-in.txt';norm_out=temp/'norm-out.txt';norm_in.write_bytes('é'.encode())
        subprocess.run([cli,'normalize',normalization,'nfd',norm_in,norm_out],check=True)
        if norm_out.read_bytes()!=b'e\xcc\x81':raise SystemExit('packaged normalization CLI smoke failed')

        corpus=temp/'author-corpus.txt';corpus.write_text('tokenizer tokenizer hello world\nनमस्ते दुनिया नमस्ते दुनिया\n',encoding='utf-8')
        authored=temp/'authored.mpack';subprocess.run(launch([author,'train-model',corpus,'-o',authored,'--vocab-size','272','--min-frequency','1']),check=True)
        authored_sample=temp/'authored-sample.txt';authored_sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode())
        if 'OK' not in run([cli,'roundtrip',authored,authored_sample]):raise SystemExit('packaged author/native integration failed')
        tik=temp/'compat.tiktoken';tikpack=temp/'compat.mpack';vocab={bytes([b]):(b*73)%256 for b in range(256)};vocab.update({b'ab':256,b'bc':257,b'abc':258})
        tik.write_bytes(b''.join(base64.b64encode(surface)+b' '+str(rank).encode()+b'\n' for surface,rank in sorted(vocab.items(),key=lambda kv:kv[1])))
        subprocess.run(launch([author,'import-tiktoken',tik,tikpack]),check=True);compat_sample=temp/'compat.bin';compat_sample.write_bytes(b'abc');ids=temp/'compat.ids';subprocess.run([cli,'encode-u32',tikpack,compat_sample,ids],check=True)
        if ids.read_bytes()!=(258).to_bytes(4,'little'):raise SystemExit('packaged raw-BPE compatibility failed')
        client=temp/'client.c';client.write_text(r'''#include <mosaic.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

typedef struct { size_t count; } visitor_state;
typedef struct { atomic_uint_fast64_t count; } observer_state;
static void package_observer(void *ctx, const mosaic_event *event) {
    observer_state *state = (observer_state *)ctx;
    if (event && event->struct_size == sizeof *event)
        atomic_fetch_add_explicit(&state->count, 1u, memory_order_relaxed);
}
static mosaic_status count_security(void *ctx, const mosaic_security_finding *finding) {
    (void)finding;
    visitor_state *s = (visitor_state *)ctx;
    ++s->count;
    return MOSAIC_OK;
}

int main(int argc, char **argv) {
    if (argc != 8) return 2;
    mosaic_tokenizer *t = 0;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &t) != MOSAIC_OK) return 3;
    if (mosaic_tokenizer_add_language_file(t, argv[3]) != MOSAIC_OK) return 4;
    if (mosaic_tokenizer_set_detector_file(t, argv[4]) != MOSAIC_OK) return 5;
    if (mosaic_tokenizer_set_security_file(t, argv[5]) != MOSAIC_OK) return 8;
    if (mosaic_tokenizer_set_normalization_file(t, argv[6]) != MOSAIC_OK) return 10;
    if (mosaic_tokenizer_set_lexer_file(t, argv[7]) != MOSAIC_OK) return 31;
    unsigned char semantic_id[32] = {0}, runtime_default[32] = {0}, runtime_limited[32] = {0};
    if (mosaic_tokenizer_fingerprint(t, semantic_id) != MOSAIC_OK ||
        mosaic_tokenizer_runtime_identity(t, runtime_default) != MOSAIC_OK) return 49;
    observer_state obs = {0};
    mosaic_observer_config ocfg = {sizeof ocfg, 0u, MOSAIC_OBSERVE_SUCCESS | MOSAIC_OBSERVE_FAILURE | MOSAIC_OBSERVE_RESOURCE, 0u, package_observer, &obs};
    if (mosaic_tokenizer_set_observer(t, &ocfg) != MOSAIC_OK) return 62;
    unsigned char runtime_observed[32] = {0};
    if (mosaic_tokenizer_runtime_identity(t, runtime_observed) != MOSAIC_OK || memcmp(runtime_default, runtime_observed, 32)) return 63;
    mosaic_runtime_limits limits = {0};
    mosaic_runtime_limits_default(&limits);
    limits.max_input_bytes = 1024 * 1024;
    limits.max_output_tokens = 1024 * 1024;
    limits.max_token_document_bytes = 1024 * 1024;
    if (mosaic_tokenizer_set_runtime_limits(t, &limits) != MOSAIC_OK ||
        mosaic_tokenizer_runtime_identity(t, runtime_limited) != MOSAIC_OK ||
        !memcmp(runtime_default, runtime_limited, 32) ||
        mosaic_tokenizer_seal(t) != MOSAIC_OK || !mosaic_tokenizer_is_sealed(t) ||
        mosaic_tokenizer_set_runtime_limits(t, &limits) != MOSAIC_ERROR_STATE) return 50;
    unsigned char semantic_after[32] = {0};
    if (mosaic_tokenizer_fingerprint(t, semantic_after) != MOSAIC_OK || memcmp(semantic_id, semantic_after, 32)) return 51;

    mosaic_executor_config ecfg = {0}; mosaic_executor_config_default(&ecfg);
    ecfg.worker_count = 2; ecfg.queue_capacity = 2; ecfg.max_batch_items = 16; ecfg.max_total_input_bytes = 1024;
    mosaic_executor *executor = 0;
    if (mosaic_executor_create(&ecfg, &executor) != MOSAIC_OK) return 58;
    const unsigned char batch_a[] = "tokenizer", batch_b[] = "hello", batch_c[] = "world";
    mosaic_batch_input batch_inputs[3] = {{batch_a, 9}, {batch_b, 5}, {batch_c, 5}};
    mosaic_batch_result *batch_results = 0;
    if (mosaic_executor_encode_batch(executor, t, batch_inputs, 3, &batch_results) != MOSAIC_OK || !batch_results) return 59;
    for (size_t bi = 0; bi < 3; ++bi) if (batch_results[bi].status != MOSAIC_OK || !batch_results[bi].count) return 60;
    mosaic_batch_results_free(batch_results, 3);
    mosaic_executor_metrics em = {0};
    if (mosaic_executor_get_metrics(executor, &em) != MOSAIC_OK || em.batches != 1 || em.items != 3 || em.failed_items) return 61;
    mosaic_executor_free(executor);

    const unsigned char in[] = "tokenizer";
    unsigned int *ids = 0;
    size_t n = 0;
    mosaic_detection dt = {0};
    if (mosaic_tokenizer_encode_auto(t, in, 9, &ids, &n, &dt) != MOSAIC_OK) return 6;
    int ok = n == 1 && ids[0] == 271 && dt.matched && dt.available && !strcmp(dt.language, "en");

    mosaic_security_finding *f = 0;
    size_t fn = 0;
    if (mosaic_tokenizer_security_scan(t, in, 9, &f, &fn) != MOSAIC_OK) return 9;
    mosaic_free(f);
    visitor_state vs = {0};
    size_t visited = 0;
    if (mosaic_tokenizer_security_visit(t, in, 9, count_security, &vs, &visited) != MOSAIC_OK) return 12;
    if (visited != fn || vs.count != fn) return 13;

    mosaic_online_stream *online = 0;
    if (mosaic_tokenizer_online_stream_create(t, 4096, &online) != MOSAIC_OK) return 14;
    size_t consumed = 0;
    unsigned int *online_ids = 0;
    size_t online_n = 0;
    if (mosaic_online_stream_push(online, in, 9, &consumed, &online_ids, &online_n) != MOSAIC_OK) return 15;
    if (consumed != 9) return 16;
    unsigned int *online_tail = 0;
    size_t online_tail_n = 0;
    if (mosaic_online_stream_finish(online, &online_tail, &online_tail_n) != MOSAIC_OK) return 17;
    if (online_n + online_tail_n != 1) return 18;
    unsigned int online_id = online_n ? online_ids[0] : online_tail[0];
    if (online_id != 271) return 19;
    mosaic_free(online_ids);
    mosaic_free(online_tail);
    mosaic_online_stream_free(online);

    mosaic_incremental_document *incremental = 0;
    if (mosaic_tokenizer_incremental_document_create(t, in, 9, &incremental) != MOSAIC_OK) return 20;
    const unsigned char ex = '!';
    if (mosaic_incremental_document_apply_edit(incremental, 9, 0, &ex, 1) != MOSAIC_OK) return 21;
    unsigned int *incremental_ids = 0;
    size_t incremental_n = 0;
    if (mosaic_incremental_document_encode(incremental, &incremental_ids, &incremental_n) != MOSAIC_OK) return 22;
    if (!incremental_n || mosaic_incremental_document_last_reprocessed_bytes(incremental) > 10) return 23;
    mosaic_free(incremental_ids);
    mosaic_incremental_document_free(incremental);

    const unsigned char rs[] = "tokenizer tokenizer tokenizer tokenizer";
    mosaic_resync_document *resync = 0;
    if (mosaic_tokenizer_resync_document_create(t, rs, sizeof rs - 1, 8, 4096, &resync) != MOSAIC_OK) return 24;
    const unsigned char rz = 'X';
    if (mosaic_resync_document_apply_edit(resync, 15, 1, &rz, 1) != MOSAIC_OK) return 25;
    unsigned int *resync_ids = 0; size_t resync_n = 0;
    if (mosaic_resync_document_encode(resync, &resync_ids, &resync_n) != MOSAIC_OK || !resync_n) return 26;
    if (mosaic_resync_document_last_reprocessed_bytes(resync) > sizeof rs - 1) return 27;
    mosaic_free(resync_ids);
    mosaic_resync_document_free(resync);

    mosaic_token_document *tdoc = 0;
    if (mosaic_tokenizer_token_document_create(t, in, 9, MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES | MOSAIC_TOKEN_DOCUMENT_LEXICAL, &tdoc) != MOSAIC_OK) return 28;
    mosaic_token_document_info tinfo = {0};
    if (mosaic_token_document_get_info(tdoc, &tinfo) != MOSAIC_OK || tinfo.source_length != 9 || !tinfo.model_token_count) return 29;
    mosaic_document_token *dtokens = 0; size_t dtn = 0;
    if (mosaic_token_document_model_tokens(tdoc, &dtokens, &dtn) != MOSAIC_OK || dtn != tinfo.model_token_count) return 30;
    mosaic_free(dtokens);
    mosaic_block_policy bp = {0}; mosaic_block_policy_default(&bp); bp.min_bytes = 1; bp.preferred_bytes = 4; bp.max_bytes = 16; bp.macroblock_bytes = 16; bp.max_blocks = 64; bp.max_macroblocks = 64;
    mosaic_block_plan *bplan = 0;
    if (mosaic_token_document_block_plan(tdoc, &bp, &bplan) != MOSAIC_OK) return 36;
    mosaic_block_plan_info binfo = {0};
    if (mosaic_block_plan_get_info(bplan, &binfo) != MOSAIC_OK || !binfo.block_count || binfo.source_length != 9) return 37;
    mosaic_processing_block *blocks = 0; size_t blockn = 0;
    if (mosaic_block_plan_blocks(bplan, &blocks, &blockn) != MOSAIC_OK || blockn != binfo.block_count) return 38;
    mosaic_free(blocks); mosaic_block_plan_free(bplan);
    unsigned char *packed_model = 0; size_t packed_model_n = 0;
    if (mosaic_token_document_pack_model(tdoc, &packed_model, &packed_model_n) != MOSAIC_OK || !packed_model_n) return 39;
    mosaic_packed_model_info pinfo = {0};
    if (mosaic_packed_model_inspect(packed_model, packed_model_n, &pinfo) != MOSAIC_OK || pinfo.source_length != 9 || pinfo.token_count != dtn) return 40;
    mosaic_document_token *decoded_model = 0; size_t decoded_model_n = 0;
    if (mosaic_packed_model_decode(packed_model, packed_model_n, &decoded_model, &decoded_model_n) != MOSAIC_OK || decoded_model_n != dtn) return 41;
    mosaic_free(decoded_model); mosaic_free(packed_model);
    mosaic_lex_token *ltokens = 0; size_t ltn = 0;
    if (mosaic_token_document_lexical_tokens(tdoc, &ltokens, &ltn) != MOSAIC_OK || !ltn) return 32;
    mosaic_free(ltokens);
    unsigned char *cold = 0; size_t cold_n = 0;
    if (mosaic_token_document_serialize(tdoc, &cold, &cold_n) != MOSAIC_OK || cold_n < 544) return 53;
    mosaic_token_document *cold_doc = 0;
    if (mosaic_token_document_deserialize(cold, cold_n, &cold_doc) != MOSAIC_OK) return 54;
    unsigned char *cold_source = 0; size_t cold_source_n = 0;
    if (mosaic_token_document_copy_source(cold_doc, &cold_source, &cold_source_n) != MOSAIC_OK ||
        cold_source_n != 9 || memcmp(cold_source, in, 9)) return 55;
    mosaic_free(cold_source);
    mosaic_token_ir_limits cold_limits = {0}; mosaic_token_ir_limits_default(&cold_limits);
    cold_limits.max_record_bytes = cold_n - 1;
    mosaic_token_document *limited = 0;
    if (mosaic_token_document_deserialize_with_limits(cold, cold_n, &cold_limits, &limited) != MOSAIC_ERROR_RESOURCE_LIMIT || limited) return 56;
    unsigned char *cold2 = 0; size_t cold2_n = 0;
    if (mosaic_token_document_serialize(cold_doc, &cold2, &cold2_n) != MOSAIC_OK || cold2_n != cold_n || memcmp(cold2, cold, cold_n)) return 57;
    mosaic_free(cold2); mosaic_token_document_free(cold_doc); mosaic_free(cold);
    mosaic_token_document_free(tdoc);
    tdoc = 0;
    if (mosaic_tokenizer_token_document_create(t, in, 9, MOSAIC_TOKEN_DOCUMENT_SEMANTIC, &tdoc) != MOSAIC_OK) return 33;
    mosaic_semantic_component *sc = 0; size_t scn = 0;
    if (mosaic_token_document_semantic_components(tdoc, &sc, &scn) != MOSAIC_OK || !scn) return 34;
    mosaic_free(sc);
    mosaic_cache_config ccfg = {sizeof ccfg, 0u, 16u, 4096u, 1024u};
    mosaic_cache *cache = 0;
    if (mosaic_cache_create(&ccfg, &cache) != MOSAIC_OK) return 42;
    unsigned char ckey[32] = {0}; ckey[0] = 7;
    const unsigned char cval[] = {1,2,3,4};
    if (mosaic_cache_put(cache, ckey, cval, sizeof cval) != MOSAIC_OK) return 43;
    unsigned char *cout = 0; size_t cout_n = 0;
    if (mosaic_cache_get(cache, ckey, &cout, &cout_n) != MOSAIC_OK || cout_n != sizeof cval || memcmp(cout, cval, sizeof cval)) return 44;
    mosaic_free(cout);
    mosaic_cache_stats cstats = {0};
    if (mosaic_cache_get_stats(cache, &cstats) != MOSAIC_OK || cstats.hits != 1 || cstats.entries != 1) return 45;
    mosaic_cache_free(cache);
    unsigned char *crecord = 0; size_t crecord_n = 0;
    if (mosaic_cache_record_encode(ckey, cval, sizeof cval, &crecord, &crecord_n) != MOSAIC_OK) return 46;
    mosaic_cache_record_info crinfo = {0};
    if (mosaic_cache_record_inspect(crecord, crecord_n, &crinfo) != MOSAIC_OK || crinfo.value_length != sizeof cval) return 47;
    unsigned char *cdecoded = 0; size_t cdecoded_n = 0;
    if (mosaic_cache_record_decode(ckey, crecord, crecord_n, &cdecoded, &cdecoded_n) != MOSAIC_OK || cdecoded_n != sizeof cval || memcmp(cdecoded, cval, sizeof cval)) return 48;
    mosaic_free(cdecoded); mosaic_free(crecord);
    const unsigned char packed = 0xad; uint64_t nibble = 0;
    mosaic_subbyte_span ss = {0, 0, 4, MOSAIC_BIT_MSB0, 0};
    if (mosaic_subbyte_extract_u64(&packed, 1, ss, &nibble) != MOSAIC_OK || nibble != 0xa) return 35;

    mosaic_normalized_view v = {0};
    const unsigned char ni[] = {0xc3, 0xa9};
    if (mosaic_tokenizer_normalize(t, MOSAIC_NORMALIZE_NFD, ni, 2, &v) != MOSAIC_OK) return 11;
    if (v.byte_length != 3 || v.bytes[0] != 'e' || v.bytes[1] != 0xcc || v.bytes[2] != 0x81) ok = 0;
    mosaic_normalized_view_free(&v);
    mosaic_runtime_metrics metrics = {0};
    if (mosaic_tokenizer_get_metrics(t, &metrics) != MOSAIC_OK || !metrics.encode_calls || !metrics.tokens_out) return 52;
    if (!atomic_load_explicit(&obs.count, memory_order_relaxed)) return 64;
    mosaic_free(ids);
    mosaic_tokenizer_free(t);
    unsigned char *doc_copy = 0; size_t doc_copy_n = 0;
    if (mosaic_token_document_copy_source(tdoc, &doc_copy, &doc_copy_n) != MOSAIC_OK || doc_copy_n != 9 || memcmp(doc_copy, in, 9)) return 31;
    mosaic_free(doc_copy);
    mosaic_token_document_free(tdoc);
    return ok ? 0 : 7;
}
''', encoding="utf-8")
        trust_available=(d/'lib/libmosaic_trust.a').exists() and (d/'lib/libmosaic_trust.so').exists() and (d/'share/mosaic/trust/conformance-ed25519.pub').exists() and (d/'share/mosaic/trust/model-v2.mpack.sig').exists()
        if trust_available:
            cflags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',f'-I{d}/include',f'-L{d/"lib"}']
            if os.name!='nt':
                cflags.append('-pthread')
            else:
                cflags+=['-O3','-DNDEBUG','-D_CRT_SECURE_NO_WARNINGS','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-nostartfiles','-nostdlib','-Xlinker','/subsystem:console','-fuse-ld=lld','-lkernel32','-luser32','-lgdi32','-lwinspool','-lshell32','-lole32','-loleaut32','-luuid','-lcomdlg32','-ladvapi32','-loldnames']
            subprocess.run([compiler(),*cflags,str(client),str(d/'lib/libmosaic.a'),str(d/'lib/libmosaic_trust.a'),str(d/'lib/libcrypto.lib'),'-o',str(temp/'client')],check=True,env=toolchain_env() if os.name=='nt' else None)
            trust_env=os.environ.copy();trust_env['PATH']=str(d/'bin')+os.pathsep+trust_env.get('PATH','')
            subprocess.run([temp/'client',model,uni,langs['en'],det,security,normalization,lexers['c']],check=True,env=trust_env)
            # Trust authoring is offline-only and must regenerate the deterministic conformance record.
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
            from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, PublicFormat, NoEncryption
            tpriv=Ed25519PrivateKey.from_private_bytes(bytes(range(1,33)))
            priv_pem=temp/'trust-private.pem'; pub_pem=temp/'trust-public.pem'; generated_sig=temp/'generated.sig'
            priv_pem.write_bytes(tpriv.private_bytes(Encoding.PEM,PrivateFormat.PKCS8,NoEncryption()))
            pub_pem.write_bytes(tpriv.public_key().public_bytes(Encoding.PEM,PublicFormat.SubjectPublicKeyInfo))
            subprocess.run(launch([author,'sign-pack',model,priv_pem,generated_sig]),check=True)
            packaged_sig=d/'share/mosaic/trust/model-v2.mpack.sig'
            if generated_sig.read_bytes()!=packaged_sig.read_bytes():raise SystemExit('packaged trust authoring is not deterministic')
            expected_key=hashlib.sha256(tpriv.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw)).hexdigest()
            if run([author,'key-id',pub_pem])!=expected_key:raise SystemExit('packaged trust key-id mismatch')
            trust_client=ROOT/'conformance/c/package_trust_client.c'
            subprocess.run([compiler(),*cflags,str(trust_client),str(d/'lib/libmosaic.a'),str(d/'lib/libmosaic_trust.a'),str(d/'lib/libcrypto.lib'),'-o',str(temp/'trust_client')],check=True,env=toolchain_env() if os.name=='nt' else None)
            subprocess.run([temp/'trust_client',model,d/'share/mosaic/trust/conformance-ed25519.pub',packaged_sig],check=True,env=trust_env)
            regdir=temp/'registry'
            subprocess.run([sys.executable,mosaicd,'--help'],check=True,stdout=subprocess.DEVNULL)
            subprocess.run([sys.executable,registry_http,'--help'],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(launch([registry,'init',regdir]),check=True)
            subprocess.run(launch([registry,'install',regdir,model,'--publisher','org.mosaic','--name','reference-model','--version','1.0.0','--signature',packaged_sig,'--public-key',d/'share/mosaic/trust/conformance-ed25519.pub','--require-signature']),check=True)
            subprocess.run(launch([registry,'install',regdir,uni,'--publisher','org.mosaic','--name','unicode17','--version','17.0.0']),check=True)
            req=temp/'requirements.json'; lock=temp/'mosaic.lock.json'
            req.write_text(json.dumps({'schema':1,'requirements':[{'role':'model','publisher':'org.mosaic','name':'reference-model','constraint':'^1.0.0'},{'role':'unicode','publisher':'org.mosaic','name':'unicode17','constraint':'==17.0.0'}]}), encoding="utf-8")
            subprocess.run(launch([registry,'resolve',regdir,req,'-o',lock]),check=True)
            subprocess.run(launch([registry,'verify-lock',regdir,lock]),check=True)
            subprocess.run(launch([registry,'audit',regdir]),check=True)
            locked=json.loads(lock.read_text(encoding="utf-8"))
            if locked['packs'][0]['role']!='model' or locked['packs'][0]['trust_status']!='verified':raise SystemExit('packaged registry did not preserve verified lock identity')
        else:
            print('SKIP: trust library artifacts not present in packaged release')
        subprocess.run([sys.executable,str(ROOT/'tools/validate_supply_chain.py'),str(d),'--source-checksums',str(ROOT/'ARTIFACT_CHECKSUMS.sha256')],check=True,cwd=ROOT)
    expected_archive=f"mosaic-tokenizer-{VERSION}-{'windows-x86_64' if os.name=='nt' else 'linux-x86_64'}.tar.gz"
    if archive.name != expected_archive:
        raise SystemExit(f'unexpected archive name: {archive.name} != {expected_archive}')
    print(f'OK: packaged release {archive.name} passes CLI, packs, manifest, checksums, SBOM/provenance, authoring, compatibility, and external static-client smoke');return 0
if __name__=='__main__':raise SystemExit(main())
