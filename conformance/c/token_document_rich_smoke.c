#include "mosaic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message) { fprintf(stderr, "%s\n", message); return 1; }

int main(int argc, char **argv) {
    if (argc != 5) return fail("usage: token_document_rich_smoke MODEL UNICODE SECURITY NORMALIZATION");
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) return fail("load tokenizer");

    mosaic_tokenizer_capabilities caps = { sizeof(caps), 0u, 0u };
    if (mosaic_tokenizer_get_capabilities(tokenizer, &caps) != MOSAIC_OK) return fail("capabilities base");
    if (!(caps.available & MOSAIC_CAP_MODEL) || !(caps.available & MOSAIC_CAP_GRAPHEMES) ||
        !(caps.available & MOSAIC_CAP_TOKEN_DOCUMENT) || (caps.available & MOSAIC_CAP_SECURITY) ||
        (caps.available & MOSAIC_CAP_NORMALIZATION)) return fail("unexpected base capabilities");

    if (mosaic_tokenizer_set_security_file(tokenizer, argv[3]) != MOSAIC_OK) return fail("load security");
    if (mosaic_tokenizer_set_normalization_file(tokenizer, argv[4]) != MOSAIC_OK) return fail("load normalization");
    caps = (mosaic_tokenizer_capabilities){ sizeof(caps), 0u, 0u };
    if (mosaic_tokenizer_get_capabilities(tokenizer, &caps) != MOSAIC_OK ||
        !(caps.available & MOSAIC_CAP_SECURITY) || !(caps.available & MOSAIC_CAP_NORMALIZATION))
        return fail("loaded capabilities");

    /* A + COMBINING RING ABOVE + space + RIGHT-TO-LEFT OVERRIDE. */
    static const uint8_t input[] = { 'A', 0xCC, 0x8A, ' ', 0xE2, 0x80, 0xAE };
    mosaic_token_document_options options = {
        sizeof(options),
        MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES |
            MOSAIC_TOKEN_DOCUMENT_SECURITY | MOSAIC_TOKEN_DOCUMENT_NORMALIZATION,
        MOSAIC_NORMALIZE_NFC,
        0u
    };
    mosaic_token_document *document = NULL;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, input, sizeof input, &options, &document) != MOSAIC_OK)
        return fail("create rich document");

    mosaic_token_document_info info;
    if (mosaic_token_document_get_info(document, &info) != MOSAIC_OK) return fail("document info");
    if (info.source_length != sizeof input || info.model_token_count == 0 || info.grapheme_count == 0 ||
        info.security_finding_count == 0 || info.normalized_byte_length >= sizeof input ||
        info.normalization_mode != MOSAIC_NORMALIZE_NFC) return fail("document info contents");

    mosaic_security_finding *findings = NULL; size_t finding_count = 0;
    if (mosaic_token_document_security_findings(document, &findings, &finding_count) != MOSAIC_OK || !finding_count)
        return fail("security projection");
    int saw_bidi = 0;
    for (size_t i = 0; i < finding_count; ++i) if (findings[i].kind == MOSAIC_SECURITY_BIDI_CONTROL) saw_bidi = 1;
    mosaic_free(findings);
    if (!saw_bidi) return fail("missing bidi evidence");

    mosaic_normalized_view normalized = {0};
    if (mosaic_token_document_normalized_view(document, &normalized) != MOSAIC_OK) return fail("normalization projection");
    static const uint8_t expected_prefix[] = { 0xC3, 0x85, ' ' };
    if (normalized.byte_length != 6 || memcmp(normalized.bytes, expected_prefix, sizeof expected_prefix) != 0)
        return fail("NFC bytes");
    mosaic_normalized_view_free(&normalized);

    uint8_t *source_copy = NULL; size_t source_len = 0;
    if (mosaic_token_document_copy_source(document, &source_copy, &source_len) != MOSAIC_OK ||
        source_len != sizeof input || memcmp(source_copy, input, sizeof input) != 0) return fail("source copy");
    mosaic_free(source_copy);

    /* Snapshot projections must remain valid after tokenizer destruction. */
    mosaic_tokenizer_free(tokenizer); tokenizer = NULL;
    normalized = (mosaic_normalized_view){0};
    if (mosaic_token_document_normalized_view(document, &normalized) != MOSAIC_OK || normalized.byte_length != 6)
        return fail("snapshot lifetime");
    mosaic_normalized_view_free(&normalized);
    mosaic_token_document_free(document);

    /* Unrequested projections are not silently materialized. */
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) return fail("reload tokenizer");
    if (mosaic_tokenizer_token_document_create(tokenizer, input, sizeof input, MOSAIC_TOKEN_DOCUMENT_MODEL, &document) != MOSAIC_OK)
        return fail("create sparse document");
    findings = NULL; finding_count = 0;
    if (mosaic_token_document_security_findings(document, &findings, &finding_count) != MOSAIC_ERROR_UNSUPPORTED)
        return fail("hidden security projection");
    normalized = (mosaic_normalized_view){0};
    if (mosaic_token_document_normalized_view(document, &normalized) != MOSAIC_ERROR_UNSUPPORTED)
        return fail("hidden normalization projection");
    mosaic_token_document_free(document);

    /* Requesting unavailable optional capabilities fails explicitly. */
    options.flags = MOSAIC_TOKEN_DOCUMENT_SECURITY;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, input, sizeof input, &options, &document) != MOSAIC_ERROR_UNSUPPORTED)
        return fail("missing security capability");
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
