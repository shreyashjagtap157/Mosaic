#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int expect_auto(mosaic_tokenizer *tok, const uint8_t *bytes, size_t len,
                       const char *tag, uint32_t available, const uint32_t *expected, size_t n) {
    uint32_t *ids=NULL; size_t count=0; mosaic_detection d;
    if(mosaic_tokenizer_encode_auto(tok,bytes,len,&ids,&count,&d)!=MOSAIC_OK)return 0;
    int ok=count==n&&(!n||memcmp(ids,expected,n*sizeof *ids)==0)&&d.matched==1&&d.available==available&&!strcmp(d.language,tag);
    mosaic_free(ids);return ok;
}

int main(int argc,char **argv){
    if(argc!=7){fprintf(stderr,"usage: %s MODEL UNICODE DETECTOR EN HI JA\n",argv[0]);return 2;}
    mosaic_tokenizer *tok=NULL;
    if(mosaic_tokenizer_load_files(argv[1],argv[2],&tok)!=MOSAIC_OK)return 3;
    if(mosaic_tokenizer_add_language_file(tok,argv[4])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(tok,argv[5])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(tok,argv[6])!=MOSAIC_OK)return 4;
    if(mosaic_tokenizer_set_detector_file(tok,argv[3])!=MOSAIC_OK||!mosaic_tokenizer_detector_loaded(tok))return 5;
    if(mosaic_tokenizer_set_detector_file(tok,argv[3])!=MOSAIC_ERROR_CONFLICT)return 6;
    const uint8_t en[]="tokenizer";const uint32_t en_ids[]={271};
    const uint8_t hi[]="\xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87 \xe0\xa4\xa6\xe0\xa5\x81\xe0\xa4\xa8\xe0\xa4\xbf\xe0\xa4\xaf\xe0\xa4\xbe";const uint32_t hi_ids[]={273};
    const uint8_t ja[]="\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe4\xb8\x96\xe7\x95\x8c";const uint32_t ja_ids[]={274};
    if(!expect_auto(tok,en,sizeof en-1,"en",1,en_ids,1)||!expect_auto(tok,hi,sizeof hi-1,"hi",1,hi_ids,1)||!expect_auto(tok,ja,sizeof ja-1,"ja",1,ja_ids,1))return 7;
    const uint8_t unknown[]="xyz";uint32_t *ids=NULL;size_t count=0;mosaic_detection d;
    if(mosaic_tokenizer_encode_auto(tok,unknown,sizeof unknown-1,&ids,&count,&d)!=MOSAIC_OK||d.matched||d.available)return 8;
    mosaic_free(ids);mosaic_tokenizer_free(tok);puts("OK detector auto routing");return 0;
}
