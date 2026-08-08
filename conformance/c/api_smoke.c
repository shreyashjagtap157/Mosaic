#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state = UINT64_C(0x4d4f534149435354);
static uint32_t rnd32(void) {
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return (uint32_t)rng_state;
}

static int exact_case(mosaic_model *model, const uint8_t *input, size_t len) {
    uint32_t *ids = NULL; size_t count = 0;
    if (mosaic_encode(model, input, len, &ids, &count) != MOSAIC_OK) return 0;
    uint8_t *decoded = NULL; size_t decoded_len = 0;
    if (mosaic_decode(model, ids, count, &decoded, &decoded_len) != MOSAIC_OK) { mosaic_free(ids); return 0; }
    int ok = decoded_len == len && (!len || memcmp(decoded, input, len) == 0);
    mosaic_free(decoded); mosaic_free(ids); return ok;
}

int main(int argc, char **argv) {
    if (mosaic_tokenizer_semantics_version() != 2u) return 60;
    if (argc != 3) return 2;
    mosaic_model *model = NULL; mosaic_unicode *unicode_data = NULL;
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_model_load_file(argv[1], &model) != MOSAIC_OK) return 3;
    if (mosaic_unicode_load_file(argv[2], &unicode_data) != MOSAIC_OK) { mosaic_model_free(model); return 4; }
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) { mosaic_unicode_free(unicode_data); mosaic_model_free(model); return 18; }
    const uint8_t input[] = {'h','e','l','l','o',' ','w','o','r','l','d',0xff};
    if (!exact_case(model, input, sizeof input)) return 5;
    {
        uint8_t fingerprint[32] = {0};
        uint32_t *ids = NULL; size_t id_count = 0; uint8_t *decoded = NULL; size_t decoded_len = 0;
        mosaic_range *integrated_ranges = NULL; size_t integrated_count = 0;
        if (mosaic_tokenizer_fingerprint(tokenizer, fingerprint) != MOSAIC_OK) return 19;
        int nonzero = 0; for (size_t i = 0; i < sizeof fingerprint; ++i) nonzero |= fingerprint[i] != 0;
        if (!nonzero) return 20;
        if (mosaic_tokenizer_encode(tokenizer, input, sizeof input, &ids, &id_count) != MOSAIC_OK) return 21;
        if (mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) != MOSAIC_OK) return 22;
        if (decoded_len != sizeof input || memcmp(decoded, input, sizeof input) != 0) return 23;
        if (mosaic_tokenizer_grapheme_ranges(tokenizer, input, sizeof input, &integrated_ranges, &integrated_count) != MOSAIC_OK) return 24;
        if (integrated_count == 0) return 25;
        mosaic_free(ids); mosaic_free(decoded); mosaic_free(integrated_ranges);
    }

    uint8_t random_input[512];
    for (size_t test = 0; test < 2000; ++test) {
        size_t len = rnd32() % (sizeof random_input + 1);
        for (size_t i = 0; i < len; ++i) random_input[i] = (uint8_t)rnd32();
        if (!exact_case(model, random_input, len)) return 6;
    }

    /* Stream/full equality in one process. */
    for (size_t test = 0; test < 100; ++test) {
        size_t len = rnd32() % (sizeof random_input + 1);
        for (size_t i = 0; i < len; ++i) random_input[i] = (uint8_t)rnd32();
        uint32_t *full = NULL, *streamed = NULL; size_t full_n = 0, stream_n = 0;
        if (mosaic_encode(model, random_input, len, &full, &full_n) != MOSAIC_OK) return 7;
        mosaic_stream *stream = NULL; if (mosaic_stream_create(model, &stream) != MOSAIC_OK) return 8;
        size_t pos = 0;
        while (pos < len) { size_t step = 1 + rnd32() % 31; if (step > len - pos) step = len - pos; if (mosaic_stream_push(stream, random_input + pos, step) != MOSAIC_OK) return 9; pos += step; }
        if (mosaic_stream_finish(stream, &streamed, &stream_n) != MOSAIC_OK) return 10;
        if (full_n != stream_n || (full_n && memcmp(full, streamed, full_n * sizeof(uint32_t)) != 0)) return 11;
        mosaic_free(full); mosaic_free(streamed); mosaic_stream_free(stream);
    }

    /* Editable document equality after randomized edits. */
    mosaic_document *doc = NULL;
    if (mosaic_document_create(model, input, sizeof input, &doc) != MOSAIC_OK) return 12;
    uint8_t expected[4096]; size_t expected_len = sizeof input; memcpy(expected, input, expected_len);
    for (size_t test = 0; test < 250; ++test) {
        size_t start = expected_len ? rnd32() % (expected_len + 1) : 0;
        size_t max_delete = expected_len - start; if (max_delete > 8) max_delete = 8;
        size_t del = max_delete ? rnd32() % (max_delete + 1) : 0;
        uint8_t replacement[8]; size_t repl = rnd32() % 9; for (size_t i = 0; i < repl; ++i) replacement[i] = (uint8_t)rnd32();
        size_t new_len = expected_len - del + repl; if (new_len > sizeof expected) return 13;
        memmove(expected + start + repl, expected + start + del, expected_len - start - del); memcpy(expected + start, replacement, repl); expected_len = new_len;
        if (mosaic_document_apply_edit(doc, start, del, replacement, repl) != MOSAIC_OK) return 14;
        uint32_t *a = NULL, *b = NULL; size_t an = 0, bn = 0;
        if (mosaic_document_encode(doc, &a, &an) != MOSAIC_OK || mosaic_encode(model, expected, expected_len, &b, &bn) != MOSAIC_OK) return 15;
        if (an != bn || (an && memcmp(a, b, an * sizeof(uint32_t)) != 0)) return 16;
        mosaic_free(a); mosaic_free(b);
    }
    mosaic_document_free(doc);

    mosaic_range *ranges = NULL; size_t range_count = 0;
    if (mosaic_grapheme_ranges(unicode_data, input, sizeof input, &ranges, &range_count) != MOSAIC_OK) return 17;
    mosaic_free(ranges);
    printf("OK api=%s random=2000 streams=100 edits=250 graphemes=%zu\n", mosaic_version_string(), range_count);
    mosaic_tokenizer_free(tokenizer); mosaic_unicode_free(unicode_data); mosaic_model_free(model); return 0;
}
