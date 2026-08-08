#ifndef MOSAIC_INTERNAL_H
#define MOSAIC_INTERNAL_H
#include <stddef.h>
#include <stdint.h>
int mosaic_internal_sha256(const uint8_t *data, size_t len, uint8_t out[32]);
int mosaic_internal_sha256_zero_range(const uint8_t *data, size_t len, size_t zero_offset, size_t zero_length, uint8_t out[32]);
#endif
