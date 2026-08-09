#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_thread.h"

typedef struct { mosaic_tokenizer *tokenizer; int failed; } Worker;
static int worker(void *arg) {
    Worker *w=(Worker*)arg;
    static const uint8_t text[]="hello";
    for(unsigned i=0;i<1000u;++i){uint32_t *ids=NULL;size_t n=0;if(mosaic_tokenizer_encode(w->tokenizer,text,5,&ids,&n)!=MOSAIC_OK||!n){mosaic_free(ids);w->failed=1;return -1;}mosaic_free(ids);}
    return 0;
}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    mosaic_tokenizer *t=NULL;if(mosaic_tokenizer_load_files(argv[1],argv[2],&t)!=MOSAIC_OK)return 3;
    uint8_t semantic_before[32],runtime_before[32],runtime_after[32];
    if(mosaic_tokenizer_fingerprint(t,semantic_before)!=MOSAIC_OK||mosaic_tokenizer_runtime_identity(t,runtime_before)!=MOSAIC_OK)return 4;
    mosaic_runtime_limits limits={0};mosaic_runtime_limits_default(&limits);limits.max_input_bytes=8;limits.max_output_tokens=8;limits.max_token_document_bytes=8;
    if(mosaic_tokenizer_set_runtime_limits(t,&limits)!=MOSAIC_OK||mosaic_tokenizer_runtime_identity(t,runtime_after)!=MOSAIC_OK||!memcmp(runtime_before,runtime_after,32))return 5;
    uint8_t semantic_after[32];if(mosaic_tokenizer_fingerprint(t,semantic_after)!=MOSAIC_OK||memcmp(semantic_before,semantic_after,32))return 6;
    if(mosaic_tokenizer_seal(t)!=MOSAIC_OK||!mosaic_tokenizer_is_sealed(t))return 7;
    if(mosaic_tokenizer_add_language_file(t,argv[3])!=MOSAIC_ERROR_STATE||mosaic_tokenizer_set_runtime_limits(t,&limits)!=MOSAIC_ERROR_STATE)return 8;
    const uint8_t too_long[]="123456789";uint32_t *ids=NULL;size_t n=0;if(mosaic_tokenizer_encode(t,too_long,9,&ids,&n)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 9;
    mosaic_stream *stream=NULL;if(mosaic_tokenizer_stream_create(t,&stream)!=MOSAIC_OK)return 10;if(mosaic_stream_push(stream,too_long,9)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 11;mosaic_stream_free(stream);
    mosaic_document *doc=NULL;if(mosaic_tokenizer_document_create(t,(const uint8_t*)"abc",3,&doc)!=MOSAIC_OK)return 12;if(mosaic_document_apply_edit(doc,3,0,(const uint8_t*)"123456",6)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 13;mosaic_document_free(doc);
    mosaic_incremental_document *inc=NULL;if(mosaic_tokenizer_incremental_document_create(t,(const uint8_t*)"abc",3,&inc)!=MOSAIC_OK)return 14;if(mosaic_incremental_document_apply_edit(inc,3,0,(const uint8_t*)"123456",6)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 15;mosaic_incremental_document_free(inc);
    if(mosaic_tokenizer_reset_metrics(t)!=MOSAIC_OK)return 16;
    enum{N=8};mosaic_thread_t threads[N];Worker workers[N];for(unsigned i=0;i<N;++i){workers[i]=(Worker){t,0};if(!mosaic_thread_create(&threads[i],worker,&workers[i]))return 17;}
    for(unsigned i=0;i<N;++i){if(!mosaic_thread_join(threads[i])||workers[i].failed)return 18;}
    mosaic_runtime_metrics metrics={0};if(mosaic_tokenizer_get_metrics(t,&metrics)!=MOSAIC_OK||metrics.encode_calls!=8000u||metrics.bytes_in!=40000u||metrics.tokens_out<8000u||metrics.failures)return 19;
    printf("OK sealed runtime threads=%u encode_calls=%llu bytes=%llu tokens=%llu\n",N,(unsigned long long)metrics.encode_calls,(unsigned long long)metrics.bytes_in,(unsigned long long)metrics.tokens_out);
    mosaic_tokenizer_free(t);return 0;
}
