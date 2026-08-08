#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { size_t count; int mixed,bidi,ign; } Stats;
static mosaic_status visitor(void *ctx,const mosaic_security_finding *f){Stats*s=(Stats*)ctx;++s->count;s->mixed+=f->kind==MOSAIC_SECURITY_MIXED_SCRIPT;s->bidi+=f->kind==MOSAIC_SECURITY_BIDI_CONTROL;s->ign+=f->kind==MOSAIC_SECURITY_DEFAULT_IGNORABLE;return MOSAIC_OK;}
static mosaic_status stop_visitor(void *ctx,const mosaic_security_finding *f){(void)ctx;(void)f;return MOSAIC_ERROR_CONFLICT;}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    mosaic_security *sec=NULL; if(mosaic_security_load_file(argv[3],&sec)!=MOSAIC_OK)return 3;
    const uint8_t text[]={'A',0xD0,0x96,0xE2,0x80,0xAE,0xE2,0x80,0x8B};
    mosaic_script_span *sp=NULL;size_t sn=0;if(mosaic_security_script_ranges(sec,text,sizeof text,&sp,&sn)!=MOSAIC_OK||sn<3)return 4;mosaic_free(sp);
    mosaic_security_finding *f=NULL;size_t fn=0;if(mosaic_security_scan(sec,text,sizeof text,&f,&fn)!=MOSAIC_OK)return 5;
    int mixed=0,bidi=0,ign=0;for(size_t i=0;i<fn;++i){mixed+=f[i].kind==MOSAIC_SECURITY_MIXED_SCRIPT;bidi+=f[i].kind==MOSAIC_SECURITY_BIDI_CONTROL;ign+=f[i].kind==MOSAIC_SECURITY_DEFAULT_IGNORABLE;}size_t expected_fn=fn;mosaic_free(f);if(!mixed||!bidi||ign<2)return 6;
    Stats stats={0};size_t visited=0;if(mosaic_security_visit(sec,text,sizeof text,visitor,&stats,&visited)!=MOSAIC_OK||visited!=expected_fn||stats.count!=expected_fn||stats.mixed!=mixed||stats.bidi!=bidi||stats.ign!=ign)return 14;
    size_t stopped=99;if(mosaic_security_visit(sec,text,sizeof text,stop_visitor,NULL,&stopped)!=MOSAIC_ERROR_CONFLICT||stopped!=0)return 15;
    char name[64];size_t need=0;if(mosaic_security_script_name(sec,1,name,sizeof name,&need)!=MOSAIC_OK||need<2)return 7;
    mosaic_tokenizer *tok=NULL;if(mosaic_tokenizer_load_files(argv[1],argv[2],&tok)!=MOSAIC_OK)return 8;
    uint8_t before[32],after[32];if(mosaic_tokenizer_fingerprint(tok,before)!=MOSAIC_OK)return 9;
    if(mosaic_tokenizer_set_security_file(tok,argv[3])!=MOSAIC_OK||!mosaic_tokenizer_security_loaded(tok))return 10;
    if(mosaic_tokenizer_set_security_file(tok,argv[3])!=MOSAIC_ERROR_CONFLICT)return 11;
    if(mosaic_tokenizer_fingerprint(tok,after)!=MOSAIC_OK||memcmp(before,after,32)==0)return 12;
    f=NULL;fn=0;if(mosaic_tokenizer_security_scan(tok,text,sizeof text,&f,&fn)!=MOSAIC_OK||!fn)return 13;mosaic_free(f);
    memset(&stats,0,sizeof stats);visited=0;if(mosaic_tokenizer_security_visit(tok,text,sizeof text,visitor,&stats,&visited)!=MOSAIC_OK||visited!=expected_fn)return 16;
    mosaic_tokenizer_free(tok);mosaic_security_free(sec);puts("OK Unicode17 script/security evidence + bounded visitor + tokenizer fingerprint");return 0;
}
