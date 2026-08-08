#include <mosaic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check(mosaic_model *model, const uint8_t *src, size_t len, const uint32_t *want, size_t wn) {
    uint32_t *ids=NULL; size_t n=0; uint8_t *decoded=NULL; size_t decoded_len=0;
    if (mosaic_encode(model,src,len,&ids,&n)!=MOSAIC_OK || n!=wn) { mosaic_free(ids); return 0; }
    for(size_t i=0;i<n;++i) if(ids[i]!=want[i]) { mosaic_free(ids); return 0; }
    if(mosaic_decode(model,ids,n,&decoded,&decoded_len)!=MOSAIC_OK || decoded_len!=len || (len&&memcmp(decoded,src,len)!=0)) { mosaic_free(ids);mosaic_free(decoded);return 0; }
    mosaic_free(ids);mosaic_free(decoded);return 1;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    mosaic_model *m=NULL;
    if(mosaic_model_load_file(argv[1],&m)!=MOSAIC_OK)return 3;
    const uint32_t a[]={258},b[]={257},c[]={258,257};
    int ok=check(m,(const uint8_t*)"abc",3,a,1)&&check(m,(const uint8_t*)"bc",2,b,1)&&check(m,(const uint8_t*)"abcbc",5,c,2);
    mosaic_model_free(m);if(!ok)return 4;puts("OK raw BPE public API");return 0;
}
