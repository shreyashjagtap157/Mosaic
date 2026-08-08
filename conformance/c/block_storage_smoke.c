#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same_tokens(const mosaic_document_token *a,const mosaic_document_token *b,size_t n){
    for(size_t i=0;i<n;++i)if(a[i].id!=b[i].id||a[i].start!=b[i].start||a[i].length!=b[i].length)return 0;
    return 1;
}
static int hash_nonzero(const uint8_t h[32]){uint8_t v=0;for(size_t i=0;i<32;++i)v|=h[i];return v!=0;}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    mosaic_tokenizer*t=NULL;if(mosaic_tokenizer_load_files(argv[1],argv[2],&t)!=MOSAIC_OK)return 3;
    size_t input_len=1024u*1024u+333u;uint8_t*input=(uint8_t*)malloc(input_len);if(!input)return 4;
    static const uint8_t pat[]="tokenizer hello world 12345 \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87\n";
    for(size_t i=0;i<input_len;++i)input[i]=pat[i%(sizeof pat-1u)];
    mosaic_token_document*d=NULL;if(mosaic_tokenizer_token_document_create(t,input,input_len,MOSAIC_TOKEN_DOCUMENT_MODEL,&d)!=MOSAIC_OK)return 5;
    mosaic_document_token*original=NULL;size_t original_count=0;if(mosaic_token_document_model_tokens(d,&original,&original_count)!=MOSAIC_OK||!original_count)return 6;

    mosaic_block_policy p={0};mosaic_block_policy_default(&p);p.min_bytes=2048;p.preferred_bytes=4096;p.max_bytes=8192;p.macroblock_bytes=32768;p.max_blocks=10000;p.max_macroblocks=1000;
    mosaic_block_plan*plan=NULL;if(mosaic_token_document_block_plan(d,&p,&plan)!=MOSAIC_OK)return 7;
    mosaic_block_plan_info info={0};if(mosaic_block_plan_get_info(plan,&info)!=MOSAIC_OK||info.source_length!=input_len||!info.block_count||!info.macroblock_count)return 8;
    mosaic_processing_block*blocks=NULL;size_t bn=0;if(mosaic_block_plan_blocks(plan,&blocks,&bn)!=MOSAIC_OK||bn!=info.block_count)return 9;
    uint64_t source_cursor=0,token_cursor=0;for(size_t i=0;i<bn;++i){
        if(blocks[i].source_start!=source_cursor||blocks[i].first_model_token!=token_cursor||!blocks[i].model_token_count||!hash_nonzero(blocks[i].source_sha256)||!hash_nonzero(blocks[i].identity_sha256))return 10;
        if(!(blocks[i].flags&MOSAIC_BLOCK_OVERSIZE_TOKEN)&&blocks[i].source_length>p.max_bytes)return 11;
        source_cursor+=blocks[i].source_length;token_cursor+=blocks[i].model_token_count;
    }
    if(source_cursor!=input_len||token_cursor!=original_count)return 12;
    mosaic_macroblock*macros=NULL;size_t mn=0;if(mosaic_block_plan_macroblocks(plan,&macros,&mn)!=MOSAIC_OK||mn!=info.macroblock_count)return 13;
    uint64_t bcursor=0,mcursor=0;for(size_t i=0;i<mn;++i){if(macros[i].first_block!=bcursor||macros[i].source_start!=mcursor||!macros[i].block_count||!hash_nonzero(macros[i].identity_sha256))return 14;bcursor+=macros[i].block_count;mcursor+=macros[i].source_length;}if(bcursor!=bn||mcursor!=input_len)return 15;

    mosaic_block_plan*plan2=NULL;if(mosaic_token_document_block_plan(d,&p,&plan2)!=MOSAIC_OK)return 16;mosaic_processing_block*b2=NULL;size_t b2n=0;if(mosaic_block_plan_blocks(plan2,&b2,&b2n)!=MOSAIC_OK||b2n!=bn)return 17;for(size_t i=0;i<bn;++i)if(memcmp(blocks[i].identity_sha256,b2[i].identity_sha256,32))return 18;

    mosaic_block_policy tight=p;tight.max_blocks=1;mosaic_block_plan*limited=NULL;if(mosaic_token_document_block_plan(d,&tight,&limited)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 19;
    static const uint8_t one_token[]="tokenizer";mosaic_token_document*small=NULL;if(mosaic_tokenizer_token_document_create(t,one_token,sizeof one_token-1,MOSAIC_TOKEN_DOCUMENT_MODEL,&small)!=MOSAIC_OK)return 20;
    mosaic_block_policy tiny=p;tiny.min_bytes=1;tiny.preferred_bytes=1;tiny.max_bytes=1;tiny.macroblock_bytes=1;tiny.max_blocks=32;tiny.max_macroblocks=32;mosaic_block_plan*oversize=NULL;if(mosaic_token_document_block_plan(small,&tiny,&oversize)!=MOSAIC_OK)return 21;mosaic_processing_block*ob=NULL;size_t obn=0;if(mosaic_block_plan_blocks(oversize,&ob,&obn)!=MOSAIC_OK||!obn)return 22;int saw_oversize=0;for(size_t i=0;i<obn;++i)if(ob[i].flags&MOSAIC_BLOCK_OVERSIZE_TOKEN)saw_oversize=1;if(!saw_oversize)return 23;mosaic_token_document_free(small);

    uint8_t*packed=NULL;size_t packed_len=0;if(mosaic_token_document_pack_model(d,&packed,&packed_len)!=MOSAIC_OK||packed_len>=original_count*sizeof(mosaic_document_token))return 23;
    mosaic_packed_model_info pi={0};if(mosaic_packed_model_inspect(packed,packed_len,&pi)!=MOSAIC_OK||pi.token_count!=original_count||pi.source_length!=input_len||!pi.id_bit_width)return 24;
    mosaic_document_token*decoded=NULL;size_t decoded_count=0;if(mosaic_packed_model_decode(packed,packed_len,&decoded,&decoded_count)!=MOSAIC_OK||decoded_count!=original_count||!same_tokens(original,decoded,original_count))return 25;
    uint8_t*corrupt=(uint8_t*)malloc(packed_len);if(!corrupt)return 26;memcpy(corrupt,packed,packed_len);corrupt[packed_len-1]^=1u;if(mosaic_packed_model_inspect(corrupt,packed_len,&pi)!=MOSAIC_ERROR_INVALID_PACK)return 27;if(mosaic_packed_model_inspect(packed,packed_len-1u,&pi)!=MOSAIC_ERROR_INVALID_PACK)return 28;

    double ratio=(double)packed_len/(double)(original_count*sizeof(mosaic_document_token));
    printf("OK blocks=%zu macroblocks=%zu packed=%zu raw_struct=%zu ratio=%.3f tokens=%zu\n",bn,mn,packed_len,original_count*sizeof(mosaic_document_token),ratio,original_count);
    free(corrupt);mosaic_free(decoded);mosaic_free(packed);mosaic_free(ob);mosaic_block_plan_free(oversize);mosaic_free(b2);mosaic_block_plan_free(plan2);mosaic_free(macros);mosaic_free(blocks);mosaic_block_plan_free(plan);mosaic_free(original);mosaic_token_document_free(d);mosaic_tokenizer_free(t);free(input);return 0;
}
