#include "mosaic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message) { fprintf(stderr, "%s\n", message); return 1; }

static int same_info(const mosaic_token_document_info *a, const mosaic_token_document_info *b) {
    return a->flags == b->flags && a->source_length == b->source_length &&
           a->model_token_count == b->model_token_count && a->grapheme_count == b->grapheme_count &&
           a->security_finding_count == b->security_finding_count &&
           a->normalized_byte_length == b->normalized_byte_length && a->normalized_unit_count == b->normalized_unit_count &&
           a->lexical_token_count == b->lexical_token_count && a->semantic_component_count == b->semantic_component_count &&
           a->normalization_mode == b->normalization_mode &&
           memcmp(a->source_sha256, b->source_sha256, 32u) == 0 &&
           memcmp(a->tokenizer_fingerprint_sha256, b->tokenizer_fingerprint_sha256, 32u) == 0 &&
           memcmp(&a->detection, &b->detection, sizeof a->detection) == 0;
}

static int compare_projection(const mosaic_token_document *a, const mosaic_token_document *b) {
    mosaic_document_token *ma = NULL, *mb = NULL; size_t mac = 0, mbc = 0;
    if (mosaic_token_document_model_tokens(a, &ma, &mac) != MOSAIC_OK ||
        mosaic_token_document_model_tokens(b, &mb, &mbc) != MOSAIC_OK || mac != mbc) goto bad_model;
    for (size_t i = 0; i < mac; ++i)
        if (ma[i].id != mb[i].id || ma[i].start != mb[i].start || ma[i].length != mb[i].length) goto bad_model;
    mosaic_free(ma); mosaic_free(mb); ma = mb = NULL;

    mosaic_range *ga = NULL, *gb = NULL; size_t gac = 0, gbc = 0;
    if (mosaic_token_document_graphemes(a, &ga, &gac) != MOSAIC_OK ||
        mosaic_token_document_graphemes(b, &gb, &gbc) != MOSAIC_OK || gac != gbc ||
        (gac && memcmp(ga, gb, gac * sizeof *ga) != 0)) goto bad_grapheme;
    mosaic_free(ga); mosaic_free(gb); ga = gb = NULL;

    mosaic_security_finding *sa = NULL, *sb = NULL; size_t sac = 0, sbc = 0;
    if (mosaic_token_document_security_findings(a, &sa, &sac) != MOSAIC_OK ||
        mosaic_token_document_security_findings(b, &sb, &sbc) != MOSAIC_OK || sac != sbc) goto bad_security;
    for (size_t i = 0; i < sac; ++i)
        if (sa[i].kind != sb[i].kind || sa[i].script_id != sb[i].script_id || sa[i].reserved != sb[i].reserved ||
            sa[i].start != sb[i].start || sa[i].length != sb[i].length) goto bad_security;
    mosaic_free(sa); mosaic_free(sb); sa = sb = NULL;

    mosaic_normalized_view na = {0}, nb = {0};
    if (mosaic_token_document_normalized_view(a, &na) != MOSAIC_OK ||
        mosaic_token_document_normalized_view(b, &nb) != MOSAIC_OK ||
        na.byte_length != nb.byte_length || na.unit_count != nb.unit_count || na.source_span_count != nb.source_span_count ||
        (na.byte_length && memcmp(na.bytes, nb.bytes, na.byte_length) != 0) ||
        (na.unit_count && memcmp(na.units, nb.units, na.unit_count * sizeof *na.units) != 0) ||
        (na.source_span_count && memcmp(na.source_spans, nb.source_spans, na.source_span_count * sizeof *na.source_spans) != 0))
        goto bad_normalized;
    mosaic_normalized_view_free(&na); mosaic_normalized_view_free(&nb);

    mosaic_lex_token *la = NULL, *lb = NULL; size_t lac = 0, lbc = 0;
    if (mosaic_token_document_lexical_tokens(a, &la, &lac) != MOSAIC_OK ||
        mosaic_token_document_lexical_tokens(b, &lb, &lbc) != MOSAIC_OK || lac != lbc) goto bad_lex;
    for (size_t i = 0; i < lac; ++i)
        if (la[i].kind != lb[i].kind || la[i].flags != lb[i].flags || la[i].start != lb[i].start || la[i].length != lb[i].length) goto bad_lex;
    mosaic_free(la); mosaic_free(lb); la = lb = NULL;

    mosaic_semantic_component *ea = NULL, *eb = NULL; size_t eac = 0, ebc = 0;
    if (mosaic_token_document_semantic_components(a, &ea, &eac) != MOSAIC_OK ||
        mosaic_token_document_semantic_components(b, &eb, &ebc) != MOSAIC_OK || eac != ebc) goto bad_semantic;
    for (size_t i = 0; i < eac; ++i)
        if (ea[i].kind != eb[i].kind || ea[i].flags != eb[i].flags || ea[i].lexical_index != eb[i].lexical_index ||
            ea[i].start != eb[i].start || ea[i].length != eb[i].length) goto bad_semantic;
    mosaic_free(ea); mosaic_free(eb);
    return 1;

bad_semantic: fprintf(stderr, "semantic mismatch\n"); mosaic_free(ea); mosaic_free(eb); return 0;
bad_lex: fprintf(stderr, "lex mismatch\n"); mosaic_free(la); mosaic_free(lb); return 0;
bad_normalized: fprintf(stderr, "normalized mismatch\n"); mosaic_normalized_view_free(&na); mosaic_normalized_view_free(&nb); return 0;
bad_security: fprintf(stderr, "security mismatch\n"); mosaic_free(sa); mosaic_free(sb); return 0;
bad_grapheme: fprintf(stderr, "grapheme mismatch\n"); mosaic_free(ga); mosaic_free(gb); return 0;
bad_model: fprintf(stderr, "model mismatch\n"); mosaic_free(ma); mosaic_free(mb); return 0;
}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) return fail("usage: token_document_serialization_smoke MODEL UNICODE SECURITY NORMALIZATION LEXER [RECORD_OUT]");
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) return fail("load tokenizer");
    if (mosaic_tokenizer_set_security_file(tokenizer, argv[3]) != MOSAIC_OK) return fail("load security");
    if (mosaic_tokenizer_set_normalization_file(tokenizer, argv[4]) != MOSAIC_OK) return fail("load normalization");
    if (mosaic_tokenizer_set_lexer_file(tokenizer, argv[5]) != MOSAIC_OK) return fail("load lexer");

    mosaic_tokenizer_capabilities caps = { sizeof caps, 0u, 0u };
    if (mosaic_tokenizer_get_capabilities(tokenizer, &caps) != MOSAIC_OK ||
        !(caps.available & MOSAIC_CAP_TOKEN_DOCUMENT_SERIALIZATION)) return fail("serialization capability");

    static const uint8_t input[] = {
        'i','n','t',' ','d','e','s','e','r','i','a','l','i','z','e','H','T','T','P','2','R','e','s','p','o','n','s','e',' ', '=', ' ',
        '0','x','1','.','f','p','2',';', ' ', '/', '*', ' ', 'A', 0xCC,0x8A, ' ', 0xE2,0x80,0xAE, ' ', '*','/'
    };
    const uint32_t flags = MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES |
                           MOSAIC_TOKEN_DOCUMENT_SECURITY | MOSAIC_TOKEN_DOCUMENT_NORMALIZATION |
                           MOSAIC_TOKEN_DOCUMENT_LEXICAL | MOSAIC_TOKEN_DOCUMENT_SEMANTIC;
    mosaic_token_document_options options = { sizeof options, flags, MOSAIC_NORMALIZE_NFD, 0u };
    mosaic_token_document *original = NULL, *roundtrip = NULL;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, input, sizeof input, &options, &original) != MOSAIC_OK)
        return fail("create full document");

    uint8_t *record = NULL; size_t record_len = 0;
    if (mosaic_token_document_serialize(original, &record, &record_len) != MOSAIC_OK || record_len < 544u)
        return fail("serialize");
    if (argc == 7) {
        FILE *out = fopen(argv[6], "wb");
        if (!out || fwrite(record, 1u, record_len, out) != record_len || fclose(out) != 0) return fail("write record");
    }
    if (mosaic_token_document_deserialize(record, record_len, &roundtrip) != MOSAIC_OK) return fail("deserialize");

    mosaic_token_document_info ai, bi;
    if (mosaic_token_document_get_info(original, &ai) != MOSAIC_OK || mosaic_token_document_get_info(roundtrip, &bi) != MOSAIC_OK || !same_info(&ai, &bi))
        return fail("info mismatch");
    if (!compare_projection(original, roundtrip)) return fail("projection mismatch");

    uint8_t *record2 = NULL; size_t record2_len = 0;
    if (mosaic_token_document_serialize(roundtrip, &record2, &record2_len) != MOSAIC_OK ||
        record_len != record2_len || memcmp(record, record2, record_len) != 0) return fail("noncanonical reserialize");
    mosaic_free(record2);

    uint8_t *source = NULL; size_t source_len = 0;
    if (mosaic_token_document_copy_source(roundtrip, &source, &source_len) != MOSAIC_OK || source_len != sizeof input || memcmp(source, input, sizeof input) != 0)
        return fail("source mismatch");
    mosaic_free(source);

    uint8_t saved = record[record_len - 1u]; record[record_len - 1u] ^= 0x80u;
    mosaic_token_document *bad = NULL;
    if (mosaic_token_document_deserialize(record, record_len, &bad) != MOSAIC_ERROR_INTEGRITY || bad) return fail("tamper accepted");
    record[record_len - 1u] = saved;

    mosaic_token_ir_limits limits; mosaic_token_ir_limits_default(&limits);
    limits.max_record_bytes = record_len - 1u;
    if (mosaic_token_document_deserialize_with_limits(record, record_len, &limits, &bad) != MOSAIC_ERROR_RESOURCE_LIMIT) return fail("record limit");
    mosaic_token_ir_limits_default(&limits); limits.max_source_bytes = sizeof input - 1u;
    if (mosaic_token_document_deserialize_with_limits(record, record_len, &limits, &bad) != MOSAIC_ERROR_RESOURCE_LIMIT) return fail("source limit");
    mosaic_token_ir_limits_default(&limits); limits.max_projection_items = 1u;
    if (mosaic_token_document_deserialize_with_limits(record, record_len, &limits, &bad) != MOSAIC_ERROR_RESOURCE_LIMIT) return fail("item limit");

    mosaic_token_document_free(roundtrip); roundtrip = NULL;
    mosaic_token_document_free(original); original = NULL;
    mosaic_free(record); record = NULL;

    /* Semantic-only documents persist the lexical dependency without exposing it publicly. */
    options.flags = MOSAIC_TOKEN_DOCUMENT_SEMANTIC;
    options.normalization_mode = MOSAIC_NORMALIZE_PRESERVE;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, input, sizeof input, &options, &original) != MOSAIC_OK) return fail("create semantic-only");
    if (mosaic_token_document_serialize(original, &record, &record_len) != MOSAIC_OK ||
        mosaic_token_document_deserialize(record, record_len, &roundtrip) != MOSAIC_OK) return fail("semantic-only roundtrip");
    mosaic_semantic_component *components = NULL; size_t component_count = 0;
    if (mosaic_token_document_semantic_components(roundtrip, &components, &component_count) != MOSAIC_OK || !component_count)
        return fail("semantic-only components");
    mosaic_free(components);
    mosaic_lex_token *lex = NULL; size_t lex_count = 0;
    if (mosaic_token_document_lexical_tokens(roundtrip, &lex, &lex_count) != MOSAIC_ERROR_UNSUPPORTED)
        return fail("semantic-only leaked lexical projection");
    mosaic_token_document_free(roundtrip); mosaic_token_document_free(original); mosaic_free(record);

    /* Empty records are canonical too. */
    options.flags = flags;
    options.normalization_mode = MOSAIC_NORMALIZE_NFC;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, NULL, 0u, &options, &original) != MOSAIC_OK ||
        mosaic_token_document_serialize(original, &record, &record_len) != MOSAIC_OK ||
        mosaic_token_document_deserialize(record, record_len, &roundtrip) != MOSAIC_OK) return fail("empty roundtrip");
    if (mosaic_token_document_get_info(roundtrip, &bi) != MOSAIC_OK || bi.source_length != 0u) return fail("empty info");
    mosaic_token_document_free(roundtrip); mosaic_token_document_free(original); mosaic_free(record);
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
