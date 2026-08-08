#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef struct {
    mosaic_cache *cache;
    unsigned tid;
    int failed;
} Worker;

static void make_key(uint8_t key[32], unsigned tid, unsigned slot) {
    for (unsigned i = 0; i < 32u; ++i) {
        key[i] = (uint8_t)(i * 17u + tid * 31u + slot * 13u);
    }
    key[0] = (uint8_t)tid;
    key[1] = (uint8_t)slot;
}

static int worker(void *arg) {
    Worker *w = (Worker *)arg;
    uint8_t key[32];
    uint8_t value[32];
    for (unsigned i = 0; i < 5000u; ++i) {
        unsigned slot = i & 127u;
        make_key(key, w->tid, slot);
        for (unsigned j = 0; j < sizeof value; ++j) {
            value[j] = (uint8_t)(w->tid ^ slot ^ j);
        }
        if (mosaic_cache_put(w->cache, key, value, sizeof value) != MOSAIC_OK) {
            w->failed = 1;
            return -1;
        }
        uint8_t *out = NULL;
        size_t n = 0;
        if (mosaic_cache_get(w->cache, key, &out, &n) != MOSAIC_OK ||
            n != sizeof value || memcmp(out, value, n) != 0) {
            mosaic_free(out);
            w->failed = 2;
            return -1;
        }
        mosaic_free(out);
    }
    return 0;
}

int main(void) {
    mosaic_cache_config cfg = {sizeof cfg, 0u, 3u, 24u, 24u};
    mosaic_cache *cache = NULL;
    if (mosaic_cache_create(&cfg, &cache) != MOSAIC_OK) return 2;

    uint8_t keys[4][32] = {{0}};
    for (unsigned i = 0; i < 4u; ++i) keys[i][0] = (uint8_t)(i + 1u);
    const uint8_t value[8] = {1,2,3,4,5,6,7,8};
    if (mosaic_cache_put(cache, keys[0], value, sizeof value) != MOSAIC_OK ||
        mosaic_cache_put(cache, keys[1], value, sizeof value) != MOSAIC_OK ||
        mosaic_cache_put(cache, keys[2], value, sizeof value) != MOSAIC_OK) return 3;

    uint8_t *out = NULL;
    size_t n = 0;
    if (mosaic_cache_get(cache, keys[0], &out, &n) != MOSAIC_OK ||
        n != sizeof value || memcmp(out, value, sizeof value) != 0) return 4;
    mosaic_free(out);

    if (mosaic_cache_put(cache, keys[3], value, sizeof value) != MOSAIC_OK) return 5;
    if (mosaic_cache_get(cache, keys[1], &out, &n) != MOSAIC_ERROR_NOT_FOUND) return 6;

    const uint8_t replacement[4] = {9,8,7,6};
    if (mosaic_cache_put(cache, keys[0], replacement, sizeof replacement) != MOSAIC_OK) return 7;
    if (mosaic_cache_get(cache, keys[0], &out, &n) != MOSAIC_OK ||
        n != sizeof replacement || memcmp(out, replacement, sizeof replacement) != 0) return 8;
    mosaic_free(out);

    uint8_t too_large[25] = {0};
    if (mosaic_cache_put(cache, keys[0], too_large, sizeof too_large) != MOSAIC_ERROR_RESOURCE_LIMIT) return 9;

    mosaic_cache_stats stats = {0};
    if (mosaic_cache_get_stats(cache, &stats) != MOSAIC_OK ||
        stats.entries > 3u || stats.bytes > 24u || !stats.hits || !stats.misses ||
        !stats.evictions || !stats.replacements) return 10;
    if (mosaic_cache_remove(cache, keys[2]) != MOSAIC_OK) return 11;
    if (mosaic_cache_remove(cache, keys[2]) != MOSAIC_ERROR_NOT_FOUND) return 12;
    if (mosaic_cache_clear(cache) != MOSAIC_OK) return 13;
    if (mosaic_cache_get_stats(cache, &stats) != MOSAIC_OK || stats.entries != 0u || stats.bytes != 0u || stats.clears != 1u || stats.cleared_entries != 2u) return 14;
    mosaic_cache_free(cache);

    cfg = (mosaic_cache_config){sizeof cfg, 0u, 2048u, 2u * 1024u * 1024u, 4096u};
    if (mosaic_cache_create(&cfg, &cache) != MOSAIC_OK) return 15;
    enum { THREAD_COUNT = 8 };
    thrd_t threads[THREAD_COUNT];
    Worker workers[THREAD_COUNT];
    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        workers[i] = (Worker){cache, i, 0};
        if (thrd_create(&threads[i], worker, &workers[i]) != thrd_success) return 16;
    }
    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        int rc = 0;
        if (thrd_join(threads[i], &rc) != thrd_success || rc != 0 || workers[i].failed != 0) return 17;
    }
    if (mosaic_cache_get_stats(cache, &stats) != MOSAIC_OK ||
        stats.entries > 2048u || stats.bytes > 2u * 1024u * 1024u ||
        stats.hits < THREAD_COUNT * 5000u) return 18;
    printf("OK cache concurrent hits=%llu misses=%llu puts=%llu evictions=%llu entries=%llu bytes=%llu peak=%llu\n",
           (unsigned long long)stats.hits,
           (unsigned long long)stats.misses,
           (unsigned long long)stats.puts,
           (unsigned long long)stats.evictions,
           (unsigned long long)stats.entries,
           (unsigned long long)stats.bytes,
           (unsigned long long)stats.peak_bytes);
    mosaic_cache_free(cache);
    return 0;
}
