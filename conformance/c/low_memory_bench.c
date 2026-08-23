#include "mosaic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

static int fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static double now_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&counter) || !freq.QuadPart) {
        return 0.0;
    }
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

static size_t peak_rss_bytes(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc)) {
        return 0;
    }
    return (size_t)pmc.PeakWorkingSetSize;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }
# if defined(__APPLE__)
    return (size_t)usage.ru_maxrss;
# else
    return (size_t)usage.ru_maxrss * 1024u;
# endif
#endif
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return fail("usage: low_memory_bench MODEL UNICODE");
    }

    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) {
        return fail("load tokenizer");
    }

    mosaic_runtime_limits limits;
    mosaic_runtime_limits_low_memory_default(&limits);
    if (mosaic_tokenizer_set_runtime_limits(tokenizer, &limits) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("set low-memory limits");
    }
    if (mosaic_tokenizer_seal(tokenizer) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("seal tokenizer");
    }

    const uint8_t input[] = "tokenizer low memory benchmark ";
    uint8_t buffer[sizeof input - 1u];
    size_t offset = 0;
    while (offset + sizeof input - 1u <= sizeof buffer) {
        memcpy(buffer + offset, input, sizeof input - 1u);
        offset += sizeof input - 1u;
    }
    const uint8_t *payload = buffer;
    size_t payload_len = offset;

    double start = now_seconds();
    size_t total_ids = 0;
    for (size_t i = 0; i < 250u; ++i) {
        uint32_t *ids = NULL;
        size_t id_count = 0;
        uint8_t *decoded = NULL;
        size_t decoded_len = 0;
        if (mosaic_tokenizer_encode(tokenizer, payload, payload_len, &ids, &id_count) != MOSAIC_OK || !ids || !id_count) {
            mosaic_tokenizer_free(tokenizer);
            return fail("encode loop");
        }
        if (mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) != MOSAIC_OK ||
            decoded_len != payload_len || memcmp(decoded, payload, payload_len) != 0) {
            mosaic_free(ids);
            mosaic_free(decoded);
            mosaic_tokenizer_free(tokenizer);
            return fail("decode loop");
        }
        total_ids += id_count;
        mosaic_free(ids);
        mosaic_free(decoded);
    }

    mosaic_executor_config cfg;
    mosaic_executor_config_low_memory_default(&cfg);
    mosaic_executor *executor = NULL;
    if (mosaic_executor_create(&cfg, &executor) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer);
        return fail("create executor");
    }

    mosaic_batch_input batch[2] = {
        {payload, payload_len},
        {(const uint8_t *)"hello", 5u},
    };
    for (size_t i = 0; i < 100u; ++i) {
        mosaic_batch_result *results = NULL;
        if (mosaic_executor_encode_batch(executor, tokenizer, batch, 2u, &results) != MOSAIC_OK || !results) {
            mosaic_executor_free(executor);
            mosaic_tokenizer_free(tokenizer);
            return fail("batch loop");
        }
        if (results[0].status != MOSAIC_OK || results[1].status != MOSAIC_OK) {
            mosaic_batch_results_free(results, 2u);
            mosaic_executor_free(executor);
            mosaic_tokenizer_free(tokenizer);
            return fail("batch status");
        }
        mosaic_batch_results_free(results, 2u);
    }

    double elapsed = now_seconds() - start;
    size_t rss = peak_rss_bytes();
    printf("bytes=%zu ids=%zu elapsed=%.6f peak_rss=%zu worker_count=%u queue_capacity=%llu max_batch_items=%llu\n",
        payload_len, total_ids, elapsed, rss, cfg.worker_count,
        (unsigned long long)cfg.queue_capacity, (unsigned long long)cfg.max_batch_items);

    mosaic_executor_free(executor);
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
