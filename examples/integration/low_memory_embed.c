#include "mosaic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_ids(uint32_t *ids) {
    mosaic_free(ids);
}

int main(void) {
    const char *model = "fixtures/packs/model-v2.mpack";
    const char *unicode = "fixtures/packs/unicode17-v1.mpack";
    const char *language_en = "fixtures/packs/language/en-v1.mpack";
    const char *language_hi = "fixtures/packs/language/hi-v1.mpack";
    const char *language_ja = "fixtures/packs/language/ja-v1.mpack";
    const char *detector = "fixtures/packs/detector/reference-v1.mpack";
    const char *input = "tokenizer नमस्ते दुनिया こんにちは世界";
    mosaic_tokenizer *tokenizer = NULL;
    uint32_t *ids = NULL;
    size_t id_count = 0;
    mosaic_detection detection = {0};
    mosaic_status status = mosaic_tokenizer_load_files(model, unicode, &tokenizer);
    if (status != MOSAIC_OK) return 1;
    if ((status = mosaic_tokenizer_add_language_file(tokenizer, language_en)) != MOSAIC_OK) return 1;
    if ((status = mosaic_tokenizer_add_language_file(tokenizer, language_hi)) != MOSAIC_OK) return 1;
    if ((status = mosaic_tokenizer_add_language_file(tokenizer, language_ja)) != MOSAIC_OK) return 1;
    if ((status = mosaic_tokenizer_set_detector_file(tokenizer, detector)) != MOSAIC_OK) return 1;

    mosaic_runtime_limits limits;
    mosaic_runtime_limits_low_memory_default(&limits);
    if ((status = mosaic_tokenizer_set_runtime_limits(tokenizer, &limits)) != MOSAIC_OK) return 1;
    if ((status = mosaic_tokenizer_seal(tokenizer)) != MOSAIC_OK) return 1;

    status = mosaic_tokenizer_encode_auto(tokenizer, (const uint8_t *)input, strlen(input), &ids, &id_count, &detection);
    if (status != MOSAIC_OK) return 1;

    printf("route=%s tokens=%zu\n", detection.matched ? detection.language : "none", id_count);
    free_ids(ids);
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
