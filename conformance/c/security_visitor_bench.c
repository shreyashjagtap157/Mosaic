#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { size_t findings; } Stats;
static mosaic_status count_finding(void *ctx, const mosaic_security_finding *finding) {
    (void)finding; Stats *s = (Stats *)ctx; if (s->findings == SIZE_MAX) return MOSAIC_ERROR_OVERFLOW; ++s->findings; return MOSAIC_OK;
}
static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0) { fclose(f); return NULL; }
    long n = ftell(f); if (fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    uint8_t *p = n ? (uint8_t *)malloc((size_t)n) : NULL;
    if (n && (!p || fread(p, 1, (size_t)n, f) != (size_t)n)) { free(p); fclose(f); return NULL; }
    fclose(f); *out_len = (size_t)n; return p;
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    size_t len = 0;
    uint8_t *input = read_file(argv[2], &len);
    if (len && !input) return 3;
    mosaic_security *security = NULL; if (mosaic_security_load_file(argv[1], &security) != MOSAIC_OK) { free(input); return 4; }
    Stats stats = {0};
    size_t count = 0;
    mosaic_status st = mosaic_security_visit(security, input, len, count_finding, &stats, &count);
    if (st != MOSAIC_OK || count != stats.findings) return 5;
    printf("bytes=%zu findings=%zu\n", len, count); mosaic_security_free(security); free(input); return 0;
}
