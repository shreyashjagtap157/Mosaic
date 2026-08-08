#include "mosaic.h"
#include "test_thread.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_u32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint64_t mix64(uint64_t h, uint64_t value) {
    h ^= value + UINT64_C(0x9e3779b97f4a7c15) + (h << 6) + (h >> 2);
    return h;
}

static size_t env_size(const char *name, size_t fallback, size_t maximum) {
    const char *text = getenv(name);
    if (!text || !*text) return fallback;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 10);
    if (!end || *end || value == 0u || value > maximum) return fallback;
    return (size_t)value;
}

static int equal_ids(const uint32_t *a, size_t an, const uint32_t *b, size_t bn) {
    return an == bn && (!an || memcmp(a, b, an * sizeof *a) == 0);
}

static int append_ids(uint32_t **ids, size_t *count, size_t *capacity,
                      const uint32_t *part, size_t part_count) {
    if (!part_count) return 1;
    if (*count > SIZE_MAX - part_count) return 0;
    size_t needed = *count + part_count;
    if (needed > *capacity) {
        size_t next = *capacity ? *capacity : 16u;
        while (next < needed) {
            if (next > SIZE_MAX / 2u) { next = needed; break; }
            next *= 2u;
        }
        if (next > SIZE_MAX / sizeof **ids) return 0;
        void *grown = realloc(*ids, next * sizeof **ids);
        if (!grown) return 0;
        *ids = (uint32_t *)grown;
        *capacity = next;
    }
    memcpy(*ids + *count, part, part_count * sizeof *part);
    *count = needed;
    return 1;
}

static int verify_online(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t len,
                         const uint32_t *expected, size_t expected_count, uint32_t *rng) {
    mosaic_online_stream *stream = NULL;
    if (mosaic_tokenizer_online_stream_create(tokenizer, 8192u, &stream) != MOSAIC_OK) return 0;
    uint32_t *all = NULL; size_t count = 0, capacity = 0;
    size_t pos = 0;
    while (pos < len) {
        size_t chunk = 1u + (next_u32(rng) % 31u);
        if (chunk > len - pos) chunk = len - pos;
        size_t consumed = 0; uint32_t *part = NULL; size_t part_count = 0;
        mosaic_status st = mosaic_online_stream_push(stream, input + pos, chunk, &consumed, &part, &part_count);
        if (st != MOSAIC_OK || consumed != chunk || !append_ids(&all, &count, &capacity, part, part_count)) {
            mosaic_free(part); free(all); mosaic_online_stream_free(stream); return 0;
        }
        mosaic_free(part); pos += consumed;
    }
    uint32_t *tail = NULL; size_t tail_count = 0;
    if (mosaic_online_stream_finish(stream, &tail, &tail_count) != MOSAIC_OK ||
        !append_ids(&all, &count, &capacity, tail, tail_count) ||
        !equal_ids(all, count, expected, expected_count)) {
        mosaic_free(tail); free(all); mosaic_online_stream_free(stream); return 0;
    }
    mosaic_free(tail); free(all); mosaic_online_stream_free(stream); return 1;
}

static int verify_incremental(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t len,
                              uint32_t *rng, uint64_t *replay) {
    size_t start = len ? next_u32(rng) % (len + 1u) : 0u;
    size_t max_delete = len - start;
    if (max_delete > 8u) max_delete = 8u;
    size_t delete_len = max_delete ? next_u32(rng) % (max_delete + 1u) : 0u;
    size_t replacement_len = next_u32(rng) % 9u;
    uint8_t replacement[8];
    for (size_t i = 0; i < replacement_len; ++i) replacement[i] = (uint8_t)next_u32(rng);
    if (len - delete_len > SIZE_MAX - replacement_len) return 0;
    size_t edited_len = len - delete_len + replacement_len;
    uint8_t *edited = edited_len ? (uint8_t *)malloc(edited_len) : NULL;
    if (edited_len && !edited) return 0;
    if (start) memcpy(edited, input, start);
    if (replacement_len) memcpy(edited + start, replacement, replacement_len);
    if (len > start + delete_len)
        memcpy(edited + start + replacement_len, input + start + delete_len, len - start - delete_len);

    mosaic_incremental_document *document = NULL;
    if (mosaic_tokenizer_incremental_document_create(tokenizer, input, len, &document) != MOSAIC_OK) { free(edited); return 0; }
    if (mosaic_incremental_document_apply_edit(document, start, delete_len, replacement, replacement_len) != MOSAIC_OK) {
        mosaic_incremental_document_free(document); free(edited); return 0;
    }
    uint32_t *incremental = NULL, *fresh = NULL; size_t incremental_count = 0, fresh_count = 0;
    uint8_t *copy = NULL; size_t copy_len = 0;
    int ok = mosaic_incremental_document_encode(document, &incremental, &incremental_count) == MOSAIC_OK &&
             mosaic_tokenizer_encode(tokenizer, edited, edited_len, &fresh, &fresh_count) == MOSAIC_OK &&
             equal_ids(incremental, incremental_count, fresh, fresh_count) &&
             mosaic_incremental_document_copy_bytes(document, &copy, &copy_len) == MOSAIC_OK &&
             copy_len == edited_len && (!edited_len || memcmp(copy, edited, edited_len) == 0);
    if (ok) {
        *replay = mix64(*replay, incremental_count);
        *replay = mix64(*replay, mosaic_incremental_document_last_reprocessed_bytes(document));
        *replay = mix64(*replay, mosaic_incremental_document_last_reused_prefix_bytes(document));
    }
    mosaic_free(copy); mosaic_free(incremental); mosaic_free(fresh);
    mosaic_incremental_document_free(document); free(edited); return ok;
}

static int verify_cold_document(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t len,
                                uint64_t *replay) {
    mosaic_token_document *doc = NULL, *restored = NULL;
    uint8_t *record = NULL, *record2 = NULL, *copy = NULL;
    size_t record_len = 0, record2_len = 0, copy_len = 0;
    int ok = mosaic_tokenizer_token_document_create(tokenizer, input, len,
                MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES, &doc) == MOSAIC_OK &&
             mosaic_token_document_serialize(doc, &record, &record_len) == MOSAIC_OK &&
             mosaic_token_document_deserialize(record, record_len, &restored) == MOSAIC_OK &&
             mosaic_token_document_serialize(restored, &record2, &record2_len) == MOSAIC_OK &&
             record_len == record2_len && (!record_len || memcmp(record, record2, record_len) == 0) &&
             mosaic_token_document_copy_source(restored, &copy, &copy_len) == MOSAIC_OK &&
             copy_len == len && (!len || memcmp(copy, input, len) == 0);
    if (ok) { *replay = mix64(*replay, record_len); *replay = mix64(*replay, copy_len); }
    mosaic_free(copy); mosaic_free(record2); mosaic_free(record);
    mosaic_token_document_free(restored); mosaic_token_document_free(doc); return ok;
}

static int verify_lifecycle(const char *model, const char *unicode) {
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(model, unicode, &tokenizer) != MOSAIC_OK) return 0;
    uint8_t fingerprint[32];
    int ok = mosaic_tokenizer_fingerprint(tokenizer, fingerprint) == MOSAIC_OK;
    mosaic_tokenizer_free(tokenizer);
    return ok;
}

static int run_executor(mosaic_tokenizer *tokenizer, size_t batches, uint32_t *rng, uint64_t *replay) {
    mosaic_executor_config cfg; mosaic_executor_config_default(&cfg);
    cfg.worker_count = 4u; cfg.queue_capacity = 7u; cfg.max_batch_items = 128u; cfg.max_total_input_bytes = 128u * 512u;
    mosaic_executor *executor = NULL;
    if (mosaic_executor_create(&cfg, &executor) != MOSAIC_OK) return 0;
    enum { ITEMS = 128, MAX_LEN = 256 };
    uint8_t storage[ITEMS][MAX_LEN]; mosaic_batch_input inputs[ITEMS];
    for (size_t b = 0; b < batches; ++b) {
        for (size_t i = 0; i < ITEMS; ++i) {
            size_t n = 1u + next_u32(rng) % MAX_LEN;
            for (size_t j = 0; j < n; ++j) storage[i][j] = (uint8_t)next_u32(rng);
            inputs[i] = (mosaic_batch_input){storage[i], n};
        }
        mosaic_batch_result *results = NULL;
        if (mosaic_executor_encode_batch(executor, tokenizer, inputs, ITEMS, &results) != MOSAIC_OK || !results) {
            mosaic_executor_free(executor); return 0;
        }
        for (size_t i = 0; i < ITEMS; ++i) {
            uint8_t *decoded = NULL; size_t decoded_len = 0;
            if (results[i].status != MOSAIC_OK ||
                mosaic_tokenizer_decode(tokenizer, results[i].ids, results[i].count, &decoded, &decoded_len) != MOSAIC_OK ||
                decoded_len != inputs[i].length || memcmp(decoded, inputs[i].data, decoded_len) != 0) {
                mosaic_free(decoded); mosaic_batch_results_free(results, ITEMS); mosaic_executor_free(executor); return 0;
            }
            *replay = mix64(*replay, results[i].count);
            mosaic_free(decoded);
        }
        mosaic_batch_results_free(results, ITEMS);
    }
    mosaic_executor_metrics metrics = {0};
    int ok = mosaic_executor_get_metrics(executor, &metrics) == MOSAIC_OK &&
             metrics.batches == batches && metrics.items == batches * ITEMS && metrics.failed_items == 0u;
    mosaic_executor_free(executor); return ok;
}

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: reliability_smoke MODEL UNICODE\n"); return 2; }
    size_t iterations = env_size("MOSAIC_RELIABILITY_ITERS", 5000u, 2000000u);
    size_t max_len = env_size("MOSAIC_RELIABILITY_MAX_BYTES", 512u, 4096u);
    uint32_t rng = (uint32_t)env_size("MOSAIC_RELIABILITY_SEED", UINT32_C(0x5a17c0de), UINT32_MAX);
    uint8_t *input = (uint8_t *)malloc(max_len ? max_len : 1u);
    if (!input) return 3;

    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) { free(input); return 4; }
    mosaic_runtime_limits limits; mosaic_runtime_limits_default(&limits);
    limits.max_input_bytes = max_len + 32u; limits.max_output_tokens = max_len + 32u; limits.max_token_document_bytes = 4u * 1024u * 1024u;
    if (mosaic_tokenizer_set_runtime_limits(tokenizer, &limits) != MOSAIC_OK || mosaic_tokenizer_seal(tokenizer) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer); free(input); return 5;
    }

    uint64_t replay = UINT64_C(0xcbf29ce484222325);
    size_t online_checks = 0, incremental_checks = 0, cold_checks = 0, lifecycle_checks = 0;
    for (size_t i = 0; i < iterations; ++i) {
        size_t len = next_u32(&rng) % (max_len + 1u);
        for (size_t j = 0; j < len; ++j) input[j] = (uint8_t)next_u32(&rng);
        uint32_t *ids = NULL; size_t count = 0; uint8_t *decoded = NULL; size_t decoded_len = 0;
        if (mosaic_tokenizer_encode(tokenizer, input, len, &ids, &count) != MOSAIC_OK ||
            mosaic_tokenizer_decode(tokenizer, ids, count, &decoded, &decoded_len) != MOSAIC_OK ||
            decoded_len != len || (len && memcmp(decoded, input, len) != 0)) {
            mosaic_free(decoded); mosaic_free(ids); mosaic_tokenizer_free(tokenizer); free(input); return 10;
        }
        replay = mix64(replay, len); replay = mix64(replay, count);
        for (size_t k = 0; k < count; ++k) replay = mix64(replay, ids[k]);
        if ((i % 23u) == 0u) {
            if (!verify_online(tokenizer, input, len, ids, count, &rng)) return 11;
            ++online_checks;
        }
        if ((i % 71u) == 0u) {
            if (!verify_incremental(tokenizer, input, len, &rng, &replay)) return 12;
            ++incremental_checks;
        }
        if ((i % 131u) == 0u) {
            if (!verify_cold_document(tokenizer, input, len, &replay)) return 13;
            ++cold_checks;
        }
        if ((i % 503u) == 0u) {
            if (!verify_lifecycle(argv[1], argv[2])) return 14;
            ++lifecycle_checks;
        }
        mosaic_free(decoded); mosaic_free(ids);
    }

    size_t batches = iterations / 500u;
    if (batches < 4u) batches = 4u;
    if (batches > 200u) batches = 200u;
    if (!run_executor(tokenizer, batches, &rng, &replay)) return 15;
    mosaic_runtime_metrics metrics = {0};
    if (mosaic_tokenizer_get_metrics(tokenizer, &metrics) != MOSAIC_OK || metrics.failures != 0u) return 16;
    printf("OK reliability iterations=%zu online=%zu incremental=%zu cold=%zu lifecycle=%zu batches=%zu encode_calls=%" PRIu64 " replay=%016" PRIx64 "\n",
           iterations, online_checks, incremental_checks, cold_checks, lifecycle_checks, batches,
           metrics.encode_calls, replay);
    mosaic_tokenizer_free(tokenizer); free(input); return 0;
}
