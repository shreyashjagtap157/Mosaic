#include <mosaic.h>
#include "mosaic_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mosaic_thread.h"

#define MOSAIC_CACHE_MAX_ENTRIES 16777216ull

typedef struct CacheEntry CacheEntry;
struct CacheEntry {
    uint8_t key[32];
    uint8_t *value;
    size_t value_len;
    CacheEntry *bucket_next;
    CacheEntry *lru_prev;
    CacheEntry *lru_next;
};

struct mosaic_cache {
    mosaic_mutex_t mutex;
    CacheEntry **buckets;
    size_t bucket_count;
    size_t max_entries;
    size_t max_bytes;
    size_t max_value_bytes;
    size_t entries;
    size_t bytes;
    CacheEntry *lru_head;
    CacheEntry *lru_tail;
    mosaic_cache_stats stats;
};

static uint64_t key_hash(const uint8_t key[32]) {
    uint64_t x = 0;
    for (unsigned i = 0; i < 8u; ++i) x |= (uint64_t)key[i] << (i * 8u);
    x ^= x >> 33;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33;
    x *= UINT64_C(0xc4ceb9fe1a85ec53);
    x ^= x >> 33;
    return x;
}

static size_t pow2_ceil(size_t v) {
    size_t p = 1;
    while (p < v) {
        if (p > SIZE_MAX / 2u) return 0;
        p <<= 1u;
    }
    return p;
}

static CacheEntry *find_entry(mosaic_cache *cache, const uint8_t key[32], CacheEntry ***out_link) {
    size_t bucket = (size_t)key_hash(key) & (cache->bucket_count - 1u);
    CacheEntry **link = &cache->buckets[bucket];
    while (*link) {
        if (!memcmp((*link)->key, key, 32)) {
            if (out_link) *out_link = link;
            return *link;
        }
        link = &(*link)->bucket_next;
    }
    if (out_link) *out_link = link;
    return NULL;
}

static void lru_unlink(mosaic_cache *cache, CacheEntry *entry) {
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    else cache->lru_head = entry->lru_next;
    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    else cache->lru_tail = entry->lru_prev;
    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

static void lru_front(mosaic_cache *cache, CacheEntry *entry) {
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->lru_prev = entry;
    else cache->lru_tail = entry;
    cache->lru_head = entry;
}

static void erase_entry(mosaic_cache *cache, CacheEntry *entry, int eviction) {
    CacheEntry **link = NULL;
    (void)find_entry(cache, entry->key, &link);
    if (link && *link == entry) *link = entry->bucket_next;
    lru_unlink(cache, entry);
    cache->entries--;
    cache->bytes -= entry->value_len;
    if (eviction) cache->stats.evictions++;
    else cache->stats.removes++;
    free(entry->value);
    free(entry);
}

void mosaic_cache_config_default(mosaic_cache_config *out_config) {
    if (!out_config) return;
    *out_config = (mosaic_cache_config){sizeof *out_config, 0u, 4096u, 256ull * 1024ull * 1024ull, 16ull * 1024ull * 1024ull};
}

mosaic_status mosaic_cache_create(const mosaic_cache_config *config, mosaic_cache **out_cache) {
    if (!out_cache) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_cache = NULL;
    mosaic_cache_config cfg;
    if (config) cfg = *config;
    else mosaic_cache_config_default(&cfg);
    if (cfg.struct_size < sizeof cfg || cfg.flags || !cfg.max_entries ||
        cfg.max_entries > MOSAIC_CACHE_MAX_ENTRIES || !cfg.max_bytes ||
        cfg.max_entries > SIZE_MAX || cfg.max_bytes > SIZE_MAX ||
        cfg.max_value_bytes > SIZE_MAX)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!cfg.max_value_bytes) cfg.max_value_bytes = cfg.max_bytes;
    if (cfg.max_value_bytes > cfg.max_bytes) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if ((size_t)cfg.max_entries > SIZE_MAX / 2u) return MOSAIC_ERROR_OVERFLOW;
    size_t buckets = pow2_ceil((size_t)cfg.max_entries * 2u);
    if (!buckets || buckets > SIZE_MAX / sizeof(CacheEntry *)) return MOSAIC_ERROR_OVERFLOW;
    mosaic_cache *cache = (mosaic_cache *)calloc(1, sizeof *cache);
    if (!cache) return MOSAIC_ERROR_OUT_OF_MEMORY;
    cache->buckets = (CacheEntry **)calloc(buckets, sizeof *cache->buckets);
    if (!cache->buckets) {
        free(cache);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    if (!mosaic_mutex_init(&cache->mutex)) {
        free(cache->buckets);
        free(cache);
        return MOSAIC_ERROR_INTERNAL;
    }
    cache->bucket_count = buckets;
    cache->max_entries = (size_t)cfg.max_entries;
    cache->max_bytes = (size_t)cfg.max_bytes;
    cache->max_value_bytes = (size_t)cfg.max_value_bytes;
    *out_cache = cache;
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_put(mosaic_cache *cache, const uint8_t key[32], const uint8_t *value, size_t value_len) {
    if (!cache || !key || (value_len && !value)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (value_len > cache->max_value_bytes || value_len > cache->max_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    uint8_t *copy = value_len ? (uint8_t *)malloc(value_len) : NULL;
    if (value_len && !copy) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (value_len) memcpy(copy, value, value_len);
    if (!mosaic_mutex_lock(&cache->mutex)) {
        free(copy);
        return MOSAIC_ERROR_INTERNAL;
    }
    CacheEntry *entry = find_entry(cache, key, NULL);
    if (entry) {
        size_t old = entry->value_len;
        free(entry->value);
        entry->value = copy;
        entry->value_len = value_len;
        cache->bytes = cache->bytes - old + value_len;
        cache->stats.replacements++;
        lru_unlink(cache, entry);
        lru_front(cache, entry);
    } else {
        entry = (CacheEntry *)calloc(1, sizeof *entry);
        if (!entry) {
            mosaic_mutex_unlock(&cache->mutex);
            free(copy);
            return MOSAIC_ERROR_OUT_OF_MEMORY;
        }
        memcpy(entry->key, key, 32);
        entry->value = copy;
        entry->value_len = value_len;
        size_t bucket = (size_t)key_hash(key) & (cache->bucket_count - 1u);
        entry->bucket_next = cache->buckets[bucket];
        cache->buckets[bucket] = entry;
        lru_front(cache, entry);
        cache->entries++;
        cache->bytes += value_len;
    }
    cache->stats.puts++;
    while (cache->entries > cache->max_entries || cache->bytes > cache->max_bytes) {
        CacheEntry *victim = cache->lru_tail;
        if (!victim) {
            mosaic_mutex_unlock(&cache->mutex);
            return MOSAIC_ERROR_INTERNAL;
        }
        erase_entry(cache, victim, 1);
    }
    if (cache->bytes > cache->stats.peak_bytes) cache->stats.peak_bytes = cache->bytes;
    cache->stats.entries = cache->entries;
    cache->stats.bytes = cache->bytes;
    mosaic_mutex_unlock(&cache->mutex);
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_get(mosaic_cache *cache, const uint8_t key[32], uint8_t **out_value, size_t *out_len) {
    if (!cache || !key || !out_value || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_value = NULL;
    *out_len = 0;
    if (!mosaic_mutex_lock(&cache->mutex)) return MOSAIC_ERROR_INTERNAL;
    CacheEntry *entry = find_entry(cache, key, NULL);
    if (!entry) {
        cache->stats.misses++;
        mosaic_mutex_unlock(&cache->mutex);
        return MOSAIC_ERROR_NOT_FOUND;
    }
    uint8_t *copy = entry->value_len ? (uint8_t *)malloc(entry->value_len) : NULL;
    if (entry->value_len && !copy) {
        mosaic_mutex_unlock(&cache->mutex);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    if (entry->value_len) memcpy(copy, entry->value, entry->value_len);
    lru_unlink(cache, entry);
    lru_front(cache, entry);
    cache->stats.hits++;
    *out_value = copy;
    *out_len = entry->value_len;
    mosaic_mutex_unlock(&cache->mutex);
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_remove(mosaic_cache *cache, const uint8_t key[32]) {
    if (!cache || !key) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!mosaic_mutex_lock(&cache->mutex)) return MOSAIC_ERROR_INTERNAL;
    CacheEntry *entry = find_entry(cache, key, NULL);
    if (!entry) {
        mosaic_mutex_unlock(&cache->mutex);
        return MOSAIC_ERROR_NOT_FOUND;
    }
    erase_entry(cache, entry, 0);
    cache->stats.entries = cache->entries;
    cache->stats.bytes = cache->bytes;
    mosaic_mutex_unlock(&cache->mutex);
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_clear(mosaic_cache *cache) {
    if (!cache) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!mosaic_mutex_lock(&cache->mutex)) return MOSAIC_ERROR_INTERNAL;
    uint64_t cleared = (uint64_t)cache->entries;
    memset(cache->buckets, 0, cache->bucket_count * sizeof *cache->buckets);
    CacheEntry *entry = cache->lru_head;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->entries = 0;
    cache->bytes = 0;
    while (entry) {
        CacheEntry *next = entry->lru_next;
        free(entry->value);
        free(entry);
        entry = next;
    }
    cache->stats.clears++;
    cache->stats.cleared_entries += cleared;
    cache->stats.entries = 0;
    cache->stats.bytes = 0;
    mosaic_mutex_unlock(&cache->mutex);
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_get_stats(mosaic_cache *cache, mosaic_cache_stats *out_stats) {
    if (!cache || !out_stats) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!mosaic_mutex_lock(&cache->mutex)) return MOSAIC_ERROR_INTERNAL;
    cache->stats.entries = cache->entries;
    cache->stats.bytes = cache->bytes;
    *out_stats = cache->stats;
    mosaic_mutex_unlock(&cache->mutex);
    return MOSAIC_OK;
}

void mosaic_cache_free(mosaic_cache *cache) {
    if (!cache) return;
    (void)mosaic_cache_clear(cache);
    mosaic_mutex_destroy(&cache->mutex);
    free(cache->buckets);
    free(cache);
}

#define MOSAIC_CACHE_RECORD_HEADER 128u
static const uint8_t CACHE_RECORD_MAGIC[8] = {'M','S','C','A','C','H','R','1'};

static void cache_wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void cache_wr32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static void cache_wr64(uint8_t *p, uint64_t v) {
    for (unsigned i = 0; i < 8u; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static uint16_t cache_rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t cache_rd32(const uint8_t *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4u; ++i) v |= (uint32_t)p[i] << (8u * i);
    return v;
}
static uint64_t cache_rd64(const uint8_t *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8u; ++i) v |= (uint64_t)p[i] << (8u * i);
    return v;
}
static int cache_record_hash(const uint8_t *record, size_t len, uint8_t out[32]) {
    return mosaic_internal_sha256_zero_range(record, len, 88u, 32u, out);
}

mosaic_status mosaic_cache_record_encode(const uint8_t key[32], const uint8_t *value, size_t value_len,
                                         uint8_t **out_record, size_t *out_record_len) {
    if (!key || (value_len && !value) || !out_record || !out_record_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_record = NULL;
    *out_record_len = 0;
    if (value_len > SIZE_MAX - MOSAIC_CACHE_RECORD_HEADER) return MOSAIC_ERROR_OVERFLOW;
    size_t total = MOSAIC_CACHE_RECORD_HEADER + value_len;
    uint8_t *record = (uint8_t *)calloc(total ? total : 1u, 1u);
    if (!record) return MOSAIC_ERROR_OUT_OF_MEMORY;
    memcpy(record, CACHE_RECORD_MAGIC, 8u);
    cache_wr16(record + 8u, 1u);
    cache_wr16(record + 10u, MOSAIC_CACHE_RECORD_HEADER);
    cache_wr32(record + 12u, 0u);
    cache_wr64(record + 16u, (uint64_t)value_len);
    memcpy(record + 24u, key, 32u);
    if (!mosaic_internal_sha256(value, value_len, record + 56u)) {
        free(record);
        return MOSAIC_ERROR_INTERNAL;
    }
    if (value_len) memcpy(record + MOSAIC_CACHE_RECORD_HEADER, value, value_len);
    if (!cache_record_hash(record, total, record + 88u)) {
        free(record);
        return MOSAIC_ERROR_INTERNAL;
    }
    *out_record = record;
    *out_record_len = total;
    return MOSAIC_OK;
}

static mosaic_status cache_record_validate(const uint8_t *record, size_t record_len,
                                           mosaic_cache_record_info *info) {
    if (!record || record_len < MOSAIC_CACHE_RECORD_HEADER) return MOSAIC_ERROR_INTEGRITY;
    if (memcmp(record, CACHE_RECORD_MAGIC, 8u) || cache_rd16(record + 8u) != 1u ||
        cache_rd16(record + 10u) != MOSAIC_CACHE_RECORD_HEADER || cache_rd32(record + 12u) != 0u)
        return MOSAIC_ERROR_INTEGRITY;
    for (size_t i = 120u; i < 128u; ++i) if (record[i]) return MOSAIC_ERROR_INTEGRITY;
    uint64_t vlen64 = cache_rd64(record + 16u);
    if (vlen64 > SIZE_MAX) return MOSAIC_ERROR_OVERFLOW;
    size_t vlen = (size_t)vlen64;
    if (vlen > SIZE_MAX - MOSAIC_CACHE_RECORD_HEADER || MOSAIC_CACHE_RECORD_HEADER + vlen != record_len)
        return MOSAIC_ERROR_INTEGRITY;
    uint8_t value_hash[32], record_hash[32];
    if (!mosaic_internal_sha256(record + MOSAIC_CACHE_RECORD_HEADER, vlen, value_hash) ||
        memcmp(value_hash, record + 56u, 32u)) return MOSAIC_ERROR_INTEGRITY;
    if (!cache_record_hash(record, record_len, record_hash) || memcmp(record_hash, record + 88u, 32u))
        return MOSAIC_ERROR_INTEGRITY;
    if (info) {
        memset(info, 0, sizeof *info);
        info->format_version = 1u;
        info->value_length = vlen64;
        memcpy(info->key, record + 24u, 32u);
        memcpy(info->value_sha256, record + 56u, 32u);
        memcpy(info->record_sha256, record + 88u, 32u);
    }
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_record_inspect(const uint8_t *record, size_t record_len,
                                          mosaic_cache_record_info *out_info) {
    if (!out_info) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return cache_record_validate(record, record_len, out_info);
}

mosaic_status mosaic_cache_record_decode(const uint8_t expected_key[32], const uint8_t *record,
                                         size_t record_len, uint8_t **out_value, size_t *out_value_len) {
    if (!expected_key || !out_value || !out_value_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_value = NULL;
    *out_value_len = 0;
    mosaic_cache_record_info info;
    mosaic_status st = cache_record_validate(record, record_len, &info);
    if (st != MOSAIC_OK) return st;
    if (memcmp(expected_key, info.key, 32u)) return MOSAIC_ERROR_INTEGRITY;
    size_t n = (size_t)info.value_length;
    uint8_t *copy = n ? (uint8_t *)malloc(n) : NULL;
    if (n && !copy) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (n) memcpy(copy, record + MOSAIC_CACHE_RECORD_HEADER, n);
    *out_value = copy;
    *out_value_len = n;
    return MOSAIC_OK;
}

static mosaic_status validate_backend(const mosaic_cache_backend *backend) {
    if (!backend || backend->struct_size < sizeof *backend || backend->flags ||
        !backend->get || !backend->put) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return MOSAIC_OK;
}

mosaic_status mosaic_cache_backend_get_value(const mosaic_cache_backend *backend, const uint8_t key[32],
                                              size_t max_record_bytes, uint8_t **out_value,
                                              size_t *out_value_len) {
    if (!key || !out_value || !out_value_len || !max_record_bytes) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_value = NULL;
    *out_value_len = 0;
    mosaic_status st = validate_backend(backend);
    if (st != MOSAIC_OK) return st;
    size_t required = 0;
    st = backend->get(backend->context, key, NULL, 0, &required);
    if (st != MOSAIC_OK) return st;
    if (required < MOSAIC_CACHE_RECORD_HEADER || required > max_record_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    uint8_t *record = (uint8_t *)malloc(required);
    if (!record) return MOSAIC_ERROR_OUT_OF_MEMORY;
    size_t actual = required;
    st = backend->get(backend->context, key, record, required, &actual);
    if (st != MOSAIC_OK) {
        free(record);
        return st;
    }
    if (actual != required) {
        free(record);
        return MOSAIC_ERROR_INTEGRITY;
    }
    st = mosaic_cache_record_decode(key, record, actual, out_value, out_value_len);
    free(record);
    return st;
}

mosaic_status mosaic_cache_backend_put_value(const mosaic_cache_backend *backend, const uint8_t key[32],
                                              const uint8_t *value, size_t value_len,
                                              size_t max_record_bytes) {
    if (!key || (value_len && !value) || !max_record_bytes) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status st = validate_backend(backend);
    if (st != MOSAIC_OK) return st;
    uint8_t *record = NULL;
    size_t n = 0;
    st = mosaic_cache_record_encode(key, value, value_len, &record, &n);
    if (st != MOSAIC_OK) return st;
    if (n > max_record_bytes) {
        free(record);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    st = backend->put(backend->context, key, record, n);
    free(record);
    return st;
}

mosaic_status mosaic_cache_backend_remove_value(const mosaic_cache_backend *backend, const uint8_t key[32]) {
    if (!key) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status st = validate_backend(backend);
    if (st != MOSAIC_OK) return st;
    if (!backend->remove) return MOSAIC_ERROR_UNSUPPORTED;
    return backend->remove(backend->context, key);
}
