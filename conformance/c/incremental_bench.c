#define _POSIX_C_SOURCE 200809L
#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return(double)t.tv_sec+(double)t.tv_nsec/1e9;}
int main(int argc,char**argv){if(argc!=2)return 2;mosaic_model*m=NULL;if(mosaic_model_load_file(argv[1],&m)!=MOSAIC_OK)return 3;size_t n=10u*1024u*1024u;uint8_t*b=(uint8_t*)malloc(n);if(!b)return 4;const char*p="hello tokenizer world ";size_t pn=strlen(p);for(size_t i=0;i<n;++i)b[i]=(uint8_t)p[i%pn];mosaic_incremental_document*d=NULL;if(mosaic_incremental_document_create(m,b,n,&d)!=MOSAIC_OK)return 5;size_t pos=n-n/100;uint8_t repl[3]={'X','Y','Z'};memcpy(b+pos,repl,3);double a=now();if(mosaic_incremental_document_apply_edit(d,pos,3,repl,3)!=MOSAIC_OK)return 6;double inc=now()-a;uint32_t*ii=NULL,*fi=NULL;size_t in=0,fn=0;if(mosaic_incremental_document_encode(d,&ii,&in)!=MOSAIC_OK)return 7;a=now();if(mosaic_encode(m,b,n,&fi,&fn)!=MOSAIC_OK)return 8;double full=now()-a;if(in!=fn||(in&&memcmp(ii,fi,in*sizeof*ii)))return 9;size_t rp=mosaic_incremental_document_last_reprocessed_bytes(d),reuse=mosaic_incremental_document_last_reused_prefix_bytes(d);printf("bytes=%zu reprocessed=%zu reused=%zu inc=%.6f full=%.6f speedup=%.2f\n",n,rp,reuse,inc,full,inc>0?full/inc:0.0);mosaic_free(ii);mosaic_free(fi);mosaic_incremental_document_free(d);mosaic_model_free(m);free(b);return rp<n/50?0:10;}
