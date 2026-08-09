#include "mosaic.h"
#include "mosaic_trust.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *read_all(const char *path, size_t *out_len) {
    FILE *f=fopen(path,"rb"); if(!f)return NULL;
    if(fseek(f,0,SEEK_END)||ftell(f)<0){fclose(f);return NULL;}
    long n=ftell(f); if(fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=(unsigned char*)malloc(n?(size_t)n:1u); if(!p){fclose(f);return NULL;}
    if(n&&fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*out_len=(size_t)n;return p;
}

int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: %s PACK PUBKEY SIGNATURE\n",argv[0]);return 2;}
    /* Trust is authorization after structural validation, never instead of it. */
    mosaic_model *model=NULL; if(mosaic_model_load_file(argv[1],&model)!=MOSAIC_OK)return 3; mosaic_model_free(model);
    size_t pn=0,kn=0,sn=0; unsigned char *pack=read_all(argv[1],&pn),*pub=read_all(argv[2],&kn),*sig=read_all(argv[3],&sn);
    if(!pack||!pub||!sig||kn!=32||sn!=MOSAIC_TRUST_SIGNATURE_RECORD_BYTES)return 4;
    mosaic_trust_store_config cfg; mosaic_trust_store_config_default(&cfg); cfg.max_keys=1;
    mosaic_trust_store *store=NULL; if(mosaic_trust_store_create(&cfg,&store)!=MOSAIC_OK)return 5;
    mosaic_pack_signature_info info;
    if(mosaic_trust_verify_pack(store,pack,pn,sig,sn,&info)!=MOSAIC_ERROR_UNTRUSTED)return 6;
    unsigned char key_id[32]; if(mosaic_trust_store_add_ed25519(store,pub,key_id)!=MOSAIC_OK)return 7;
    unsigned char derived[32]; if(mosaic_trust_key_id_ed25519(pub,derived)!=MOSAIC_OK||memcmp(key_id,derived,32))return 8;
    if(mosaic_trust_verify_pack(store,pack,pn,sig,sn,&info)!=MOSAIC_OK||memcmp(info.key_id,key_id,32)||info.revoked)return 9;
    if(mosaic_trust_store_add_ed25519(store,pub,NULL)!=MOSAIC_OK)return 10;
    unsigned char other[32]; memset(other,0xa5,sizeof other); if(mosaic_trust_store_add_ed25519(store,other,NULL)!=MOSAIC_ERROR_RESOURCE_LIMIT)return 11;

    unsigned char *tampered=(unsigned char*)malloc(pn?pn:1u); if(!tampered)return 12; memcpy(tampered,pack,pn); if(pn)tampered[pn/2]^=1u;
    if(mosaic_trust_verify_pack(store,tampered,pn,sig,sn,&info)!=MOSAIC_ERROR_INTEGRITY) return 13;
    free(tampered);
    unsigned char badsig[160]; memcpy(badsig,sig,160); badsig[88]^=1u;
    if(mosaic_trust_verify_pack(store,pack,pn,badsig,sizeof badsig,&info)!=MOSAIC_ERROR_UNTRUSTED)return 14;
    memcpy(badsig,sig,160); badsig[152]=1u;
    if(mosaic_trust_signature_inspect(badsig,sizeof badsig,&info)!=MOSAIC_ERROR_INTEGRITY)return 15;
    if(mosaic_trust_signature_inspect(sig,sn-1,&info)!=MOSAIC_ERROR_INVALID_ARGUMENT)return 16;
    if(mosaic_trust_store_revoke(store,key_id)!=MOSAIC_OK)return 17;
    if(mosaic_trust_verify_pack(store,pack,pn,sig,sn,&info)!=MOSAIC_ERROR_REVOKED||!info.revoked)return 18;
    if(mosaic_trust_store_revoke(store,other)!=MOSAIC_ERROR_NOT_FOUND)return 19;
    mosaic_trust_store_free(store); free(pack);free(pub);free(sig);
    puts("OK Ed25519 trust: structural-first verify + unknown/tamper/revocation/resource gates");return 0;
}
