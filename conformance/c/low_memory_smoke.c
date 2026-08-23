#include "mosaic.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return fail("usage: low_memory_smoke MODEL UNICODE");
    }

    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) {
        return fail("load tokenizer");
    }

    mosaic_runtime_limits limits;
    mosaic_runtime_limits_low_memory_default(&limits);
    if (limits.max_input_bytes > 64ull * 1024ull * 1024ull ||
        limits.max_output_tokens > 64ull * 1024ull * 1024ull ||
        limits.max_token_document_bytes > 32ull * 1024ull * 1024ull) {
        mosaic_tokenizer_free(tokenizer);
        return fail("low-memory defaults too large");
    }
    if (mosaic_tokenizer_set_runtime_limits(tokenizer, &limits) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("set low-memory limits");
    }
    if (mosaic_tokenizer_seal(tokenizer) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("seal tokenizer");
    }

    const uint8_t input[] = "tokenizer low memory";
    uint32_t *ids = NULL;
    size_t id_count = 0;
    if (mosaic_tokenizer_encode(tokenizer, input, sizeof input - 1u, &ids, &id_count) != MOSAIC_OK || !ids || !id_count) {
        mosaic_tokenizer_free(tokenizer);
        return fail("encode under low-memory profile");
    }

    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    if (mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) != MOSAIC_OK ||
        decoded_len != sizeof input - 1u || memcmp(decoded, input, decoded_len) != 0) {
        mosaic_free(ids);
        mosaic_free(decoded);
        mosaic_tokenizer_free(tokenizer);
        return fail("decode under low-memory profile");
    }
    mosaic_free(ids);
    mosaic_free(decoded);

    mosaic_executor_config cfg;
    mosaic_executor_config_low_memory_default(&cfg);
    if (cfg.worker_count != 1u || cfg.queue_capacity > 64u || cfg.max_batch_items > 4096u ||
        cfg.max_total_input_bytes > 64ull * 1024ull * 1024ull) {
        mosaic_tokenizer_free(tokenizer);
        return fail("low-memory executor defaults unexpected");
    }
    mosaic_executor *executor = NULL;
    if (mosaic_executor_create(&cfg, &executor) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("create low-memory executor");
    }

    mosaic_batch_input batch[2] = {
        {input, sizeof input - 1u},
        {(const uint8_t *)"hello", 5u},
    };
    mosaic_batch_result *results = NULL;
    if (mosaic_executor_encode_batch(executor, tokenizer, batch, 2u, &results) != MOSAIC_OK || !results) {
        mosaic_executor_free(executor);
        mosaic_tokenizer_free(tokenizer);
        return fail("batch under low-memory profile");
    }
    if (results[0].status != MOSAIC_OK || results[1].status != MOSAIC_OK) {
        mosaic_batch_results_free(results, 2u);
        mosaic_executor_free(executor);
        mosaic_tokenizer_free(tokenizer);
        return fail("batch status under low-memory profile");
    }

    mosaic_batch_results_free(results, 2u);
    mosaic_executor_free(executor);
    mosaic_tokenizer_free(tokenizer);
    printf("OK low-memory defaults\n");
    return 0;
}
