#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int has_component(const mosaic_semantic_component *c,size_t n,uint32_t kind,const unsigned char *src,const char *text){
    size_t want=strlen(text);for(size_t i=0;i<n;++i)if(c[i].kind==kind&&c[i].length==want&&!memcmp(src+c[i].start,text,want))return 1;return 0;
}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    mosaic_tokenizer*t=NULL;if(mosaic_tokenizer_load_files(argv[1],argv[2],&t)!=MOSAIC_OK)return 3;
    if(mosaic_tokenizer_set_lexer_file(t,argv[3])!=MOSAIC_OK)return 4;
    mosaic_tokenizer_capabilities caps={sizeof caps,0,0};if(mosaic_tokenizer_get_capabilities(t,&caps)!=MOSAIC_OK)return 5;
    if(!(caps.available&MOSAIC_CAP_SEMANTIC)||!(caps.available&MOSAIC_CAP_SUBBYTE))return 6;
    const unsigned char src[]="deserializeHTTP2Response 0x1f 12.5e-2 \"hello\"";
    mosaic_token_document*d=NULL;
    if(mosaic_tokenizer_token_document_create(t,src,sizeof src-1,MOSAIC_TOKEN_DOCUMENT_SEMANTIC,&d)!=MOSAIC_OK)return 7;
    mosaic_token_document_info info={0};if(mosaic_token_document_get_info(d,&info)!=MOSAIC_OK||!info.semantic_component_count||info.lexical_token_count)return 8;
    mosaic_semantic_component*c=NULL;size_t n=0;if(mosaic_token_document_semantic_components(d,&c,&n)!=MOSAIC_OK||n!=info.semantic_component_count)return 9;
    const char*parts[]={"deserialize","HTTP","2","Response"};for(size_t i=0;i<4;++i)if(!has_component(c,n,MOSAIC_SEM_IDENTIFIER_PART,src,parts[i]))return 10;
    if(!has_component(c,n,MOSAIC_SEM_NUMBER_RADIX_PREFIX,src,"0x")||!has_component(c,n,MOSAIC_SEM_NUMBER_INTEGER,src,"1f"))return 11;
    if(!has_component(c,n,MOSAIC_SEM_NUMBER_FRACTION,src,"5")||!has_component(c,n,MOSAIC_SEM_NUMBER_EXPONENT_MARK,src,"e")||!has_component(c,n,MOSAIC_SEM_NUMBER_EXPONENT_SIGN,src,"-")||!has_component(c,n,MOSAIC_SEM_NUMBER_EXPONENT_DIGITS,src,"2"))return 12;
    if(!has_component(c,n,MOSAIC_SEM_STRING_CONTENT,src,"hello"))return 13;
    mosaic_free(c);mosaic_token_document_free(d);mosaic_tokenizer_free(t);
    const uint8_t b[]={0xad,0x61};uint64_t v=0;
    mosaic_subbyte_span hi={0,0,4,MOSAIC_BIT_MSB0,0};if(mosaic_subbyte_extract_u64(b,sizeof b,hi,&v)!=MOSAIC_OK||v!=0xau)return 14;
    mosaic_subbyte_span lo={0,0,4,MOSAIC_BIT_LSB0,0};if(mosaic_subbyte_extract_u64(b,sizeof b,lo,&v)!=MOSAIC_OK||v!=0xdu)return 15;
    mosaic_subbyte_span cross={0,4,8,MOSAIC_BIT_MSB0,0};if(mosaic_subbyte_extract_u64(b,sizeof b,cross,&v)!=MOSAIC_OK||v!=0xd6u)return 16;
    mosaic_subbyte_span bad={1,7,2,MOSAIC_BIT_MSB0,0};if(mosaic_subbyte_extract_u64(b,sizeof b,bad,&v)!=MOSAIC_ERROR_OVERFLOW)return 17;
    if(mosaic_subbyte_extract_u64(NULL,0,hi,&v)!=MOSAIC_ERROR_INVALID_ARGUMENT)return 18;
    printf("OK semantic identifier/number/string enrichment + sub-byte extraction components=%zu\n",n);return 0;
}
