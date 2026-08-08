#include "mosaic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int die(const char *s){fprintf(stderr,"%s\n",s);return 1;}
static int exact_partition(const mosaic_lex_token*t,size_t n,size_t len){size_t p=0;for(size_t i=0;i<n;++i){if(t[i].start!=p||!t[i].length||t[i].length>len-p)return 0;p+=(size_t)t[i].length;}return p==len;}
static size_t count_kind(const mosaic_lex_token*t,size_t n,uint32_t kind){size_t c=0;for(size_t i=0;i<n;++i)c+=t[i].kind==kind;return c;}

int main(int argc,char**argv){
    if(argc!=7)return die("usage: lexer_smoke MODEL UNICODE C PYTHON RUST JSON");
    const char *profiles[4]={argv[3],argv[4],argv[5],argv[6]};
    const char *names[4]={"c","python","rust","json"};
    for(size_t pi=0;pi<4;++pi){
        mosaic_lexer*l=NULL;if(mosaic_lexer_load_file(profiles[pi],&l)!=MOSAIC_OK)return die("lexer load");
        char name[64];size_t need=0;if(mosaic_lexer_profile_name(l,name,sizeof name,&need)!=MOSAIC_OK||strcmp(name,names[pi]))return die("lexer name");
        mosaic_lexer_free(l);
    }
    static const uint8_t csrc[]="int main(){ // hello\n char *s=\"x\\\"y\"; /* block */ return 42; }\n";
    mosaic_lexer*l=NULL;if(mosaic_lexer_load_file(argv[3],&l)!=MOSAIC_OK)return die("c lexer load");
    mosaic_lex_token*t=NULL;size_t n=0;if(mosaic_lex(l,csrc,sizeof csrc-1,&t,&n)!=MOSAIC_OK||!exact_partition(t,n,sizeof csrc-1))return die("c partition");
    if(count_kind(t,n,MOSAIC_LEX_KEYWORD)<3||count_kind(t,n,MOSAIC_LEX_COMMENT)!=2||count_kind(t,n,MOSAIC_LEX_STRING)!=1||count_kind(t,n,MOSAIC_LEX_NUMBER)!=1)return die("c token classes");
    mosaic_free(t);mosaic_lexer_free(l);

    /* Longest-prefix recognition: Python triple quote must win over one-byte quote. */
    static const uint8_t pysrc[]="def f():\n    x = \"\"\"hello\nworld\"\"\"\n    return x\n";
    if(mosaic_lexer_load_file(argv[4],&l)!=MOSAIC_OK)return die("python lexer load");
    if(mosaic_lex(l,pysrc,sizeof pysrc-1,&t,&n)!=MOSAIC_OK||!exact_partition(t,n,sizeof pysrc-1)||count_kind(t,n,MOSAIC_LEX_STRING)!=1)return die("python triple string");
    mosaic_free(t);mosaic_lexer_free(l);

    /* Nested block comments are profile data, not global behavior. */
    static const uint8_t rsrc[]="fn main(){ /* outer /* inner */ done */ let x=1; }";
    if(mosaic_lexer_load_file(argv[5],&l)!=MOSAIC_OK)return die("rust lexer load");
    if(mosaic_lex(l,rsrc,sizeof rsrc-1,&t,&n)!=MOSAIC_OK||!exact_partition(t,n,sizeof rsrc-1)||count_kind(t,n,MOSAIC_LEX_COMMENT)!=1||count_kind(t,n,MOSAIC_LEX_ERROR))return die("nested comment");
    mosaic_free(t);mosaic_lexer_free(l);

    static const uint8_t bad[]="\"unterminated";
    if(mosaic_lexer_load_file(argv[6],&l)!=MOSAIC_OK)return die("json lexer load");
    if(mosaic_lex(l,bad,sizeof bad-1,&t,&n)!=MOSAIC_OK||n!=1||t[0].kind!=MOSAIC_LEX_ERROR||t[0].length!=sizeof bad-1)return die("unterminated string error span");
    mosaic_free(t);mosaic_lexer_free(l);

    /* Integrated TokenDocument projection + fingerprint/capability behavior. */
    mosaic_tokenizer*tz=NULL;if(mosaic_tokenizer_load_files(argv[1],argv[2],&tz)!=MOSAIC_OK)return die("tokenizer load");
    uint8_t before[32],after[32];if(mosaic_tokenizer_fingerprint(tz,before)!=MOSAIC_OK)return die("fp before");
    if(mosaic_tokenizer_set_lexer_file(tz,argv[3])!=MOSAIC_OK||!mosaic_tokenizer_lexer_loaded(tz))return die("attach lexer");
    if(mosaic_tokenizer_fingerprint(tz,after)!=MOSAIC_OK||memcmp(before,after,32)==0)return die("lexer fingerprint");
    mosaic_tokenizer_capabilities caps={sizeof caps,0,0};if(mosaic_tokenizer_get_capabilities(tz,&caps)!=MOSAIC_OK||!(caps.available&MOSAIC_CAP_LEXER))return die("lexer capability");
    mosaic_token_document*doc=NULL;if(mosaic_tokenizer_token_document_create(tz,csrc,sizeof csrc-1,MOSAIC_TOKEN_DOCUMENT_LEXICAL,&doc)!=MOSAIC_OK)return die("lexical document");
    mosaic_token_document_info info;if(mosaic_token_document_get_info(doc,&info)!=MOSAIC_OK||!info.lexical_token_count)return die("lexical info");
    mosaic_lex_token*copy=NULL;size_t cn=0;if(mosaic_token_document_lexical_tokens(doc,&copy,&cn)!=MOSAIC_OK||cn!=info.lexical_token_count||!exact_partition(copy,cn,sizeof csrc-1))return die("lexical document projection");
    mosaic_free(copy);mosaic_tokenizer_free(tz);
    if(mosaic_token_document_lexical_tokens(doc,&copy,&cn)!=MOSAIC_OK||!exact_partition(copy,cn,sizeof csrc-1))return die("lexical snapshot lifetime");
    mosaic_free(copy);mosaic_token_document_free(doc);
    puts("OK declarative lexer profiles + exact lexical TokenDocument projection");
    return 0;
}
