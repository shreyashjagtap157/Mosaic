#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int expect_ids(mosaic_tokenizer *tok, const uint8_t *bytes, size_t len, const uint32_t *expected, size_t n) {
    uint32_t *ids = NULL; size_t count = 0;
    if (mosaic_tokenizer_encode(tok, bytes, len, &ids, &count) != MOSAIC_OK) return 0;
    int ok = count == n && (!n || memcmp(ids, expected, n * sizeof *ids) == 0);
    mosaic_free(ids); return ok;
}

int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: %s MODEL UNICODE EN HI JA\n", argv[0]); return 2; }
    mosaic_tokenizer *tok = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tok) != MOSAIC_OK) return 1;
    const uint8_t en[] = "tokenizer"; const uint32_t en_base[] = {263,264}, en_special[] = {271};
    if (!expect_ids(tok,en,sizeof en-1,en_base,2)) return 1;
    if (mosaic_tokenizer_add_language_file(tok,argv[3]) != MOSAIC_OK || !expect_ids(tok,en,sizeof en-1,en_special,1)) return 1;
    if (mosaic_tokenizer_add_language_file(tok,argv[3]) != MOSAIC_ERROR_CONFLICT) return 1;
    if (mosaic_tokenizer_add_language_file(tok,argv[4]) != MOSAIC_OK || mosaic_tokenizer_add_language_file(tok,argv[5]) != MOSAIC_OK) return 1;
    if (mosaic_tokenizer_language_count(tok) != 3) return 1;
    char tag[8]; size_t needed=0;
    if (mosaic_tokenizer_language_tag(tok,0,tag,sizeof tag,&needed) != MOSAIC_OK || strcmp(tag,"en") || needed != 3) return 1;
    mosaic_stream *stream=NULL;
    if (mosaic_tokenizer_stream_create(tok,&stream) != MOSAIC_OK || mosaic_stream_push(stream,en,sizeof en-1) != MOSAIC_OK) return 1;
    uint32_t *ids=NULL; size_t count=0;
    if (mosaic_stream_finish(stream,&ids,&count) != MOSAIC_OK || count != 1 || ids[0] != 271) return 1;
    mosaic_free(ids); mosaic_stream_free(stream);
    mosaic_tokenizer_free(tok);
    puts("OK language packs"); return 0;
}
