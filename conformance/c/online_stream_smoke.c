#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static uint64_t rng_state=0x243f6a8885a308d3ULL;
static uint32_t rnd(void){rng_state^=rng_state<<7;rng_state^=rng_state>>9;rng_state^=rng_state<<8;return (uint32_t)rng_state;}
static int append(uint32_t **all,size_t *n,size_t *cap,const uint32_t *ids,size_t count){
    if(count>SIZE_MAX-*n)return 0;
    size_t need=*n+count;
    if(need>*cap){
        size_t next=*cap?*cap:64;
        while(next<need){if(next>SIZE_MAX/2){next=need;break;}next*=2;}
        if(next>SIZE_MAX/sizeof(uint32_t))return 0;
        uint32_t*p=(uint32_t*)realloc(*all,next*sizeof*p);if(!p)return 0;
        *all=p;*cap=next;
    }
    if(count)memcpy(*all+*n,ids,count*sizeof*ids);
    *n=need;return 1;
}
static int check_model(mosaic_model *model,const uint8_t *data,size_t len){
    uint32_t *full=NULL;size_t fulln=0;CHECK(mosaic_encode(model,data,len,&full,&fulln)==MOSAIC_OK);
    mosaic_online_stream *st=NULL;CHECK(mosaic_online_stream_create(model,65536,&st)==MOSAIC_OK);
    uint32_t *all=NULL;size_t n=0,cap=0,pos=0;
    while(pos<len){size_t chunk=1u+(rnd()%257u);if(chunk>len-pos)chunk=len-pos;size_t consumed=0;uint32_t *ids=NULL;size_t got=0;mosaic_status status=mosaic_online_stream_push(st,data+pos,chunk,&consumed,&ids,&got);CHECK(status==MOSAIC_OK);CHECK(consumed==chunk);CHECK(mosaic_online_stream_pending_bytes(st)<=65536u);CHECK(append(&all,&n,&cap,ids,got));mosaic_free(ids);pos+=consumed;}
    uint32_t *tail=NULL;size_t tailn=0;CHECK(mosaic_online_stream_finish(st,&tail,&tailn)==MOSAIC_OK);CHECK(append(&all,&n,&cap,tail,tailn));mosaic_free(tail);CHECK(n==fulln);CHECK(!n||memcmp(all,full,n*sizeof*all)==0);
    uint8_t *decoded=NULL;size_t decodedn=0;CHECK(mosaic_decode(model,all,n,&decoded,&decodedn)==MOSAIC_OK);CHECK(decodedn==len);CHECK(!len||memcmp(decoded,data,len)==0);
    mosaic_free(decoded);mosaic_free(full);free(all);mosaic_online_stream_free(st);return 0;
}
int main(int argc,char **argv){
    CHECK(argc==8);mosaic_model *model=NULL;CHECK(mosaic_model_load_file(argv[1],&model)==MOSAIC_OK);
    for(int tc=0;tc<500;++tc){size_t len=(size_t)(rnd()%2049u);uint8_t *data=len?(uint8_t*)malloc(len):NULL;CHECK(!len||data);for(size_t i=0;i<len;++i)data[i]=(uint8_t)rnd();CHECK(check_model(model,data,len)==0);free(data);}
    mosaic_tokenizer *tok=NULL;CHECK(mosaic_tokenizer_load_files(argv[1],argv[2],&tok)==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(tok,argv[3])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(tok,argv[4])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(tok,argv[5])==MOSAIC_OK);
    const uint8_t mixed[]="tokenizer \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87 \xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe4\xb8\x96\xe7\x95\x8c";
    uint32_t *full=NULL;size_t fulln=0;CHECK(mosaic_tokenizer_encode(tok,mixed,sizeof mixed-1,&full,&fulln)==MOSAIC_OK);mosaic_online_stream *ts=NULL;CHECK(mosaic_tokenizer_online_stream_create(tok,4096,&ts)==MOSAIC_OK);uint32_t *all=NULL;size_t n=0,cap=0,pos=0;while(pos<sizeof mixed-1){size_t chunk=3;if(chunk>sizeof mixed-1-pos)chunk=sizeof mixed-1-pos;size_t consumed=0;uint32_t *ids=NULL;size_t got=0;CHECK(mosaic_online_stream_push(ts,mixed+pos,chunk,&consumed,&ids,&got)==MOSAIC_OK);CHECK(consumed==chunk);CHECK(append(&all,&n,&cap,ids,got));mosaic_free(ids);pos+=chunk;}uint32_t *tail=NULL;size_t tailn=0;CHECK(mosaic_online_stream_finish(ts,&tail,&tailn)==MOSAIC_OK);CHECK(append(&all,&n,&cap,tail,tailn));CHECK(n==fulln&&(!n||!memcmp(all,full,n*sizeof*all)));mosaic_free(tail);mosaic_free(full);free(all);mosaic_online_stream_free(ts);mosaic_tokenizer_free(tok);
    mosaic_model *bpe=NULL;CHECK(mosaic_model_load_file(argv[6],&bpe)==MOSAIC_OK);mosaic_online_stream *bad=NULL;CHECK(mosaic_online_stream_create(bpe,4096,&bad)==MOSAIC_ERROR_UNSUPPORTED);CHECK(!bad);mosaic_model_free(bpe);mosaic_model_free(model);
    mosaic_model *adv=NULL;CHECK(mosaic_model_load_file(argv[7],&adv)==MOSAIC_OK);mosaic_online_stream *limited=NULL;CHECK(mosaic_online_stream_create(adv,3,&limited)==MOSAIC_OK);size_t consumed=0;uint32_t *early=NULL;size_t earlyn=0;CHECK(mosaic_online_stream_push(limited,(const uint8_t*)"abx",3,&consumed,&early,&earlyn)==MOSAIC_OK);CHECK(consumed==3&&earlyn==0&&mosaic_online_stream_pending_bytes(limited)==3);mosaic_free(early);consumed=99;early=NULL;earlyn=99;CHECK(mosaic_online_stream_push(limited,(const uint8_t*)"q",1,&consumed,&early,&earlyn)==MOSAIC_ERROR_RESOURCE_LIMIT);CHECK(consumed==0&&earlyn==0&&mosaic_online_stream_pending_bytes(limited)==3);mosaic_free(early);uint32_t *limtail=NULL;size_t limn=0;CHECK(mosaic_online_stream_finish(limited,&limtail,&limn)==MOSAIC_OK);mosaic_free(limtail);mosaic_online_stream_free(limited);mosaic_model_free(adv);
    puts("OK online Viterbi stream: 500 arbitrary-byte/chunk differentials + language snapshot + raw-BPE rejection");return 0;
}
