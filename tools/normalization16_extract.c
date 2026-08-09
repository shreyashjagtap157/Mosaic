#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/uversion.h>

static void wr16(FILE *f,uint16_t v){uint8_t b[2]={(uint8_t)v,(uint8_t)(v>>8)};if(fwrite(b,1,2,f)!=2)exit(3);}
static void wr32(FILE *f,uint32_t v){uint8_t b[4]={(uint8_t)v,(uint8_t)(v>>8),(uint8_t)(v>>16),(uint8_t)(v>>24)};if(fwrite(b,1,4,f)!=4)exit(3);}

static int normalize_cp(const UNormalizer2 *norm,UChar32 cp,UChar32 **out,int32_t *out_len){
    UErrorCode ec=U_ZERO_ERROR;UChar in[2];int32_t inlen=0;U16_APPEND_UNSAFE(in,inlen,cp);
    int32_t need=unorm2_normalize(norm,in,inlen,NULL,0,&ec);if(ec!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(ec))return 0;ec=U_ZERO_ERROR;
    UChar *buf=(UChar*)malloc((size_t)(need+1)*sizeof(UChar));if(!buf)return 0;
    int32_t got=unorm2_normalize(norm,in,inlen,buf,need+1,&ec);if(U_FAILURE(ec)){free(buf);return 0;}
    ec=U_ZERO_ERROR;int32_t n32=0;u_strToUTF32(NULL,0,&n32,buf,got,&ec);if(ec!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(ec)){free(buf);return 0;}ec=U_ZERO_ERROR;
    UChar32 *v=n32?(UChar32*)malloc((size_t)n32*sizeof(UChar32)):NULL;if(n32&&!v){free(buf);return 0;}
    u_strToUTF32(v,n32,&n32,buf,got,&ec);free(buf);if(U_FAILURE(ec)){free(v);return 0;}*out=v;*out_len=n32;return 1;
}

static int raw_decomp_pair(const UNormalizer2 *norm,UChar32 cp,UChar32 pair[2]){
    UErrorCode ec=U_ZERO_ERROR;int32_t need=unorm2_getRawDecomposition(norm,cp,NULL,0,&ec);
    if(ec==U_ZERO_ERROR&&need<0)return 0;
    if(ec!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(ec))return 0;
    ec=U_ZERO_ERROR;UChar *buf=(UChar*)malloc((size_t)(need+1)*sizeof(UChar));if(!buf)return 0;
    int32_t got=unorm2_getRawDecomposition(norm,cp,buf,need+1,&ec);if(U_FAILURE(ec)||got<0){free(buf);return 0;}
    ec=U_ZERO_ERROR;UChar32 tmp[4];int32_t n32=0;u_strToUTF32(tmp,4,&n32,buf,got,&ec);free(buf);if(U_FAILURE(ec)||n32!=2)return 0;pair[0]=tmp[0];pair[1]=tmp[1];return 1;
}

static int same_single(const UChar32 *v,int32_t n,UChar32 cp){return n==1&&v[0]==cp;}
static void rec_seq(FILE*f,uint8_t type,UChar32 cp,const UChar32*v,int32_t n){fputc(type,f);fputc(0,f);wr16(f,(uint16_t)n);wr32(f,(uint32_t)cp);for(int32_t i=0;i<n;i++)wr32(f,(uint32_t)v[i]);}

int main(void){
    UVersionInfo uv;u_getUnicodeVersion(uv);if(uv[0]!=16||uv[1]!=0){fprintf(stderr,"expected ICU Unicode 16.0, got %u.%u.%u\n",uv[0],uv[1],uv[2]);return 2;}
    UErrorCode ec=U_ZERO_ERROR;const UNormalizer2*nfd=unorm2_getNFDInstance(&ec);const UNormalizer2*nfkd=unorm2_getNFKDInstance(&ec);const UNormalizer2*nfc=unorm2_getNFCInstance(&ec);const UNormalizer2*nfkc_cf=unorm2_getNFKCCasefoldInstance(&ec);if(U_FAILURE(ec))return 2;
    fwrite("MSNEX16\0",1,8,stdout);fputc(uv[0],stdout);fputc(uv[1],stdout);fputc(uv[2],stdout);fputc(uv[3],stdout);
    for(UChar32 cp=0;cp<=0x10ffff;cp++){
        if(cp>=0xd800&&cp<=0xdfff)continue;
        uint8_t ccc=u_getCombiningClass(cp);if(ccc){fputc(1,stdout);fputc(ccc,stdout);wr16(stdout,0);wr32(stdout,(uint32_t)cp);}
        UChar32 *d=NULL,*k=NULL,*cf=NULL;int32_t dn=0,kn=0,cfn=0;
        if(!normalize_cp(nfd,cp,&d,&dn)||!normalize_cp(nfkd,cp,&k,&kn)||!normalize_cp(nfkc_cf,cp,&cf,&cfn))return 4;
        int hangul=cp>=0xac00&&cp<=0xd7a3;
        if(!hangul&&!same_single(d,dn,cp))rec_seq(stdout,2,cp,d,dn);
        if(!hangul&&!same_single(k,kn,cp))rec_seq(stdout,3,cp,k,kn);
        if(!same_single(cf,cfn,cp))rec_seq(stdout,4,cp,cf,cfn);
        if(!hangul){UChar32 pair[2];if(raw_decomp_pair(nfc,cp,pair)){UChar32 composed=unorm2_composePair(nfc,pair[0],pair[1]);if(composed==cp){fputc(5,stdout);fputc(0,stdout);wr16(stdout,0);wr32(stdout,(uint32_t)pair[0]);wr32(stdout,(uint32_t)pair[1]);wr32(stdout,(uint32_t)cp);}}}
        free(d);free(k);free(cf);
    }
    fputc(0,stdout);return ferror(stdout)?3:0;
}
