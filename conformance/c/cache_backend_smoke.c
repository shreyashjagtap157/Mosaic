#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t key[32];
    uint8_t *record;
    size_t len;
    int present;
    int corrupt_on_read;
} FakeBackend;

static mosaic_status fake_get(void *context, const uint8_t key[32], uint8_t *buffer,
                              size_t capacity, size_t *out_required) {
    FakeBackend *b = (FakeBackend *)context;
    if (!b || !key || !out_required) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!b->present || memcmp(key, b->key, 32u)) return MOSAIC_ERROR_NOT_FOUND;
    *out_required = b->len;
    if (!buffer) return MOSAIC_OK;
    if (capacity < b->len) return MOSAIC_ERROR_RESOURCE_LIMIT;
    memcpy(buffer, b->record, b->len);
    if (b->corrupt_on_read && b->len > 128u) buffer[b->len - 1u] ^= 0x80u;
    return MOSAIC_OK;
}

static mosaic_status fake_put(void *context, const uint8_t key[32], const uint8_t *record,
                              size_t record_len) {
    FakeBackend *b = (FakeBackend *)context;
    if (!b || !key || !record || !record_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (b->present) {
        if (memcmp(key, b->key, 32u)) return MOSAIC_ERROR_RESOURCE_LIMIT;
        if (record_len != b->len || memcmp(record, b->record, record_len)) return MOSAIC_ERROR_INTEGRITY;
        return MOSAIC_OK;
    }
    uint8_t *copy = (uint8_t *)malloc(record_len);
    if (!copy) return MOSAIC_ERROR_OUT_OF_MEMORY;
    memcpy(copy, record, record_len);
    memcpy(b->key, key, 32u);
    b->record = copy;
    b->len = record_len;
    b->present = 1;
    return MOSAIC_OK;
}

static mosaic_status fake_remove(void *context, const uint8_t key[32]) {
    FakeBackend *b = (FakeBackend *)context;
    if (!b || !key) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!b->present || memcmp(key, b->key, 32u)) return MOSAIC_ERROR_NOT_FOUND;
    free(b->record);
    b->record = NULL;
    b->len = 0;
    b->present = 0;
    return MOSAIC_OK;
}

int main(void) {
    uint8_t key[32];
    for (unsigned i = 0; i < 32u; ++i) key[i] = (uint8_t)(i * 7u + 3u);
    const uint8_t value[] = "enterprise-cache-record";
    uint8_t *record = NULL;
    size_t record_len = 0;
    if (mosaic_cache_record_encode(key, value, sizeof value - 1u, &record, &record_len) != MOSAIC_OK) return 2;
    mosaic_cache_record_info info = {0};
    if (mosaic_cache_record_inspect(record, record_len, &info) != MOSAIC_OK ||
        info.format_version != 1u || info.value_length != sizeof value - 1u || memcmp(info.key, key, 32u)) return 3;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    if (mosaic_cache_record_decode(key, record, record_len, &decoded, &decoded_len) != MOSAIC_OK ||
        decoded_len != sizeof value - 1u || memcmp(decoded, value, decoded_len)) return 4;
    mosaic_free(decoded);

    uint8_t wrong[32] = {0};
    if (mosaic_cache_record_decode(wrong, record, record_len, &decoded, &decoded_len) != MOSAIC_ERROR_INTEGRITY) return 5;
    uint8_t saved = record[record_len - 1u];
    record[record_len - 1u] ^= 1u;
    if (mosaic_cache_record_inspect(record, record_len, &info) != MOSAIC_ERROR_INTEGRITY) return 6;
    record[record_len - 1u] = saved;

    FakeBackend fake = {0};
    mosaic_cache_backend backend = {sizeof backend, 0u, &fake, fake_get, fake_put, fake_remove};
    if (mosaic_cache_backend_put_value(&backend, key, value, sizeof value - 1u, 4096u) != MOSAIC_OK) return 7;
    if (mosaic_cache_backend_put_value(&backend, key, value, sizeof value - 1u, 4096u) != MOSAIC_OK) return 8;
    const uint8_t other[] = "wrong-value";
    if (mosaic_cache_backend_put_value(&backend, key, other, sizeof other - 1u, 4096u) != MOSAIC_ERROR_INTEGRITY) return 9;
    if (mosaic_cache_backend_get_value(&backend, key, 4096u, &decoded, &decoded_len) != MOSAIC_OK ||
        decoded_len != sizeof value - 1u || memcmp(decoded, value, decoded_len)) return 10;
    mosaic_free(decoded);
    fake.corrupt_on_read = 1;
    if (mosaic_cache_backend_get_value(&backend, key, 4096u, &decoded, &decoded_len) != MOSAIC_ERROR_INTEGRITY) return 11;
    fake.corrupt_on_read = 0;
    if (mosaic_cache_backend_get_value(&backend, key, 64u, &decoded, &decoded_len) != MOSAIC_ERROR_RESOURCE_LIMIT) return 12;
    if (mosaic_cache_backend_remove_value(&backend, key) != MOSAIC_OK) return 13;
    if (mosaic_cache_backend_get_value(&backend, key, 4096u, &decoded, &decoded_len) != MOSAIC_ERROR_NOT_FOUND) return 14;

    free(record);
    printf("OK authenticated cache record bytes=%zu backend corruption rejected\n", record_len);
    return 0;
}
