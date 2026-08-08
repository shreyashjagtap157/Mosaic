#!/usr/bin/env python3
from __future__ import annotations
import argparse,base64,hashlib,json,subprocess,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];VERSION=(ROOT/'VERSION').read_text().strip()
def run(cmd,cwd=None):return subprocess.check_output([str(x) for x in cmd],cwd=cwd,text=True).strip()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('archive',nargs='?',default=str(ROOT/f'dist/mosaic-tokenizer-{VERSION}-linux-x86_64.tar.gz'));a=ap.parse_args();archive=Path(a.archive).resolve()
    if not archive.exists():raise SystemExit(f'missing archive: {archive}')
    with tempfile.TemporaryDirectory() as td:
        temp=Path(td)
        with tarfile.open(archive,'r:gz') as tf:tf.extractall(temp,filter='data')
        roots=[p for p in temp.iterdir() if p.is_dir()]
        if len(roots)!=1:raise SystemExit('archive must contain exactly one root directory')
        d=roots[0];cli=d/'bin/mosaic-tokenizer';author=d/'bin/mosaic-author';model=d/'share/mosaic/packs/model-v2.mpack';uni=d/'share/mosaic/packs/unicode17-v1.mpack';langs={t:d/f'share/mosaic/packs/language/{t}-v1.mpack' for t in ('en','hi','ja')};det=d/'share/mosaic/packs/detector/reference-v1.mpack';security=d/'share/mosaic/packs/security17-v1.mpack';normalization=d/'share/mosaic/packs/normalization16-v1.mpack';lexers={t:d/f'share/mosaic/packs/lexer/{t}-v1.mpack' for t in ('c','python','rust','json')}
        if run([cli,'--version'])!=f'mosaic-tokenizer {VERSION}':raise SystemExit('packaged CLI version mismatch')
        if run([author,'--version'])!=f'mosaic-author {VERSION}':raise SystemExit('packaged author version mismatch')
        manifest=json.loads((d/'share/mosaic/release-manifest.json').read_text())
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
        for line in (d/'SHA256SUMS').read_text().splitlines():
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
        security_sample=temp/'security.txt';security_sample.write_bytes('AЖ\u202e'.encode())
        security_out=run([cli,'security',security,security_sample])
        if 'findings=' not in security_out or 'CYRILLIC' not in security_out:raise SystemExit('packaged security CLI smoke failed')
        norm_in=temp/'norm-in.txt';norm_out=temp/'norm-out.txt';norm_in.write_bytes('é'.encode())
        subprocess.run([cli,'normalize',normalization,'nfd',norm_in,norm_out],check=True)
        if norm_out.read_bytes()!=b'e\xcc\x81':raise SystemExit('packaged normalization CLI smoke failed')

        corpus=temp/'author-corpus.txt';corpus.write_text('tokenizer tokenizer hello world\nनमस्ते दुनिया नमस्ते दुनिया\n',encoding='utf-8')
        authored=temp/'authored.mpack';subprocess.run([author,'train-model',corpus,'-o',authored,'--vocab-size','272','--min-frequency','1'],check=True)
        authored_sample=temp/'authored-sample.txt';authored_sample.write_bytes(b'tokenizer '+ 'नमस्ते दुनिया'.encode())
        if 'OK' not in run([cli,'roundtrip',authored,authored_sample]):raise SystemExit('packaged author/native integration failed')
        tik=temp/'compat.tiktoken';tikpack=temp/'compat.mpack';vocab={bytes([b]):(b*73)%256 for b in range(256)};vocab.update({b'ab':256,b'bc':257,b'abc':258})
        tik.write_bytes(b''.join(base64.b64encode(surface)+b' '+str(rank).encode()+b'\n' for surface,rank in sorted(vocab.items(),key=lambda kv:kv[1])))
        subprocess.run([author,'import-tiktoken',tik,tikpack],check=True);compat_sample=temp/'compat.bin';compat_sample.write_bytes(b'abc');ids=temp/'compat.ids';subprocess.run([cli,'encode-u32',tikpack,compat_sample,ids],check=True)
        if ids.read_bytes()!=(258).to_bytes(4,'little'):raise SystemExit('packaged raw-BPE compatibility failed')
        client=temp/'client.c';client.write_text(r'''#include <mosaic.h>
#include <stddef.h>
#include <string.h>

typedef struct { size_t count; } visitor_state;
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
    mosaic_lex_token *ltokens = 0; size_t ltn = 0;
    if (mosaic_token_document_lexical_tokens(tdoc, &ltokens, &ltn) != MOSAIC_OK || !ltn) return 32;
    mosaic_free(ltokens);

    mosaic_normalized_view v = {0};
    const unsigned char ni[] = {0xc3, 0xa9};
    if (mosaic_tokenizer_normalize(t, MOSAIC_NORMALIZE_NFD, ni, 2, &v) != MOSAIC_OK) return 11;
    if (v.byte_length != 3 || v.bytes[0] != 'e' || v.bytes[1] != 0xcc || v.bytes[2] != 0x81) ok = 0;
    mosaic_normalized_view_free(&v);
    mosaic_free(ids);
    mosaic_tokenizer_free(t);
    unsigned char *doc_copy = 0; size_t doc_copy_n = 0;
    if (mosaic_token_document_copy_source(tdoc, &doc_copy, &doc_copy_n) != MOSAIC_OK || doc_copy_n != 9 || memcmp(doc_copy, in, 9)) return 31;
    mosaic_free(doc_copy);
    mosaic_token_document_free(tdoc);
    return ok ? 0 : 7;
}
''')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',f'-I{d}/include',client,d/'lib/libmosaic.a','-o',temp/'client'],check=True)
        subprocess.run([temp/'client',model,uni,langs['en'],det,security,normalization,lexers['c']],check=True)
    print(f'OK: packaged release {archive.name} passes CLI, detector/language/security/normalization/lexer packs, manifest, checksums, authoring, compatibility, and external static-client smoke');return 0
if __name__=='__main__':raise SystemExit(main())
