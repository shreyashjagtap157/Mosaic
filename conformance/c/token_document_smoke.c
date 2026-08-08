#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static int same_model_projection(mosaic_tokenizer *t,const uint8_t *src,size_t n,mosaic_token_document *d){
    mosaic_token *a=NULL;size_t an=0;mosaic_document_token*b=NULL;size_t bn=0;
    if(mosaic_tokenizer_encode_tokens(t,src,n,&a,&an)!=MOSAIC_OK||mosaic_token_document_model_tokens(d,&b,&bn)!=MOSAIC_OK)return 0;
    int ok=an==bn;for(size_t i=0;ok&&i<an;++i)ok=a[i].id==b[i].id&&a[i].start==b[i].start&&a[i].length==b[i].length;
    mosaic_free(a);mosaic_free(b);return ok;
}
int main(int argc,char**argv){
    CHECK(argc==7);mosaic_tokenizer*t=NULL;CHECK(mosaic_tokenizer_load_files(argv[1],argv[2],&t)==MOSAIC_OK);
    CHECK(mosaic_tokenizer_add_language_file(t,argv[3])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[4])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[5])==MOSAIC_OK);CHECK(mosaic_tokenizer_set_detector_file(t,argv[6])==MOSAIC_OK);
    const uint8_t src[]={ 'A',0xff,'B',0xf0,0x9f,0x91,0xa9,0xe2,0x80,0x8d,0xf0,0x9f,0x92,0xbb,0 };
    const size_t n=sizeof src;
    mosaic_token_document*d=NULL;CHECK(mosaic_tokenizer_token_document_create(t,src,n,MOSAIC_TOKEN_DOCUMENT_MODEL|MOSAIC_TOKEN_DOCUMENT_GRAPHEMES,&d)==MOSAIC_OK);CHECK(d);CHECK(same_model_projection(t,src,n,d));
    mosaic_token_document_info info={0};CHECK(mosaic_token_document_get_info(d,&info)==MOSAIC_OK);CHECK(info.source_length==n&&info.model_token_count>0&&info.grapheme_count>0);uint8_t fp[32];CHECK(mosaic_tokenizer_fingerprint(t,fp)==MOSAIC_OK&&!memcmp(fp,info.tokenizer_fingerprint_sha256,32));
    uint8_t*copy=NULL;size_t cn=0;CHECK(mosaic_token_document_copy_source(d,&copy,&cn)==MOSAIC_OK&&cn==n&&!memcmp(copy,src,n));mosaic_free(copy);
    mosaic_range *ga=NULL,*gb=NULL;size_t gan=0,gbn=0;CHECK(mosaic_token_document_graphemes(d,&ga,&gan)==MOSAIC_OK);CHECK(mosaic_tokenizer_grapheme_ranges(t,src,n,&gb,&gbn)==MOSAIC_OK);CHECK(gan==gbn&&!memcmp(ga,gb,gan*sizeof*ga));mosaic_free(ga);mosaic_free(gb);
    mosaic_tokenizer_free(t);t=NULL;CHECK(mosaic_token_document_copy_source(d,&copy,&cn)==MOSAIC_OK&&cn==n&&!memcmp(copy,src,n));mosaic_free(copy);mosaic_token_document_free(d);d=NULL;
    /* Unrequested projections remain absent and do not trigger hidden work. */
    CHECK(mosaic_tokenizer_load_files(argv[1],argv[2],&t)==MOSAIC_OK);CHECK(mosaic_tokenizer_token_document_create(t,src,n,0,&d)==MOSAIC_OK);mosaic_document_token*mt=NULL;size_t mn=0;CHECK(mosaic_token_document_model_tokens(d,&mt,&mn)==MOSAIC_ERROR_UNSUPPORTED&&!mt&&!mn);CHECK(mosaic_token_document_graphemes(d,&ga,&gan)==MOSAIC_ERROR_UNSUPPORTED&&!ga&&!gan);mosaic_token_document_free(d);d=NULL;
    /* Auto TokenDocument snapshots the route used for the model projection. */
    CHECK(mosaic_tokenizer_add_language_file(t,argv[3])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[4])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[5])==MOSAIC_OK);CHECK(mosaic_tokenizer_set_detector_file(t,argv[6])==MOSAIC_OK);const uint8_t en[]="tokenizer";CHECK(mosaic_tokenizer_token_document_create_auto(t,en,sizeof en-1,MOSAIC_TOKEN_DOCUMENT_MODEL,&d)==MOSAIC_OK);CHECK(mosaic_token_document_get_info(d,&info)==MOSAIC_OK);CHECK(info.detection.matched&&info.detection.available&&!strcmp(info.detection.language,"en")&&info.model_token_count==1);mosaic_token_document_free(d);d=NULL;
    /* Empty source is a valid immutable document. */
    CHECK(mosaic_tokenizer_token_document_create(t,NULL,0,MOSAIC_TOKEN_DOCUMENT_MODEL|MOSAIC_TOKEN_DOCUMENT_GRAPHEMES,&d)==MOSAIC_OK);CHECK(mosaic_token_document_get_info(d,&info)==MOSAIC_OK&&info.source_length==0&&info.model_token_count==0&&info.grapheme_count==0);mosaic_token_document_free(d);d=NULL;
    CHECK(mosaic_tokenizer_token_document_create(t,src,n,UINT32_C(0x80000000),&d)==MOSAIC_ERROR_INVALID_ARGUMENT&&!d);
    mosaic_tokenizer_free(t);puts("OK TokenDocument exact source/model/grapheme projections + identities + auto snapshot");return 0;
}
