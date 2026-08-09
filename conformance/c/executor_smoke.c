#include "mosaic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_thread.h"

#define CALLERS 8
#define ITEMS 100
#define MAX_INPUT 48

typedef struct caller_ctx {
    mosaic_executor *executor;
    const mosaic_tokenizer *tokenizer;
    unsigned seed;
    int failed;
} caller_ctx;

static int fail(const char *message) { fprintf(stderr, "%s\n", message); return 1; }

static unsigned next_u32(unsigned *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int caller_main(void *arg) {
    caller_ctx *ctx = (caller_ctx *)arg;
    uint8_t storage[ITEMS][MAX_INPUT];
    mosaic_batch_input inputs[ITEMS];
    unsigned state = ctx->seed;
    for (size_t i = 0; i < ITEMS; ++i) {
        size_t n = 1u + (next_u32(&state) % (MAX_INPUT - 1u));
        for (size_t j = 0; j < n; ++j) storage[i][j] = (uint8_t)next_u32(&state);
        inputs[i] = (mosaic_batch_input){storage[i], n};
    }
    mosaic_batch_result *results = NULL;
    if (mosaic_executor_encode_batch(ctx->executor, ctx->tokenizer, inputs, ITEMS, &results) != MOSAIC_OK || !results) {
        ctx->failed = 1; return 0;
    }
    for (size_t i = 0; i < ITEMS; ++i) {
        if (results[i].status != MOSAIC_OK || !results[i].count) { ctx->failed = 1; break; }
        uint8_t *decoded = NULL; size_t decoded_len = 0;
        if (mosaic_tokenizer_decode(ctx->tokenizer, results[i].ids, results[i].count, &decoded, &decoded_len) != MOSAIC_OK ||
            decoded_len != inputs[i].length || memcmp(decoded, inputs[i].data, decoded_len) != 0) ctx->failed = 1;
        mosaic_free(decoded);
        if (ctx->failed) break;
    }
    mosaic_batch_results_free(results, ITEMS);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) return fail("usage: executor_smoke MODEL UNICODE");
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) return fail("load tokenizer");

    mosaic_executor_config cfg; mosaic_executor_config_default(&cfg);
    cfg.worker_count = 4u; cfg.queue_capacity = 3u; cfg.max_batch_items = 1024u; cfg.max_total_input_bytes = 1024u * 1024u;
    mosaic_executor *executor = NULL;
    if (mosaic_executor_create(&cfg, &executor) != MOSAIC_OK) return fail("create executor");

    static const uint8_t hello[] = "hello";
    mosaic_batch_input one = {hello, sizeof hello - 1u};
    mosaic_batch_result *results = NULL;
    if (mosaic_executor_encode_batch(executor, tokenizer, &one, 1u, &results) != MOSAIC_ERROR_STATE || results)
        return fail("unsealed tokenizer accepted");

    mosaic_runtime_limits limits; mosaic_runtime_limits_default(&limits); limits.max_input_bytes = 128u;
    if (mosaic_tokenizer_set_runtime_limits(tokenizer, &limits) != MOSAIC_OK || mosaic_tokenizer_seal(tokenizer) != MOSAIC_OK)
        return fail("seal tokenizer");

    uint8_t too_large[129]; memset(too_large, 'x', sizeof too_large);
    mosaic_batch_input partial[3] = {{hello, sizeof hello - 1u}, {too_large, sizeof too_large}, {(const uint8_t *)"world", 5u}};
    if (mosaic_executor_encode_batch(executor, tokenizer, partial, 3u, &results) != MOSAIC_OK || !results)
        return fail("partial batch scheduling");
    if (results[0].status != MOSAIC_OK || results[1].status != MOSAIC_ERROR_RESOURCE_LIMIT || results[1].ids || results[1].count ||
        results[2].status != MOSAIC_OK) return fail("partial batch statuses");
    mosaic_batch_results_free(results, 3u);

    mosaic_batch_input too_many[1025] = {0};
    if (mosaic_executor_encode_batch(executor, tokenizer, too_many, 1025u, &results) != MOSAIC_ERROR_RESOURCE_LIMIT)
        return fail("batch item ceiling");
    cfg.max_total_input_bytes = 4u;
    mosaic_executor *small_executor = NULL;
    if (mosaic_executor_create(&cfg, &small_executor) != MOSAIC_OK) return fail("create small executor");
    if (mosaic_executor_encode_batch(small_executor, tokenizer, &one, 1u, &results) != MOSAIC_ERROR_RESOURCE_LIMIT)
        return fail("batch byte ceiling");
    mosaic_executor_free(small_executor);

    if (mosaic_executor_reset_metrics(executor) != MOSAIC_OK) return fail("reset metrics");
    caller_ctx callers[CALLERS]; mosaic_thread_t threads[CALLERS];
    for (size_t i = 0; i < CALLERS; ++i) {
        callers[i] = (caller_ctx){executor, tokenizer, (unsigned)(0x1234u + i * 31u), 0};
        if (mosaic_thread_create(&threads[i], caller_main, &callers[i]) == 0) return fail("create caller");
    }
    for (size_t i = 0; i < CALLERS; ++i) {
        if (!mosaic_thread_join(threads[i])) return fail("join caller");
        if (callers[i].failed) return fail("concurrent batch mismatch");
    }
    mosaic_executor_metrics metrics = {0};
    if (mosaic_executor_get_metrics(executor, &metrics) != MOSAIC_OK || metrics.batches != CALLERS ||
        metrics.items != CALLERS * ITEMS || metrics.succeeded_items != CALLERS * ITEMS || metrics.failed_items != 0u || !metrics.input_bytes)
        return fail("executor metrics");

    printf("OK executor workers=%u queue=%u batches=%llu items=%llu bytes=%llu\n",
           cfg.worker_count, cfg.queue_capacity, (unsigned long long)metrics.batches,
           (unsigned long long)metrics.items, (unsigned long long)metrics.input_bytes);
    mosaic_executor_free(executor);
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
