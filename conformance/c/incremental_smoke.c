#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static uint64_t rng=UINT64_C(0x696e6372656d656e);
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return(uint32_t)rng;}
static int compare_model(mosaic_incremental_document*d,mosaic_model*m,const uint8_t*b,size_t n){uint32_t*a=NULL,*c=NULL;size_t an=0,cn=0;if(mosaic_incremental_document_encode(d,&a,&an)!=MOSAIC_OK||mosaic_encode(m,b,n,&c,&cn)!=MOSAIC_OK)return 0;int ok=an==cn&&(!an||!memcmp(a,c,an*sizeof*a));mosaic_free(a);mosaic_free(c);return ok;}
static int compare_tokenizer(mosaic_incremental_document*d,mosaic_tokenizer*t,const uint8_t*b,size_t n){uint32_t*a=NULL,*c=NULL;size_t an=0,cn=0;if(mosaic_incremental_document_encode(d,&a,&an)!=MOSAIC_OK||mosaic_tokenizer_encode(t,b,n,&c,&cn)!=MOSAIC_OK)return 0;int ok=an==cn&&(!an||!memcmp(a,c,an*sizeof*a));mosaic_free(a);mosaic_free(c);return ok;}
int main(int argc,char**argv){
    CHECK(argc==7);mosaic_model*m=NULL;CHECK(mosaic_model_load_file(argv[1],&m)==MOSAIC_OK);
    uint8_t buf[16384];size_t len=4096;for(size_t i=0;i<len;++i)buf[i]=(uint8_t)rnd();
    mosaic_incremental_document*d=NULL;CHECK(mosaic_incremental_document_create(m,buf,len,&d)==MOSAIC_OK);CHECK(compare_model(d,m,buf,len));
    size_t total_reprocessed=0,total_full=0;
    for(size_t tc=0;tc<1000;++tc){size_t start=rnd()%(len+1u);size_t maxdel=len-start;if(maxdel>16)maxdel=16;size_t del=maxdel?(rnd()%(maxdel+1u)):0;uint8_t repl[16];size_t rn=rnd()%17u;for(size_t i=0;i<rn;++i)repl[i]=(uint8_t)rnd();CHECK(len-del+rn<=sizeof buf);memmove(buf+start+rn,buf+start+del,len-start-del);if(rn)memcpy(buf+start,repl,rn);len=len-del+rn;CHECK(mosaic_incremental_document_apply_edit(d,start,del,repl,rn)==MOSAIC_OK);CHECK(compare_model(d,m,buf,len));CHECK(mosaic_incremental_document_last_reprocessed_bytes(d)<=len);total_reprocessed+=mosaic_incremental_document_last_reprocessed_bytes(d);total_full+=len;}
    uint8_t*copy=NULL;size_t copylen=0;CHECK(mosaic_incremental_document_copy_bytes(d,&copy,&copylen)==MOSAIC_OK);CHECK(copylen==len&&(!len||!memcmp(copy,buf,len)));mosaic_free(copy);mosaic_incremental_document_free(d);
    mosaic_tokenizer*t=NULL;CHECK(mosaic_tokenizer_load_files(argv[1],argv[2],&t)==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[3])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[4])==MOSAIC_OK);CHECK(mosaic_tokenizer_add_language_file(t,argv[5])==MOSAIC_OK);
    const uint8_t mixed[]="tokenizer \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87 \xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe4\xb8\x96\xe7\x95\x8c";
    CHECK(mosaic_tokenizer_incremental_document_create(t,mixed,sizeof mixed-1,&d)==MOSAIC_OK);CHECK(compare_tokenizer(d,t,mixed,sizeof mixed-1));const uint8_t bang='!';CHECK(mosaic_incremental_document_apply_edit(d,sizeof mixed-1,0,&bang,1)==MOSAIC_OK);uint8_t mixed2[sizeof mixed];memcpy(mixed2,mixed,sizeof mixed-1);mixed2[sizeof mixed-1]='!';CHECK(compare_tokenizer(d,t,mixed2,sizeof mixed));mosaic_incremental_document_free(d);mosaic_tokenizer_free(t);
    mosaic_model*bpe=NULL;CHECK(mosaic_model_load_file(argv[6],&bpe)==MOSAIC_OK);CHECK(mosaic_incremental_document_create(bpe,(const uint8_t*)"abc",3,&d)==MOSAIC_ERROR_UNSUPPORTED);CHECK(!d);mosaic_model_free(bpe);
    /* Large near-end edit must reuse most of the source prefix. */
    size_t biglen=1024u*1024u;uint8_t*big=(uint8_t*)malloc(biglen);CHECK(big);for(size_t i=0;i<biglen;++i)big[i]=(uint8_t)("hello tokenizer "[i%16]);CHECK(mosaic_incremental_document_create(m,big,biglen,&d)==MOSAIC_OK);size_t pos=biglen-64;const uint8_t zz[]="XYZ";memcpy(big+pos,zz,3);CHECK(mosaic_incremental_document_apply_edit(d,pos,3,zz,3)==MOSAIC_OK);CHECK(compare_model(d,m,big,biglen));size_t rp=mosaic_incremental_document_last_reprocessed_bytes(d),reuse=mosaic_incremental_document_last_reused_prefix_bytes(d);CHECK(rp<4096);CHECK(reuse>biglen-4096);mosaic_incremental_document_free(d);free(big);mosaic_model_free(m);
    printf("OK incremental Viterbi: edits=1000 avg_reprocessed=%.1f%% near_end=%zu reused=%zu\n",total_full?100.0*(double)total_reprocessed/(double)total_full:0.0,rp,reuse);return 0;
}
