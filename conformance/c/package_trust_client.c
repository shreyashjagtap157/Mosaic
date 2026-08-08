#include <mosaic.h>
#include <mosaic_trust.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char *read_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0) { fclose(f); return NULL; }
    long n = ftell(f);
    rewind(f);
    unsigned char *p = (unsigned char *)malloc(n ? (size_t)n : 1u);
    if (!p) { fclose(f); return NULL; }
    if (n && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); fclose(f); return NULL; }
    fclose(f); *out_len = (size_t)n; return p;
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    mosaic_model *model = NULL;
    if (mosaic_model_load_file(argv[1], &model) != MOSAIC_OK) return 3;
    mosaic_model_free(model);
    size_t pack_n=0,key_n=0,sig_n=0;
    unsigned char *pack=read_all(argv[1],&pack_n),*key=read_all(argv[2],&key_n),*sig=read_all(argv[3],&sig_n);
    if (!pack || !key || !sig || key_n != 32u) return 4;
    mosaic_trust_store_config cfg; mosaic_trust_store_config_default(&cfg);
    mosaic_trust_store *store = NULL;
    if (mosaic_trust_store_create(&cfg, &store) != MOSAIC_OK) return 5;
    if (mosaic_trust_store_add_ed25519(store, key, NULL) != MOSAIC_OK) return 6;
    mosaic_pack_signature_info info;
    if (mosaic_trust_verify_pack(store, pack, pack_n, sig, sig_n, &info) != MOSAIC_OK) return 7;
    mosaic_trust_store_free(store); free(pack); free(key); free(sig); return 0;
}
