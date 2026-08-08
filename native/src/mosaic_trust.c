#include "mosaic_trust.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#define TRUST_MAGIC "MSSIGV01"
#define TRUST_VERSION 1u
#define TRUST_ALGORITHM_ED25519 1u
#define TRUST_HEADER_SIZE 160u
#define TRUST_DEFAULT_MAX_KEYS 4096u

static const uint8_t TRUST_DOMAIN[] = "MOSAIC-PACK-SIGNATURE-v1";

typedef struct {
    uint8_t key_id[32];
    uint8_t public_key[32];
    uint8_t revoked;
} TrustKey;

struct mosaic_trust_store {
    TrustKey *keys;
    size_t count;
    size_t capacity;
    uint32_t max_keys;
};

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int all_zero(const uint8_t *p, size_t n) {
    uint8_t x = 0;
    for (size_t i = 0; i < n; ++i) x |= p[i];
    return x == 0;
}

static int sha256_bytes(const uint8_t *bytes, size_t len, uint8_t out[32]) {
    unsigned int n = 0;
    return EVP_Digest(bytes, len, out, &n, EVP_sha256(), NULL) == 1 && n == 32u;
}

static const TrustKey *find_key(const mosaic_trust_store *store, const uint8_t key_id[32]) {
    if (!store) return NULL;
    for (size_t i = 0; i < store->count; ++i)
        if (CRYPTO_memcmp(store->keys[i].key_id, key_id, 32u) == 0) return &store->keys[i];
    return NULL;
}

static TrustKey *find_key_mut(mosaic_trust_store *store, const uint8_t key_id[32]) {
    return (TrustKey *)find_key(store, key_id);
}

static mosaic_status parse_signature(const uint8_t *record, size_t len, mosaic_pack_signature_info *out) {
    if (!record || len != TRUST_HEADER_SIZE) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (memcmp(record, TRUST_MAGIC, 8u) != 0 || rd32(record + 8) != TRUST_VERSION ||
        rd32(record + 12) != TRUST_HEADER_SIZE || rd32(record + 16) != 0u ||
        rd32(record + 20) != TRUST_ALGORITHM_ED25519 || !all_zero(record + 152, 8u))
        return MOSAIC_ERROR_INTEGRITY;
    if (out) {
        memset(out, 0, sizeof *out);
        out->version = TRUST_VERSION;
        out->algorithm = TRUST_ALGORITHM_ED25519;
        memcpy(out->key_id, record + 24, 32u);
        memcpy(out->pack_sha256, record + 56, 32u);
    }
    return MOSAIC_OK;
}

const char *mosaic_trust_version(void) { return "0.20.0"; }

void mosaic_trust_store_config_default(mosaic_trust_store_config *out_config) {
    if (!out_config) return;
    *out_config = (mosaic_trust_store_config){sizeof *out_config, 0u, TRUST_DEFAULT_MAX_KEYS};
}

mosaic_status mosaic_trust_store_create(const mosaic_trust_store_config *config, mosaic_trust_store **out_store) {
    if (!config || !out_store || config->struct_size < sizeof *config || config->flags || !config->max_keys)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_store = NULL;
    mosaic_trust_store *store = (mosaic_trust_store *)calloc(1, sizeof *store);
    if (!store) return MOSAIC_ERROR_OUT_OF_MEMORY;
    store->max_keys = config->max_keys;
    *out_store = store;
    return MOSAIC_OK;
}

void mosaic_trust_store_free(mosaic_trust_store *store) {
    if (!store) return;
    if (store->keys) OPENSSL_cleanse(store->keys, store->capacity * sizeof *store->keys);
    free(store->keys);
    free(store);
}

mosaic_status mosaic_trust_key_id_ed25519(const uint8_t public_key[32], uint8_t out_key_id[32]) {
    if (!public_key || !out_key_id) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return sha256_bytes(public_key, 32u, out_key_id) ? MOSAIC_OK : MOSAIC_ERROR_INTERNAL;
}

mosaic_status mosaic_trust_store_add_ed25519(mosaic_trust_store *store, const uint8_t public_key[32], uint8_t out_key_id[32]) {
    if (!store || !public_key) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t id[32];
    if (!sha256_bytes(public_key, 32u, id)) return MOSAIC_ERROR_INTERNAL;
    TrustKey *existing = find_key_mut(store, id);
    if (existing) {
        if (CRYPTO_memcmp(existing->public_key, public_key, 32u) != 0) return MOSAIC_ERROR_INTEGRITY;
        if (out_key_id) memcpy(out_key_id, id, 32u);
        return MOSAIC_OK;
    }
    if (store->count >= store->max_keys) return MOSAIC_ERROR_RESOURCE_LIMIT;
    if (store->count == store->capacity) {
        size_t next = store->capacity ? store->capacity * 2u : 16u;
        if (next > store->max_keys) next = store->max_keys;
        if (next <= store->capacity || next > SIZE_MAX / sizeof *store->keys) return MOSAIC_ERROR_OVERFLOW;
        TrustKey *grown = (TrustKey *)realloc(store->keys, next * sizeof *grown);
        if (!grown) return MOSAIC_ERROR_OUT_OF_MEMORY;
        store->keys = grown;
        store->capacity = next;
    }
    TrustKey *key = &store->keys[store->count++];
    memset(key, 0, sizeof *key);
    memcpy(key->key_id, id, 32u);
    memcpy(key->public_key, public_key, 32u);
    if (out_key_id) memcpy(out_key_id, id, 32u);
    return MOSAIC_OK;
}

mosaic_status mosaic_trust_store_revoke(mosaic_trust_store *store, const uint8_t key_id[32]) {
    if (!store || !key_id) return MOSAIC_ERROR_INVALID_ARGUMENT;
    TrustKey *key = find_key_mut(store, key_id);
    if (!key) return MOSAIC_ERROR_NOT_FOUND;
    key->revoked = 1u;
    return MOSAIC_OK;
}

mosaic_status mosaic_trust_signature_inspect(const uint8_t *record, size_t record_len, mosaic_pack_signature_info *out_info) {
    if (!out_info) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return parse_signature(record, record_len, out_info);
}

mosaic_status mosaic_trust_verify_pack(const mosaic_trust_store *store, const uint8_t *pack, size_t pack_len,
                                       const uint8_t *signature_record, size_t signature_record_len,
                                       mosaic_pack_signature_info *out_info) {
    if (!store || (!pack && pack_len) || !signature_record) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_pack_signature_info info;
    mosaic_status st = parse_signature(signature_record, signature_record_len, &info);
    if (st != MOSAIC_OK) return st;
    uint8_t actual_hash[32];
    if (!sha256_bytes(pack, pack_len, actual_hash)) return MOSAIC_ERROR_INTERNAL;
    if (CRYPTO_memcmp(actual_hash, info.pack_sha256, 32u) != 0) return MOSAIC_ERROR_INTEGRITY;
    const TrustKey *key = find_key(store, info.key_id);
    if (!key) return MOSAIC_ERROR_UNTRUSTED;
    info.revoked = key->revoked;
    if (key->revoked) { if (out_info) *out_info = info; return MOSAIC_ERROR_REVOKED; }

    uint8_t message[(sizeof TRUST_DOMAIN - 1u) + 32u + 32u];
    size_t pos = 0;
    memcpy(message + pos, TRUST_DOMAIN, sizeof TRUST_DOMAIN - 1u); pos += sizeof TRUST_DOMAIN - 1u;
    memcpy(message + pos, info.key_id, 32u); pos += 32u;
    memcpy(message + pos, info.pack_sha256, 32u); pos += 32u;

    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, key->public_key, 32u);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!pkey || !ctx) { EVP_MD_CTX_free(ctx); EVP_PKEY_free(pkey); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    int ok = EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) == 1 &&
             EVP_DigestVerify(ctx, signature_record + 88, 64u, message, pos) == 1;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    if (!ok) return MOSAIC_ERROR_UNTRUSTED;
    if (out_info) *out_info = info;
    return MOSAIC_OK;
}
