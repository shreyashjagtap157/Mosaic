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
static int reject_language(const char *root, mosaic_tokenizer *tok, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-language/%s",root,name) < 0) return 0;
    return mosaic_tokenizer_add_language_file(tok,path)==MOSAIC_ERROR_INVALID_PACK;
}
static int reject_security(const char *root, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-security17/%s",root,name) < 0) return 0;
    mosaic_security *security=NULL; mosaic_status s=mosaic_security_load_file(path,&security); if(security)mosaic_security_free(security); return s==MOSAIC_ERROR_INVALID_PACK;
}
static int reject_normalization(const char *root, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-normalization16/%s",root,name) < 0) return 0;
    mosaic_normalization *n=NULL; mosaic_status s=mosaic_normalization_load_file(path,&n); if(n)mosaic_normalization_free(n); return s==MOSAIC_ERROR_INVALID_PACK;
}
static int reject_detector(const char *root, const char *name) {
    char path[1024]; if (snprintf(path,sizeof path,"%s/fixtures/packs/malformed-detector/%s",root,name) < 0) return 0;
    mosaic_detector *d=NULL; mosaic_status s=mosaic_detector_load_file(path,&d); if(d)mosaic_detector_free(d); return s==MOSAIC_ERROR_INVALID_PACK;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    const char *models[]={"vocab-bad-magic.mpack","vocab-version.mpack","vocab-flags.mpack","vocab-entry-flags.mpack","vocab-zero-surface.mpack","vocab-surface-oob.mpack","vocab-id-index-duplicate.mpack","vocab-first-index-bad.mpack","vocab-missing-byte-fallback.mpack","vocab-noncanonical-order.mpack","vocab-duplicate-token-id.mpack","vocab-wrong-bucket.mpack"};
    const char *unicode[]={"unicode-bad-magic.mpack","unicode-version.mpack","unicode-flags.mpack","unicode-data-version.mpack","unicode-layout.mpack","unicode-gcb-zero-value.mpack","unicode-gcb-reserved.mpack","unicode-gcb-overlap.mpack","unicode-ep-empty.mpack"};
    const char *languages[]={"language-bad-magic.mpack","language-version.mpack","language-flags.mpack","language-invalid-tag.mpack","language-entry-flags.mpack","language-entry-reserved.mpack","language-zero-surface.mpack","language-max-surface.mpack","language-min-cost.mpack","language-noncanonical-order.mpack"};
    const char *security[]={"security-bad-magic.mpack","security-version.mpack","security-header-size.mpack","security-unicode-version.mpack","security-reserved.mpack","security-zero-scripts.mpack","security-script-id.mpack","security-script-name.mpack","security-script-overlap.mpack","security-ignorable-overlap.mpack","security-layout.mpack"};
    const char *normalizations[]={"normalization-bad-magic.mpack","normalization-version.mpack","normalization-header-size.mpack","normalization-unicode-version.mpack","normalization-reserved.mpack","normalization-count-limit.mpack","normalization-ccc-overlap.mpack","normalization-map-order.mpack","normalization-sequence-oob.mpack","normalization-composition-order.mpack","normalization-layout.mpack"};
    const char *detectors[]={"detector-bad-magic.mpack","detector-version.mpack","detector-flags.mpack","detector-zero-profiles.mpack","detector-profile-width.mpack","detector-negative-margin.mpack","detector-invalid-tag.mpack","detector-profile-reserved.mpack","detector-zero-weight.mpack","detector-profile-oob.mpack","detector-feature-reserved.mpack","detector-max-feature.mpack","detector-first-endpoint.mpack","detector-first-range.mpack"};
    for(size_t i=0;i<sizeof models/sizeof models[0];++i)if(!reject_model(argv[1],models[i]))return 3;
    for(size_t i=0;i<sizeof unicode/sizeof unicode[0];++i)if(!reject_unicode(argv[1],unicode[i]))return 4;
    for(size_t i=0;i<sizeof detectors/sizeof detectors[0];++i)if(!reject_detector(argv[1],detectors[i]))return 7;
    for(size_t i=0;i<sizeof security/sizeof security[0];++i)if(!reject_security(argv[1],security[i]))return 8;
    for(size_t i=0;i<sizeof normalizations/sizeof normalizations[0];++i)if(!reject_normalization(argv[1],normalizations[i]))return 10;
    char model[1024],uni[1024];
    if(snprintf(model,sizeof model,"%s/fixtures/packs/model-v2.mpack",argv[1])<0 || snprintf(uni,sizeof uni,"%s/fixtures/packs/unicode17-v1.mpack",argv[1])<0)return 5;
    mosaic_tokenizer *tok=NULL;if(mosaic_tokenizer_load_files(model,uni,&tok)!=MOSAIC_OK)return 6;
    for(size_t i=0;i<sizeof languages/sizeof languages[0];++i)if(!reject_language(argv[1],tok,languages[i])){mosaic_tokenizer_free(tok);return 9;}
    if(mosaic_tokenizer_language_count(tok)!=0){mosaic_tokenizer_free(tok);return 9;}
    mosaic_tokenizer_free(tok);
    puts("OK malformed model=12 unicode=9 language=10 detector=14 security=11 normalization=11");return 0;
}
