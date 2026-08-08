#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mosaic.h"

typedef struct {
    const uint8_t *bytes;
    size_t len;
} Slice;

typedef struct {
    uint32_t kind;
    uint64_t offset;
    uint64_t length;
} Section;

typedef struct {
    uint32_t token_id;
    int32_t cost;
    uint32_t surface_offset;
    uint16_t surface_len;
} VocabEntry;

typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint32_t count;
    uint32_t entries_offset;
    uint32_t id_index_offset;
    uint32_t first_index_offset;
    uint32_t blob_offset;
    uint32_t blob_len;
    uint32_t max_surface_len;
    uint32_t algorithm; /* 0=weighted Viterbi, 1=raw byte BPE */
} Vocab;

typedef struct {
    uint32_t surface_offset;
    uint16_t surface_len;
    int32_t cost_delta;
} LanguageEntry;

typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint32_t count;
    uint32_t entries_offset;
    uint32_t tag_offset;
    uint16_t tag_len;
    uint32_t blob_offset;
    uint32_t blob_len;
    uint32_t max_surface_len;
} LanguageView;

typedef struct {
    uint8_t *pack;
    size_t pack_len;
    LanguageView view;
    uint8_t hash[32];
    int64_t *adjustments;
} LanguagePack;

typedef struct {
    uint32_t tag_offset;
    uint16_t tag_len;
    int32_t min_score;
} DetectorProfile;

typedef struct {
    uint32_t surface_offset;
    uint16_t surface_len;
    uint16_t profile_index;
    int32_t weight;
} DetectorFeature;

typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint32_t profile_count;
    uint32_t feature_count;
    uint32_t profiles_offset;
    uint32_t features_offset;
    uint32_t first_index_offset;
    uint32_t blob_offset;
    uint32_t blob_len;
    uint32_t max_feature_len;
    int32_t min_margin;
} DetectorView;

typedef struct {
    size_t position;
    int reachable;
    int64_t cost;
    size_t count;
} RingState;

typedef struct {
    uint32_t *back;
    size_t input_len;
    int64_t cost;
    size_t count;
} Tokenization;

typedef struct {
    size_t start;
    size_t end;
    uint32_t entry_index;
} PathToken;

typedef struct {
    uint32_t state[8];
    uint8_t block[64];
    size_t block_len;
    uint64_t total_len;
} Sha256Ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rotr32(uint32_t x, unsigned n){return (x>>n)|(x<<(32u-n));}
static uint32_t rd32be(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void wr32be(uint8_t*p,uint32_t x){p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x;}
static void sha256_compress(Sha256Ctx*c,const uint8_t block[64]){
    uint32_t w[64];for(size_t i=0;i<16;++i)w[i]=rd32be(block+i*4);
    for(size_t i=16;i<64;++i){uint32_t s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);uint32_t s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    uint32_t a=c->state[0],b=c->state[1],cc=c->state[2],d=c->state[3],e=c->state[4],f=c->state[5],g=c->state[6],h=c->state[7];
    for(size_t i=0;i<64;++i){uint32_t s1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25);uint32_t ch=(e&f)^((~e)&g);uint32_t t1=h+s1+ch+SHA256_K[i]+w[i];uint32_t s0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22);uint32_t maj=(a&b)^(a&cc)^(b&cc);uint32_t t2=s0+maj;h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;}
    c->state[0]+=a;c->state[1]+=b;c->state[2]+=cc;c->state[3]+=d;c->state[4]+=e;c->state[5]+=f;c->state[6]+=g;c->state[7]+=h;
}
static void sha256_init(Sha256Ctx*c){*c=(Sha256Ctx){{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u},{0},0,0};}
static int sha256_update(Sha256Ctx*c,const uint8_t*data,size_t len){
    if ((uint64_t)len > UINT64_MAX - c->total_len) return 0;
    c->total_len += (uint64_t)len;
    if(c->block_len){size_t take=64-c->block_len;if(take>len)take=len;memcpy(c->block+c->block_len,data,take);c->block_len+=take;data+=take;len-=take;if(c->block_len==64){sha256_compress(c,c->block);c->block_len=0;}}
    while(len>=64){sha256_compress(c,data);data+=64;len-=64;}if(len){memcpy(c->block,data,len);c->block_len=len;}return 1;
}
static int sha256_final(Sha256Ctx*c,uint8_t out[32]){
    if (c->total_len > UINT64_MAX / 8u) return 0;
    uint64_t bits = c->total_len * 8u;
    c->block[c->block_len++] = 0x80;
    if(c->block_len>56){memset(c->block+c->block_len,0,64-c->block_len);sha256_compress(c,c->block);c->block_len=0;}
    memset(c->block+c->block_len,0,56-c->block_len);for(size_t i=0;i<8;++i)c->block[63-i]=(uint8_t)(bits>>(i*8));sha256_compress(c,c->block);
    for (size_t i = 0; i < 8; ++i) wr32be(out + i * 4, c->state[i]);
    return 1;
}
static int sha256_bytes(const uint8_t*data,size_t len,uint8_t out[32]){Sha256Ctx c;sha256_init(&c);return sha256_update(&c,data,len)&&sha256_final(&c,out);}

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}
static int32_t rdi32(const uint8_t *p) {
    uint32_t u = rd32(p);
    int32_t out;
    memcpy(&out, &u, sizeof out);
    return out;
}
static int add_size(size_t a, size_t b, size_t *out) {
    if (b > SIZE_MAX - a) return 0;
    *out = a + b;
    return 1;
}
static int mul_size(size_t a, size_t b, size_t *out) {
    if (a && b > SIZE_MAX / a) return 0;
    *out = a * b;
    return 1;
}
static int add_i64_i32(int64_t a, int32_t b, int64_t *out) {
    int64_t v = (int64_t)b;
    if ((v > 0 && a > INT64_MAX - v) || (v < 0 && a < INT64_MIN - v)) return 0;
    *out = a + v;
    return 1;
}
static int add_i64(int64_t a, int64_t b, int64_t *out) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
    *out = a + b;
    return 1;
}
static int all_zero(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) if (p[i] != 0) return 0;
    return 1;
}
static int fail(const char *msg) {

#ifndef MOSAIC_LIBRARY_ONLY
    fprintf(stderr, "mosaic-ref: %s\n", msg);
#else
    (void)msg;
#endif
    return 0;
}

static int read_file(const char *path, uint8_t **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 0; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long end = ftell(f);
    if (end < 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    size_t n = (size_t)end;
    uint8_t *buf = n ? (uint8_t *)malloc(n) : (uint8_t *)malloc(1);
    if (!buf) { fclose(f); return 0; }
    if (n && fread(buf, 1, n, f) != n) { free(buf); fclose(f); return 0; }
    fclose(f); *out = buf; *out_len = n; return 1;
}

static int sha256_canonical_pack(const uint8_t *data, size_t len, uint8_t out[32]) {
    if (len < 80) return 0;
    uint8_t *copy = (uint8_t *)malloc(len);
    if (!copy) return 0;
    memcpy(copy, data, len);
    memset(copy + 48, 0, 32);
    if (!sha256_bytes(copy, len, out)) { free(copy); return 0; }
    free(copy); return 1;
}

static int parse_outer(const uint8_t *data, size_t len, Section **sections_out, uint32_t *count_out,
                       uint32_t *manifest_index, uint32_t *lock_index) {
    if (len < 96) return fail("pack too short");
    if (memcmp(data, "MOSPACK\0", 8) != 0) return fail("invalid pack magic");
    if (rd16(data + 8) != 1 || rd16(data + 12) != 96 || rd16(data + 14) != 0) return fail("unsupported pack header");
    if (rd64(data + 16) != (uint64_t)len) return fail("pack length mismatch");
    uint32_t count = rd32(data + 24);
    if (count > 4096) return fail("section count exceeds reference limit");
    if (rd16(data + 28) != 32 || rd16(data + 30) != 1) return fail("unsupported section/hash profile");
    uint64_t dir64 = rd64(data + 32);
    if (dir64 > SIZE_MAX) return fail("section directory too large");
    size_t dir = (size_t)dir64;
    size_t dir_bytes;
    if (!mul_size((size_t)count, 32, &dir_bytes)) return fail("section directory overflow");
    size_t dir_end;
    if (!add_size(dir, dir_bytes, &dir_end)) return fail("invalid section directory");
    if (dir < 96 || dir_end > len || (dir & 7)) return fail("invalid section directory");
    if (!all_zero(data + 80, 16) || !all_zero(data + 96, dir - 96)) return fail("noncanonical header padding");
    *manifest_index = rd32(data + 40); *lock_index = rd32(data + 44);
    if (*manifest_index >= count || *lock_index >= count) return fail("special section index out of bounds");
    uint8_t actual[32];
    if (!sha256_canonical_pack(data, len, actual) || memcmp(actual, data + 48, 32) != 0) return fail("pack content hash mismatch");

    Section *sections = count ? (Section *)calloc(count, sizeof *sections) : NULL;
    if (count && !sections) return fail("out of memory");
    size_t prev_end = dir_end;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *e = data + dir + (size_t)i * 32;
        uint32_t kind = rd32(e), flags = rd32(e + 4);
        uint64_t off64 = rd64(e + 8), n64 = rd64(e + 16);
        uint16_t element_width = rd16(e + 28); uint8_t align_log2 = e[30]; uint8_t reserved = e[31];
        if (!kind || flags || reserved || align_log2 > 20) { free(sections); return fail("invalid section descriptor"); }
        if (off64 > SIZE_MAX || n64 > SIZE_MAX) { free(sections); return fail("section exceeds platform size"); }
        size_t off = (size_t)off64, n = (size_t)n64, end;
        size_t alignment = (size_t)1 << align_log2;
        if ((off & (alignment - 1)) || off < prev_end || !add_size(off, n, &end) || end > len) { free(sections); return fail("section bounds/order invalid"); }
        if (!all_zero(data + prev_end, off - prev_end)) { free(sections); return fail("noncanonical section padding"); }
        if (element_width) {
            uint64_t min_len = (uint64_t)rd32(e + 24) * element_width;
            if (min_len > n64) { free(sections); return fail("invalid element layout"); }
        }
        sections[i].kind = kind; sections[i].offset = off64; sections[i].length = n64; prev_end = end;
    }
    if (prev_end != len) { free(sections); return fail("trailing bytes outside declared sections"); }
    if (sections[*manifest_index].kind != 1 || sections[*lock_index].kind != 2) { free(sections); return fail("special section kind mismatch"); }
    *sections_out = sections; *count_out = count; return 1;
}

static Slice sec_slice(const uint8_t *data, const Section *s) {
    Slice out = { data + (size_t)s->offset, (size_t)s->length }; return out;
}

static int validate_manifest_lock(Slice manifest, Slice lock) {
    if (manifest.len != 64 || memcmp(manifest.bytes, "MSMF", 4) != 0 || rd16(manifest.bytes + 4) != 1 || rd16(manifest.bytes + 6) != 0)
        return fail("invalid manifest");
    if (rd32(manifest.bytes + 28) != 0) return fail("manifest reserved field nonzero");
    if (lock.len < 16 || memcmp(lock.bytes, "MSLK", 4) != 0 || rd16(lock.bytes + 4) != 1 || rd16(lock.bytes + 6) != 48 || rd32(lock.bytes + 12) != 0)
        return fail("invalid lock graph");
    uint32_t deps = rd32(lock.bytes + 8);
    if (deps != 0) return fail("C M3 conformance CLI currently requires self-contained model pack");
    if (lock.len != 16) return fail("unexpected lock payload");
    uint8_t digest[32]; if (!sha256_bytes(lock.bytes, lock.len, digest)) return fail("sha256 failure");
    if (memcmp(digest, manifest.bytes + 32, 32) != 0) return fail("manifest lock hash mismatch");
    return 1;
}

static int vocab_entry(const Vocab *v, uint32_t index, VocabEntry *out, Slice *surface) {
    if (index >= v->count) return 0;
    const uint8_t *e = v->section + v->entries_offset + (size_t)index * 16;
    out->token_id = rd32(e); out->cost = rdi32(e + 4); out->surface_offset = rd32(e + 8); out->surface_len = rd16(e + 12);
    if (rd16(e + 14) != 0 || !out->surface_len || out->surface_offset > v->blob_len || out->surface_len > v->blob_len - out->surface_offset) return 0;
    surface->bytes = v->section + v->blob_offset + out->surface_offset; surface->len = out->surface_len; return 1;
}

static uint32_t vocab_id_index(const Vocab *v, uint32_t pos) {
    return rd32(v->section + v->id_index_offset + (size_t)pos * 4);
}
static uint32_t vocab_first(const Vocab *v, uint32_t pos) {
    return rd32(v->section + v->first_index_offset + (size_t)pos * 4);
}

static int compare_i32(const void *a, const void *b) {
    int32_t av = *(const int32_t *)a, bv = *(const int32_t *)b;
    return av < bv ? -1 : av > bv ? 1 : 0;
}

static int parse_vocab(Slice s, Vocab *v) {
    if (s.len < 40 || memcmp(s.bytes, "MSVC", 4) != 0 || rd16(s.bytes + 4) != 1 || rd16(s.bytes + 6) != 0) return fail("invalid vocabulary header");
    uint32_t count = rd32(s.bytes + 8); if (count < 256 || count > 1048576) return fail("vocabulary count outside limits");
    if (rd16(s.bytes + 12) != 16 || rd16(s.bytes + 14) != 0 || rd32(s.bytes + 16) != 40) return fail("invalid vocabulary layout");
    uint32_t algorithm = rd32(s.bytes + 36);
    if (algorithm > 1u) return fail("unsupported vocabulary algorithm");
    uint32_t entries = rd32(s.bytes + 16), ids = rd32(s.bytes + 20), first = rd32(s.bytes + 24), blob = rd32(s.bytes + 28), blob_len = rd32(s.bytes + 32);
    uint64_t entries_end = (uint64_t)entries + (uint64_t)count * 16;
    uint64_t ids_end = (uint64_t)ids + (uint64_t)count * 4;
    uint64_t first_end = (uint64_t)first + 257u * 4u;
    uint64_t blob_end = (uint64_t)blob + blob_len;
    if (!(entries_end <= ids && ids_end <= first && first_end <= blob && blob_end == s.len)) return fail("vocabulary regions overlap/out of bounds");
    if (!all_zero(s.bytes + entries_end, ids - (size_t)entries_end) || !all_zero(s.bytes + ids_end, first - (size_t)ids_end) || !all_zero(s.bytes + first_end, blob - (size_t)first_end)) return fail("vocabulary padding nonzero");
    *v = (Vocab){s.bytes, s.len, count, entries, ids, first, blob, blob_len, 0, algorithm};

    Slice prev_surface = {0}; uint32_t prev_id_for_surface = 0;
    for (uint32_t i = 0; i < count; ++i) {
        VocabEntry e; Slice surf; if (!vocab_entry(v, i, &e, &surf)) return fail("invalid vocabulary entry");
        if (surf.len > v->max_surface_len) v->max_surface_len = (uint32_t)surf.len;
        if (i) {
            size_t min = prev_surface.len < surf.len ? prev_surface.len : surf.len;
            int cmp = memcmp(prev_surface.bytes, surf.bytes, min);
            if (!cmp) cmp = prev_surface.len < surf.len ? -1 : prev_surface.len > surf.len ? 1 : 0;
            if (cmp > 0 || (cmp == 0 && prev_id_for_surface >= e.token_id)) return fail("vocabulary surface order noncanonical");
        }
        prev_surface = surf; prev_id_for_surface = e.token_id;
    }
    uint32_t prev_token_id = 0;
    for (uint32_t p = 0; p < count; ++p) {
        uint32_t idx = vocab_id_index(v, p); if (idx >= count) return fail("vocabulary id index out of bounds");
        VocabEntry e; Slice surf; if (!vocab_entry(v, idx, &e, &surf)) return fail("invalid id-index entry");
        if (p && prev_token_id >= e.token_id) return fail("token IDs not strictly increasing");
        prev_token_id = e.token_id;
    }
    if (vocab_first(v, 0) != 0 || vocab_first(v, 256) != count) return fail("invalid first-byte index endpoints");
    for (uint32_t b = 0; b < 256; ++b) {
        uint32_t a = vocab_first(v, b), z = vocab_first(v, b + 1); if (a > z || z > count) return fail("invalid first-byte index range");
        for (uint32_t i = a; i < z; ++i) { VocabEntry e; Slice surf; if (!vocab_entry(v, i, &e, &surf) || surf.bytes[0] != (uint8_t)b) return fail("first-byte index points to wrong surface"); }
    }
    for (uint32_t byte = 0; byte < 256; ++byte) {
        uint32_t a = vocab_first(v, byte), z = vocab_first(v, byte + 1), found = 0;
        for (uint32_t i = a; i < z; ++i) {
            VocabEntry e; Slice surf;
            if (!vocab_entry(v, i, &e, &surf)) return fail("invalid byte fallback candidate");
            if (surf.len == 1 && surf.bytes[0] == (uint8_t)byte) ++found;
        }
        if (!found) return fail("missing byte fallback");
    }
    if (v->algorithm == 1u) {
        /* Raw BPE requires one rank per unique byte surface. */
        Slice previous = {0};
        for (uint32_t i = 0; i < count; ++i) {
            VocabEntry e; Slice surf; if (!vocab_entry(v, i, &e, &surf)) return fail("invalid BPE entry");
            if (i && previous.len == surf.len && memcmp(previous.bytes, surf.bytes, surf.len) == 0) return fail("duplicate BPE surface");
            previous = surf;
        }
        /* Raw BPE ranks must be unique so merge priority is a total order independent of storage order. */
        int32_t *ranks = (int32_t *)malloc((size_t)count * sizeof *ranks);
        if (!ranks) return fail("out of memory validating BPE ranks");
        for (uint32_t i = 0; i < count; ++i) {
            VocabEntry e; Slice surf;
            if (!vocab_entry(v, i, &e, &surf) || e.cost < 0) { free(ranks); return fail("invalid BPE rank"); }
            ranks[i] = e.cost;
        }
        qsort(ranks, count, sizeof *ranks, compare_i32);
        for (uint32_t i = 1; i < count; ++i) if (ranks[i - 1] == ranks[i]) { free(ranks); return fail("BPE ranks must be unique"); }
        free(ranks);
    }
    return 1;
}

static int load_model_pack(const uint8_t *data, size_t len, Vocab *v) {
    Section *sections = NULL; uint32_t count = 0, manifest_i = 0, lock_i = 0;
    if (!parse_outer(data, len, &sections, &count, &manifest_i, &lock_i)) return 0;
    int vocab_count = 0; Section vocab_section = {0};
    for (uint32_t i = 0; i < count; ++i) if (sections[i].kind == 4) { ++vocab_count; vocab_section = sections[i]; }
    if (vocab_count != 1) { free(sections); return fail("expected exactly one vocabulary section"); }
    int ok = validate_manifest_lock(sec_slice(data, &sections[manifest_i]), sec_slice(data, &sections[lock_i])) && parse_vocab(sec_slice(data, &vocab_section), v);
    free(sections); return ok;
}

static int language_entry(const LanguageView *v, uint32_t index, LanguageEntry *out, Slice *surface) {
    if (index >= v->count) return 0;
    const uint8_t *e = v->section + v->entries_offset + (size_t)index * 16;
    out->surface_offset = rd32(e);
    out->surface_len = rd16(e + 4);
    if (rd16(e + 6) != 0 || rd32(e + 12) != 0 || !out->surface_len ||
        out->surface_offset > v->blob_len || out->surface_len > v->blob_len - out->surface_offset) return 0;
    out->cost_delta = rdi32(e + 8);
    surface->bytes = v->section + v->blob_offset + out->surface_offset;
    surface->len = out->surface_len;
    return 1;
}

static int language_tag_valid(const uint8_t *tag, size_t len) {
    if (!len || len > 63) return 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = tag[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '-';
        if (!ok) return 0;
    }
    return 1;
}

static int compare_slices(Slice a, Slice b) {
    size_t min = a.len < b.len ? a.len : b.len;
    int cmp = min ? memcmp(a.bytes, b.bytes, min) : 0;
    if (cmp) return cmp;
    return a.len < b.len ? -1 : a.len > b.len ? 1 : 0;
}

static int parse_language(Slice s, LanguageView *v) {
    if (s.len < 40 || memcmp(s.bytes, "MSLG", 4) != 0 || rd16(s.bytes + 4) != 1 || rd16(s.bytes + 6) != 0)
        return fail("invalid language header");
    uint32_t count = rd32(s.bytes + 8);
    uint16_t width = rd16(s.bytes + 12), tag_len = rd16(s.bytes + 14);
    uint32_t entries = rd32(s.bytes + 16), tag = rd32(s.bytes + 20), blob = rd32(s.bytes + 24), blob_len = rd32(s.bytes + 28);
    uint32_t max_surface = rd32(s.bytes + 32);
    if (width != 16 || rd32(s.bytes + 36) != 0 || entries != 40 || count > 1048576 || !tag_len || tag_len > 63)
        return fail("invalid language layout");
    uint64_t entries_end = (uint64_t)entries + (uint64_t)count * 16u;
    uint64_t tag_end = (uint64_t)tag + tag_len;
    uint64_t blob_end = (uint64_t)blob + blob_len;
    if (!(entries_end <= tag && tag_end <= blob && blob_end == s.len)) return fail("language regions overlap/out of bounds");
    if (!all_zero(s.bytes + (size_t)entries_end, tag - (size_t)entries_end) ||
        !all_zero(s.bytes + (size_t)tag_end, blob - (size_t)tag_end)) return fail("language padding nonzero");
    if (!language_tag_valid(s.bytes + tag, tag_len)) return fail("invalid language tag");
    *v = (LanguageView){s.bytes, s.len, count, entries, tag, tag_len, blob, blob_len, max_surface};
    Slice previous = {0};
    uint32_t observed_max = 0;
    for (uint32_t i = 0; i < count; ++i) {
        LanguageEntry entry;
        Slice surface;
        if (!language_entry(v, i, &entry, &surface)) return fail("invalid language entry");
        if (entry.cost_delta == INT32_MIN) return fail("language cost delta outside policy");
        if (surface.len > observed_max) observed_max = (uint32_t)surface.len;
        if (i && compare_slices(previous, surface) >= 0) return fail("language surfaces not strictly canonical");
        previous = surface;
    }
    if (observed_max != max_surface) return fail("language maximum surface length mismatch");
    return 1;
}

static int load_language_pack(const uint8_t *data, size_t len, LanguageView *v) {
    Section *sections = NULL; uint32_t count = 0, manifest_i = 0, lock_i = 0;
    if (!parse_outer(data, len, &sections, &count, &manifest_i, &lock_i)) return 0;
    int language_count = 0; Section language_section = {0};
    for (uint32_t i = 0; i < count; ++i) if (sections[i].kind == 5) { ++language_count; language_section = sections[i]; }
    if (language_count != 1) { free(sections); return fail("expected exactly one language section"); }
    int ok = validate_manifest_lock(sec_slice(data, &sections[manifest_i]), sec_slice(data, &sections[lock_i])) &&
             parse_language(sec_slice(data, &language_section), v);
    free(sections);
    return ok;
}

static int language_surface_delta(const LanguageView *v, Slice surface, int32_t *out) {
    uint32_t lo = 0, hi = v->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        LanguageEntry entry;
        Slice candidate;
        if (!language_entry(v, mid, &entry, &candidate)) return 0;
        int cmp = compare_slices(candidate, surface);
        if (cmp < 0) lo = mid + 1;
        else if (cmp > 0) hi = mid;
        else { *out = entry.cost_delta; return 1; }
    }
    *out = 0;
    return 1;
}

static int detector_profile(const DetectorView *v, uint32_t index, DetectorProfile *out, Slice *tag) {
    if (index >= v->profile_count) return 0;
    const uint8_t *e = v->section + v->profiles_offset + (size_t)index * 16u;
    out->tag_offset = rd32(e);
    out->tag_len = rd16(e + 4);
    if (rd16(e + 6) != 0 || rd32(e + 12) != 0 || !out->tag_len || out->tag_len > 63 ||
        out->tag_offset > v->blob_len || out->tag_len > v->blob_len - out->tag_offset) return 0;
    out->min_score = rdi32(e + 8);
    if (out->min_score < 0) return 0;
    tag->bytes = v->section + v->blob_offset + out->tag_offset;
    tag->len = out->tag_len;
    return language_tag_valid(tag->bytes, tag->len);
}

static int detector_feature(const DetectorView *v, uint32_t index, DetectorFeature *out, Slice *surface) {
    if (index >= v->feature_count) return 0;
    const uint8_t *e = v->section + v->features_offset + (size_t)index * 16u;
    out->surface_offset = rd32(e);
    out->surface_len = rd16(e + 4);
    out->profile_index = rd16(e + 6);
    out->weight = rdi32(e + 8);
    if (rd32(e + 12) != 0 || !out->surface_len || out->profile_index >= v->profile_count || out->weight <= 0 ||
        out->surface_offset > v->blob_len || out->surface_len > v->blob_len - out->surface_offset) return 0;
    surface->bytes = v->section + v->blob_offset + out->surface_offset;
    surface->len = out->surface_len;
    return 1;
}

static uint32_t detector_first(const DetectorView *v, uint32_t index) {
    return rd32(v->section + v->first_index_offset + (size_t)index * 4u);
}

static int parse_detector(Slice s, DetectorView *v) {
    if (s.len < 48 || memcmp(s.bytes, "MSDT", 4) != 0 || rd16(s.bytes + 4) != 1 || rd16(s.bytes + 6) != 0)
        return fail("invalid detector header");
    uint32_t profiles = rd32(s.bytes + 8), features = rd32(s.bytes + 12);
    uint16_t profile_width = rd16(s.bytes + 16), feature_width = rd16(s.bytes + 18);
    uint32_t profiles_off = rd32(s.bytes + 20), features_off = rd32(s.bytes + 24), first_off = rd32(s.bytes + 28);
    uint32_t blob_off = rd32(s.bytes + 32), blob_len = rd32(s.bytes + 36), max_feature = rd32(s.bytes + 40);
    int32_t min_margin = rdi32(s.bytes + 44);
    if (!profiles || profiles > 256 || features > 65536 || profile_width != 16 || feature_width != 16 ||
        profiles_off != 48 || min_margin < 0) return fail("invalid detector layout");
    uint64_t profiles_end = (uint64_t)profiles_off + (uint64_t)profiles * 16u;
    uint64_t features_end = (uint64_t)features_off + (uint64_t)features * 16u;
    uint64_t first_end = (uint64_t)first_off + 257u * 4u;
    uint64_t blob_end = (uint64_t)blob_off + blob_len;
    if (!(profiles_end <= features_off && features_end <= first_off && first_end <= blob_off && blob_end == s.len))
        return fail("detector regions overlap/out of bounds");
    if (!all_zero(s.bytes + (size_t)profiles_end, features_off - (size_t)profiles_end) ||
        !all_zero(s.bytes + (size_t)features_end, first_off - (size_t)features_end) ||
        !all_zero(s.bytes + (size_t)first_end, blob_off - (size_t)first_end)) return fail("detector padding nonzero");
    *v = (DetectorView){s.bytes, s.len, profiles, features, profiles_off, features_off, first_off,
                        blob_off, blob_len, max_feature, min_margin};

    Slice previous_tag = {0};
    for (uint32_t i = 0; i < profiles; ++i) {
        DetectorProfile profile; Slice tag;
        if (!detector_profile(v, i, &profile, &tag)) return fail("invalid detector profile");
        if (i && compare_slices(previous_tag, tag) >= 0) return fail("detector profile tags not canonical");
        previous_tag = tag;
    }
    uint32_t observed_max = 0;
    Slice previous_surface = {0}; uint16_t previous_profile = 0;
    for (uint32_t i = 0; i < features; ++i) {
        DetectorFeature feature; Slice surface;
        if (!detector_feature(v, i, &feature, &surface)) return fail("invalid detector feature");
        if (surface.len > observed_max) observed_max = (uint32_t)surface.len;
        if (i) {
            int cmp = compare_slices(previous_surface, surface);
            if (cmp > 0 || (cmp == 0 && previous_profile >= feature.profile_index))
                return fail("detector features not canonical");
        }
        previous_surface = surface; previous_profile = feature.profile_index;
    }
    if (observed_max != max_feature) return fail("detector maximum feature length mismatch");
    if (detector_first(v, 0) != 0 || detector_first(v, 256) != features) return fail("invalid detector first-byte endpoints");
    for (uint32_t b = 0; b < 256; ++b) {
        uint32_t a = detector_first(v, b), z = detector_first(v, b + 1);
        if (a > z || z > features) return fail("invalid detector first-byte range");
        for (uint32_t i = a; i < z; ++i) {
            DetectorFeature feature; Slice surface;
            if (!detector_feature(v, i, &feature, &surface) || surface.bytes[0] != (uint8_t)b)
                return fail("detector first-byte index points to wrong feature");
        }
    }
    return 1;
}

static int load_detector_pack(const uint8_t *data, size_t len, DetectorView *v) {
    Section *sections = NULL; uint32_t count = 0, manifest_i = 0, lock_i = 0;
    if (!parse_outer(data, len, &sections, &count, &manifest_i, &lock_i)) return 0;
    int detector_count = 0; Section detector_section = {0};
    for (uint32_t i = 0; i < count; ++i) if (sections[i].kind == 6) { ++detector_count; detector_section = sections[i]; }
    if (detector_count != 1) { free(sections); return fail("expected exactly one detector section"); }
    int ok = validate_manifest_lock(sec_slice(data, &sections[manifest_i]), sec_slice(data, &sections[lock_i])) &&
             parse_detector(sec_slice(data, &detector_section), v);
    free(sections); return ok;
}

static int detector_detect_view(const DetectorView *v, Slice input, mosaic_detection *out) {
    if (!v || !out) return 0;
    memset(out, 0, sizeof *out);
    int64_t scores[256] = {0};
    for (size_t pos = 0; pos < input.len; ++pos) {
        uint32_t a = detector_first(v, input.bytes[pos]), z = detector_first(v, (uint32_t)input.bytes[pos] + 1u);
        for (uint32_t i = a; i < z; ++i) {
            DetectorFeature feature; Slice surface;
            if (!detector_feature(v, i, &feature, &surface)) return 0;
            if (surface.len <= input.len - pos && memcmp(input.bytes + pos, surface.bytes, surface.len) == 0) {
                if (!add_i64_i32(scores[feature.profile_index], feature.weight, &scores[feature.profile_index])) return 0;
            }
        }
    }
    uint32_t best = 0, second = UINT32_MAX;
    for (uint32_t i = 1; i < v->profile_count; ++i) {
        if (scores[i] > scores[best]) { second = best; best = i; }
        else if (second == UINT32_MAX || scores[i] > scores[second]) second = i;
    }
    int64_t second_score = second == UINT32_MAX ? 0 : scores[second];
    int64_t margin;
    if (!add_i64(scores[best], -second_score, &margin)) return 0;
    DetectorProfile profile; Slice tag;
    if (!detector_profile(v, best, &profile, &tag)) return 0;
    out->score = scores[best]; out->margin = margin;
    if (scores[best] >= profile.min_score && margin >= v->min_margin) {
        out->matched = 1u;
        memcpy(out->language, tag.bytes, tag.len);
        out->language[tag.len] = '\0';
    }
    return 1;
}

static int compare_path_tokens(const Vocab *v, const PathToken *a, size_t an, const PathToken *b, size_t bn) {
    if (an != bn) return an < bn ? -1 : 1;
    for (size_t i = 0; i < an; ++i) {
        if (a[i].start == b[i].start && a[i].end == b[i].end && a[i].entry_index == b[i].entry_index) continue;
        size_t alen = a[i].end - a[i].start, blen = b[i].end - b[i].start;
        if (alen != blen) return alen > blen ? -1 : 1;
        VocabEntry ae, be; Slice as, bs;
        if (!vocab_entry(v, a[i].entry_index, &ae, &as) || !vocab_entry(v, b[i].entry_index, &be, &bs)) return 0;
        (void)as; (void)bs;
        if (ae.token_id != be.token_id) return ae.token_id < be.token_id ? -1 : 1;
        if (a[i].start != b[i].start) return a[i].start < b[i].start ? -1 : 1;
        if (a[i].end != b[i].end) return a[i].end < b[i].end ? -1 : 1;
        if (a[i].entry_index != b[i].entry_index) return a[i].entry_index < b[i].entry_index ? -1 : 1;
    }
    return 0;
}

static size_t build_path_back(const Vocab *v, const uint32_t *back, size_t base_end, size_t final_end,
                              uint32_t extra_entry, PathToken *out, size_t cap) {
    size_t pos = cap;
    if (extra_entry != UINT32_MAX) {
        if (!pos) return SIZE_MAX;
        out[--pos] = (PathToken){base_end, final_end, extra_entry};
    }
    size_t cursor = base_end;
    while (cursor) {
        uint32_t entry_index = back[cursor];
        VocabEntry e; Slice surf;
        if (entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surf) || surf.len > cursor || !pos) return SIZE_MAX;
        size_t previous = cursor - surf.len;
        out[--pos] = (PathToken){previous, cursor, entry_index};
        cursor = previous;
    }
    return pos == 0 ? cap : SIZE_MAX;
}

static int candidate_better(const Vocab *v, const uint32_t *back, const RingState *current,
                            size_t start, size_t end, uint32_t entry_index,
                            int64_t cost, size_t count) {
    if (!current->reachable) return 1;
    if (cost != current->cost) return cost < current->cost;
    if (count != current->count) return count < current->count;
    if (count > SIZE_MAX / sizeof(PathToken)) return 0;
    PathToken *a = count ? (PathToken *)malloc(count * sizeof *a) : NULL;
    PathToken *b = count ? (PathToken *)malloc(count * sizeof *b) : NULL;
    if (count && (!a || !b)) { free(a); free(b); return 0; }
    size_t an = build_path_back(v, back, start, end, entry_index, a, count);
    size_t bn = build_path_back(v, back, end, end, UINT32_MAX, b, count);
    int result = an != SIZE_MAX && bn != SIZE_MAX && compare_path_tokens(v, a, an, b, bn) < 0;
    free(a); free(b); return result;
}


static int slice_compare_bytes(Slice a, Slice b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int cmp = n ? memcmp(a.bytes, b.bytes, n) : 0;
    if (cmp) return cmp;
    return a.len < b.len ? -1 : a.len > b.len ? 1 : 0;
}

static int vocab_find_surface(const Vocab *v, Slice target, uint32_t *out_index) {
    uint32_t lo = 0, hi = v->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        VocabEntry e; Slice surf;
        if (!vocab_entry(v, mid, &e, &surf)) return 0;
        int cmp = slice_compare_bytes(surf, target);
        if (cmp < 0) lo = mid + 1; else hi = mid;
    }
    if (lo >= v->count) return 0;
    VocabEntry e; Slice surf;
    if (!vocab_entry(v, lo, &e, &surf) || slice_compare_bytes(surf, target) != 0) return 0;
    *out_index = lo;
    return 1;
}

typedef struct {
    size_t start, end, prev, next;
    uint32_t entry_index, generation;
    uint8_t alive;
} BpeNode;

typedef struct {
    int64_t rank;
    size_t left, right;
    uint32_t left_generation, right_generation, entry_index, token_id;
} BpePair;

typedef struct { BpePair *items; size_t len, cap; } BpeHeap;

static int bpe_pair_less(const BpePair *a, const BpePair *b) {
    if (a->rank != b->rank) return a->rank < b->rank;
    if (a->left != b->left) return a->left < b->left;
    if (a->token_id != b->token_id) return a->token_id < b->token_id;
    return a->entry_index < b->entry_index;
}

static int bpe_heap_push(BpeHeap *h, BpePair value) {
    if (h->len == h->cap) {
        size_t cap = h->cap ? h->cap * 2 : 64;
        if (cap < h->cap || cap > SIZE_MAX / sizeof *h->items) return 0;
        BpePair *next = (BpePair *)realloc(h->items, cap * sizeof *next);
        if (!next) return 0;
        h->items = next; h->cap = cap;
    }
    size_t i = h->len++; h->items[i] = value;
    while (i) { size_t p = (i - 1) / 2; if (!bpe_pair_less(&h->items[i], &h->items[p])) break; BpePair t=h->items[i];h->items[i]=h->items[p];h->items[p]=t;i=p; }
    return 1;
}

static int bpe_heap_pop(BpeHeap *h, BpePair *out) {
    if (!h->len) return 0;
    *out = h->items[0];
    h->items[0] = h->items[--h->len];
    size_t i = 0;
    for (;;) { size_t l=i*2+1,r=l+1,b=i; if(l<h->len&&bpe_pair_less(&h->items[l],&h->items[b]))b=l;if(r<h->len&&bpe_pair_less(&h->items[r],&h->items[b]))b=r;if(b==i)break;BpePair t=h->items[i];h->items[i]=h->items[b];h->items[b]=t;i=b; }
    return 1;
}

static int bpe_push_neighbor(const Vocab *v, Slice input, const int64_t *adjustments,
                             BpeNode *nodes, size_t left, BpeHeap *heap) {
    if (left == SIZE_MAX || !nodes[left].alive || nodes[left].next == SIZE_MAX) return 1;
    size_t right = nodes[left].next;
    if (!nodes[right].alive || nodes[left].end != nodes[right].start) return 0;
    size_t len = nodes[right].end - nodes[left].start;
    if (!len || len > v->max_surface_len) return 1;
    uint32_t entry_index;
    if (!vocab_find_surface(v, (Slice){input.bytes + nodes[left].start, len}, &entry_index)) return 1;
    VocabEntry e; Slice surf; if (!vocab_entry(v, entry_index, &e, &surf)) return 0;
    int64_t rank = e.cost;
    if (adjustments && !add_i64(rank, adjustments[entry_index], &rank)) return 0;
    return bpe_heap_push(heap, (BpePair){rank,left,right,nodes[left].generation,nodes[right].generation,entry_index,e.token_id});
}

static int tokenize_bpe_with_adjustments(const Vocab *v, Slice input, const int64_t *adjustments, Tokenization *out) {
    if (input.len == SIZE_MAX || input.len > SIZE_MAX / sizeof(uint32_t) - 1 || input.len > SIZE_MAX / sizeof(BpeNode)) return fail("input too large");
    uint32_t *back = (uint32_t *)malloc((input.len + 1) * sizeof *back);
    if (!back) return fail("out of memory");
    memset(back, 0xff, (input.len + 1) * sizeof *back);
    if (!input.len) { *out=(Tokenization){back,0,0,0}; return 1; }
    BpeNode *nodes=(BpeNode *)calloc(input.len,sizeof *nodes); BpeHeap heap={0};
    if(!nodes){free(back);return fail("out of memory");}
    for(size_t i=0;i<input.len;++i){
        uint32_t idx; if(!vocab_find_surface(v,(Slice){input.bytes+i,1},&idx)){free(nodes);free(back);return fail("missing byte fallback");}
        nodes[i]=(BpeNode){i,i+1,i?i-1:SIZE_MAX,i+1<input.len?i+1:SIZE_MAX,idx,1u,1u};
    }
    for(size_t i=0;i+1<input.len;++i) if(!bpe_push_neighbor(v,input,adjustments,nodes,i,&heap)){free(heap.items);free(nodes);free(back);return fail("BPE candidate failure");}
    BpePair pair;
    while(bpe_heap_pop(&heap,&pair)){
        BpeNode *l=&nodes[pair.left],*r=&nodes[pair.right];
        if(!l->alive||!r->alive||l->generation!=pair.left_generation||r->generation!=pair.right_generation||l->next!=pair.right||r->prev!=pair.left)continue;
        size_t prev=l->prev,next=r->next;
        l->end=r->end;l->entry_index=pair.entry_index;++l->generation;l->next=next;
        r->alive=0;++r->generation;
        if(next!=SIZE_MAX){nodes[next].prev=pair.left;++nodes[next].generation;}
        if(prev!=SIZE_MAX&&!bpe_push_neighbor(v,input,adjustments,nodes,prev,&heap)){free(heap.items);free(nodes);free(back);return fail("BPE candidate failure");}
        if(!bpe_push_neighbor(v,input,adjustments,nodes,pair.left,&heap)){free(heap.items);free(nodes);free(back);return fail("BPE candidate failure");}
    }
    free(heap.items);
    size_t count=0;int64_t total=0;size_t cur=0;
    while(cur!=SIZE_MAX){BpeNode *n=&nodes[cur];if(!n->alive){free(nodes);free(back);return fail("invalid BPE survivor chain");}back[n->end]=n->entry_index;VocabEntry e;Slice surf;if(!vocab_entry(v,n->entry_index,&e,&surf)){free(nodes);free(back);return fail("invalid BPE result");}int64_t cost=e.cost;if(adjustments&&!add_i64(cost,adjustments[n->entry_index],&cost)){free(nodes);free(back);return fail("BPE cost overflow");}if(!add_i64(total,cost,&total)){free(nodes);free(back);return fail("BPE cost overflow");}++count;cur=n->next;}
    free(nodes);*out=(Tokenization){back,input.len,total,count};return 1;
}

static int tokenize_viterbi_with_adjustments(const Vocab *v, Slice input, const int64_t *adjustments, Tokenization *out) {
    if (!v->max_surface_len) return fail("vocabulary has no surfaces");
    if (input.len == SIZE_MAX || input.len > SIZE_MAX / sizeof(uint32_t) - 1) return fail("input too large");
    uint32_t *back = (uint32_t *)malloc((input.len + 1) * sizeof *back);
    if (!back) return fail("out of memory");
    memset(back, 0xff, (input.len + 1) * sizeof *back);
    size_t ring_size = (size_t)v->max_surface_len + 1;
    if (!ring_size || ring_size > SIZE_MAX / sizeof(RingState)) { free(back); return fail("token window too large"); }
    RingState *ring = (RingState *)calloc(ring_size, sizeof *ring);
    if (!ring) { free(back); return fail("out of memory"); }
    ring[0] = (RingState){0, 1, 0, 0};

    for (size_t start = 0; start < input.len; ++start) {
        RingState *source_state = &ring[start % ring_size];
        if (source_state->position != start || !source_state->reachable) {
            free(ring); free(back); return fail("unreachable input despite byte fallback");
        }
        int64_t source_cost = source_state->cost;
        size_t source_count = source_state->count;
        uint8_t byte = input.bytes[start];
        uint32_t a = vocab_first(v, byte), z = vocab_first(v, (uint32_t)byte + 1);
        for (uint32_t i = a; i < z; ++i) {
            VocabEntry e; Slice surf;
            if (!vocab_entry(v, i, &e, &surf)) { free(ring); free(back); return fail("vocabulary changed after validation"); }
            if (surf.len > input.len - start || memcmp(input.bytes + start, surf.bytes, surf.len) != 0) continue;
            size_t end = start + surf.len; int64_t total;
            if (!add_i64_i32(source_cost, e.cost, &total)) { free(ring); free(back); return fail("path cost overflow"); }
            if (adjustments && !add_i64(total, adjustments[i], &total)) {
                free(ring); free(back); return fail("language-adjusted path cost overflow");
            }
            if (source_count == SIZE_MAX) { free(ring); free(back); return fail("token count overflow"); }
            size_t count = source_count + 1;
            RingState *destination = &ring[end % ring_size];
            if (destination->position != end) *destination = (RingState){end, 0, 0, 0};
            if (candidate_better(v, back, destination, start, end, i, total, count)) {
                *destination = (RingState){end, 1, total, count};
                back[end] = i;
            }
        }
    }
    RingState *terminal = &ring[input.len % ring_size];
    if (terminal->position != input.len || !terminal->reachable) { free(ring); free(back); return fail("unreachable input despite byte fallback"); }
    *out = (Tokenization){back, input.len, terminal->cost, terminal->count};
    free(ring); return 1;
}

static int tokenize_with_adjustments(const Vocab *v, Slice input, const int64_t *adjustments, Tokenization *out) {
    if (v->algorithm == 1u) return tokenize_bpe_with_adjustments(v, input, adjustments, out);
    return tokenize_viterbi_with_adjustments(v, input, adjustments, out);
}

static void tokenization_free(Tokenization *result) {
    free(result->back);
    *result = (Tokenization){0};
}


#ifndef MOSAIC_LIBRARY_ONLY
static int roundtrip(const Vocab *v, Slice input, const Tokenization *result) {
    if (result->input_len != input.len) return 0;
    size_t cursor = input.len, count = 0;
    while (cursor) {
        uint32_t entry_index = result->back[cursor];
        VocabEntry e; Slice surf;
        if (entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surf) || surf.len > cursor) return 0;
        size_t start = cursor - surf.len;
        if (memcmp(input.bytes + start, surf.bytes, surf.len) != 0) return 0;
        cursor = start; ++count;
    }
    return count == result->count;
}
#endif

static int reconstruct_entry_indices(const Vocab *v, const Tokenization *result, uint32_t **indices_out) {
    if (result->count > SIZE_MAX / sizeof(uint32_t)) return 0;
    uint32_t *indices = result->count ? (uint32_t *)malloc(result->count * sizeof *indices) : NULL;
    if (result->count && !indices) return 0;
    size_t cursor = result->input_len, pos = result->count;
    while (cursor) {
        uint32_t entry_index = result->back[cursor];
        VocabEntry e; Slice surf;
        if (!pos || entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surf) || surf.len > cursor) { free(indices); return 0; }
        indices[--pos] = entry_index;
        cursor -= surf.len;
    }
    if (pos != 0) { free(indices); return 0; }
    *indices_out = indices; return 1;
}


#ifndef MOSAIC_LIBRARY_ONLY
static void print_encoding(const Vocab *v, Slice input, const Tokenization *result) {
    uint32_t *indices = NULL;
    if (!reconstruct_entry_indices(v, result, &indices)) return;
    size_t start = 0;
    for (size_t i = 0; i < result->count; ++i) {
        VocabEntry e; Slice surf;
        if (!vocab_entry(v, indices[i], &e, &surf)) break;
        size_t end = start + surf.len;
        printf("%" PRIu32 ":%zu:%zu:%" PRId32 "%c", e.token_id, start, end, e.cost, i + 1 == result->count ? '\n' : ' ');
        start = end;
    }
    if (!result->count) putchar('\n');
    free(indices); (void)input;
}

#endif


typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint32_t script_count, script_range_count, bidi_count, ignorable_count, nonchar_count, deprecated_count;
    uint32_t script_meta_offset, script_range_offset, bidi_offset, ignorable_offset, nonchar_offset, deprecated_offset, blob_offset;
    uint16_t common_id, inherited_id, unknown_id;
} SecurityView;

typedef struct { uint32_t start, end; uint16_t script_id; } SecurityScriptRange;

static int security_script_meta(const SecurityView *v, uint32_t index, Slice *name, uint16_t *id) {
    if (index >= v->script_count) return 0;
    const uint8_t *p = v->section + v->script_meta_offset + (size_t)index * 8u;
    uint32_t off = rd32(p); uint16_t len = rd16(p + 4); uint16_t sid = rd16(p + 6);
    size_t blob_len = v->section_len - v->blob_offset;
    if ((uint64_t)off + len > blob_len || !len || !sid) return 0;
    name->bytes = v->section + v->blob_offset + off; name->len = len; *id = sid; return 1;
}
static int security_script_range_at(const SecurityView *v, uint32_t index, SecurityScriptRange *out) {
    if (index >= v->script_range_count) return 0;
    const uint8_t *p = v->section + v->script_range_offset + (size_t)index * 12u;
    out->start = rd32(p); out->end = rd32(p + 4); out->script_id = rd16(p + 8);
    return p[10] == 0 && p[11] == 0;
}
static int security_bool_range_at(const SecurityView *v, uint32_t offset, uint32_t count, uint32_t index,
                                  uint32_t *start, uint32_t *end) {
    if (index >= count) return 0;
    const uint8_t *p = v->section + offset + (size_t)index * 8u;
    *start = rd32(p); *end = rd32(p + 4); return 1;
}
static int security_bool_lookup(const SecurityView *v, uint32_t offset, uint32_t count, uint32_t cp) {
    uint32_t lo=0,hi=count; while(lo<hi){uint32_t mid=lo+(hi-lo)/2,a,b;if(!security_bool_range_at(v,offset,count,mid,&a,&b))return 0;if(cp<a)hi=mid;else if(cp>=b)lo=mid+1;else return 1;}return 0;
}
static uint16_t security_script_lookup(const SecurityView *v, uint32_t cp) {
    uint32_t lo=0,hi=v->script_range_count; while(lo<hi){uint32_t mid=lo+(hi-lo)/2;SecurityScriptRange r;if(!security_script_range_at(v,mid,&r))return 0;if(cp<r.start)hi=mid;else if(cp>=r.end)lo=mid+1;else return r.script_id;}return 0;
}
static int parse_security_section(Slice s, SecurityView *v) {
    if (s.len < 72 || memcmp(s.bytes,"MSSC",4)!=0 || rd16(s.bytes+4)!=1 || rd16(s.bytes+6)!=72) return fail("invalid security header");
    if (rd16(s.bytes+8)!=17 || rd16(s.bytes+10)!=0 || rd16(s.bytes+12)!=0 || rd16(s.bytes+14)!=0) return fail("unsupported security Unicode version");
    uint32_t sc=rd32(s.bytes+16),src=rd32(s.bytes+20),bc=rd32(s.bytes+24),ic=rd32(s.bytes+28),nc=rd32(s.bytes+32),dc=rd32(s.bytes+36);
    uint32_t mo=rd32(s.bytes+40),ro=rd32(s.bytes+44),bo=rd32(s.bytes+48),io=rd32(s.bytes+52),no=rd32(s.bytes+56),doff=rd32(s.bytes+60),blob=rd32(s.bytes+64);
    if (rd32(s.bytes+68)!=0 || !sc || sc>1024 || src>200000 || bc>10000 || ic>10000 || nc>10000 || dc>10000 || mo!=72) return fail("security limits/header");
    uint64_t me=(uint64_t)mo+(uint64_t)sc*8u,re=(uint64_t)ro+(uint64_t)src*12u,be=(uint64_t)bo+(uint64_t)bc*8u,ie=(uint64_t)io+(uint64_t)ic*8u,ne=(uint64_t)no+(uint64_t)nc*8u,de=(uint64_t)doff+(uint64_t)dc*8u;
    if (!(me<=ro && re<=bo && be<=io && ie<=no && ne<=doff && de<=blob && blob<=s.len)) return fail("security section layout");
    if (!all_zero(s.bytes+(size_t)me,(size_t)ro-(size_t)me)||!all_zero(s.bytes+(size_t)re,(size_t)bo-(size_t)re)||!all_zero(s.bytes+(size_t)be,(size_t)io-(size_t)be)||!all_zero(s.bytes+(size_t)ie,(size_t)no-(size_t)ie)||!all_zero(s.bytes+(size_t)ne,(size_t)doff-(size_t)ne)||!all_zero(s.bytes+(size_t)de,(size_t)blob-(size_t)de)) return fail("security padding nonzero");
    *v=(SecurityView){s.bytes,s.len,sc,src,bc,ic,nc,dc,mo,ro,bo,io,no,doff,blob,0,0,0};
    for(uint32_t i=0;i<sc;++i){Slice name;uint16_t sid;if(!security_script_meta(v,i,&name,&sid)||sid!=i+1u)return fail("invalid security script metadata");for(size_t j=0;j<name.len;++j)if(name.bytes[j]<'A'||name.bytes[j]>'Z')return fail("invalid security script name");if(name.len==6&&memcmp(name.bytes,"COMMON",6)==0)v->common_id=sid;else if(name.len==9&&memcmp(name.bytes,"INHERITED",9)==0)v->inherited_id=sid;else if(name.len==7&&memcmp(name.bytes,"UNKNOWN",7)==0)v->unknown_id=sid;}
    uint32_t prev=0;for(uint32_t i=0;i<src;++i){SecurityScriptRange r;if(!security_script_range_at(v,i,&r)||r.start>=r.end||r.end>0x110000u||(i&&r.start<prev)||!r.script_id||r.script_id>sc)return fail("invalid security script range");prev=r.end;}
    const uint32_t offs[4]={bo,io,no,doff},counts[4]={bc,ic,nc,dc};for(int t=0;t<4;++t){prev=0;for(uint32_t i=0;i<counts[t];++i){uint32_t a,b;if(!security_bool_range_at(v,offs[t],counts[t],i,&a,&b)||a>=b||b>0x110000u||(i&&a<prev))return fail("invalid security boolean range");prev=b;}}
    if(!v->common_id||!v->inherited_id||!v->unknown_id)return fail("required security script IDs missing");
    return 1;
}
static int load_security_pack(const uint8_t *data,size_t len,SecurityView *v){Section*sections=NULL;uint32_t count=0,mi=0,li=0;if(!parse_outer(data,len,&sections,&count,&mi,&li))return 0;int n=0;Section ss={0};for(uint32_t i=0;i<count;++i)if(sections[i].kind==7){++n;ss=sections[i];}if(n!=1){free(sections);return fail("expected exactly one security section");}int ok=validate_manifest_lock(sec_slice(data,&sections[mi]),sec_slice(data,&sections[li]))&&parse_security_section(sec_slice(data,&ss),v);free(sections);return ok;}

typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint32_t gcb_count;
    uint32_t incb_count;
    uint32_t ep_count;
    uint32_t gcb_offset;
    uint32_t incb_offset;
    uint32_t ep_offset;
} UnicodeView;

typedef struct {
    size_t start;
    size_t end;
    uint32_t cp;
    int valid;
} ScalarUnit;

enum {
    G_CONTROL=1, G_LF=2, G_CR=3, G_EXTEND=4, G_PREPEND=5, G_SPACING_MARK=6,
    G_L=7, G_V=8, G_T=9, G_ZWJ=10, G_LV=11, G_LVT=12, G_RI=13,
    I_EXTEND=1, I_CONSONANT=2, I_LINKER=3
};

static int unicode_prop_range(const UnicodeView *u, uint32_t offset, uint32_t count, uint32_t index,
                              uint32_t *start, uint32_t *end, uint8_t *value) {
    if (index >= count) return 0;
    const uint8_t *r = u->section + offset + (size_t)index * 12;
    *start = rd32(r); *end = rd32(r + 4); *value = r[8];
    return r[9] == 0 && r[10] == 0 && r[11] == 0;
}
static int unicode_ep_range(const UnicodeView *u, uint32_t index, uint32_t *start, uint32_t *end) {
    if (index >= u->ep_count) return 0;
    const uint8_t *r = u->section + u->ep_offset + (size_t)index * 8;
    *start = rd32(r); *end = rd32(r + 4); return 1;
}
static uint8_t unicode_prop_lookup(const UnicodeView *u, uint32_t offset, uint32_t count, uint32_t cp) {
    uint32_t lo=0,hi=count;
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2,start,end;uint8_t val;if(!unicode_prop_range(u,offset,count,mid,&start,&end,&val))return 0;if(cp<start)hi=mid;else if(cp>=end)lo=mid+1;else return val;}
    return 0;
}
static int unicode_ep_lookup(const UnicodeView *u, uint32_t cp) {
    uint32_t lo=0,hi=u->ep_count;
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2,start,end;if(!unicode_ep_range(u,mid,&start,&end))return 0;if(cp<start)hi=mid;else if(cp>=end)lo=mid+1;else return 1;}
    return 0;
}
static int parse_unicode_section(Slice s, UnicodeView *u) {
    if (s.len < 48 || memcmp(s.bytes,"MSUC",4)!=0 || rd16(s.bytes+4)!=1 || rd16(s.bytes+6)!=0) return fail("invalid Unicode header");
    if (rd16(s.bytes+8)!=17 || rd16(s.bytes+10)!=0 || rd16(s.bytes+12)!=0 || rd16(s.bytes+14)!=0 || rd64(s.bytes+40)!=0) return fail("unsupported Unicode version/header");
    uint32_t gc=rd32(s.bytes+16),ic=rd32(s.bytes+20),ec=rd32(s.bytes+24),go=rd32(s.bytes+28),io=rd32(s.bytes+32),eo=rd32(s.bytes+36);
    if ((uint64_t)gc+ic+ec>100000u || go!=48) return fail("Unicode range limit/layout");
    uint64_t ge=(uint64_t)go+(uint64_t)gc*12,ie=(uint64_t)io+(uint64_t)ic*12,ee=(uint64_t)eo+(uint64_t)ec*8;
    if (!(ge<=io && ie<=eo && ee==s.len)) return fail("invalid Unicode section layout");
    if (!all_zero(s.bytes+(size_t)ge,(size_t)io-(size_t)ge) || !all_zero(s.bytes+(size_t)ie,(size_t)eo-(size_t)ie)) return fail("Unicode padding nonzero");
    *u=(UnicodeView){s.bytes,s.len,gc,ic,ec,go,io,eo};
    for(int table=0;table<2;++table){uint32_t off=table?io:go,count=table?ic:gc,prev=0;uint8_t max=table?3:13;for(uint32_t i=0;i<count;++i){uint32_t a,b;uint8_t v;if(!unicode_prop_range(u,off,count,i,&a,&b,&v)||a>=b||b>0x110000u||(i&&a<prev)||v==0||v>max)return fail("invalid Unicode property range");prev=b;}}
    uint32_t prev=0;for(uint32_t i=0;i<ec;++i){uint32_t a,b;if(!unicode_ep_range(u,i,&a,&b)||a>=b||b>0x110000u||(i&&a<prev))return fail("invalid extended pictographic range");prev=b;}
    return 1;
}
static int load_unicode_pack(const uint8_t *data,size_t len,UnicodeView *u){
    Section *sections=NULL;uint32_t count=0,mi=0,li=0;if(!parse_outer(data,len,&sections,&count,&mi,&li))return 0;int n=0;Section us={0};for(uint32_t i=0;i<count;++i)if(sections[i].kind==5){++n;us=sections[i];}
    if(n!=1){free(sections);return fail("expected exactly one Unicode section");}
    int ok=validate_manifest_lock(sec_slice(data,&sections[mi]),sec_slice(data,&sections[li]))&&parse_unicode_section(sec_slice(data,&us),u);free(sections);return ok;
}
static int is_cont_byte(uint8_t b){return b>=0x80&&b<=0xbf;}
static ScalarUnit decode_one_unit(Slice input,size_t i){
    uint8_t b0=input.bytes[i];ScalarUnit unit={i,i+1,0,0};
    if(b0<=0x7f){unit.cp=b0;unit.valid=1;}
    else if(b0>=0xc2&&b0<=0xdf&&i+1<input.len&&is_cont_byte(input.bytes[i+1])){unit.end=i+2;unit.cp=((uint32_t)(b0&0x1f)<<6)|(input.bytes[i+1]&0x3f);unit.valid=1;}
    else if(b0>=0xe0&&b0<=0xef&&i+2<input.len){uint8_t b1=input.bytes[i+1],b2=input.bytes[i+2];int vb1=b0==0xe0?(b1>=0xa0&&b1<=0xbf):b0==0xed?(b1>=0x80&&b1<=0x9f):is_cont_byte(b1);if(vb1&&is_cont_byte(b2)){unit.end=i+3;unit.cp=((uint32_t)(b0&0x0f)<<12)|((uint32_t)(b1&0x3f)<<6)|(b2&0x3f);unit.valid=1;}}
    else if(b0>=0xf0&&b0<=0xf4&&i+3<input.len){uint8_t b1=input.bytes[i+1],b2=input.bytes[i+2],b3=input.bytes[i+3];int vb1=b0==0xf0?(b1>=0x90&&b1<=0xbf):b0==0xf4?(b1>=0x80&&b1<=0x8f):is_cont_byte(b1);if(vb1&&is_cont_byte(b2)&&is_cont_byte(b3)){unit.end=i+4;unit.cp=((uint32_t)(b0&7)<<18)|((uint32_t)(b1&0x3f)<<12)|((uint32_t)(b2&0x3f)<<6)|(b3&0x3f);unit.valid=1;}}
    return unit;
}
static int decode_units(Slice input,ScalarUnit **out,size_t *count_out){
    ScalarUnit *units=input.len?(ScalarUnit*)malloc(input.len*sizeof *units):NULL;if(input.len&&!units)return 0;size_t count=0,i=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);units[count++]=unit;i=unit.end;}
    *out=units;*count_out=count;return 1;
}
static int security_script_ranges_internal(const SecurityView *v, Slice input, mosaic_script_span **out_ranges, size_t *out_count) {
    *out_ranges=NULL;*out_count=0;
    if(!input.len)return 1;
    size_t span_count=0,i=0,end=0;uint16_t current=0;int have=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);uint16_t sid=unit.valid?security_script_lookup(v,unit.cp):0;
        if(!have){current=sid;end=unit.end;have=1;}
        else if(sid==current&&unit.start==end)end=unit.end;
        else{if(span_count==SIZE_MAX)return 0;++span_count;current=sid;end=unit.end;}
        i=unit.end;
    }
    if(have){if(span_count==SIZE_MAX)return 0;++span_count;}
    if(span_count>SIZE_MAX/sizeof(mosaic_script_span))return 0;
    mosaic_script_span *ranges=(mosaic_script_span*)malloc(span_count*sizeof *ranges);if(!ranges)return 0;
    size_t n=0,start=0;i=0;end=0;current=0;have=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);uint16_t sid=unit.valid?security_script_lookup(v,unit.cp):0;
        if(!have){current=sid;start=unit.start;end=unit.end;have=1;}
        else if(sid==current&&unit.start==end)end=unit.end;
        else{ranges[n++]=(mosaic_script_span){(uint64_t)start,(uint64_t)(end-start),current,0};current=sid;start=unit.start;end=unit.end;}
        i=unit.end;
    }
    if(have)ranges[n++]=(mosaic_script_span){(uint64_t)start,(uint64_t)(end-start),current,0};
    if(n!=span_count){free(ranges);return 0;}
    *out_ranges=ranges;*out_count=n;return 1;
}
static int security_significant_script(const SecurityView *v,uint16_t sid){
    return sid&&sid<=v->script_count&&sid!=v->common_id&&sid!=v->inherited_id&&sid!=v->unknown_id;
}
static size_t security_unit_finding_count(const SecurityView *v,const ScalarUnit *unit,uint16_t primary,size_t active){
    if(!unit->valid)return 0;
    uint32_t cp=unit->cp;
    uint16_t sid=security_script_lookup(v,cp);
    size_t n=0;
    n+=(size_t)security_bool_lookup(v,v->bidi_offset,v->bidi_count,cp);
    n+=(size_t)security_bool_lookup(v,v->ignorable_offset,v->ignorable_count,cp);
    n+=(size_t)security_bool_lookup(v,v->nonchar_offset,v->nonchar_count,cp);
    n+=(size_t)security_bool_lookup(v,v->deprecated_offset,v->deprecated_count,cp);
    if(active>1&&security_significant_script(v,sid)&&sid!=primary)++n;
    return n;
}
static int security_scan_internal(const SecurityView *v, Slice input, mosaic_security_finding **out_findings, size_t *out_count) {
    *out_findings=NULL;*out_count=0;
    uint64_t script_counts[1025]={0};size_t i=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);if(unit.valid){uint16_t sid=security_script_lookup(v,unit.cp);if(security_significant_script(v,sid)){if(script_counts[sid]==UINT64_MAX)return 0;++script_counts[sid];}}i=unit.end;}
    uint16_t primary=0;uint64_t best=0;size_t active=0;for(uint32_t sid=1;sid<=v->script_count;++sid){if(script_counts[sid]){++active;if(script_counts[sid]>best||(script_counts[sid]==best&&(!primary||sid<primary))){best=script_counts[sid];primary=(uint16_t)sid;}}}
    size_t finding_count=0;i=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);size_t add=security_unit_finding_count(v,&unit,primary,active);if(add>SIZE_MAX-finding_count)return 0;finding_count+=add;i=unit.end;}
    if(finding_count>SIZE_MAX/sizeof(mosaic_security_finding))return 0;
    mosaic_security_finding *findings=finding_count?(mosaic_security_finding*)malloc(finding_count*sizeof *findings):NULL;if(finding_count&&!findings)return 0;
    size_t n=0;i=0;
    while(i<input.len){ScalarUnit unit=decode_one_unit(input,i);if(unit.valid){uint32_t cp=unit.cp;uint16_t sid=security_script_lookup(v,cp);uint32_t kinds[4];size_t kn=0;
        if(security_bool_lookup(v,v->bidi_offset,v->bidi_count,cp))kinds[kn++]=MOSAIC_SECURITY_BIDI_CONTROL;
        if(security_bool_lookup(v,v->ignorable_offset,v->ignorable_count,cp))kinds[kn++]=MOSAIC_SECURITY_DEFAULT_IGNORABLE;
        if(security_bool_lookup(v,v->nonchar_offset,v->nonchar_count,cp))kinds[kn++]=MOSAIC_SECURITY_NONCHARACTER;
        if(security_bool_lookup(v,v->deprecated_offset,v->deprecated_count,cp))kinds[kn++]=MOSAIC_SECURITY_DEPRECATED;
        for(size_t k=0;k<kn;++k){if(n>=finding_count){free(findings);return 0;}findings[n++]=(mosaic_security_finding){kinds[k],sid,0,(uint64_t)unit.start,(uint64_t)(unit.end-unit.start)};}
        if(active>1&&security_significant_script(v,sid)&&sid!=primary){if(n>=finding_count){free(findings);return 0;}findings[n++]=(mosaic_security_finding){MOSAIC_SECURITY_MIXED_SCRIPT,sid,0,(uint64_t)unit.start,(uint64_t)(unit.end-unit.start)};}
    }i=unit.end;}
    if(n!=finding_count){free(findings);return 0;}
    *out_findings=findings;*out_count=n;return 1;
}

static uint8_t ugcb(const UnicodeView*u,const ScalarUnit*x){return x->valid?unicode_prop_lookup(u,u->gcb_offset,u->gcb_count,x->cp):0;}
static uint8_t uincb(const UnicodeView*u,const ScalarUnit*x){return x->valid?unicode_prop_lookup(u,u->incb_offset,u->incb_count,x->cp):0;}
static int uep(const UnicodeView*u,const ScalarUnit*x){return x->valid&&unicode_ep_lookup(u,x->cp);}
static int gb9c_c(const UnicodeView*u,const ScalarUnit*units,size_t i){size_t j=i;int linker=0;while(j>0){--j;uint8_t p=uincb(u,&units[j]);if(p==I_EXTEND)continue;if(p==I_LINKER){linker=1;continue;}return linker&&p==I_CONSONANT;}return 0;}
static int gb11_c(const UnicodeView*u,const ScalarUnit*units,size_t i){if(i<2)return 0;size_t j=i-2;for(;;){if(ugcb(u,&units[j])!=G_EXTEND)return uep(u,&units[j]);if(j==0)return 0;--j;}}
static size_t ri_count_c(const UnicodeView*u,const ScalarUnit*units,size_t i){size_t c=0,j=i;while(j>0){--j;if(ugcb(u,&units[j])==G_RI)++c;else break;}return c;}
static int unicode_should_break(const UnicodeView*u,const ScalarUnit*units,size_t i){const ScalarUnit*a=&units[i-1],*b=&units[i];if(!a->valid||!b->valid)return 1;uint8_t ga=ugcb(u,a),gb=ugcb(u,b);if(ga==G_CR&&gb==G_LF)return 0;if(ga==G_CONTROL||ga==G_CR||ga==G_LF)return 1;if(gb==G_CONTROL||gb==G_CR||gb==G_LF)return 1;if(ga==G_L&&(gb==G_L||gb==G_V||gb==G_LV||gb==G_LVT))return 0;if((ga==G_LV||ga==G_V)&&(gb==G_V||gb==G_T))return 0;if((ga==G_LVT||ga==G_T)&&gb==G_T)return 0;if(gb==G_EXTEND||gb==G_ZWJ)return 0;if(gb==G_SPACING_MARK)return 0;if(ga==G_PREPEND)return 0;if(uincb(u,b)==I_CONSONANT&&gb9c_c(u,units,i))return 0;if(uep(u,b)&&ga==G_ZWJ&&gb11_c(u,units,i))return 0;if(ga==G_RI&&gb==G_RI&&(ri_count_c(u,units,i)&1u))return 0;return 1;}
static int compute_graphemes(const UnicodeView*u,Slice input,mosaic_range **ranges_out,size_t *range_count){
    ScalarUnit*units=NULL;size_t count=0;if(!decode_units(input,&units,&count))return 0;
    if(!count){free(units);*ranges_out=NULL;*range_count=0;return 1;}
    if(count>SIZE_MAX/sizeof(mosaic_range)){free(units);return 0;}
    mosaic_range*ranges=(mosaic_range*)malloc(count*sizeof *ranges);if(!ranges){free(units);return 0;}
    size_t cluster=0,n=0;
    for(size_t i=1;i<=count;++i){if(i==count||unicode_should_break(u,units,i)){size_t start=units[cluster].start,end=units[i-1].end;ranges[n++]=(mosaic_range){(uint64_t)start,(uint64_t)(end-start)};cluster=i;}}
    free(units);*ranges_out=ranges;*range_count=n;return 1;
}

#ifndef MOSAIC_LIBRARY_ONLY
static int print_graphemes(const UnicodeView *u, Slice input) {
    mosaic_range *ranges = NULL;
    size_t count = 0;
    if (!compute_graphemes(u, input, &ranges, &count)) return fail("out of memory");
    for (size_t i = 0; i < count; ++i) {
        printf("%" PRIu64 ":%" PRIu64 "%c", ranges[i].start, ranges[i].start + ranges[i].length,
               i + 1 == count ? '\n' : ' ');
    }
    if (!count) putchar('\n');
    free(ranges);
    return 1;
}
#endif



static int vocab_by_id(const Vocab *v, uint32_t token_id, VocabEntry *entry, Slice *surface) {
    uint32_t lo = 0, hi = v->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uint32_t idx = vocab_id_index(v, mid);
        VocabEntry e; Slice surf;
        if (!vocab_entry(v, idx, &e, &surf)) return 0;
        if (e.token_id < token_id) lo = mid + 1;
        else if (e.token_id > token_id) hi = mid;
        else { *entry = e; *surface = surf; return 1; }
    }
    return 0;
}


#ifndef MOSAIC_LIBRARY_ONLY
static int write_encoded_u32(const Vocab *v, const Tokenization *result, const char *path) {
    uint32_t *indices = NULL;
    if (!reconstruct_entry_indices(v, result, &indices)) return fail("path reconstruction failed");
    FILE *file = fopen(path, "wb");
    if (!file) { perror(path); free(indices); return 0; }
    for (size_t i = 0; i < result->count; ++i) {
        VocabEntry e; Slice surface;
        if (!vocab_entry(v, indices[i], &e, &surface)) { fclose(file); free(indices); return 0; }
        (void)surface;
        uint8_t word[4] = {(uint8_t)e.token_id, (uint8_t)(e.token_id >> 8), (uint8_t)(e.token_id >> 16), (uint8_t)(e.token_id >> 24)};
        if (fwrite(word, 1, 4, file) != 4) { fclose(file); free(indices); return 0; }
    }
    int ok = fclose(file) == 0;
    free(indices); return ok;
}

static int decode_u32_file(const Vocab *v, const char *ids_path, const char *out_path) {
    uint8_t *ids = NULL; size_t len = 0;
    if (!read_file(ids_path, &ids, &len)) return 0;
    if (len % 4 != 0) { free(ids); return fail("u32 token stream length is not a multiple of four"); }
    FILE *out = fopen(out_path, "wb");
    if (!out) { perror(out_path); free(ids); return 0; }
    for (size_t i = 0; i < len; i += 4) {
        uint32_t id = rd32(ids + i);
        VocabEntry e; Slice surface;
        if (!vocab_by_id(v, id, &e, &surface)) { fclose(out); free(ids); return fail("unknown token ID"); }
        (void)e;
        if (surface.len && fwrite(surface.bytes, 1, surface.len, out) != surface.len) { fclose(out); free(ids); return 0; }
    }
    int ok = fclose(out) == 0;
    free(ids);
    return ok;
}

#endif


struct mosaic_model {
    uint8_t *pack;
    size_t pack_len;
    Vocab vocab;
};

struct mosaic_unicode {
    uint8_t *pack;
    size_t pack_len;
    UnicodeView view;
};

struct mosaic_detector {
    uint8_t *pack;
    size_t pack_len;
    DetectorView view;
    uint8_t hash[32];
};

struct mosaic_security {
    uint8_t *pack;
    size_t pack_len;
    SecurityView view;
    uint8_t hash[32];
};

struct mosaic_tokenizer {
    mosaic_model *model;
    mosaic_unicode *unicode_data;
    LanguagePack *languages;
    size_t language_count;
    size_t language_capacity;
    int64_t *adjustments;
    mosaic_detector *detector;
    mosaic_security *security;
    uint8_t fingerprint[32];
};

struct mosaic_stream {
    mosaic_model *model;
    int64_t *adjustments;
    mosaic_tokenizer *tokenizer;
    int auto_mode;
    uint8_t *buffer;
    size_t len;
    size_t capacity;
    int finished;
};

struct mosaic_document {
    mosaic_model *model;
    int64_t *adjustments;
    mosaic_tokenizer *tokenizer;
    int auto_mode;
    uint8_t *buffer;
    size_t len;
    size_t capacity;
};

void mosaic_free(void *pointer) { free(pointer); }
const char *mosaic_version_string(void) { return "0.7.0"; }
uint32_t mosaic_tokenizer_semantics_version(void) { return 2u; }

const char *mosaic_status_string(mosaic_status status) {
    switch (status) {
        case MOSAIC_OK: return "ok";
        case MOSAIC_ERROR_INVALID_ARGUMENT: return "invalid argument";
        case MOSAIC_ERROR_IO: return "I/O error";
        case MOSAIC_ERROR_INVALID_PACK: return "invalid pack";
        case MOSAIC_ERROR_OUT_OF_MEMORY: return "out of memory";
        case MOSAIC_ERROR_OVERFLOW: return "overflow";
        case MOSAIC_ERROR_UNKNOWN_TOKEN_ID: return "unknown token ID";
        case MOSAIC_ERROR_INTERNAL: return "internal error";
        case MOSAIC_ERROR_CONFLICT: return "conflict";
        default: return "unknown status";
    }
}

static mosaic_status copy_bytes(const uint8_t *src, size_t len, uint8_t **out) {
    if (len && !src) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t *copy = len ? (uint8_t *)malloc(len) : (uint8_t *)malloc(1);
    if (!copy) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (len) memcpy(copy, src, len);
    *out = copy;
    return MOSAIC_OK;
}

static void language_pack_release(LanguagePack *pack) {
    if (!pack) return;
    free(pack->adjustments);
    free(pack->pack);
    *pack = (LanguagePack){0};
}

static mosaic_status language_pack_copy(const uint8_t *bytes, size_t len, LanguagePack *out) {
    if ((!bytes && len) || !out) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out = (LanguagePack){0};
    mosaic_status status = copy_bytes(bytes, len, &out->pack);
    if (status != MOSAIC_OK) return status;
    out->pack_len = len;
    if (!load_language_pack(out->pack, out->pack_len, &out->view) || !sha256_bytes(out->pack, out->pack_len, out->hash)) {
        language_pack_release(out);
        return MOSAIC_ERROR_INVALID_PACK;
    }
    return MOSAIC_OK;
}

static void language_pack_array_free(LanguagePack *packs, size_t count) {
    for (size_t i = 0; i < count; ++i) language_pack_release(&packs[i]);
    free(packs);
}

static int language_same_tag(const LanguagePack *a, const LanguagePack *b) {
    if (a->view.tag_len != b->view.tag_len) return 0;
    return memcmp(a->view.section + a->view.tag_offset,
                  b->view.section + b->view.tag_offset,
                  a->view.tag_len) == 0;
}

static int language_matches_cstr(const LanguagePack *pack, const char *tag) {
    size_t len = strlen(tag);
    return len == pack->view.tag_len &&
           memcmp(pack->view.section + pack->view.tag_offset, tag, len) == 0;
}

mosaic_status mosaic_detector_load_memory(const uint8_t *pack, size_t pack_len, mosaic_detector **out_detector) {
    if (!out_detector || (!pack && pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_detector = NULL;
    mosaic_detector *detector = (mosaic_detector *)calloc(1, sizeof *detector);
    if (!detector) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = copy_bytes(pack, pack_len, &detector->pack);
    if (status != MOSAIC_OK) { free(detector); return status; }
    detector->pack_len = pack_len;
    if (!load_detector_pack(detector->pack, detector->pack_len, &detector->view) ||
        !sha256_bytes(detector->pack, detector->pack_len, detector->hash)) {
        mosaic_detector_free(detector); return MOSAIC_ERROR_INVALID_PACK;
    }
    *out_detector = detector;
    return MOSAIC_OK;
}

mosaic_status mosaic_detector_load_file(const char *path, mosaic_detector **out_detector) {
    if (!path || !out_detector) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_detector = NULL;
    uint8_t *pack = NULL; size_t len = 0;
    if (!read_file(path, &pack, &len)) return MOSAIC_ERROR_IO;
    mosaic_status status = mosaic_detector_load_memory(pack, len, out_detector);
    free(pack); return status;
}

void mosaic_detector_free(mosaic_detector *detector) {
    if (!detector) return;
    free(detector->pack); free(detector);
}

mosaic_status mosaic_detector_detect(const mosaic_detector *detector, const uint8_t *input, size_t input_len,
                                     mosaic_detection *out_detection) {
    if (!detector || (!input && input_len) || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!detector_detect_view(&detector->view, (Slice){input, input_len}, out_detection)) return MOSAIC_ERROR_OVERFLOW;
    return MOSAIC_OK;
}

mosaic_status mosaic_model_load_memory(const uint8_t *pack, size_t pack_len, mosaic_model **out_model) {
    if (!out_model || (!pack && pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_model = NULL;
    mosaic_model *model = (mosaic_model *)calloc(1, sizeof *model);
    if (!model) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = copy_bytes(pack, pack_len, &model->pack);
    if (status != MOSAIC_OK) {
        free(model);
        return status;
    }
    model->pack_len = pack_len;
    if (!load_model_pack(model->pack, model->pack_len, &model->vocab)) {
        free(model->pack);
        free(model);
        return MOSAIC_ERROR_INVALID_PACK;
    }
    *out_model = model;
    return MOSAIC_OK;
}

mosaic_status mosaic_model_load_file(const char *path, mosaic_model **out_model) {
    if (!path || !out_model) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t *data = NULL;
    size_t len = 0;
    if (!read_file(path, &data, &len)) return MOSAIC_ERROR_IO;
    mosaic_status status = mosaic_model_load_memory(data, len, out_model);
    free(data);
    return status;
}

void mosaic_model_free(mosaic_model *model) {
    if (!model) return;
    free(model->pack);
    free(model);
}

static mosaic_status encode_tokens_internal(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                            const int64_t *adjustments,
                                            mosaic_token **out_tokens, size_t *out_count) {
    if (!model || (!input && input_len) || !out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokens = NULL;
    *out_count = 0;
    Tokenization result = {0};
    if (!tokenize_with_adjustments(&model->vocab, (Slice){input, input_len}, adjustments, &result)) return MOSAIC_ERROR_INTERNAL;
    uint32_t *indices = NULL;
    if (!reconstruct_entry_indices(&model->vocab, &result, &indices)) {
        tokenization_free(&result);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    size_t count = result.count;
    if (count > SIZE_MAX / sizeof(mosaic_token)) {
        free(indices);
        tokenization_free(&result);
        return MOSAIC_ERROR_OVERFLOW;
    }
    mosaic_token *tokens = count ? (mosaic_token *)malloc(count * sizeof *tokens) : NULL;
    if (count && !tokens) {
        free(indices);
        tokenization_free(&result);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i) {
        VocabEntry entry;
        Slice surface;
        if (!vocab_entry(&model->vocab, indices[i], &entry, &surface)) {
            free(tokens);
            free(indices);
            tokenization_free(&result);
            return MOSAIC_ERROR_INTERNAL;
        }
        tokens[i] = (mosaic_token){entry.token_id, (uint64_t)offset, (uint64_t)surface.len, entry.cost};
        offset += surface.len;
    }
    free(indices);
    tokenization_free(&result);
    *out_tokens = tokens;
    *out_count = count;
    return MOSAIC_OK;
}

mosaic_status mosaic_encode_tokens(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                   mosaic_token **out_tokens, size_t *out_count) {
    return encode_tokens_internal(model, input, input_len, NULL, out_tokens, out_count);
}


static mosaic_status encode_internal(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                     const int64_t *adjustments,
                                     uint32_t **out_ids, size_t *out_count) {
    if (!model || (!input && input_len) || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL;
    *out_count = 0;
    Tokenization result = {0};
    if (!tokenize_with_adjustments(&model->vocab, (Slice){input, input_len}, adjustments, &result)) return MOSAIC_ERROR_INTERNAL;
    uint32_t *indices = NULL;
    if (!reconstruct_entry_indices(&model->vocab, &result, &indices)) {
        tokenization_free(&result);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    size_t count = result.count;
    if (count > SIZE_MAX / sizeof(uint32_t)) {
        free(indices);
        tokenization_free(&result);
        return MOSAIC_ERROR_OVERFLOW;
    }
    uint32_t *ids = count ? (uint32_t *)malloc(count * sizeof *ids) : NULL;
    if (count && !ids) {
        free(indices);
        tokenization_free(&result);
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < count; ++i) {
        VocabEntry entry;
        Slice surface;
        if (!vocab_entry(&model->vocab, indices[i], &entry, &surface)) {
            free(ids);
            free(indices);
            tokenization_free(&result);
            return MOSAIC_ERROR_INTERNAL;
        }
        ids[i] = entry.token_id;
    }
    free(indices);
    tokenization_free(&result);
    *out_ids = ids;
    *out_count = count;
    return MOSAIC_OK;
}

mosaic_status mosaic_encode(const mosaic_model *model, const uint8_t *input, size_t input_len,
                            uint32_t **out_ids, size_t *out_count) {
    return encode_internal(model, input, input_len, NULL, out_ids, out_count);
}


mosaic_status mosaic_decode(const mosaic_model *model, const uint32_t *ids, size_t count,
                            uint8_t **out_bytes, size_t *out_len) {
    if (!model || (!ids && count) || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL;
    *out_len = 0;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        VocabEntry entry;
        Slice surface;
        if (!vocab_by_id(&model->vocab, ids[i], &entry, &surface)) return MOSAIC_ERROR_UNKNOWN_TOKEN_ID;
        if (surface.len > SIZE_MAX - total) return MOSAIC_ERROR_OVERFLOW;
        total += surface.len;
    }
    uint8_t *out = total ? (uint8_t *)malloc(total) : (uint8_t *)malloc(1);
    if (!out) return MOSAIC_ERROR_OUT_OF_MEMORY;
    size_t pos = 0;
    for (size_t i = 0; i < count; ++i) {
        VocabEntry entry;
        Slice surface;
        if (!vocab_by_id(&model->vocab, ids[i], &entry, &surface)) {
            free(out);
            return MOSAIC_ERROR_INTERNAL;
        }
        memcpy(out + pos, surface.bytes, surface.len);
        pos += surface.len;
    }
    *out_bytes = out;
    *out_len = total;
    return MOSAIC_OK;
}

mosaic_status mosaic_stream_create(const mosaic_model *model, mosaic_stream **out_stream) {
    if (!model || !out_stream) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_stream = NULL;
    mosaic_stream *stream = (mosaic_stream *)calloc(1, sizeof *stream);
    if (!stream) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model->pack, model->pack_len, &stream->model);
    if (status != MOSAIC_OK) {
        free(stream);
        return status;
    }
    *out_stream = stream;
    return MOSAIC_OK;
}

mosaic_status mosaic_stream_push(mosaic_stream *stream, const uint8_t *bytes, size_t len) {
    if (!stream || (!bytes && len) || stream->finished) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (len > SIZE_MAX - stream->len) return MOSAIC_ERROR_OVERFLOW;
    size_t needed = stream->len + len;
    if (needed > stream->capacity) {
        size_t capacity = stream->capacity ? stream->capacity : 4096;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) { capacity = needed; break; }
            capacity *= 2;
        }
        uint8_t *grown = (uint8_t *)realloc(stream->buffer, capacity);
        if (!grown) return MOSAIC_ERROR_OUT_OF_MEMORY;
        stream->buffer = grown;
        stream->capacity = capacity;
    }
    if (len) memcpy(stream->buffer + stream->len, bytes, len);
    stream->len = needed;
    return MOSAIC_OK;
}

mosaic_status mosaic_stream_finish(mosaic_stream *stream, uint32_t **out_ids, size_t *out_count) {
    if (!stream || stream->finished || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status status;
    if (stream->tokenizer) {
        if (stream->auto_mode) {
            mosaic_detection detection;
            status = mosaic_tokenizer_encode_auto(stream->tokenizer, stream->buffer, stream->len,
                                                   out_ids, out_count, &detection);
        } else {
            status = mosaic_tokenizer_encode(stream->tokenizer, stream->buffer, stream->len, out_ids, out_count);
        }
    } else {
        status = encode_internal(stream->model, stream->buffer, stream->len, stream->adjustments, out_ids, out_count);
    }
    if (status == MOSAIC_OK) stream->finished = 1;
    return status;
}

mosaic_status mosaic_stream_finish_auto(mosaic_stream *stream, uint32_t **out_ids, size_t *out_count,
                                        mosaic_detection *out_detection) {
    if (!stream || !stream->tokenizer || !stream->auto_mode || stream->finished || !out_ids || !out_count || !out_detection)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status status = mosaic_tokenizer_encode_auto(stream->tokenizer, stream->buffer, stream->len,
                                                         out_ids, out_count, out_detection);
    if (status == MOSAIC_OK) stream->finished = 1;
    return status;
}

mosaic_status mosaic_stream_reset(mosaic_stream *stream) {
    if (!stream) return MOSAIC_ERROR_INVALID_ARGUMENT;
    stream->len = 0;
    stream->finished = 0;
    return MOSAIC_OK;
}

void mosaic_stream_free(mosaic_stream *stream) {
    if (!stream) return;
    mosaic_model_free(stream->model);
    mosaic_tokenizer_free(stream->tokenizer);
    free(stream->adjustments);
    free(stream->buffer);
    free(stream);
}

static mosaic_status reserve_buffer(uint8_t **buffer, size_t *capacity, size_t needed) {
    if (needed <= *capacity) return MOSAIC_OK;
    size_t next = *capacity ? *capacity : 4096;
    while (next < needed) {
        if (next > SIZE_MAX / 2) { next = needed; break; }
        next *= 2;
    }
    uint8_t *grown = (uint8_t *)realloc(*buffer, next);
    if (!grown) return MOSAIC_ERROR_OUT_OF_MEMORY;
    *buffer = grown;
    *capacity = next;
    return MOSAIC_OK;
}

mosaic_status mosaic_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                     mosaic_document **out_document) {
    if (!model || (!input && input_len) || !out_document) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    mosaic_document *document = (mosaic_document *)calloc(1, sizeof *document);
    if (!document) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model->pack, model->pack_len, &document->model);
    if (status != MOSAIC_OK) { free(document); return status; }
    if (input_len) {
        status = reserve_buffer(&document->buffer, &document->capacity, input_len);
        if (status != MOSAIC_OK) { mosaic_model_free(document->model); free(document); return status; }
        memcpy(document->buffer, input, input_len);
    }
    document->len = input_len;
    *out_document = document;
    return MOSAIC_OK;
}

mosaic_status mosaic_document_apply_edit(mosaic_document *document, uint64_t start64, uint64_t delete64,
                                         const uint8_t *replacement, size_t replacement_len) {
    if (!document || (!replacement && replacement_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (start64 > SIZE_MAX || delete64 > SIZE_MAX) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t start = (size_t)start64, delete_len = (size_t)delete64;
    if (start > document->len || delete_len > document->len - start) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t kept = document->len - delete_len;
    if (replacement_len > SIZE_MAX - kept) return MOSAIC_ERROR_OVERFLOW;
    size_t new_len = kept + replacement_len;
    mosaic_status status = reserve_buffer(&document->buffer, &document->capacity, new_len);
    if (status != MOSAIC_OK) return status;
    size_t old_tail = start + delete_len;
    size_t tail_len = document->len - old_tail;
    memmove(document->buffer + start + replacement_len, document->buffer + old_tail, tail_len);
    if (replacement_len) memcpy(document->buffer + start, replacement, replacement_len);
    document->len = new_len;
    return MOSAIC_OK;
}

mosaic_status mosaic_document_encode(const mosaic_document *document, uint32_t **out_ids, size_t *out_count) {
    if (!document || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (document->tokenizer) {
        if (document->auto_mode) {
            mosaic_detection detection;
            return mosaic_tokenizer_encode_auto(document->tokenizer, document->buffer, document->len,
                                                 out_ids, out_count, &detection);
        }
        return mosaic_tokenizer_encode(document->tokenizer, document->buffer, document->len, out_ids, out_count);
    }
    return encode_internal(document->model, document->buffer, document->len, document->adjustments, out_ids, out_count);
}

mosaic_status mosaic_document_encode_auto(const mosaic_document *document, uint32_t **out_ids, size_t *out_count,
                                          mosaic_detection *out_detection) {
    if (!document || !document->tokenizer || !document->auto_mode || !out_ids || !out_count || !out_detection)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    return mosaic_tokenizer_encode_auto(document->tokenizer, document->buffer, document->len,
                                         out_ids, out_count, out_detection);
}

mosaic_status mosaic_document_copy_bytes(const mosaic_document *document, uint8_t **out_bytes, size_t *out_len) {
    if (!document || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL; *out_len = 0;
    mosaic_status status = copy_bytes(document->buffer, document->len, out_bytes);
    if (status == MOSAIC_OK) *out_len = document->len;
    return status;
}

void mosaic_document_free(mosaic_document *document) {
    if (!document) return;
    mosaic_model_free(document->model);
    mosaic_tokenizer_free(document->tokenizer);
    free(document->adjustments);
    free(document->buffer);
    free(document);
}

mosaic_status mosaic_unicode_load_memory(const uint8_t *pack, size_t pack_len, mosaic_unicode **out_unicode) {
    if (!out_unicode || (!pack && pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_unicode = NULL;
    mosaic_unicode *unicode_data = (mosaic_unicode *)calloc(1, sizeof *unicode_data);
    if (!unicode_data) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = copy_bytes(pack, pack_len, &unicode_data->pack);
    if (status != MOSAIC_OK) {
        free(unicode_data);
        return status;
    }
    unicode_data->pack_len = pack_len;
    if (!load_unicode_pack(unicode_data->pack, unicode_data->pack_len, &unicode_data->view)) {
        free(unicode_data->pack);
        free(unicode_data);
        return MOSAIC_ERROR_INVALID_PACK;
    }
    *out_unicode = unicode_data;
    return MOSAIC_OK;
}

mosaic_status mosaic_unicode_load_file(const char *path, mosaic_unicode **out_unicode) {
    if (!path || !out_unicode) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t *data = NULL;
    size_t len = 0;
    if (!read_file(path, &data, &len)) return MOSAIC_ERROR_IO;
    mosaic_status status = mosaic_unicode_load_memory(data, len, out_unicode);
    free(data);
    return status;
}

void mosaic_unicode_free(mosaic_unicode *unicode_data) {
    if (!unicode_data) return;
    free(unicode_data->pack);
    free(unicode_data);
}

mosaic_status mosaic_security_load_memory(const uint8_t *pack, size_t pack_len, mosaic_security **out_security) {
    if (!out_security || (!pack && pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_security = NULL; mosaic_security *security=(mosaic_security*)calloc(1,sizeof *security); if(!security)return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status=copy_bytes(pack,pack_len,&security->pack);if(status!=MOSAIC_OK){free(security);return status;}security->pack_len=pack_len;
    if(!load_security_pack(security->pack,security->pack_len,&security->view)||!sha256_bytes(security->pack,security->pack_len,security->hash)){free(security->pack);free(security);return MOSAIC_ERROR_INVALID_PACK;}
    *out_security=security;return MOSAIC_OK;
}
mosaic_status mosaic_security_load_file(const char *path,mosaic_security **out_security){if(!path||!out_security)return MOSAIC_ERROR_INVALID_ARGUMENT;uint8_t*data=NULL;size_t len=0;if(!read_file(path,&data,&len))return MOSAIC_ERROR_IO;mosaic_status status=mosaic_security_load_memory(data,len,out_security);free(data);return status;}
void mosaic_security_free(mosaic_security *security){if(!security)return;free(security->pack);free(security);}
mosaic_status mosaic_security_script_name(const mosaic_security *security,uint16_t script_id,char *buffer,size_t capacity,size_t *out_required){if(!security||!script_id||script_id>security->view.script_count||!out_required)return MOSAIC_ERROR_INVALID_ARGUMENT;Slice name;uint16_t sid;if(!security_script_meta(&security->view,(uint32_t)script_id-1u,&name,&sid)||sid!=script_id)return MOSAIC_ERROR_INTERNAL;*out_required=name.len+1u;if(!buffer)return capacity==0?MOSAIC_OK:MOSAIC_ERROR_INVALID_ARGUMENT;if(capacity<name.len+1u)return MOSAIC_ERROR_OVERFLOW;memcpy(buffer,name.bytes,name.len);buffer[name.len]='\0';return MOSAIC_OK;}
mosaic_status mosaic_security_script_ranges(const mosaic_security *security,const uint8_t *input,size_t input_len,mosaic_script_span **out_ranges,size_t *out_count){if(!security||(!input&&input_len)||!out_ranges||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_ranges=NULL;*out_count=0;if(!security_script_ranges_internal(&security->view,(Slice){input,input_len},out_ranges,out_count))return MOSAIC_ERROR_OUT_OF_MEMORY;return MOSAIC_OK;}
mosaic_status mosaic_security_scan(const mosaic_security *security,const uint8_t *input,size_t input_len,mosaic_security_finding **out_findings,size_t *out_count){if(!security||(!input&&input_len)||!out_findings||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_findings=NULL;*out_count=0;if(!security_scan_internal(&security->view,(Slice){input,input_len},out_findings,out_count))return MOSAIC_ERROR_OUT_OF_MEMORY;return MOSAIC_OK;}

mosaic_status mosaic_grapheme_ranges(const mosaic_unicode *unicode_data, const uint8_t *input, size_t input_len,
                                     mosaic_range **out_ranges, size_t *out_count) {
    if (!unicode_data || (!input && input_len) || !out_ranges || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ranges = NULL;
    *out_count = 0;
    if (!compute_graphemes(&unicode_data->view, (Slice){input, input_len}, out_ranges, out_count))
        return MOSAIC_ERROR_OUT_OF_MEMORY;
    return MOSAIC_OK;
}



static void tokenizer_compute_fingerprint(mosaic_tokenizer *tokenizer) {
    static const uint8_t domain[] = "MOSAIC-TOKENIZER-RUNTIME\0semantics-v2\0";
    Sha256Ctx ctx;
    sha256_init(&ctx);
    (void)sha256_update(&ctx, domain, sizeof domain - 1u);
    (void)sha256_update(&ctx, tokenizer->model->pack, tokenizer->model->pack_len);
    (void)sha256_update(&ctx, tokenizer->unicode_data->pack, tokenizer->unicode_data->pack_len);
    uint8_t count_bytes[4];
    wr32be(count_bytes, (uint32_t)tokenizer->language_count);
    (void)sha256_update(&ctx, count_bytes, sizeof count_bytes);
    if (tokenizer->language_count) {
        uint8_t hashes[64][32];
        for (size_t i = 0; i < tokenizer->language_count; ++i)
            memcpy(hashes[i], tokenizer->languages[i].hash, 32);
        for (size_t i = 1; i < tokenizer->language_count; ++i) {
            uint8_t key[32]; memcpy(key, hashes[i], 32);
            size_t j = i;
            while (j && memcmp(hashes[j - 1u], key, 32) > 0) {
                memcpy(hashes[j], hashes[j - 1u], 32); --j;
            }
            memcpy(hashes[j], key, 32);
        }
        (void)sha256_update(&ctx, (const uint8_t *)hashes, tokenizer->language_count * 32u);
    }
    uint8_t detector_present = tokenizer->detector ? 1u : 0u;
    (void)sha256_update(&ctx, &detector_present, 1u);
    if (tokenizer->detector) (void)sha256_update(&ctx, tokenizer->detector->hash, 32u);
    if (tokenizer->security) {
        static const uint8_t security_domain[] = "SECURITY\0";
        (void)sha256_update(&ctx, security_domain, sizeof security_domain - 1u);
        (void)sha256_update(&ctx, tokenizer->security->hash, 32u);
    }
    (void)sha256_final(&ctx, tokenizer->fingerprint);
}

mosaic_status mosaic_tokenizer_load_memory(const uint8_t *model_pack, size_t model_pack_len,
                                           const uint8_t *unicode_pack, size_t unicode_pack_len,
                                           mosaic_tokenizer **out_tokenizer) {
    if (!out_tokenizer || (!model_pack && model_pack_len) || (!unicode_pack && unicode_pack_len))
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokenizer = NULL;
    mosaic_tokenizer *tokenizer = (mosaic_tokenizer *)calloc(1, sizeof *tokenizer);
    if (!tokenizer) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model_pack, model_pack_len, &tokenizer->model);
    if (status != MOSAIC_OK) { free(tokenizer); return status; }
    status = mosaic_unicode_load_memory(unicode_pack, unicode_pack_len, &tokenizer->unicode_data);
    if (status != MOSAIC_OK) { mosaic_model_free(tokenizer->model); free(tokenizer); return status; }
    tokenizer_compute_fingerprint(tokenizer);
    *out_tokenizer = tokenizer;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_load_files(const char *model_path, const char *unicode_path,
                                          mosaic_tokenizer **out_tokenizer) {
    if (!model_path || !unicode_path || !out_tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokenizer = NULL;
    uint8_t *model_pack = NULL, *unicode_pack = NULL;
    size_t model_len = 0, unicode_len = 0;
    if (!read_file(model_path, &model_pack, &model_len)) return MOSAIC_ERROR_IO;
    if (!read_file(unicode_path, &unicode_pack, &unicode_len)) { free(model_pack); return MOSAIC_ERROR_IO; }
    mosaic_status status = mosaic_tokenizer_load_memory(model_pack, model_len, unicode_pack, unicode_len, out_tokenizer);
    free(model_pack);
    free(unicode_pack);
    return status;
}


mosaic_status mosaic_tokenizer_add_language_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *language_pack, size_t language_pack_len) {
    if (!tokenizer || (!language_pack && language_pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->language_count >= 64) return MOSAIC_ERROR_OVERFLOW;
    LanguagePack candidate = {0};
    mosaic_status status = language_pack_copy(language_pack, language_pack_len, &candidate);
    if (status != MOSAIC_OK) return status;
    for (size_t i = 0; i < tokenizer->language_count; ++i) {
        if (language_same_tag(&tokenizer->languages[i], &candidate)) {
            language_pack_release(&candidate);
            return MOSAIC_ERROR_CONFLICT;
        }
    }
    size_t vocab_count = (size_t)tokenizer->model->vocab.count;
    if (!vocab_count) { language_pack_release(&candidate); return MOSAIC_ERROR_INTERNAL; }
    size_t adjustment_bytes;
    if (!mul_size(vocab_count, sizeof(int64_t), &adjustment_bytes)) {
        language_pack_release(&candidate); return MOSAIC_ERROR_OVERFLOW;
    }
    int64_t *next_adjustments = (int64_t *)malloc(adjustment_bytes);
    if (!next_adjustments) { language_pack_release(&candidate); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    candidate.adjustments = (int64_t *)calloc(vocab_count, sizeof(int64_t));
    if (!candidate.adjustments) { free(next_adjustments); language_pack_release(&candidate); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    if (tokenizer->adjustments) memcpy(next_adjustments, tokenizer->adjustments, adjustment_bytes);
    else memset(next_adjustments, 0, adjustment_bytes);
    for (uint32_t i = 0; i < tokenizer->model->vocab.count; ++i) {
        VocabEntry entry; Slice surface; int32_t delta = 0;
        if (!vocab_entry(&tokenizer->model->vocab, i, &entry, &surface) ||
            !language_surface_delta(&candidate.view, surface, &delta) ||
            !add_i64(next_adjustments[i], (int64_t)delta, &next_adjustments[i])) {
            free(next_adjustments); language_pack_release(&candidate); return MOSAIC_ERROR_INTERNAL;
        }
        candidate.adjustments[i] = (int64_t)delta;
        (void)entry;
    }
    if (tokenizer->language_count == tokenizer->language_capacity) {
        size_t next = tokenizer->language_capacity ? tokenizer->language_capacity * 2u : 4u;
        if (next > 64) next = 64;
        LanguagePack *grown = (LanguagePack *)realloc(tokenizer->languages, next * sizeof *grown);
        if (!grown) { free(next_adjustments); language_pack_release(&candidate); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        tokenizer->languages = grown;
        tokenizer->language_capacity = next;
    }
    free(tokenizer->adjustments);
    tokenizer->adjustments = next_adjustments;
    tokenizer->languages[tokenizer->language_count++] = candidate;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_add_language_file(mosaic_tokenizer *tokenizer, const char *path) {
    if (!tokenizer || !path) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t *pack = NULL; size_t len = 0;
    if (!read_file(path, &pack, &len)) return MOSAIC_ERROR_IO;
    mosaic_status status = mosaic_tokenizer_add_language_memory(tokenizer, pack, len);
    free(pack);
    return status;
}

size_t mosaic_tokenizer_language_count(const mosaic_tokenizer *tokenizer) {
    return tokenizer ? tokenizer->language_count : 0;
}

mosaic_status mosaic_tokenizer_language_tag(const mosaic_tokenizer *tokenizer, size_t index,
                                            char *buffer, size_t capacity, size_t *out_required) {
    if (!tokenizer || index >= tokenizer->language_count || !out_required) return MOSAIC_ERROR_INVALID_ARGUMENT;
    const LanguageView *view = &tokenizer->languages[index].view;
    size_t required = (size_t)view->tag_len + 1u;
    *out_required = required;
    if (!buffer) return capacity == 0 ? MOSAIC_OK : MOSAIC_ERROR_INVALID_ARGUMENT;
    if (capacity < required) return MOSAIC_ERROR_OVERFLOW;
    memcpy(buffer, view->section + view->tag_offset, view->tag_len);
    buffer[view->tag_len] = '\0';
    return MOSAIC_OK;
}


static mosaic_status tokenizer_clone(const mosaic_tokenizer *source, mosaic_tokenizer **out_tokenizer) {
    if (!source || !out_tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokenizer = NULL;
    mosaic_tokenizer *copy = NULL;
    mosaic_status status = mosaic_tokenizer_load_memory(source->model->pack, source->model->pack_len,
                                                        source->unicode_data->pack, source->unicode_data->pack_len,
                                                        &copy);
    if (status != MOSAIC_OK) return status;
    for (size_t i = 0; i < source->language_count; ++i) {
        status = mosaic_tokenizer_add_language_memory(copy, source->languages[i].pack, source->languages[i].pack_len);
        if (status != MOSAIC_OK) { mosaic_tokenizer_free(copy); return status; }
    }
    if (source->detector) {
        status = mosaic_tokenizer_set_detector_memory(copy, source->detector->pack, source->detector->pack_len);
        if (status != MOSAIC_OK) { mosaic_tokenizer_free(copy); return status; }
    }
    if (source->security) {
        status = mosaic_tokenizer_set_security_memory(copy, source->security->pack, source->security->pack_len);
        if (status != MOSAIC_OK) { mosaic_tokenizer_free(copy); return status; }
    }
    *out_tokenizer = copy;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_set_detector_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *detector_pack, size_t detector_pack_len) {
    if (!tokenizer || (!detector_pack && detector_pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->detector) return MOSAIC_ERROR_CONFLICT;
    mosaic_detector *detector = NULL;
    mosaic_status status = mosaic_detector_load_memory(detector_pack, detector_pack_len, &detector);
    if (status != MOSAIC_OK) return status;
    tokenizer->detector = detector;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_set_detector_file(mosaic_tokenizer *tokenizer, const char *path) {
    if (!tokenizer || !path) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->detector) return MOSAIC_ERROR_CONFLICT;
    mosaic_detector *detector = NULL;
    mosaic_status status = mosaic_detector_load_file(path, &detector);
    if (status != MOSAIC_OK) return status;
    tokenizer->detector = detector;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

int mosaic_tokenizer_detector_loaded(const mosaic_tokenizer *tokenizer) {
    return tokenizer && tokenizer->detector;
}

mosaic_status mosaic_tokenizer_set_security_memory(mosaic_tokenizer *tokenizer, const uint8_t *security_pack, size_t security_pack_len) {
    if (!tokenizer || (!security_pack && security_pack_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->security) return MOSAIC_ERROR_CONFLICT;
    mosaic_security *security = NULL;
    mosaic_status status = mosaic_security_load_memory(security_pack, security_pack_len, &security);
    if (status != MOSAIC_OK) return status;
    tokenizer->security = security;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_set_security_file(mosaic_tokenizer *tokenizer, const char *path) {
    if (!tokenizer || !path) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->security) return MOSAIC_ERROR_CONFLICT;
    mosaic_security *security = NULL;
    mosaic_status status = mosaic_security_load_file(path, &security);
    if (status != MOSAIC_OK) return status;
    tokenizer->security = security;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

int mosaic_tokenizer_security_loaded(const mosaic_tokenizer *tokenizer) { return tokenizer && tokenizer->security; }

mosaic_status mosaic_tokenizer_security_scan(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                              mosaic_security_finding **out_findings, size_t *out_count) {
    if (!tokenizer || !tokenizer->security) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return mosaic_security_scan(tokenizer->security, input, input_len, out_findings, out_count);
}

static const LanguagePack *tokenizer_language_for_detection(const mosaic_tokenizer *tokenizer,
                                                             const mosaic_detection *detection) {
    if (!detection->matched) return NULL;
    for (size_t i = 0; i < tokenizer->language_count; ++i)
        if (language_matches_cstr(&tokenizer->languages[i], detection->language)) return &tokenizer->languages[i];
    return NULL;
}

mosaic_status mosaic_tokenizer_detect_language(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_detection *out_detection) {
    if (!tokenizer || !tokenizer->detector || (!input && input_len) || !out_detection)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status status = mosaic_detector_detect(tokenizer->detector, input, input_len, out_detection);
    if (status != MOSAIC_OK) return status;
    out_detection->available = tokenizer_language_for_detection(tokenizer, out_detection) ? 1u : 0u;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_encode_auto(const mosaic_tokenizer *tokenizer,
                                           const uint8_t *input, size_t input_len,
                                           uint32_t **out_ids, size_t *out_count,
                                           mosaic_detection *out_detection) {
    if (!tokenizer || !out_ids || !out_count || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status status = mosaic_tokenizer_detect_language(tokenizer, input, input_len, out_detection);
    if (status != MOSAIC_OK) return status;
    const LanguagePack *pack = tokenizer_language_for_detection(tokenizer, out_detection);
    return encode_internal(tokenizer->model, input, input_len, pack ? pack->adjustments : NULL, out_ids, out_count);
}

mosaic_status mosaic_tokenizer_encode_tokens_auto(const mosaic_tokenizer *tokenizer,
                                                  const uint8_t *input, size_t input_len,
                                                  mosaic_token **out_tokens, size_t *out_count,
                                                  mosaic_detection *out_detection) {
    if (!tokenizer || !out_tokens || !out_count || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_status status = mosaic_tokenizer_detect_language(tokenizer, input, input_len, out_detection);
    if (status != MOSAIC_OK) return status;
    const LanguagePack *pack = tokenizer_language_for_detection(tokenizer, out_detection);
    return encode_tokens_internal(tokenizer->model, input, input_len, pack ? pack->adjustments : NULL, out_tokens, out_count);
}

void mosaic_tokenizer_free(mosaic_tokenizer *tokenizer) {
    if (!tokenizer) return;
    mosaic_model_free(tokenizer->model);
    mosaic_unicode_free(tokenizer->unicode_data);
    language_pack_array_free(tokenizer->languages, tokenizer->language_count);
    mosaic_detector_free(tokenizer->detector);
    mosaic_security_free(tokenizer->security);
    free(tokenizer->adjustments);
    free(tokenizer);
}

mosaic_status mosaic_tokenizer_fingerprint(const mosaic_tokenizer *tokenizer, uint8_t out_sha256[32]) {
    if (!tokenizer || !out_sha256) return MOSAIC_ERROR_INVALID_ARGUMENT;
    memcpy(out_sha256, tokenizer->fingerprint, 32);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_encode(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                      uint32_t **out_ids, size_t *out_count) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return encode_internal(tokenizer->model, input, input_len, tokenizer->adjustments, out_ids, out_count);
}

mosaic_status mosaic_tokenizer_encode_tokens(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                             mosaic_token **out_tokens, size_t *out_count) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return encode_tokens_internal(tokenizer->model, input, input_len, tokenizer->adjustments, out_tokens, out_count);
}

mosaic_status mosaic_tokenizer_decode(const mosaic_tokenizer *tokenizer, const uint32_t *ids, size_t count,
                                      uint8_t **out_bytes, size_t *out_len) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return mosaic_decode(tokenizer->model, ids, count, out_bytes, out_len);
}

mosaic_status mosaic_tokenizer_grapheme_ranges(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_range **out_ranges, size_t *out_count) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return mosaic_grapheme_ranges(tokenizer->unicode_data, input, input_len, out_ranges, out_count);
}

mosaic_status mosaic_tokenizer_stream_create(const mosaic_tokenizer *tokenizer, mosaic_stream **out_stream) {
    if (!tokenizer || !out_stream) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_stream = NULL;
    mosaic_stream *stream = (mosaic_stream *)calloc(1, sizeof *stream);
    if (!stream) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = tokenizer_clone(tokenizer, &stream->tokenizer);
    if (status != MOSAIC_OK) { free(stream); return status; }
    *out_stream = stream;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_stream_create_auto(const mosaic_tokenizer *tokenizer, mosaic_stream **out_stream) {
    mosaic_status status = mosaic_tokenizer_stream_create(tokenizer, out_stream);
    if (status == MOSAIC_OK) (*out_stream)->auto_mode = 1;
    return status;
}

mosaic_status mosaic_tokenizer_document_create(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                               mosaic_document **out_document) {
    if (!tokenizer || (!input && input_len) || !out_document) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    mosaic_document *document = (mosaic_document *)calloc(1, sizeof *document);
    if (!document) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = tokenizer_clone(tokenizer, &document->tokenizer);
    if (status != MOSAIC_OK) { free(document); return status; }
    if (input_len) {
        status = reserve_buffer(&document->buffer, &document->capacity, input_len);
        if (status != MOSAIC_OK) { mosaic_document_free(document); return status; }
        memcpy(document->buffer, input, input_len);
    }
    document->len = input_len;
    *out_document = document;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_document_create_auto(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                                    mosaic_document **out_document) {
    mosaic_status status = mosaic_tokenizer_document_create(tokenizer, input, input_len, out_document);
    if (status == MOSAIC_OK) (*out_document)->auto_mode = 1;
    return status;
}

#ifndef MOSAIC_LIBRARY_ONLY
static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s --version\n"
            "       %s validate MODEL_PACK\n"
            "       %s encode MODEL_PACK INPUT\n"
            "       %s roundtrip MODEL_PACK INPUT\n"
            "       %s encode-u32 MODEL_PACK INPUT OUTPUT\n"
            "       %s decode-u32 MODEL_PACK IDS OUTPUT\n"
            "       %s graphemes UNICODE_PACK INPUT\n"
            "       %s fingerprint MODEL_PACK UNICODE_PACK\n"
            "       %s fingerprint-languages MODEL_PACK UNICODE_PACK LANGUAGE_PACK...\n"
            "       %s analyze MODEL_PACK UNICODE_PACK INPUT\n"
            "       %s analyze-languages MODEL_PACK UNICODE_PACK INPUT LANGUAGE_PACK...\n"
            "       %s roundtrip-languages MODEL_PACK UNICODE_PACK INPUT LANGUAGE_PACK...\n"
            "       %s detect DETECTOR_PACK INPUT\n"
            "       %s fingerprint-auto MODEL_PACK UNICODE_PACK DETECTOR_PACK LANGUAGE_PACK...\n"
            "       %s analyze-auto MODEL_PACK UNICODE_PACK DETECTOR_PACK INPUT LANGUAGE_PACK...\n"
            "       %s roundtrip-auto MODEL_PACK UNICODE_PACK DETECTOR_PACK INPUT LANGUAGE_PACK...\n"
            "       %s security SECURITY_PACK INPUT\n"
            "       %s fingerprint-security MODEL_PACK UNICODE_PACK SECURITY_PACK\n"
            "       %s analyze-security MODEL_PACK UNICODE_PACK SECURITY_PACK INPUT\n",
            argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
            argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}

static void print_hex32(const uint8_t bytes[32]) {
    static const char hex[] = "0123456789abcdef";
    char out[65];
    for (size_t i = 0; i < 32; ++i) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[64] = '\0';
    puts(out);
}

static int integrated_cli(int argc, char **argv) {
    int fingerprint_only = !strcmp(argv[1], "fingerprint") || !strcmp(argv[1], "fingerprint-languages");
    int roundtrip_languages = !strcmp(argv[1], "roundtrip-languages");
    int with_languages = !strcmp(argv[1], "fingerprint-languages") || !strcmp(argv[1], "analyze-languages") || roundtrip_languages;
    if ((!with_languages && fingerprint_only && argc != 4) ||
        (!with_languages && !fingerprint_only && argc != 5) ||
        (with_languages && fingerprint_only && argc < 5) ||
        (with_languages && !fingerprint_only && argc < 6)) return -1;
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[2], argv[3], &tokenizer) != MOSAIC_OK) return 0;
    int language_start = fingerprint_only ? 4 : 5;
    if (with_languages) {
        for (int i = language_start; i < argc; ++i) {
            if (mosaic_tokenizer_add_language_file(tokenizer, argv[i]) != MOSAIC_OK) {
                mosaic_tokenizer_free(tokenizer); return 0;
            }
        }
    }
    uint8_t fingerprint[32];
    if (mosaic_tokenizer_fingerprint(tokenizer, fingerprint) != MOSAIC_OK) {
        mosaic_tokenizer_free(tokenizer); return 0;
    }
    if (fingerprint_only) {
        print_hex32(fingerprint);
        mosaic_tokenizer_free(tokenizer);
        return 1;
    }
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[4], &input, &input_len)) { mosaic_tokenizer_free(tokenizer); return 0; }
    if (roundtrip_languages) {
        uint32_t *ids = NULL; size_t id_count = 0; uint8_t *decoded = NULL; size_t decoded_len = 0;
        mosaic_status es = mosaic_tokenizer_encode(tokenizer, input, input_len, &ids, &id_count);
        mosaic_status ds = es == MOSAIC_OK ? mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) : es;
        int ok = es == MOSAIC_OK && ds == MOSAIC_OK && decoded_len == input_len && (!input_len || memcmp(decoded, input, input_len) == 0);
        if (ok) printf("OK bytes=%zu tokens=%zu languages=%zu\n", input_len, id_count, mosaic_tokenizer_language_count(tokenizer));
        mosaic_free(ids); mosaic_free(decoded); free(input); mosaic_tokenizer_free(tokenizer); return ok ? 1 : 0;
    }
    mosaic_token *tokens = NULL; size_t token_count = 0;
    mosaic_range *ranges = NULL; size_t range_count = 0;
    mosaic_status a = mosaic_tokenizer_encode_tokens(tokenizer, input, input_len, &tokens, &token_count);
    mosaic_status b = mosaic_tokenizer_grapheme_ranges(tokenizer, input, input_len, &ranges, &range_count);
    if (a != MOSAIC_OK || b != MOSAIC_OK) {
        mosaic_free(tokens); mosaic_free(ranges); free(input); mosaic_tokenizer_free(tokenizer); return 0;
    }
    printf("fingerprint=");
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) printf("%c%c", hex[fingerprint[i] >> 4], hex[fingerprint[i] & 0x0f]);
    printf(" bytes=%zu tokens=%zu graphemes=%zu languages=%zu\n",
           input_len, token_count, range_count, mosaic_tokenizer_language_count(tokenizer));
    for (size_t i = 0; i < token_count; ++i)
        printf("token id=%" PRIu32 " start=%" PRIu64 " length=%" PRIu64 " cost=%" PRId32 "\n",
               tokens[i].id, tokens[i].start, tokens[i].length, tokens[i].cost);
    for (size_t i = 0; i < range_count; ++i)
        printf("grapheme start=%" PRIu64 " length=%" PRIu64 "\n", ranges[i].start, ranges[i].length);
    mosaic_free(tokens); mosaic_free(ranges); free(input); mosaic_tokenizer_free(tokenizer);
    return 1;
}

static int security_cli(int argc, char **argv) {
    if (argc != 4) return -1;
    mosaic_security *security = NULL;
    if (mosaic_security_load_file(argv[2], &security) != MOSAIC_OK) return 0;
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[3], &input, &input_len)) { mosaic_security_free(security); return 0; }
    mosaic_script_span *spans = NULL; size_t span_count = 0;
    mosaic_security_finding *findings = NULL; size_t finding_count = 0;
    mosaic_status sa = mosaic_security_script_ranges(security, input, input_len, &spans, &span_count);
    mosaic_status sb = mosaic_security_scan(security, input, input_len, &findings, &finding_count);
    if (sa == MOSAIC_OK && sb == MOSAIC_OK) {
        printf("bytes=%zu script_spans=%zu findings=%zu\n", input_len, span_count, finding_count);
        for (size_t i = 0; i < span_count; ++i) {
            char name[64] = "INVALID"; size_t need = 0;
            if (spans[i].script_id == 0) strcpy(name, "OPAQUE");
            else if (mosaic_security_script_name(security, spans[i].script_id, name, sizeof name, &need) != MOSAIC_OK) strcpy(name, "UNKNOWN");
            printf("script start=%" PRIu64 " length=%" PRIu64 " id=%u name=%s\n",
                   spans[i].start, spans[i].length, (unsigned)spans[i].script_id, name);
        }
        for (size_t i = 0; i < finding_count; ++i)
            printf("finding kind=%u start=%" PRIu64 " length=%" PRIu64 " script=%u\n",
                   findings[i].kind, findings[i].start, findings[i].length, (unsigned)findings[i].script_id);
    }
    mosaic_free(spans); mosaic_free(findings); free(input); mosaic_security_free(security);
    return sa == MOSAIC_OK && sb == MOSAIC_OK ? 1 : 0;
}

static int integrated_security_cli(int argc, char **argv) {
    int fingerprint_only = !strcmp(argv[1], "fingerprint-security");
    if ((fingerprint_only && argc != 5) || (!fingerprint_only && argc != 6)) return -1;
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[2], argv[3], &tokenizer) != MOSAIC_OK) return 0;
    if (mosaic_tokenizer_set_security_file(tokenizer, argv[4]) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    uint8_t hash[32];
    if (mosaic_tokenizer_fingerprint(tokenizer, hash) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    if (fingerprint_only) { print_hex32(hash); mosaic_tokenizer_free(tokenizer); return 1; }
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[5], &input, &input_len)) { mosaic_tokenizer_free(tokenizer); return 0; }
    mosaic_security_finding *findings = NULL; size_t finding_count = 0;
    mosaic_token *tokens = NULL; size_t token_count = 0;
    mosaic_status sa = mosaic_tokenizer_encode_tokens(tokenizer, input, input_len, &tokens, &token_count);
    mosaic_status sb = mosaic_tokenizer_security_scan(tokenizer, input, input_len, &findings, &finding_count);
    if (sa == MOSAIC_OK && sb == MOSAIC_OK) {
        printf("fingerprint="); static const char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < 32; ++i) printf("%c%c", hex[hash[i] >> 4], hex[hash[i] & 15]);
        printf(" bytes=%zu tokens=%zu findings=%zu\n", input_len, token_count, finding_count);
        for (size_t i = 0; i < finding_count; ++i)
            printf("finding kind=%u start=%" PRIu64 " length=%" PRIu64 " script=%u\n",
                   findings[i].kind, findings[i].start, findings[i].length, (unsigned)findings[i].script_id);
    }
    mosaic_free(tokens); mosaic_free(findings); free(input); mosaic_tokenizer_free(tokenizer);
    return sa == MOSAIC_OK && sb == MOSAIC_OK ? 1 : 0;
}

static int detector_cli(int argc, char **argv) {
    if (argc != 4) return -1;
    mosaic_detector *detector = NULL;
    if (mosaic_detector_load_file(argv[2], &detector) != MOSAIC_OK) return 0;
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[3], &input, &input_len)) { mosaic_detector_free(detector); return 0; }
    mosaic_detection detection;
    mosaic_status status = mosaic_detector_detect(detector, input, input_len, &detection);
    if (status == MOSAIC_OK) {
        if (detection.matched) printf("language=%s score=%" PRId64 " margin=%" PRId64 "\n", detection.language, detection.score, detection.margin);
        else printf("language=none score=%" PRId64 " margin=%" PRId64 "\n", detection.score, detection.margin);
    }
    free(input); mosaic_detector_free(detector); return status == MOSAIC_OK ? 1 : 0;
}

static int auto_cli(int argc, char **argv) {
    int fingerprint_only = !strcmp(argv[1], "fingerprint-auto");
    int roundtrip_only = !strcmp(argv[1], "roundtrip-auto");
    if ((fingerprint_only && argc < 6) || (!fingerprint_only && argc < 7)) return -1;
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[2], argv[3], &tokenizer) != MOSAIC_OK) return 0;
    if (mosaic_tokenizer_set_detector_file(tokenizer, argv[4]) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    int language_start = fingerprint_only ? 5 : 6;
    for (int i = language_start; i < argc; ++i) {
        if (mosaic_tokenizer_add_language_file(tokenizer, argv[i]) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    }
    uint8_t fingerprint[32];
    if (mosaic_tokenizer_fingerprint(tokenizer, fingerprint) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    if (fingerprint_only) { print_hex32(fingerprint); mosaic_tokenizer_free(tokenizer); return 1; }
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[5], &input, &input_len)) { mosaic_tokenizer_free(tokenizer); return 0; }
    mosaic_detection detection;
    if (roundtrip_only) {
        uint32_t *ids=NULL; size_t id_count=0; uint8_t *decoded=NULL; size_t decoded_len=0;
        mosaic_status es=mosaic_tokenizer_encode_auto(tokenizer,input,input_len,&ids,&id_count,&detection);
        mosaic_status ds=es==MOSAIC_OK?mosaic_tokenizer_decode(tokenizer,ids,id_count,&decoded,&decoded_len):es;
        int ok=es==MOSAIC_OK&&ds==MOSAIC_OK&&decoded_len==input_len&&(!input_len||memcmp(decoded,input,input_len)==0);
        if(ok)printf("OK bytes=%zu tokens=%zu route=%s available=%u languages=%zu\n",input_len,id_count,detection.matched?detection.language:"none",detection.available,mosaic_tokenizer_language_count(tokenizer));
        mosaic_free(ids);mosaic_free(decoded);free(input);mosaic_tokenizer_free(tokenizer);return ok?1:0;
    }
    mosaic_token *tokens=NULL;size_t token_count=0;mosaic_range*ranges=NULL;size_t range_count=0;
    mosaic_status a=mosaic_tokenizer_encode_tokens_auto(tokenizer,input,input_len,&tokens,&token_count,&detection);
    mosaic_status b=mosaic_tokenizer_grapheme_ranges(tokenizer,input,input_len,&ranges,&range_count);
    if(a!=MOSAIC_OK||b!=MOSAIC_OK){mosaic_free(tokens);mosaic_free(ranges);free(input);mosaic_tokenizer_free(tokenizer);return 0;}
    printf("fingerprint=");static const char hex[]="0123456789abcdef";for(size_t i=0;i<32;++i)printf("%c%c",hex[fingerprint[i]>>4],hex[fingerprint[i]&15]);
    printf(" bytes=%zu tokens=%zu graphemes=%zu route=%s available=%u score=%" PRId64 " margin=%" PRId64 " languages=%zu\n",
           input_len,token_count,range_count,detection.matched?detection.language:"none",detection.available,detection.score,detection.margin,mosaic_tokenizer_language_count(tokenizer));
    for(size_t i=0;i<token_count;++i)printf("token id=%" PRIu32 " start=%" PRIu64 " length=%" PRIu64 " base_cost=%" PRId32 "\n",tokens[i].id,tokens[i].start,tokens[i].length,tokens[i].cost);
    for(size_t i=0;i<range_count;++i)printf("grapheme start=%" PRIu64 " length=%" PRIu64 "\n",ranges[i].start,ranges[i].length);
    mosaic_free(tokens);mosaic_free(ranges);free(input);mosaic_tokenizer_free(tokenizer);return 1;
}

int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "version"))) {
        printf("mosaic-tokenizer %s\n", mosaic_version_string());
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "security")) {
        int result=security_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint-security") || !strcmp(argv[1], "analyze-security"))) {
        int result=integrated_security_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && !strcmp(argv[1], "detect")) {
        int result=detector_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint-auto") || !strcmp(argv[1], "analyze-auto") || !strcmp(argv[1], "roundtrip-auto"))) {
        int result=auto_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint") || !strcmp(argv[1], "analyze") ||
                      !strcmp(argv[1], "fingerprint-languages") || !strcmp(argv[1], "analyze-languages") ||
                      !strcmp(argv[1], "roundtrip-languages"))) {
        int result = integrated_cli(argc, argv);
        if (result < 0) { usage(argv[0]); return 2; }
        return result ? 0 : 1;
    }
    if (argc < 3) { usage(argv[0]); return 2; }
    uint8_t *pack = NULL; size_t pack_len = 0; if (!read_file(argv[2], &pack, &pack_len)) return 1;
    if (!strcmp(argv[1], "graphemes")) {
        if (argc != 4) { usage(argv[0]); free(pack); return 2; }
        UnicodeView unicode; if (!load_unicode_pack(pack, pack_len, &unicode)) { free(pack); return 1; }
        uint8_t *input=NULL; size_t input_len=0; if(!read_file(argv[3],&input,&input_len)){free(pack);return 1;}
        int ok=print_graphemes(&unicode,(Slice){input,input_len});free(input);free(pack);return ok?0:1;
    }
    Vocab v; if (!load_model_pack(pack, pack_len, &v)) { free(pack); return 1; }
    if (!strcmp(argv[1], "validate")) { printf("OK vocabulary_entries=%" PRIu32 " pack_bytes=%zu\n", v.count, pack_len); free(pack); return 0; }
    if (!strcmp(argv[1], "decode-u32")) {
        if (argc != 5) { usage(argv[0]); free(pack); return 2; }
        int ok = decode_u32_file(&v, argv[3], argv[4]);
        free(pack);
        return ok ? 0 : 1;
    }
    if (!strcmp(argv[1], "encode-u32")) {
        if (argc != 5) { usage(argv[0]); free(pack); return 2; }
        uint8_t *input = NULL; size_t input_len = 0;
        if (!read_file(argv[3], &input, &input_len)) { free(pack); return 1; }
        Tokenization result = {0};
        if (!tokenize_with_adjustments(&v, (Slice){input, input_len}, NULL, &result)) { free(input); free(pack); return 1; }
        int ok = roundtrip(&v, (Slice){input, input_len}, &result)
            && write_encoded_u32(&v, &result, argv[4]);
        tokenization_free(&result); free(input); free(pack);
        return ok ? 0 : 1;
    }
    if (argc != 4) { usage(argv[0]); free(pack); return 2; }
    uint8_t *input = NULL; size_t input_len = 0; if (!read_file(argv[3], &input, &input_len)) { free(pack); return 1; }
    Tokenization result = {0}; if (!tokenize_with_adjustments(&v, (Slice){input, input_len}, NULL, &result)) { free(input); free(pack); return 1; }
    int ok = roundtrip(&v, (Slice){input, input_len}, &result);
    if (!ok) { fail("internal round-trip mismatch"); tokenization_free(&result); free(input); free(pack); return 1; }
    if (!strcmp(argv[1], "encode")) print_encoding(&v, (Slice){input, input_len}, &result);
    else if (!strcmp(argv[1], "roundtrip")) printf("OK bytes=%zu tokens=%zu cost=%" PRId64 "\n", input_len, result.count, result.cost);
    else { usage(argv[0]); tokenization_free(&result); free(input); free(pack); return 2; }
    tokenization_free(&result); free(input); free(pack); return 0;
}
#endif /* MOSAIC_LIBRARY_ONLY */
