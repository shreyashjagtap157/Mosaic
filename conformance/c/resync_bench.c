#include "mosaic.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static double seconds(void){return (double)clock()/(double)CLOCKS_PER_SEC;}
int main(int argc,char **argv){
    CHECK(argc==2); mosaic_model *m=NULL; CHECK(mosaic_model_load_file(argv[1],&m)==MOSAIC_OK);
    const size_t n=10u*1024u*1024u; uint8_t *buf=(uint8_t*)malloc(n); CHECK(buf);
    const char pat[]="hello tokenizer world :: value->_id \n"; const size_t pn=sizeof pat-1u;
    for(size_t i=0;i<n;++i) buf[i]=(uint8_t)pat[i%pn];
    mosaic_resync_document *d=NULL; CHECK(mosaic_resync_document_create(m,buf,n,65536u,1048576u,&d)==MOSAIC_OK);
    const size_t pos=n/2u; const uint8_t repl[3]={'X','Y','Z'}; memcpy(buf+pos,repl,3u);
    double t0=seconds(); CHECK(mosaic_resync_document_apply_edit(d,pos,3u,repl,3u)==MOSAIC_OK); double t1=seconds();
    uint32_t *inc_ids=NULL,*full_ids=NULL; size_t inc_n=0,full_n=0;
    CHECK(mosaic_resync_document_encode(d,&inc_ids,&inc_n)==MOSAIC_OK);
    double f0=seconds(); CHECK(mosaic_encode(m,buf,n,&full_ids,&full_n)==MOSAIC_OK); double f1=seconds();
    CHECK(inc_n==full_n&&(!inc_n||memcmp(inc_ids,full_ids,inc_n*sizeof *inc_ids)==0));
    size_t rp=mosaic_resync_document_last_reprocessed_bytes(d),pre=mosaic_resync_document_last_reused_prefix_bytes(d),suf=mosaic_resync_document_last_reused_suffix_bytes(d);
    CHECK(mosaic_resync_document_last_resynchronized(d)); CHECK(rp<262144u); CHECK(pre>pos-131072u); CHECK(suf>n/3u);
    double inc=t1-t0,full=f1-f0,speed=full/(inc>1e-9?inc:1e-9);
    printf("OK resync benchmark: bytes=%zu reprocessed=%zu reused_prefix=%zu reused_suffix=%zu inc=%.6f full=%.6f speedup=%.2f\n",n,rp,pre,suf,inc,full,speed);
    mosaic_free(inc_ids); mosaic_free(full_ids); mosaic_resync_document_free(d); mosaic_model_free(m); free(buf); return 0;
}
