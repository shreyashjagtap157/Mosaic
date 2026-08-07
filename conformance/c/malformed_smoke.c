#include "mosaic.h"
#include <stdio.h>
#include <string.h>

static int reject_model(const char *root, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-m3/%s",root,name) < 0) return 0;
    mosaic_model *model=NULL; mosaic_status s=mosaic_model_load_file(path,&model); if(model)mosaic_model_free(model); return s==MOSAIC_ERROR_INVALID_PACK;
}
static int reject_unicode(const char *root, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-unicode17/%s",root,name) < 0) return 0;
    mosaic_unicode *u=NULL; mosaic_status s=mosaic_unicode_load_file(path,&u); if(u)mosaic_unicode_free(u); return s==MOSAIC_ERROR_INVALID_PACK;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    const char *models[]={"vocab-bad-magic.mpack","vocab-version.mpack","vocab-flags.mpack","vocab-entry-flags.mpack","vocab-zero-surface.mpack","vocab-surface-oob.mpack","vocab-id-index-duplicate.mpack","vocab-first-index-bad.mpack","vocab-missing-byte-fallback.mpack","vocab-noncanonical-order.mpack","vocab-duplicate-token-id.mpack","vocab-wrong-bucket.mpack"};
    const char *unicode[]={"unicode-bad-magic.mpack","unicode-version.mpack","unicode-flags.mpack","unicode-data-version.mpack","unicode-layout.mpack","unicode-gcb-zero-value.mpack","unicode-gcb-reserved.mpack","unicode-gcb-overlap.mpack","unicode-ep-empty.mpack"};
    for(size_t i=0;i<sizeof models/sizeof models[0];++i)if(!reject_model(argv[1],models[i]))return 3;
    for(size_t i=0;i<sizeof unicode/sizeof unicode[0];++i)if(!reject_unicode(argv[1],unicode[i]))return 4;
    puts("OK malformed model=12 unicode=9");return 0;
}
