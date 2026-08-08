#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    uint8_t *p = n ? (uint8_t *)malloc((size_t)n) : NULL;
    if (n && (!p || fread(p, 1, (size_t)n, f) != (size_t)n)) { free(p); fclose(f); return NULL; }
    fclose(f); *out_len = (size_t)n; return p;
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    size_t input_len = 0; uint8_t *input = read_file(argv[2], &input_len);
    if (input_len && !input) return 3;
    char *end = NULL; unsigned long long cap_ull = strtoull(argv[3], &end, 10);
    if (!end || *end || !cap_ull || cap_ull > SIZE_MAX) { free(input); return 4; }
    mosaic_model *model = NULL; if (mosaic_model_load_file(argv[1], &model) != MOSAIC_OK) { free(input); return 5; }
    mosaic_online_stream *stream = NULL;
    mosaic_status st = mosaic_online_stream_create(model, (size_t)cap_ull, &stream);
    if (st != MOSAIC_OK) { mosaic_model_free(model); free(input); return 6; }
    size_t pos = 0, total_ids = 0, max_pending = 0;
    while (pos < input_len) {
        size_t want = input_len - pos; if (want > 65536u) want = 65536u;
        size_t consumed = 0; uint32_t *ids = NULL; size_t count = 0;
        st = mosaic_online_stream_push(stream, input + pos, want, &consumed, &ids, &count);
        if (count > SIZE_MAX - total_ids) { mosaic_free(ids); return 7; }
        total_ids += count; mosaic_free(ids); pos += consumed;
        size_t pending = mosaic_online_stream_pending_bytes(stream); if (pending > max_pending) max_pending = pending;
        if (st == MOSAIC_ERROR_RESOURCE_LIMIT) { fprintf(stderr, "resource-limit at %zu pending=%zu\n", pos, pending); return 8; }
        if (st != MOSAIC_OK || !consumed) return 9;
    }
    uint32_t *tail = NULL; size_t tail_count = 0;
    if (mosaic_online_stream_finish(stream, &tail, &tail_count) != MOSAIC_OK) return 10;
    if (tail_count > SIZE_MAX - total_ids) { mosaic_free(tail); return 11; }
    total_ids += tail_count; mosaic_free(tail);
    printf("bytes=%zu tokens=%zu max_pending=%zu cap=%llu\n", input_len, total_ids, max_pending, cap_ull);
    mosaic_online_stream_free(stream); mosaic_model_free(model); free(input); return 0;
}
