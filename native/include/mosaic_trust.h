#ifndef MOSAIC_TRUST_H
#define MOSAIC_TRUST_H

#include "mosaic.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAIC_TRUST_API_VERSION_MAJOR 0
#define MOSAIC_TRUST_API_VERSION_MINOR 1
#define MOSAIC_TRUST_API_VERSION_PATCH 0
#define MOSAIC_TRUST_ED25519_PUBLIC_KEY_BYTES 32u
#define MOSAIC_TRUST_ED25519_SIGNATURE_BYTES 64u
#define MOSAIC_TRUST_KEY_ID_BYTES 32u
#define MOSAIC_TRUST_SIGNATURE_RECORD_BYTES 160u

typedef struct mosaic_trust_store mosaic_trust_store;

typedef struct mosaic_trust_store_config {
    size_t struct_size;
    uint32_t flags;
    uint32_t max_keys;
} mosaic_trust_store_config;

typedef struct mosaic_pack_signature_info {
    uint32_t version;
    uint32_t algorithm;
    uint8_t key_id[MOSAIC_TRUST_KEY_ID_BYTES];
    uint8_t pack_sha256[32];
    uint8_t revoked;
    uint8_t reserved[7];
} mosaic_pack_signature_info;

const char *mosaic_trust_version(void);
void mosaic_trust_store_config_default(mosaic_trust_store_config *out_config);
mosaic_status mosaic_trust_store_create(const mosaic_trust_store_config *config, mosaic_trust_store **out_store);
void mosaic_trust_store_free(mosaic_trust_store *store);
mosaic_status mosaic_trust_key_id_ed25519(const uint8_t public_key[MOSAIC_TRUST_ED25519_PUBLIC_KEY_BYTES],
                                           uint8_t out_key_id[MOSAIC_TRUST_KEY_ID_BYTES]);
mosaic_status mosaic_trust_store_add_ed25519(mosaic_trust_store *store,
                                              const uint8_t public_key[MOSAIC_TRUST_ED25519_PUBLIC_KEY_BYTES],
                                              uint8_t out_key_id[MOSAIC_TRUST_KEY_ID_BYTES]);
mosaic_status mosaic_trust_store_revoke(mosaic_trust_store *store,
                                         const uint8_t key_id[MOSAIC_TRUST_KEY_ID_BYTES]);
mosaic_status mosaic_trust_signature_inspect(const uint8_t *record, size_t record_len,
                                             mosaic_pack_signature_info *out_info);
mosaic_status mosaic_trust_verify_pack(const mosaic_trust_store *store,
                                       const uint8_t *pack, size_t pack_len,
                                       const uint8_t *signature_record, size_t signature_record_len,
                                       mosaic_pack_signature_info *out_info);

#ifdef __cplusplus
}
#endif
#endif
