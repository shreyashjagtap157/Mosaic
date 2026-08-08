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
    /* Auto stream snapshots the complete tokenizer and remains valid after parent release. */
    mosaic_tokenizer *stream_parent=NULL; mosaic_stream *stream=NULL;
    if(mosaic_tokenizer_load_files(argv[1],argv[2],&stream_parent)!=MOSAIC_OK)return 20;
    if(mosaic_tokenizer_add_language_file(stream_parent,argv[4])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(stream_parent,argv[5])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(stream_parent,argv[6])!=MOSAIC_OK||mosaic_tokenizer_set_detector_file(stream_parent,argv[3])!=MOSAIC_OK)return 21;
    if(mosaic_tokenizer_stream_create_auto(stream_parent,&stream)!=MOSAIC_OK)return 22;
    mosaic_tokenizer_free(stream_parent); stream_parent=NULL;
    if(mosaic_stream_push(stream,en,3)!=MOSAIC_OK||mosaic_stream_push(stream,en+3,(sizeof en-1)-3)!=MOSAIC_OK)return 23;
    uint32_t *stream_ids=NULL; size_t stream_n=0; mosaic_detection stream_det;
    if(mosaic_stream_finish_auto(stream,&stream_ids,&stream_n,&stream_det)!=MOSAIC_OK||stream_n!=1||stream_ids[0]!=271||strcmp(stream_det.language,"en")||!stream_det.available)return 24;
    mosaic_free(stream_ids);
    if(mosaic_stream_reset(stream)!=MOSAIC_OK||mosaic_stream_push(stream,ja,sizeof ja-1)!=MOSAIC_OK)return 25;
    stream_ids=NULL;stream_n=0;
    if(mosaic_stream_finish_auto(stream,&stream_ids,&stream_n,&stream_det)!=MOSAIC_OK||stream_n!=1||stream_ids[0]!=274||strcmp(stream_det.language,"ja")||!stream_det.available)return 26;
    mosaic_free(stream_ids); mosaic_stream_free(stream);

    /* Auto editable documents re-detect exact current bytes after edits and own their snapshot. */
    mosaic_tokenizer *doc_parent=NULL; mosaic_document *doc=NULL;
    if(mosaic_tokenizer_load_files(argv[1],argv[2],&doc_parent)!=MOSAIC_OK)return 27;
    if(mosaic_tokenizer_add_language_file(doc_parent,argv[4])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(doc_parent,argv[5])!=MOSAIC_OK||mosaic_tokenizer_add_language_file(doc_parent,argv[6])!=MOSAIC_OK||mosaic_tokenizer_set_detector_file(doc_parent,argv[3])!=MOSAIC_OK)return 28;
    if(mosaic_tokenizer_document_create_auto(doc_parent,en,sizeof en-1,&doc)!=MOSAIC_OK)return 29;
    mosaic_tokenizer_free(doc_parent); doc_parent=NULL;
    uint32_t *doc_ids=NULL;size_t doc_n=0;mosaic_detection doc_det;
    if(mosaic_document_encode_auto(doc,&doc_ids,&doc_n,&doc_det)!=MOSAIC_OK||doc_n!=1||doc_ids[0]!=271||strcmp(doc_det.language,"en")||!doc_det.available)return 30;
    mosaic_free(doc_ids);
    if(mosaic_document_apply_edit(doc,0,sizeof en-1,hi,sizeof hi-1)!=MOSAIC_OK)return 31;
    doc_ids=NULL;doc_n=0;
    if(mosaic_document_encode_auto(doc,&doc_ids,&doc_n,&doc_det)!=MOSAIC_OK||doc_n!=1||doc_ids[0]!=273||strcmp(doc_det.language,"hi")||!doc_det.available)return 32;
    mosaic_free(doc_ids); mosaic_document_free(doc);
    const uint8_t unknown[]="xyz";uint32_t *ids=NULL;size_t count=0;mosaic_detection d;
    if(mosaic_tokenizer_encode_auto(tok,unknown,sizeof unknown-1,&ids,&count,&d)!=MOSAIC_OK||d.matched||d.available)return 8;
    mosaic_free(ids);mosaic_tokenizer_free(tok);puts("OK detector auto routing + auto stream/document snapshots");return 0;
}
