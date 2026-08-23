#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
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
    VocabEntry *runtime_entries; /* validated native-endian execution table */
    uint32_t *runtime_id_index;
    uint32_t runtime_first[257];
    uint32_t *bpe_surface_slots; /* entry index + 1, zero means empty */
    size_t bpe_surface_capacity;
    uint32_t *bpe_pair_entry; /* 65,536 direct two-byte surface entries, entry index + 1 */
    uint8_t *bpe_bigram_bits; /* vocabulary bigrams: exact proof that a boundary may be crossed */
    uint32_t bpe_byte_entry[256];
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

typedef struct { uint32_t offset; uint16_t len; uint16_t flags; } LexerShort;
typedef struct { uint32_t start_offset; uint32_t end_offset; uint16_t start_len; uint16_t end_len; uint16_t flags; uint16_t reserved; } LexerBlock;
typedef struct { uint32_t offset; uint16_t len; uint8_t escape; uint8_t flags; uint32_t reserved; } LexerString;
typedef struct {
    const uint8_t *section; size_t section_len;
    uint32_t line_count, block_count, string_count, keyword_count;
    uint32_t line_offset, block_offset, string_offset, keyword_offset;
    uint32_t name_offset; uint16_t name_len; uint16_t flags;
    uint32_t blob_offset, blob_len, max_delim_len;
} LexerView;

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
} PrefixPathToken;

typedef struct {
    PrefixPathToken *candidate;
    PrefixPathToken *current;
    size_t capacity;
} PrefixTieScratch;

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
int mosaic_internal_sha256(const uint8_t*data,size_t len,uint8_t out[32]){Sha256Ctx c;sha256_init(&c);return sha256_update(&c,data,len)&&sha256_final(&c,out);}
int mosaic_internal_sha256_zero_range(const uint8_t *data,size_t len,size_t zero_offset,size_t zero_length,uint8_t out[32]){
    static const uint8_t zeros[64]={0};
    if(!data||!out||zero_offset>len||zero_length>len-zero_offset)return 0;
    Sha256Ctx c;sha256_init(&c);
    if(!sha256_update(&c,data,zero_offset))return 0;
    size_t remaining=zero_length;while(remaining){size_t n=remaining>sizeof zeros?sizeof zeros:remaining;if(!sha256_update(&c,zeros,n))return 0;remaining-=n;}
    return sha256_update(&c,data+zero_offset+zero_length,len-zero_offset-zero_length)&&sha256_final(&c,out);
}
static int sha256_bytes(const uint8_t*data,size_t len,uint8_t out[32]){return mosaic_internal_sha256(data,len,out);}

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}
static void wr16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void wr64(uint8_t *p, uint64_t v) { wr32(p,(uint32_t)v); wr32(p+4,(uint32_t)(v>>32)); }
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
    if (v->runtime_entries) {
        *out = v->runtime_entries[index];
        surface->bytes = v->section + v->blob_offset + out->surface_offset;
        surface->len = out->surface_len;
        return 1;
    }
    const uint8_t *e = v->section + v->entries_offset + (size_t)index * 16;
    out->token_id = rd32(e); out->cost = rdi32(e + 4); out->surface_offset = rd32(e + 8); out->surface_len = rd16(e + 12);
    if (rd16(e + 14) != 0 || !out->surface_len || out->surface_offset > v->blob_len || out->surface_len > v->blob_len - out->surface_offset) return 0;
    surface->bytes = v->section + v->blob_offset + out->surface_offset; surface->len = out->surface_len; return 1;
}

static uint32_t vocab_id_index(const Vocab *v, uint32_t pos) {
    return v->runtime_id_index ? v->runtime_id_index[pos] : rd32(v->section + v->id_index_offset + (size_t)pos * 4);
}
static uint32_t vocab_first(const Vocab *v, uint32_t pos) {
    return v->runtime_entries ? v->runtime_first[pos] : rd32(v->section + v->first_index_offset + (size_t)pos * 4);
}

static uint64_t bpe_surface_hash(Slice surface) {
    uint64_t hash = UINT64_C(1469598103934665603) ^ (uint64_t)surface.len;
    for (size_t i = 0; i < surface.len; ++i) {
        hash ^= surface.bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    hash ^= hash >> 32;
    hash *= UINT64_C(0xd6e8feb86659fd93);
    hash ^= hash >> 32;
    return hash;
}

static void vocab_release_runtime_index(Vocab *v) {
    if (!v) return;
    free(v->runtime_entries);
    free(v->runtime_id_index);
    free(v->bpe_surface_slots);
    free(v->bpe_pair_entry);
    free(v->bpe_bigram_bits);
    v->runtime_entries = NULL;
    v->runtime_id_index = NULL;
    v->bpe_surface_slots = NULL;
    v->bpe_pair_entry = NULL;
    v->bpe_bigram_bits = NULL;
    v->bpe_surface_capacity = 0;
}

static int vocab_build_runtime_index(Vocab *v) {
    VocabEntry *entries = (VocabEntry *)malloc((size_t)v->count * sizeof *entries);
    uint32_t *ids = (uint32_t *)malloc((size_t)v->count * sizeof *ids);
    if (!entries || !ids) { free(entries); free(ids); return 0; }
    for (uint32_t i = 0; i < v->count; ++i) {
        const uint8_t *raw = v->section + v->entries_offset + (size_t)i * 16u;
        entries[i] = (VocabEntry){rd32(raw), rdi32(raw + 4), rd32(raw + 8), rd16(raw + 12)};
    }
    for (uint32_t i = 0; i < v->count; ++i) ids[i] = rd32(v->section + v->id_index_offset + (size_t)i * 4u);
    for (uint32_t i = 0; i <= 256u; ++i) v->runtime_first[i] = rd32(v->section + v->first_index_offset + (size_t)i * 4u);
    v->runtime_entries = entries;
    v->runtime_id_index = ids;
    return 1;
}

static int vocab_build_bpe_runtime_index(Vocab *v) {
    if (v->algorithm != 1u) return 1;
    size_t capacity = 512u;
    while (capacity < (size_t)v->count * 2u) {
        if (capacity > SIZE_MAX / 2u) return 0;
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(uint32_t)) return 0;
    uint32_t *slots = (uint32_t *)calloc(capacity, sizeof *slots);
    uint32_t *pairs = (uint32_t *)calloc(65536u, sizeof *pairs);
    uint8_t *bigrams = (uint8_t *)calloc(8192u, 1u);
    if (!slots || !pairs || !bigrams) { free(slots); free(pairs); free(bigrams); return 0; }
    for (size_t i = 0; i < 256u; ++i) v->bpe_byte_entry[i] = UINT32_MAX;
    for (uint32_t index = 0; index < v->count; ++index) {
        VocabEntry entry; Slice surface;
        if (!vocab_entry(v, index, &entry, &surface)) { free(slots); free(pairs); free(bigrams); return 0; }
        if (surface.len == 1u) v->bpe_byte_entry[surface.bytes[0]] = index;
        else {
            if (surface.len == 2u) pairs[((uint32_t)surface.bytes[0] << 8) | surface.bytes[1]] = index + 1u;
            for (size_t j = 1; j < surface.len; ++j) {
                uint32_t key = ((uint32_t)surface.bytes[j - 1u] << 8) | surface.bytes[j];
                bigrams[key >> 3] |= (uint8_t)(1u << (key & 7u));
            }
        }
        size_t slot = (size_t)bpe_surface_hash(surface) & (capacity - 1u);
        while (slots[slot]) slot = (slot + 1u) & (capacity - 1u);
        slots[slot] = index + 1u;
    }
    for (size_t i = 0; i < 256u; ++i) if (v->bpe_byte_entry[i] == UINT32_MAX) { free(slots); free(pairs); free(bigrams); return 0; }
    v->bpe_surface_slots = slots;
    v->bpe_surface_capacity = capacity;
    v->bpe_pair_entry = pairs;
    v->bpe_bigram_bits = bigrams;
    return 1;
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
    *v = (Vocab){.section=s.bytes,.section_len=s.len,.count=count,.entries_offset=entries,.id_index_offset=ids,.first_index_offset=first,.blob_offset=blob,.blob_len=blob_len,.max_surface_len=0,.algorithm=algorithm};

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
    if (!vocab_build_runtime_index(v)) return fail("out of memory building vocabulary runtime index");
    if (v->algorithm == 1u && !vocab_build_bpe_runtime_index(v)) {
        vocab_release_runtime_index(v);
        return fail("out of memory building BPE runtime index");
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

static int lexer_blob_slice(const LexerView *v, uint32_t off, uint16_t len, Slice *out) {
    if (!len || off > v->blob_len || len > v->blob_len - off) return 0;
    out->bytes = v->section + v->blob_offset + off; out->len = len; return 1;
}
static int lexer_short_at(const LexerView *v, uint32_t base, uint32_t count, uint32_t index, LexerShort *out, Slice *surface) {
    if (index >= count) return 0;
    const uint8_t *e = v->section + base + (size_t)index * 8u;
    out->offset = rd32(e); out->len = rd16(e + 4); out->flags = rd16(e + 6);
    return lexer_blob_slice(v, out->offset, out->len, surface);
}
static int lexer_block_at(const LexerView *v, uint32_t index, LexerBlock *out, Slice *start, Slice *end) {
    if (index >= v->block_count) return 0;
    const uint8_t *e = v->section + v->block_offset + (size_t)index * 16u;
    out->start_offset = rd32(e); out->end_offset = rd32(e + 4); out->start_len = rd16(e + 8); out->end_len = rd16(e + 10);
    out->flags = rd16(e + 12); out->reserved = rd16(e + 14);
    return !out->reserved && !(out->flags & ~1u) && lexer_blob_slice(v, out->start_offset, out->start_len, start) && lexer_blob_slice(v, out->end_offset, out->end_len, end);
}
static int lexer_string_at(const LexerView *v, uint32_t index, LexerString *out, Slice *delim) {
    if (index >= v->string_count) return 0;
    const uint8_t *e = v->section + v->string_offset + (size_t)index * 12u;
    out->offset = rd32(e); out->len = rd16(e + 4); out->escape = e[6]; out->flags = e[7]; out->reserved = rd32(e + 8);
    return !out->reserved && !out->flags && lexer_blob_slice(v, out->offset, out->len, delim);
}
static int parse_lexer(Slice s, LexerView *v) {
    if (s.len < 64 || memcmp(s.bytes, "MSLX", 4) != 0 || rd16(s.bytes + 4) != 1 || rd16(s.bytes + 6) != 0) return fail("invalid lexer header");
    uint32_t lc=rd32(s.bytes+8),bc=rd32(s.bytes+12),sc=rd32(s.bytes+16),kc=rd32(s.bytes+20);
    uint32_t lo=rd32(s.bytes+24),bo=rd32(s.bytes+28),so=rd32(s.bytes+32),ko=rd32(s.bytes+36),no=rd32(s.bytes+40);
    uint16_t nl=rd16(s.bytes+44),flags=rd16(s.bytes+46); uint32_t blob=rd32(s.bytes+48),blob_len=rd32(s.bytes+52),maxd=rd32(s.bytes+56);
    if (rd32(s.bytes+60) || lo!=64 || lc>64 || bc>32 || sc>32 || kc>4096 || !nl || nl>63 || (flags & ~1u)) return fail("invalid lexer layout");
    uint64_t le=(uint64_t)lo+lc*8u,be=(uint64_t)bo+bc*16u,se=(uint64_t)so+sc*12u,ke=(uint64_t)ko+kc*8u,ble=(uint64_t)blob+blob_len;
    if (!(le<=bo && be<=so && se<=ko && ke<=blob && ble==s.len)) return fail("lexer regions overlap/out of bounds");
    if (no>blob_len || nl>blob_len-no) return fail("lexer name out of bounds");
    if (!all_zero(s.bytes+(size_t)le,bo-(size_t)le)||!all_zero(s.bytes+(size_t)be,so-(size_t)be)||!all_zero(s.bytes+(size_t)se,ko-(size_t)se)||!all_zero(s.bytes+(size_t)ke,blob-(size_t)ke)) return fail("lexer padding nonzero");
    *v=(LexerView){s.bytes,s.len,lc,bc,sc,kc,lo,bo,so,ko,no,nl,flags,blob,blob_len,maxd};
    Slice prev={0}; uint32_t observed=0;
    for(uint32_t i=0;i<lc;++i){LexerShort r;Slice x;if(!lexer_short_at(v,lo,lc,i,&r,&x)||r.flags)return fail("invalid line delimiter");if(i&&compare_slices(prev,x)>=0)return fail("line delimiters not canonical");prev=x;if(x.len>observed)observed=(uint32_t)x.len;}
    prev=(Slice){0};for(uint32_t i=0;i<bc;++i){LexerBlock r;Slice a,z;if(!lexer_block_at(v,i,&r,&a,&z))return fail("invalid block delimiter");if(i&&compare_slices(prev,a)>=0)return fail("block delimiters not canonical");prev=a;if(a.len>observed)observed=(uint32_t)a.len;if(z.len>observed)observed=(uint32_t)z.len;}
    prev=(Slice){0};for(uint32_t i=0;i<sc;++i){LexerString r;Slice x;if(!lexer_string_at(v,i,&r,&x))return fail("invalid string delimiter");if(i&&compare_slices(prev,x)>=0)return fail("string delimiters not canonical");prev=x;if(x.len>observed)observed=(uint32_t)x.len;}
    prev=(Slice){0};for(uint32_t i=0;i<kc;++i){LexerShort r;Slice x;if(!lexer_short_at(v,ko,kc,i,&r,&x)||r.flags)return fail("invalid keyword");if(i&&compare_slices(prev,x)>=0)return fail("keywords not canonical");prev=x;}
    if(observed!=maxd)return fail("lexer max delimiter mismatch");
    return 1;
}
static int load_lexer_pack(const uint8_t *data,size_t len,LexerView *v){Section*sections=NULL;uint32_t count=0,mi=0,li=0;if(!parse_outer(data,len,&sections,&count,&mi,&li))return 0;int n=0;Section ls={0};for(uint32_t i=0;i<count;++i)if(sections[i].kind==9){++n;ls=sections[i];}if(n!=1){free(sections);return fail("expected exactly one lexer section");}int ok=validate_manifest_lock(sec_slice(data,&sections[mi]),sec_slice(data,&sections[li]))&&parse_lexer(sec_slice(data,&ls),v);free(sections);return ok;}
static int lexer_prefix_at(const uint8_t *input,size_t len,size_t pos,Slice x){return x.len<=len-pos&&memcmp(input+pos,x.bytes,x.len)==0;}
static int lexer_keyword(const LexerView*v,const uint8_t*input,size_t len){uint32_t lo=0,hi=v->keyword_count;Slice target={input,len};while(lo<hi){uint32_t mid=lo+(hi-lo)/2;LexerShort r;Slice x;if(!lexer_short_at(v,v->keyword_offset,v->keyword_count,mid,&r,&x))return 0;int c=compare_slices(x,target);if(c<0)lo=mid+1;else if(c>0)hi=mid;else return 1;}return 0;}
static int lex_token_append(mosaic_lex_token **out,size_t *count,size_t *cap,uint32_t kind,size_t start,size_t end){if(end<=start)return 0;if(*count==*cap){size_t n=*cap?*cap*2u:64u;if(n<*count||n>SIZE_MAX/sizeof**out)return 0;void*p=realloc(*out,n*sizeof**out);if(!p)return 0;*out=(mosaic_lex_token*)p;*cap=n;}(*out)[(*count)++]=(mosaic_lex_token){kind,0u,(uint64_t)start,(uint64_t)(end-start)};return 1;}
static int ascii_ident_start(uint8_t c,int dollar){return(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||(dollar&&c=='$')||c>=0x80;}
static int ascii_ident_continue(uint8_t c,int dollar){return ascii_ident_start(c,dollar)||(c>='0'&&c<='9');}
static int lex_view(const LexerView *v, const uint8_t *input, size_t len, mosaic_lex_token **out, size_t *out_count) {
    *out = NULL; *out_count = 0;
    size_t cap = 0, pos = 0;
    int dollar = (v->flags & 1u) != 0;
    while (pos < len) {
        size_t start = pos;
        uint8_t c = input[pos];
        if (c == '\r' || c == '\n') {
            if (c == '\r' && pos + 1u < len && input[pos + 1u] == '\n') pos += 2u; else ++pos;
            if (!lex_token_append(out, out_count, &cap, MOSAIC_LEX_NEWLINE, start, pos)) goto oom;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\v' || c == '\f') {
            do { ++pos; } while (pos < len && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\v' || input[pos] == '\f'));
            if (!lex_token_append(out, out_count, &cap, MOSAIC_LEX_WHITESPACE, start, pos)) goto oom;
            continue;
        }
        uint32_t best = UINT32_MAX; size_t best_len = 0;
        for (uint32_t i = 0; i < v->line_count; ++i) {
            LexerShort r; Slice x;
            if (!lexer_short_at(v, v->line_offset, v->line_count, i, &r, &x)) goto oom;
            if (x.len > best_len && lexer_prefix_at(input, len, pos, x)) { best = i; best_len = x.len; }
        }
        if (best != UINT32_MAX) {
            pos += best_len; while (pos < len && input[pos] != '\r' && input[pos] != '\n') ++pos;
            if (!lex_token_append(out, out_count, &cap, MOSAIC_LEX_COMMENT, start, pos)) goto oom;
            continue;
        }
        best = UINT32_MAX; best_len = 0;
        for (uint32_t i = 0; i < v->block_count; ++i) {
            LexerBlock r; Slice a, z;
            if (!lexer_block_at(v, i, &r, &a, &z)) goto oom;
            if (a.len > best_len && lexer_prefix_at(input, len, pos, a)) { best = i; best_len = a.len; }
        }
        if (best != UINT32_MAX) {
            LexerBlock r; Slice a, z;
            if (!lexer_block_at(v, best, &r, &a, &z)) goto oom;
            pos += a.len; size_t depth = 1u;
            while (pos < len && depth) {
                if ((r.flags & 1u) && lexer_prefix_at(input, len, pos, a)) { ++depth; pos += a.len; continue; }
                if (lexer_prefix_at(input, len, pos, z)) { --depth; pos += z.len; continue; }
                ++pos;
            }
            if (!lex_token_append(out, out_count, &cap, depth ? MOSAIC_LEX_ERROR : MOSAIC_LEX_COMMENT, start, pos)) goto oom;
            continue;
        }
        best = UINT32_MAX; best_len = 0;
        for (uint32_t i = 0; i < v->string_count; ++i) {
            LexerString r; Slice d;
            if (!lexer_string_at(v, i, &r, &d)) goto oom;
            if (d.len > best_len && lexer_prefix_at(input, len, pos, d)) { best = i; best_len = d.len; }
        }
        if (best != UINT32_MAX) {
            LexerString r; Slice d;
            if (!lexer_string_at(v, best, &r, &d)) goto oom;
            pos += d.len; int closed = 0;
            while (pos < len) {
                if (r.escape && input[pos] == r.escape) { pos += pos + 1u < len ? 2u : 1u; continue; }
                if (lexer_prefix_at(input, len, pos, d)) { pos += d.len; closed = 1; break; }
                ++pos;
            }
            if (!lex_token_append(out, out_count, &cap, closed ? MOSAIC_LEX_STRING : MOSAIC_LEX_ERROR, start, pos)) goto oom;
            continue;
        }
        if (ascii_ident_start(c, dollar)) {
            ++pos; while (pos < len && ascii_ident_continue(input[pos], dollar)) ++pos;
            uint32_t kind = lexer_keyword(v, input + start, pos - start) ? MOSAIC_LEX_KEYWORD : MOSAIC_LEX_IDENTIFIER;
            if (!lex_token_append(out, out_count, &cap, kind, start, pos)) goto oom;
            continue;
        }
        if (c >= '0' && c <= '9') {
            ++pos; int exp_sign = 0;
            while (pos < len) {
                uint8_t q = input[pos];
                if ((q >= '0' && q <= '9') || (q >= 'A' && q <= 'Z') || (q >= 'a' && q <= 'z') || q == '_' || q == '.' || q == '\'') {
                    exp_sign = q == 'e' || q == 'E' || q == 'p' || q == 'P'; ++pos; continue;
                }
                if (exp_sign && (q == '+' || q == '-')) { exp_sign = 0; ++pos; continue; }
                break;
            }
            if (!lex_token_append(out, out_count, &cap, MOSAIC_LEX_NUMBER, start, pos)) goto oom;
            continue;
        }
        ++pos;
        if (!lex_token_append(out, out_count, &cap, MOSAIC_LEX_PUNCTUATION, start, pos)) goto oom;
    }
    return 1;
oom:
    free(*out); *out = NULL; *out_count = 0; return 0;
}

static int semantic_append(mosaic_semantic_component **out,size_t *count,size_t *cap,uint32_t kind,size_t lex_index,size_t start,size_t end){
    if(end<=start)return 1;
    if(*count==*cap){size_t n=*cap?*cap*2u:32u;if(n<*count||n>SIZE_MAX/sizeof**out)return 0;void*p=realloc(*out,n*sizeof**out);if(!p)return 0;*out=(mosaic_semantic_component*)p;*cap=n;}
    (*out)[(*count)++]=(mosaic_semantic_component){kind,0u,(uint64_t)lex_index,(uint64_t)start,(uint64_t)(end-start)};return 1;
}
static int ascii_upper(uint8_t c){return c>='A'&&c<='Z';}
static int ascii_lower(uint8_t c){return c>='a'&&c<='z';}
static int ascii_digit(uint8_t c){return c>='0'&&c<='9';}
static int semantic_identifier(const uint8_t*source,size_t start,size_t end,size_t li,mosaic_semantic_component**out,size_t*count,size_t*cap){
    size_t part=start,i=start;
    while(i<end){uint8_t c=source[i];
        if(c=='_'||c=='$'){if(!semantic_append(out,count,cap,MOSAIC_SEM_IDENTIFIER_PART,li,part,i))return 0;part=++i;continue;}
        if(i>part){uint8_t prev=source[i-1];uint8_t next=i+1<end?source[i+1]:0;int split=(ascii_digit(c)&&!ascii_digit(prev))||(!ascii_digit(c)&&ascii_digit(prev))||(ascii_upper(c)&&ascii_lower(prev))||(ascii_upper(c)&&ascii_upper(prev)&&ascii_lower(next));if(split){if(!semantic_append(out,count,cap,MOSAIC_SEM_IDENTIFIER_PART,li,part,i))return 0;part=i;}}
        ++i;
    }
    return semantic_append(out,count,cap,MOSAIC_SEM_IDENTIFIER_PART,li,part,end);
}
static int semantic_number(const uint8_t*source,size_t start,size_t end,size_t li,mosaic_semantic_component**out,size_t*count,size_t*cap){
    size_t p=start;
    if(p<end&&(source[p]=='+'||source[p]=='-')){if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_SIGN,li,p,p+1))return 0;++p;}
    if(p+1<end&&source[p]=='0'&&(source[p+1]=='x'||source[p+1]=='X'||source[p+1]=='b'||source[p+1]=='B'||source[p+1]=='o'||source[p+1]=='O')){if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_RADIX_PREFIX,li,p,p+2))return 0;p+=2;}
    size_t integer=p;while(p<end&&source[p]!='.'&&source[p]!='e'&&source[p]!='E'&&source[p]!='p'&&source[p]!='P')++p;
    if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_INTEGER,li,integer,p))return 0;
    if(p<end&&source[p]=='.'){size_t q=++p;while(p<end&&source[p]!='e'&&source[p]!='E'&&source[p]!='p'&&source[p]!='P')++p;if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_FRACTION,li,q,p))return 0;}
    if(p<end&&(source[p]=='e'||source[p]=='E'||source[p]=='p'||source[p]=='P')){if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_EXPONENT_MARK,li,p,p+1))return 0;++p;if(p<end&&(source[p]=='+'||source[p]=='-')){if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_EXPONENT_SIGN,li,p,p+1))return 0;++p;}if(!semantic_append(out,count,cap,MOSAIC_SEM_NUMBER_EXPONENT_DIGITS,li,p,end))return 0;}
    return 1;
}
static int semantic_string(const LexerView*v,const uint8_t*source,size_t source_len,const mosaic_lex_token*t,size_t li,mosaic_semantic_component**out,size_t*count,size_t*cap){
    size_t start=(size_t)t->start,end=start+(size_t)t->length;if(end>source_len)return 0;uint32_t best=UINT32_MAX;size_t best_len=0;
    for(uint32_t i=0;i<v->string_count;++i){LexerString r;Slice d;if(!lexer_string_at(v,i,&r,&d))return 0;if(d.len>best_len&&lexer_prefix_at(source,source_len,start,d)){best=i;best_len=d.len;}}
    if (best == UINT32_MAX || best_len > end - start) return 1;
    LexerString r; Slice d;
    if (!lexer_string_at(v, best, &r, &d)) return 0;
    if(!semantic_append(out,count,cap,MOSAIC_SEM_STRING_DELIMITER,li,start,start+d.len))return 0;
    size_t content_start=start+d.len,content_end=end;
    if(end>=d.len&&end-d.len>=content_start&&lexer_prefix_at(source,source_len,end-d.len,d)){content_end=end-d.len;if(!semantic_append(out,count,cap,MOSAIC_SEM_STRING_DELIMITER,li,content_end,end))return 0;}
    return semantic_append(out,count,cap,MOSAIC_SEM_STRING_CONTENT,li,content_start,content_end);
}
static int semantic_enrich(const LexerView*v,const uint8_t*source,size_t source_len,const mosaic_lex_token*tokens,size_t token_count,mosaic_semantic_component**out,size_t*out_count){
    *out=NULL;*out_count=0;size_t cap=0;
    for(size_t i=0;i<token_count;++i){const mosaic_lex_token*t=&tokens[i];size_t start=(size_t)t->start,end=start+(size_t)t->length;if(start>source_len||end<start||end>source_len)goto fail;
        if(t->kind==MOSAIC_LEX_IDENTIFIER){if(!semantic_identifier(source,start,end,i,out,out_count,&cap))goto fail;}
        else if(t->kind==MOSAIC_LEX_NUMBER){if(!semantic_number(source,start,end,i,out,out_count,&cap))goto fail;}
        else if(t->kind==MOSAIC_LEX_STRING){if(!semantic_string(v,source,source_len,t,i,out,out_count,&cap))goto fail;}
    }
    return 1;
fail:free(*out);*out=NULL;*out_count=0;return 0;
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

static int slice_compare_bytes(Slice a, Slice b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int cmp = n ? memcmp(a.bytes, b.bytes, n) : 0;
    if (cmp) return cmp;
    return a.len < b.len ? -1 : a.len > b.len ? 1 : 0;
}

static int vocab_find_surface(const Vocab *v, Slice target, uint32_t *out_index) {
    if (!target.len) return 0;
    if (v->bpe_surface_slots && v->bpe_surface_capacity) {
        size_t mask = v->bpe_surface_capacity - 1u;
        size_t slot = (size_t)bpe_surface_hash(target) & mask;
        for (size_t probes = 0; probes < v->bpe_surface_capacity; ++probes) {
            uint32_t encoded = v->bpe_surface_slots[slot];
            if (!encoded) return 0;
            uint32_t index = encoded - 1u;
            VocabEntry entry; Slice surface;
            if (!vocab_entry(v, index, &entry, &surface)) return 0;
            if (surface.len == target.len && memcmp(surface.bytes, target.bytes, target.len) == 0) {
                *out_index = index;
                return 1;
            }
            slot = (slot + 1u) & mask;
        }
        return 0;
    }
    uint32_t lo = vocab_first(v, target.bytes[0]), hi = vocab_first(v, (uint32_t)target.bytes[0] + 1u);
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
    uint32_t prev_distance, next_distance;
    uint32_t entry_index, generation;
} BpeNode;

typedef struct {
    int64_t rank;
    size_t left;
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

static size_t bpe_node_prev(const BpeNode *nodes, size_t index) {
    return nodes[index].prev_distance ? index - (size_t)nodes[index].prev_distance : SIZE_MAX;
}

static size_t bpe_node_next(const BpeNode *nodes, size_t index) {
    return nodes[index].next_distance ? index + (size_t)nodes[index].next_distance : SIZE_MAX;
}

static size_t bpe_node_end(const BpeNode *nodes, size_t index, size_t input_len) {
    size_t next = bpe_node_next(nodes, index);
    return next == SIZE_MAX ? input_len : next;
}

static int bpe_push_initial_pair(const Vocab *v, Slice input, const int64_t *adjustments,
                                 BpeNode *nodes, size_t left, BpeHeap *heap) {
    if (left + 1u >= input.len) return 1;
    uint32_t encoded = v->bpe_pair_entry[((uint32_t)input.bytes[left] << 8) | input.bytes[left + 1u]];
    if (!encoded) return 1;
    uint32_t entry_index = encoded - 1u;
    VocabEntry entry; Slice surface;
    if (!vocab_entry(v, entry_index, &entry, &surface)) return 0;
    int64_t rank = entry.cost;
    if (adjustments && !add_i64(rank, adjustments[entry_index], &rank)) return 0;
    return bpe_heap_push(heap, (BpePair){rank,left,nodes[left].generation,nodes[left + 1u].generation,entry_index,entry.token_id});
}

static int bpe_push_neighbor(const Vocab *v, Slice input, const int64_t *adjustments,
                             BpeNode *nodes, size_t left, BpeHeap *heap) {
    if (left == SIZE_MAX || nodes[left].entry_index == UINT32_MAX || !nodes[left].next_distance) return 1;
    size_t right = bpe_node_next(nodes, left);
    if (right == SIZE_MAX || nodes[right].entry_index == UINT32_MAX || bpe_node_prev(nodes, right) != left) return 0;
    size_t end = bpe_node_end(nodes, right, input.len);
    if (end <= left) return 0;
    size_t len = end - left;
    if (len > v->max_surface_len) return 1;
    uint32_t entry_index;
    if (!vocab_find_surface(v, (Slice){input.bytes + left, len}, &entry_index)) return 1;
    VocabEntry e; Slice surf; if (!vocab_entry(v, entry_index, &e, &surf)) return 0;
    int64_t rank = e.cost;
    if (adjustments && !add_i64(rank, adjustments[entry_index], &rank)) return 0;
    return bpe_heap_push(heap, (BpePair){rank,left,nodes[left].generation,nodes[right].generation,entry_index,e.token_id});
}

static int bpe_emit_byte_fallback(const Vocab *v, Slice input, const int64_t *adjustments,
                                  uint32_t *back, int64_t *out_cost, size_t *out_count) {
    int64_t total = 0;
    for (size_t i = 0; i < input.len; ++i) {
        uint32_t entry_index = v->bpe_byte_entry[input.bytes[i]];
        if (entry_index == UINT32_MAX) return 0;
        VocabEntry entry; Slice surface;
        if (!vocab_entry(v, entry_index, &entry, &surface)) return 0;
        int64_t cost = entry.cost;
        if (adjustments && !add_i64(cost, adjustments[entry_index], &cost)) return 0;
        if (!add_i64(total, cost, &total)) return 0;
        back[i + 1u] = entry_index;
    }
    *out_cost = total;
    *out_count = input.len;
    return 1;
}

static int bpe_bigram_may_cross(const Vocab *v, uint8_t left, uint8_t right) {
    uint32_t key = ((uint32_t)left << 8) | right;
    return v->bpe_bigram_bits && (v->bpe_bigram_bits[key >> 3] & (uint8_t)(1u << (key & 7u))) != 0;
}

static int tokenize_bpe_segment(const Vocab *v, Slice input, const int64_t *adjustments,
                                uint32_t *back, int64_t *out_cost, size_t *out_count) {
    if (!input.len) { *out_cost = 0; *out_count = 0; return 1; }
    if (input.len > SIZE_MAX / sizeof(BpeNode)) return 0;

    int any_initial_merge = 0;
    if (v->max_surface_len > 1u) {
        for (size_t i = 0; i + 1u < input.len; ++i) {
            if (v->bpe_pair_entry[((uint32_t)input.bytes[i] << 8) | input.bytes[i + 1u]]) { any_initial_merge = 1; break; }
        }
    }
    if (!any_initial_merge) return bpe_emit_byte_fallback(v, input, adjustments, back, out_cost, out_count);

    BpeNode *nodes=(BpeNode *)malloc(input.len*sizeof *nodes); BpeHeap heap={0};
    if(!nodes)return 0;
    for(size_t i=0;i<input.len;++i){
        uint32_t idx=v->bpe_byte_entry[input.bytes[i]];
        if(idx==UINT32_MAX){free(nodes);return 0;}
        nodes[i]=(BpeNode){i?1u:0u,i+1<input.len?1u:0u,idx,1u};
    }
    for(size_t i=0;i+1<input.len;++i) if(!bpe_push_initial_pair(v,input,adjustments,nodes,i,&heap)){free(heap.items);free(nodes);return 0;}
    BpePair pair;
    while(bpe_heap_pop(&heap,&pair)){
        BpeNode *l=&nodes[pair.left];
        if(l->entry_index==UINT32_MAX||l->generation!=pair.left_generation||!l->next_distance)continue;
        size_t right=bpe_node_next(nodes,pair.left); BpeNode *r=&nodes[right];
        if(r->entry_index==UINT32_MAX||r->generation!=pair.right_generation||bpe_node_prev(nodes,right)!=pair.left)continue;
        size_t prev=bpe_node_prev(nodes,pair.left),next=bpe_node_next(nodes,right);
        l->entry_index=pair.entry_index;++l->generation;
        l->next_distance=next==SIZE_MAX?0u:(uint32_t)(next-pair.left);
        r->entry_index=UINT32_MAX;++r->generation;r->prev_distance=0u;r->next_distance=0u;
        if(next!=SIZE_MAX){nodes[next].prev_distance=(uint32_t)(next-pair.left);++nodes[next].generation;}
        if(prev!=SIZE_MAX&&!bpe_push_neighbor(v,input,adjustments,nodes,prev,&heap)){free(heap.items);free(nodes);return 0;}
        if(!bpe_push_neighbor(v,input,adjustments,nodes,pair.left,&heap)){free(heap.items);free(nodes);return 0;}
    }
    free(heap.items);
    size_t count=0;int64_t total=0;size_t cur=0;
    while(cur!=SIZE_MAX){
        BpeNode *n=&nodes[cur];
        if(n->entry_index==UINT32_MAX){free(nodes);return 0;}
        size_t end=bpe_node_end(nodes,cur,input.len);back[end]=n->entry_index;
        VocabEntry e;Slice surf;if(!vocab_entry(v,n->entry_index,&e,&surf)){free(nodes);return 0;}
        int64_t cost=e.cost;if(adjustments&&!add_i64(cost,adjustments[n->entry_index],&cost)){free(nodes);return 0;}
        if(!add_i64(total,cost,&total)){free(nodes);return 0;}
        ++count;cur=bpe_node_next(nodes,cur);
    }
    free(nodes);*out_cost=total;*out_count=count;return 1;
}

static int tokenize_bpe_with_adjustments(const Vocab *v, Slice input, const int64_t *adjustments, Tokenization *out) {
    if (!v->bpe_surface_slots || !v->bpe_pair_entry || !v->bpe_bigram_bits) return fail("BPE runtime index unavailable");
    if (input.len == SIZE_MAX || input.len > SIZE_MAX / sizeof(uint32_t) - 1) return fail("input too large");
    uint32_t *back = (uint32_t *)malloc((input.len + 1u) * sizeof *back);
    if (!back) return fail("out of memory");
    back[0] = UINT32_MAX;
    if (!input.len) { *out=(Tokenization){back,0,0,0}; return 1; }

    size_t segment_start = 0;
    size_t total_count = 0;
    int64_t total_cost = 0;
    for (size_t boundary = 1; boundary <= input.len; ++boundary) {
        int split = boundary == input.len || !bpe_bigram_may_cross(v, input.bytes[boundary - 1u], input.bytes[boundary]);
        if (!split) continue;
        size_t segment_len = boundary - segment_start;
        int64_t segment_cost = 0;
        size_t segment_count = 0;
        if (!tokenize_bpe_segment(v, (Slice){input.bytes + segment_start, segment_len}, adjustments,
                                  back + segment_start, &segment_cost, &segment_count)) {
            free(back); return fail("BPE segment failure");
        }
        if (!add_i64(total_cost, segment_cost, &total_cost) || total_count > SIZE_MAX - segment_count) {
            free(back); return fail("BPE aggregate overflow");
        }
        total_count += segment_count;
        segment_start = boundary;
    }
    *out = (Tokenization){back,input.len,total_cost,total_count};
    return 1;
}

static int prefix_compare_tokens(const Vocab *v, const PrefixPathToken *a, size_t an,
                                 const PrefixPathToken *b, size_t bn) {
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

static size_t prefix_build_path(const Vocab *v, const uint32_t *back, size_t base_end, size_t final_end,
                                uint32_t extra_entry, PrefixPathToken *out, size_t cap) {
    size_t pos = cap;
    if (extra_entry != UINT32_MAX) {
        if (!pos) return SIZE_MAX;
        out[--pos] = (PrefixPathToken){base_end, final_end, extra_entry};
    }
    size_t cursor = base_end;
    while (cursor) {
        uint32_t entry_index = back[cursor];
        VocabEntry e; Slice surf;
        if (entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surf) || surf.len > cursor || !pos) return SIZE_MAX;
        size_t previous = cursor - surf.len;
        out[--pos] = (PrefixPathToken){previous, cursor, entry_index};
        cursor = previous;
    }
    return pos == 0 ? cap : SIZE_MAX;
}

static int prefix_tie_reserve(PrefixTieScratch *scratch, size_t count) {
    if (count <= scratch->capacity) return 1;
    if (count > SIZE_MAX / sizeof(PrefixPathToken)) return 0;
    size_t capacity = scratch->capacity ? scratch->capacity : 64u;
    while (capacity < count) {
        if (capacity > SIZE_MAX / 2u) { capacity = count; break; }
        capacity *= 2u;
    }
    PrefixPathToken *candidate = (PrefixPathToken *)realloc(scratch->candidate, capacity * sizeof *candidate);
    if (!candidate) return 0;
    scratch->candidate = candidate;
    PrefixPathToken *current = (PrefixPathToken *)realloc(scratch->current, capacity * sizeof *current);
    if (!current) return 0;
    scratch->current = current;
    scratch->capacity = capacity;
    return 1;
}

static void prefix_tie_free(PrefixTieScratch *scratch) {
    free(scratch->candidate); free(scratch->current); *scratch = (PrefixTieScratch){0};
}

static int prefix_candidate_better(const Vocab *v, const uint32_t *back, const RingState *current,
                                   size_t start, size_t end, uint32_t entry_index,
                                   int64_t cost, size_t count, PrefixTieScratch *scratch) {
    if (!current->reachable) return 1;
    if (cost != current->cost) return cost < current->cost;
    if (count != current->count) return count < current->count;
    if (!prefix_tie_reserve(scratch, count)) return 0;
    size_t an = prefix_build_path(v, back, start, end, entry_index, scratch->candidate, count);
    size_t bn = prefix_build_path(v, back, end, end, UINT32_MAX, scratch->current, count);
    return an != SIZE_MAX && bn != SIZE_MAX &&
           prefix_compare_tokens(v, scratch->candidate, an, scratch->current, bn) < 0;
}

/* Builds the optimal-prefix backpointer tree for every byte position. This is used by the
 * online commit proof, which needs LCAs across frontier endpoints rather than only the final path. */
static int build_viterbi_prefix_tree(const Vocab *v, Slice input, const int64_t *adjustments,
                                     Tokenization *out, size_t **out_depth) {
    if (!v->max_surface_len || !out_depth) return 0;
    *out_depth = NULL;
    if (input.len == SIZE_MAX || input.len > SIZE_MAX / sizeof(uint32_t) - 1u || input.len > SIZE_MAX / sizeof(size_t) - 1u) return 0;
    uint32_t *back = (uint32_t *)malloc((input.len + 1u) * sizeof *back);
    size_t *depth = (size_t *)malloc((input.len + 1u) * sizeof *depth);
    if (!back || !depth) { free(back); free(depth); return 0; }
    memset(back, 0xff, (input.len + 1u) * sizeof *back);
    depth[0] = 0;
    size_t ring_size = (size_t)v->max_surface_len + 1u;
    if (!ring_size || ring_size > SIZE_MAX / sizeof(RingState)) { free(depth); free(back); return 0; }
    RingState *ring = (RingState *)calloc(ring_size, sizeof *ring);
    if (!ring) { free(depth); free(back); return 0; }
    PrefixTieScratch scratch = {0};
    size_t start_slot = 0;
    ring[start_slot] = (RingState){0,1,0,0};
    for (size_t start = 0; start < input.len; ++start) {
        RingState *source = &ring[start_slot];
        if (source->position != start || !source->reachable) { prefix_tie_free(&scratch); free(ring); free(depth); free(back); return 0; }
        uint32_t a = v->runtime_first[input.bytes[start]], z = v->runtime_first[(uint32_t)input.bytes[start]+1u];
        for (uint32_t i = a; i < z; ++i) {
            const VocabEntry *entry = &v->runtime_entries[i];
            size_t surface_len = entry->surface_len;
            if (surface_len > input.len - start) continue;
            const uint8_t *surface = v->section + v->blob_offset + entry->surface_offset;
            if (surface_len > 1u && memcmp(input.bytes+start+1u,surface+1u,surface_len-1u)!=0) continue;
            size_t end = start + surface_len;
            int64_t total;
            if (!add_i64_i32(source->cost,entry->cost,&total)) { prefix_tie_free(&scratch); free(ring); free(depth); free(back); return 0; }
            if (adjustments && !add_i64(total,adjustments[i],&total)) { prefix_tie_free(&scratch); free(ring); free(depth); free(back); return 0; }
            if (source->count == SIZE_MAX) { prefix_tie_free(&scratch); free(ring); free(depth); free(back); return 0; }
            size_t count = source->count + 1u;
            size_t end_slot = start_slot + surface_len;
            if (end_slot >= ring_size) end_slot -= ring_size;
            RingState *destination = &ring[end_slot];
            if (destination->position != end) *destination = (RingState){end,0,0,0};
            if (prefix_candidate_better(v,back,destination,start,end,i,total,count,&scratch)) {
                *destination = (RingState){end,1,total,count}; back[end]=i; depth[end]=count;
            }
        }
        if (++start_slot == ring_size) start_slot = 0;
    }
    RingState terminal = ring[start_slot];
    prefix_tie_free(&scratch); free(ring);
    if (terminal.position != input.len || !terminal.reachable) { free(depth); free(back); return 0; }
    *out = (Tokenization){back,input.len,terminal.cost,terminal.count};
    *out_depth = depth;
    return 1;
}

static int viterbi_token_better(const VocabEntry *candidate, size_t candidate_len, uint32_t candidate_index,
                                 const VocabEntry *current, size_t current_len, uint32_t current_index) {
    if (candidate_len != current_len) return candidate_len > current_len;
    if (candidate->token_id != current->token_id) return candidate->token_id < current->token_id;
    return candidate_index < current_index;
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
    size_t start_slot = input.len % ring_size;
    ring[start_slot] = (RingState){input.len, 1, 0, 0};

    for (size_t start = input.len; start-- > 0;) {
        start_slot = start_slot ? start_slot - 1u : ring_size - 1u;
        RingState best = (RingState){start, 0, 0, 0};
        uint32_t best_entry = UINT32_MAX;
        VocabEntry best_vocab = {0};
        size_t best_len = 0;
        uint8_t byte = input.bytes[start];
        uint32_t a = v->runtime_first[byte], z = v->runtime_first[(uint32_t)byte + 1u];
        if (z == a + 1u) {
            const VocabEntry *entry = &v->runtime_entries[a];
            if (entry->surface_len != 1u) { free(ring); free(back); return fail("invalid singleton byte candidate"); }
            size_t suffix_slot = start_slot + 1u;
            if (suffix_slot == ring_size) suffix_slot = 0;
            RingState *suffix = &ring[suffix_slot];
            if (suffix->position != start + 1u || !suffix->reachable || suffix->count == SIZE_MAX) {
                free(ring); free(back); return fail("unreachable input despite byte fallback");
            }
            int64_t edge_cost = entry->cost;
            if (adjustments && !add_i64(edge_cost, adjustments[a], &edge_cost)) {
                free(ring); free(back); return fail("language-adjusted path cost overflow");
            }
            int64_t total;
            if (!add_i64(edge_cost, suffix->cost, &total)) { free(ring); free(back); return fail("path cost overflow"); }
            best = (RingState){start, 1, total, suffix->count + 1u};
            best_entry = a;
            best_vocab = *entry;
            best_len = 1u;
        } else {
            for (uint32_t i = a; i < z; ++i) {
                const VocabEntry *entry = &v->runtime_entries[i];
                size_t surface_len = entry->surface_len;
                if (surface_len > input.len - start) continue;
                const uint8_t *surface = v->section + v->blob_offset + entry->surface_offset;
                /* The validated first-byte index already proves surface[0] == input[start]. */
                if (surface_len > 1u && memcmp(input.bytes + start + 1u, surface + 1u, surface_len - 1u) != 0) continue;
                size_t end = start + surface_len;
                size_t suffix_slot = start_slot + surface_len;
                if (suffix_slot >= ring_size) suffix_slot -= ring_size;
                RingState *suffix = &ring[suffix_slot];
                if (suffix->position != end || !suffix->reachable) continue;
                int64_t edge_cost = entry->cost;
                if (adjustments && !add_i64(edge_cost, adjustments[i], &edge_cost)) {
                    free(ring); free(back); return fail("language-adjusted path cost overflow");
                }
                int64_t total;
                if (!add_i64(edge_cost, suffix->cost, &total)) { free(ring); free(back); return fail("path cost overflow"); }
                if (suffix->count == SIZE_MAX) { free(ring); free(back); return fail("token count overflow"); }
                size_t count = suffix->count + 1u;
                int better = !best.reachable || total < best.cost ||
                             (total == best.cost && (count < best.count ||
                              (count == best.count && viterbi_token_better(entry, surface_len, i, &best_vocab, best_len, best_entry))));
                if (better) {
                    best = (RingState){start, 1, total, count};
                    best_entry = i;
                    best_vocab = *entry;
                    best_len = surface_len;
                }
            }
        }
        if (!best.reachable || best_entry == UINT32_MAX) { free(ring); free(back); return fail("unreachable input despite byte fallback"); }
        ring[start_slot] = best;
        back[start] = best_entry; /* Forward choice; converted in-place to end-indexed back-map below. */
    }

    RingState root = ring[start_slot];
    if (root.position != 0 || !root.reachable) { free(ring); free(back); return fail("unreachable input despite byte fallback"); }
    if (input.len) {
        size_t position = 0;
        uint32_t entry_index = back[0];
        back[0] = UINT32_MAX;
        while (position < input.len) {
            VocabEntry e; Slice surf;
            if (entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surf) || surf.len > input.len - position) {
                free(ring); free(back); return fail("invalid Viterbi reconstruction");
            }
            size_t end_position = position + surf.len;
            uint32_t next_entry = end_position < input.len ? back[end_position] : UINT32_MAX;
            back[end_position] = entry_index;
            position = end_position;
            entry_index = next_entry;
        }
    }
    *out = (Tokenization){back, input.len, root.cost, root.count};
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

#ifndef MOSAIC_LIBRARY_ONLY
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

typedef struct {
    const uint8_t *section;
    size_t section_len;
    uint16_t unicode_major, unicode_minor, unicode_micro;
    uint32_t ccc_count, canonical_count, compatibility_count, casefold_count, composition_count, sequence_count;
    uint32_t ccc_offset, canonical_offset, compatibility_offset, casefold_offset, composition_offset, sequence_offset;
} NormalizationView;

typedef struct { uint32_t start,end; uint8_t ccc; } NormCccRange;
typedef struct { uint32_t cp, seq_offset; uint16_t seq_len; } NormMapEntry;
typedef struct { uint32_t first, second, composed; } NormComposition;

static int norm_ccc_range_at(const NormalizationView *v,uint32_t index,NormCccRange *out){
    if(index>=v->ccc_count)return 0;
    const uint8_t*p=v->section+v->ccc_offset+(size_t)index*12u;
    out->start=rd32(p);out->end=rd32(p+4);out->ccc=p[8];return p[9]==0&&p[10]==0&&p[11]==0;
}
static int norm_map_at(const NormalizationView*v,uint32_t offset,uint32_t count,uint32_t index,NormMapEntry*out){
    if(index>=count)return 0;
    const uint8_t*p=v->section+offset+(size_t)index*12u;
    out->cp=rd32(p);out->seq_offset=rd32(p+4);out->seq_len=rd16(p+8);return rd16(p+10)==0;
}
static int norm_comp_at(const NormalizationView*v,uint32_t index,NormComposition*out){
    if(index>=v->composition_count)return 0;
    const uint8_t*p=v->section+v->composition_offset+(size_t)index*12u;
    out->first=rd32(p);out->second=rd32(p+4);out->composed=rd32(p+8);return 1;
}
static int norm_scalar(uint32_t cp){return cp<=0x10ffffu&&!(cp>=0xd800u&&cp<=0xdfffu);}
static int parse_normalization_section(Slice s,NormalizationView*v){
    if(s.len<80||memcmp(s.bytes,"MSNM",4)!=0||rd16(s.bytes+4)!=1||rd16(s.bytes+6)!=80)return fail("invalid normalization header");
    uint16_t umaj=rd16(s.bytes+8),umin=rd16(s.bytes+10),umic=rd16(s.bytes+12);if(umaj!=16||umin!=0||umic!=0||rd16(s.bytes+14)!=0)return fail("unsupported normalization Unicode version");
    uint32_t cc=rd32(s.bytes+16),ca=rd32(s.bytes+20),co=rd32(s.bytes+24),cf=rd32(s.bytes+28),pc=rd32(s.bytes+32),sq=rd32(s.bytes+36);
    uint32_t cco=rd32(s.bytes+40),cao=rd32(s.bytes+44),coo=rd32(s.bytes+48),cfo=rd32(s.bytes+52),pco=rd32(s.bytes+56),sqo=rd32(s.bytes+60);
    if(!all_zero(s.bytes+64,16)||cc>10000u||ca>100000u||co>100000u||cf>100000u||pc>100000u||sq>1000000u||cco!=80)return fail("normalization limits/header");
    uint64_t cce=(uint64_t)cco+(uint64_t)cc*12u,cae=(uint64_t)cao+(uint64_t)ca*12u,coe=(uint64_t)coo+(uint64_t)co*12u,cfe=(uint64_t)cfo+(uint64_t)cf*12u,pce=(uint64_t)pco+(uint64_t)pc*12u,sqe=(uint64_t)sqo+(uint64_t)sq*4u;
    if(!(cce<=cao&&cae<=coo&&coe<=cfo&&cfe<=pco&&pce<=sqo&&sqe==s.len))return fail("normalization section layout");
    if(!all_zero(s.bytes+(size_t)cce,(size_t)cao-(size_t)cce)||!all_zero(s.bytes+(size_t)cae,(size_t)coo-(size_t)cae)||!all_zero(s.bytes+(size_t)coe,(size_t)cfo-(size_t)coe)||!all_zero(s.bytes+(size_t)cfe,(size_t)pco-(size_t)cfe)||!all_zero(s.bytes+(size_t)pce,(size_t)sqo-(size_t)pce))return fail("normalization padding nonzero");
    *v=(NormalizationView){s.bytes,s.len,umaj,umin,umic,cc,ca,co,cf,pc,sq,cco,cao,coo,cfo,pco,sqo};
    uint32_t prev=0;for(uint32_t i=0;i<cc;++i){NormCccRange r;if(!norm_ccc_range_at(v,i,&r)||r.start>=r.end||r.end>0x110000u||(i&&r.start<prev)||!r.ccc)return fail("invalid CCC range");prev=r.end;}
    const uint32_t offs[3]={cao,coo,cfo},counts[3]={ca,co,cf};
    for(int t=0;t<3;++t){uint32_t last=0;for(uint32_t i=0;i<counts[t];++i){NormMapEntry e;if(!norm_map_at(v,offs[t],counts[t],i,&e)||!norm_scalar(e.cp)||(i&&e.cp<=last)||(uint64_t)e.seq_offset+e.seq_len>sq)return fail("invalid normalization mapping");for(uint32_t j=0;j<e.seq_len;++j)if(!norm_scalar(rd32(v->section+sqo+((size_t)e.seq_offset+j)*4u)))return fail("invalid normalization sequence scalar");last=e.cp;}}
    uint32_t pf=0,ps=0;for(uint32_t i=0;i<pc;++i){NormComposition c;if(!norm_comp_at(v,i,&c)||!norm_scalar(c.first)||!norm_scalar(c.second)||!norm_scalar(c.composed)||(i&&(c.first<pf||(c.first==pf&&c.second<=ps))))return fail("invalid composition pair");pf=c.first;ps=c.second;}
    return 1;
}
static int load_normalization_pack(const uint8_t*data,size_t len,NormalizationView*v){
    Section*sections=NULL;uint32_t count=0,mi=0,li=0;if(!parse_outer(data,len,&sections,&count,&mi,&li))return 0;int n=0;Section ns={0};for(uint32_t i=0;i<count;++i)if(sections[i].kind==8){++n;ns=sections[i];}
    if(n!=1){free(sections);return fail("expected exactly one normalization section");}
    int ok=validate_manifest_lock(sec_slice(data,&sections[mi]),sec_slice(data,&sections[li]))&&parse_normalization_section(sec_slice(data,&ns),v);free(sections);return ok;
}
static uint8_t norm_ccc_lookup(const NormalizationView*v,uint32_t cp){
    uint32_t lo=0,hi=v->ccc_count;while(lo<hi){uint32_t mid=lo+(hi-lo)/2;NormCccRange r;if(!norm_ccc_range_at(v,mid,&r))return 0;if(cp<r.start)hi=mid;else if(cp>=r.end)lo=mid+1;else return r.ccc;}return 0;
}
static int norm_map_lookup(const NormalizationView*v,uint32_t offset,uint32_t count,uint32_t cp,NormMapEntry*out){
    uint32_t lo=0,hi=count;while(lo<hi){uint32_t mid=lo+(hi-lo)/2;NormMapEntry e;if(!norm_map_at(v,offset,count,mid,&e))return 0;if(cp<e.cp)hi=mid;else if(cp>e.cp)lo=mid+1;else{*out=e;return 1;}}return 0;
}
static uint32_t norm_seq_cp(const NormalizationView*v,const NormMapEntry*e,uint32_t index){return rd32(v->section+v->sequence_offset+((size_t)e->seq_offset+index)*4u);}
static uint32_t norm_comp_lookup(const NormalizationView*v,uint32_t a,uint32_t b){
    const uint32_t LBase=0x1100,VBase=0x1161,TBase=0x11a7,SBase=0xac00,LCount=19,VCount=21,TCount=28,NCount=588,SCount=11172;
    if(a>=LBase&&a<LBase+LCount&&b>=VBase&&b<VBase+VCount)return SBase+(a-LBase)*NCount+(b-VBase)*TCount;
    if(a>=SBase&&a<SBase+SCount&&((a-SBase)%TCount)==0&&b>TBase&&b<TBase+TCount)return a+(b-TBase);
    uint32_t lo=0,hi=v->composition_count;while(lo<hi){uint32_t mid=lo+(hi-lo)/2;NormComposition c;if(!norm_comp_at(v,mid,&c))return 0;if(a<c.first||(a==c.first&&b<c.second))hi=mid;else if(a>c.first||(a==c.first&&b>c.second))lo=mid+1;else return c.composed;}return 0;
}

typedef struct {size_t left,right,leaf_count; mosaic_range leaf; uint8_t is_leaf;} NormMapNode;
typedef struct {uint32_t cp;uint8_t raw,valid,ccc,reserved;size_t map_node;} NormUnit;
typedef struct {NormMapNode*data;size_t count,cap;} NormNodeVec;
typedef struct {NormUnit*data;size_t count,cap;} NormUnitVec;
static int norm_reserve(void**data,size_t*cap,size_t need,size_t width){if(need<=*cap)return 1;size_t n=*cap?*cap:16;while(n<need){if(n>SIZE_MAX/2)return 0;n*=2;}size_t bytes;if(!mul_size(n,width,&bytes))return 0;void*p=realloc(*data,bytes);if(!p)return 0;*data=p;*cap=n;return 1;}
static int norm_node_leaf(NormNodeVec*v,uint64_t start,uint64_t len,size_t*out){if(!norm_reserve((void**)&v->data,&v->cap,v->count+1,sizeof*v->data))return 0;*out=v->count;v->data[v->count++]=(NormMapNode){0,0,1,{start,len},1};return 1;}
static int norm_node_branch(NormNodeVec*v,size_t left,size_t right,size_t*out){if(left>=v->count||right>=v->count)return 0;size_t leaves=v->data[left].leaf_count;if(v->data[right].leaf_count>SIZE_MAX-leaves)return 0;leaves+=v->data[right].leaf_count;if(!norm_reserve((void**)&v->data,&v->cap,v->count+1,sizeof*v->data))return 0;*out=v->count;v->data[v->count++]=(NormMapNode){left,right,leaves,{0,0},0};return 1;}
static int norm_unit_push(NormUnitVec*v,NormUnit u){if(!norm_reserve((void**)&v->data,&v->cap,v->count+1,sizeof*v->data))return 0;v->data[v->count++]=u;return 1;}
static int norm_append_scalar(const NormalizationView*v,NormUnitVec*units,uint32_t cp,size_t map){return norm_unit_push(units,(NormUnit){cp,0,1,norm_ccc_lookup(v,cp),0,map});}
static int norm_append_decomp(const NormalizationView*v,NormUnitVec*units,uint32_t cp,size_t map,int compatibility){
    const uint32_t SBase=0xac00,LBase=0x1100,VBase=0x1161,TBase=0x11a7,TCount=28,NCount=588,SCount=11172;
    if(cp>=SBase&&cp<SBase+SCount){uint32_t si=cp-SBase,l=LBase+si/NCount,vo=VBase+(si%NCount)/TCount,t=si%TCount;if(!norm_append_scalar(v,units,l,map)||!norm_append_scalar(v,units,vo,map))return 0;return !t||norm_append_scalar(v,units,TBase+t,map);}
    NormMapEntry e;uint32_t off=compatibility?v->compatibility_offset:v->canonical_offset,count=compatibility?v->compatibility_count:v->canonical_count;
    if(norm_map_lookup(v,off,count,cp,&e)){for(uint32_t i=0;i<e.seq_len;++i)if(!norm_append_scalar(v,units,norm_seq_cp(v,&e,i),map))return 0;return 1;}
    return norm_append_scalar(v,units,cp,map);
}
static int norm_reorder(NormUnitVec*v){
    size_t maxrun=0;for(size_t i=0;i<v->count;){if(!v->data[i].valid||v->data[i].ccc==0){++i;continue;}size_t j=i+1;while(j<v->count&&v->data[j].valid&&v->data[j].ccc)++j;if(j-i>maxrun)maxrun=j-i;i=j;}
    if(maxrun<2)return 1;
    NormUnit*tmp=(NormUnit*)malloc(maxrun*sizeof*tmp);if(!tmp)return 0;
    for(size_t i=0;i<v->count;){if(!v->data[i].valid||v->data[i].ccc==0){++i;continue;}size_t j=i+1;while(j<v->count&&v->data[j].valid&&v->data[j].ccc)++j;size_t n=j-i;if(n>1){size_t counts[256]={0},pos[256]={0},sum=0;for(size_t k=i;k<j;++k)++counts[v->data[k].ccc];for(size_t c=1;c<256;++c){pos[c]=sum;sum+=counts[c];}for(size_t k=i;k<j;++k)tmp[pos[v->data[k].ccc]++]=v->data[k];memcpy(v->data+i,tmp,n*sizeof*tmp);}i=j;}
    free(tmp);return 1;
}
static int norm_compose(const NormalizationView*v,NormUnitVec*units,NormNodeVec*nodes){
    size_t out=0,starter=SIZE_MAX;uint8_t last_ccc=0;
    for(size_t i=0;i<units->count;++i){NormUnit u=units->data[i];if(!u.valid){units->data[out++]=u;starter=SIZE_MAX;last_ccc=0;continue;}uint8_t ccc=u.ccc;uint32_t composed=0;
        if(starter!=SIZE_MAX&&(last_ccc<ccc||last_ccc==0))composed=norm_comp_lookup(v,units->data[starter].cp,u.cp);
        if(composed){size_t map;if(!norm_node_branch(nodes,units->data[starter].map_node,u.map_node,&map))return 0;units->data[starter].cp=composed;units->data[starter].ccc=norm_ccc_lookup(v,composed);units->data[starter].map_node=map;continue;}
        units->data[out]=u;if(ccc==0){starter=out;last_ccc=0;}else last_ccc=ccc;++out;
    }units->count=out;return 1;
}
static size_t norm_utf8_len(uint32_t cp){return cp<=0x7f?1:cp<=0x7ff?2:cp<=0xffff?3:4;}
static void norm_utf8_write(uint8_t*out,uint32_t cp){if(cp<=0x7f)out[0]=(uint8_t)cp;else if(cp<=0x7ff){out[0]=(uint8_t)(0xc0|(cp>>6));out[1]=(uint8_t)(0x80|(cp&0x3f));}else if(cp<=0xffff){out[0]=(uint8_t)(0xe0|(cp>>12));out[1]=(uint8_t)(0x80|((cp>>6)&0x3f));out[2]=(uint8_t)(0x80|(cp&0x3f));}else{out[0]=(uint8_t)(0xf0|(cp>>18));out[1]=(uint8_t)(0x80|((cp>>12)&0x3f));out[2]=(uint8_t)(0x80|((cp>>6)&0x3f));out[3]=(uint8_t)(0x80|(cp&0x3f));}}
static int range_cmp_start(const void*a,const void*b){const mosaic_range*x=(const mosaic_range*)a,*y=(const mosaic_range*)b;return x->start<y->start?-1:x->start>y->start?1:0;}
static int norm_append_output_spans(const NormNodeVec*nodes,size_t root,mosaic_range**arena,size_t*count,size_t*cap,uint32_t*index,uint32_t*nspans){
    if(root>=nodes->count)return 0;
    const NormMapNode*r=&nodes->data[root];size_t leaves=r->leaf_count;if(!leaves||leaves>UINT32_MAX)return 0;
    mosaic_range one;if(r->is_leaf){one=r->leaf;if(*count>UINT32_MAX)return 0;*index=(uint32_t)*count;if(!norm_reserve((void**)arena,cap,*count+1,sizeof**arena))return 0;(*arena)[(*count)++]=one;*nspans=1;return 1;}
    if(leaves>SIZE_MAX/2)return 0;
    size_t*stack=(size_t*)malloc(leaves*2u*sizeof*stack);mosaic_range*tmp=(mosaic_range*)malloc(leaves*sizeof*tmp);if(!stack||!tmp){free(stack);free(tmp);return 0;}size_t top=0,tn=0;stack[top++]=root;
    while(top){size_t ni=stack[--top];if(ni>=nodes->count){free(stack);free(tmp);return 0;}const NormMapNode*n=&nodes->data[ni];if(n->is_leaf){tmp[tn++]=n->leaf;}else{if(top+2>leaves*2u){free(stack);free(tmp);return 0;}stack[top++]=n->right;stack[top++]=n->left;}}
    qsort(tmp,tn,sizeof*tmp,range_cmp_start);size_t merged=0;for(size_t i=0;i<tn;++i){if(merged&&tmp[i].start==tmp[merged-1].start&&tmp[i].length==tmp[merged-1].length)continue;if(merged&&tmp[merged-1].start+tmp[merged-1].length==tmp[i].start){tmp[merged-1].length+=tmp[i].length;continue;}tmp[merged++]=tmp[i];}
    if(*count>UINT32_MAX||merged>UINT32_MAX||*count+merged>UINT32_MAX){free(stack);free(tmp);return 0;}*index=(uint32_t)*count;*nspans=(uint32_t)merged;if(!norm_reserve((void**)arena,cap,*count+merged,sizeof**arena)){free(stack);free(tmp);return 0;}memcpy(*arena+*count,tmp,merged*sizeof*tmp);*count+=merged;free(stack);free(tmp);return 1;
}
static int normalize_internal(const NormalizationView*v,mosaic_normalization_mode mode,Slice input,mosaic_normalized_view*out){
    memset(out,0,sizeof*out);if(mode<MOSAIC_NORMALIZE_PRESERVE||mode>MOSAIC_NORMALIZE_NFKC_CASEFOLD)return 0;NormUnitVec units={0};NormNodeVec nodes={0};size_t i=0;
    while(i<input.len){ScalarUnit src=decode_one_unit(input,i);size_t map;if(!norm_node_leaf(&nodes,(uint64_t)src.start,(uint64_t)(src.end-src.start),&map))goto oom;
        if(!src.valid){if(!norm_unit_push(&units,(NormUnit){0,input.bytes[i],0,0,0,map}))goto oom;}
        else if(mode==MOSAIC_NORMALIZE_PRESERVE){if(!norm_append_scalar(v,&units,src.cp,map))goto oom;}
        else if(mode==MOSAIC_NORMALIZE_NFKC_CASEFOLD){NormMapEntry e;if(norm_map_lookup(v,v->casefold_offset,v->casefold_count,src.cp,&e)){for(uint32_t j=0;j<e.seq_len;++j)if(!norm_append_decomp(v,&units,norm_seq_cp(v,&e,j),map,0))goto oom;}else if(!norm_append_decomp(v,&units,src.cp,map,0))goto oom;}
        else if(!norm_append_decomp(v,&units,src.cp,map,mode==MOSAIC_NORMALIZE_NFKD||mode==MOSAIC_NORMALIZE_NFKC))goto oom;
        i=src.end;
    }
    if(mode!=MOSAIC_NORMALIZE_PRESERVE&&!norm_reorder(&units))goto oom;
    if((mode==MOSAIC_NORMALIZE_NFC||mode==MOSAIC_NORMALIZE_NFKC||mode==MOSAIC_NORMALIZE_NFKC_CASEFOLD)&&!norm_compose(v,&units,&nodes))goto oom;
    size_t bytes=0;for(size_t j=0;j<units.count;++j){size_t n=units.data[j].valid?norm_utf8_len(units.data[j].cp):1;if(n>SIZE_MAX-bytes)goto oom;bytes+=n;}
    out->bytes=bytes?(uint8_t*)malloc(bytes):(uint8_t*)malloc(1);out->units=units.count?(mosaic_normalized_unit*)calloc(units.count,sizeof*out->units):NULL;if(!out->bytes||(units.count&&!out->units))goto oom;
    out->byte_length=bytes;out->unit_count=units.count;size_t bo=0,span_cap=0;
    for(size_t j=0;j<units.count;++j){size_t n=units.data[j].valid?norm_utf8_len(units.data[j].cp):1;if(units.data[j].valid)norm_utf8_write(out->bytes+bo,units.data[j].cp);else out->bytes[bo]=units.data[j].raw;uint32_t si=0,sc=0;if(!norm_append_output_spans(&nodes,units.data[j].map_node,&out->source_spans,&out->source_span_count,&span_cap,&si,&sc))goto oom;out->units[j]=(mosaic_normalized_unit){(uint64_t)bo,(uint64_t)n,si,sc};bo+=n;}
    free(units.data);free(nodes.data);return 1;
oom:
    free(units.data); free(nodes.data);
    free(out->bytes); out->bytes = NULL; out->byte_length = 0;
    free(out->units); out->units = NULL; out->unit_count = 0;
    free(out->source_spans); out->source_spans = NULL; out->source_span_count = 0;
    return 0;
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

static mosaic_status security_visit_internal(const SecurityView *v, Slice input, mosaic_security_visitor visitor, void *context, size_t *out_count) {
    if (!visitor || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_count = 0;
    uint64_t script_counts[1025] = {0}; size_t i = 0;
    while (i < input.len) {
        ScalarUnit unit = decode_one_unit(input, i);
        if (unit.valid) {
            uint16_t sid = security_script_lookup(v, unit.cp);
            if (security_significant_script(v, sid)) {
                if (script_counts[sid] == UINT64_MAX) return MOSAIC_ERROR_OVERFLOW;
                ++script_counts[sid];
            }
        }
        i = unit.end;
    }
    uint16_t primary = 0; uint64_t best = 0; size_t active = 0;
    for (uint32_t sid = 1; sid <= v->script_count; ++sid) {
        if (script_counts[sid]) {
            ++active;
            if (script_counts[sid] > best || (script_counts[sid] == best && (!primary || sid < primary))) {
                best = script_counts[sid]; primary = (uint16_t)sid;
            }
        }
    }
    size_t n = 0; i = 0;
    while (i < input.len) {
        ScalarUnit unit = decode_one_unit(input, i);
        if (unit.valid) {
            uint32_t cp = unit.cp; uint16_t sid = security_script_lookup(v, cp);
            uint32_t kinds[4]; size_t kn = 0;
            if (security_bool_lookup(v, v->bidi_offset, v->bidi_count, cp)) kinds[kn++] = MOSAIC_SECURITY_BIDI_CONTROL;
            if (security_bool_lookup(v, v->ignorable_offset, v->ignorable_count, cp)) kinds[kn++] = MOSAIC_SECURITY_DEFAULT_IGNORABLE;
            if (security_bool_lookup(v, v->nonchar_offset, v->nonchar_count, cp)) kinds[kn++] = MOSAIC_SECURITY_NONCHARACTER;
            if (security_bool_lookup(v, v->deprecated_offset, v->deprecated_count, cp)) kinds[kn++] = MOSAIC_SECURITY_DEPRECATED;
            for (size_t k = 0; k < kn; ++k) {
                mosaic_security_finding f = {kinds[k], sid, 0, (uint64_t)unit.start, (uint64_t)(unit.end - unit.start)};
                mosaic_status status = visitor(context, &f); if (status != MOSAIC_OK) { *out_count = n; return status; }
                if (n == SIZE_MAX) return MOSAIC_ERROR_OVERFLOW;
                ++n;
            }
            if (active > 1 && security_significant_script(v, sid) && sid != primary) {
                mosaic_security_finding f = {MOSAIC_SECURITY_MIXED_SCRIPT, sid, 0, (uint64_t)unit.start, (uint64_t)(unit.end - unit.start)};
                mosaic_status status = visitor(context, &f); if (status != MOSAIC_OK) { *out_count = n; return status; }
                if (n == SIZE_MAX) return MOSAIC_ERROR_OVERFLOW;
                ++n;
            }
        }
        i = unit.end;
    }
    *out_count = n; return MOSAIC_OK;
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

struct mosaic_normalization {
    uint8_t *pack;
    size_t pack_len;
    NormalizationView view;
    uint8_t hash[32];
};

struct mosaic_lexer {
    uint8_t *pack;
    size_t pack_len;
    LexerView view;
    uint8_t hash[32];
};

typedef struct {
    _Atomic uint64_t encode_calls;
    _Atomic uint64_t decode_calls;
    _Atomic uint64_t bytes_in;
    _Atomic uint64_t bytes_out;
    _Atomic uint64_t tokens_out;
    _Atomic uint64_t failures;
    _Atomic uint64_t resource_rejections;
} TokenizerTelemetry;

struct mosaic_tokenizer {
    mosaic_model *model;
    mosaic_unicode *unicode_data;
    LanguagePack *languages;
    size_t language_count;
    size_t language_capacity;
    int64_t *adjustments;
    mosaic_detector *detector;
    mosaic_security *security;
    mosaic_normalization *normalization;
    mosaic_lexer *lexer;
    uint8_t fingerprint[32];
    mosaic_runtime_limits limits;
    mosaic_observer_config observer;
    _Atomic uint64_t observer_sequence;
    TokenizerTelemetry telemetry;
    _Atomic int sealed;
};

static uint32_t observer_mask_for_kind(uint32_t kind) {
    if (kind == MOSAIC_EVENT_SUCCESS) return MOSAIC_OBSERVE_SUCCESS;
    if (kind == MOSAIC_EVENT_RESOURCE_REJECTED) return MOSAIC_OBSERVE_RESOURCE;
    return MOSAIC_OBSERVE_FAILURE;
}

static _Thread_local const mosaic_tokenizer *observer_active_tokenizer = NULL;

static void tokenizer_observe(const mosaic_tokenizer *tokenizer, uint32_t kind, uint32_t operation,
                              mosaic_status status, size_t input_units, size_t output_units, uint64_t resource_limit) {
    if (!tokenizer || !tokenizer->observer.callback || !(tokenizer->observer.event_mask & observer_mask_for_kind(kind)) ||
        observer_active_tokenizer == tokenizer) return;
    mosaic_event event = {0};
    event.struct_size = sizeof event; event.kind = kind; event.operation = operation; event.status = (uint32_t)status;
    event.sequence = atomic_fetch_add_explicit(&((mosaic_tokenizer *)tokenizer)->observer_sequence, 1u, memory_order_relaxed) + 1u;
    event.input_units = (uint64_t)input_units; event.output_units = (uint64_t)output_units; event.resource_limit = resource_limit;
    memcpy(event.tokenizer_fingerprint_sha256, tokenizer->fingerprint, 32u);
    const mosaic_tokenizer *previous = observer_active_tokenizer;
    observer_active_tokenizer = tokenizer;
    tokenizer->observer.callback(tokenizer->observer.context, &event);
    observer_active_tokenizer = previous;
}

static int tokenizer_input_allowed(const mosaic_tokenizer *tokenizer, size_t input_len);

struct mosaic_stream {
    mosaic_model *model;
    int64_t *adjustments;
    mosaic_tokenizer *tokenizer;
    int auto_mode;
    uint8_t *buffer;
    size_t len;
    size_t capacity;
    size_t max_buffer_bytes;
    int finished;
};

struct mosaic_online_stream {
    mosaic_model *model;
    int64_t *adjustments;
    uint8_t *pending;
    size_t pending_len;
    size_t pending_capacity;
    size_t max_pending_bytes;
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
    size_t max_buffer_bytes;
};

struct mosaic_incremental_document {
    mosaic_model *model;
    int64_t *adjustments;
    uint8_t *buffer;
    size_t len;
    size_t capacity;
    mosaic_token *tokens;
    size_t token_count;
    size_t token_capacity;
    size_t last_reprocessed_bytes;
    size_t last_reused_prefix_bytes;
    size_t max_document_bytes;
};

typedef struct {
    uint32_t id;
    uint16_t length;
    uint16_t reserved;
} ResyncToken;

typedef struct {
    size_t consumed;
    size_t committed_end;
    size_t prefix_token_count;
    uint8_t *pending;
    size_t pending_len;
} ResyncCheckpoint;

struct mosaic_resync_document {
    mosaic_model *model;
    int64_t *adjustments;
    uint8_t *buffer;
    size_t len;
    ResyncToken *tokens;
    size_t token_count;
    size_t token_capacity;
    ResyncCheckpoint *checkpoints;
    size_t checkpoint_count;
    size_t checkpoint_capacity;
    size_t checkpoint_bytes;
    size_t max_pending_bytes;
    size_t last_reprocessed_bytes;
    size_t last_reused_prefix_bytes;
    size_t last_reused_suffix_bytes;
    int last_resynchronized;
    size_t max_document_bytes;
};

struct mosaic_token_document {
    uint8_t *source;
    size_t source_len;
    ResyncToken *model_tokens;
    size_t model_token_count;
    mosaic_range *graphemes;
    size_t grapheme_count;
    mosaic_security_finding *security_findings;
    size_t security_finding_count;
    mosaic_normalized_view normalized;
    mosaic_lex_token *lexical_tokens;
    size_t lexical_token_count;
    mosaic_semantic_component *semantic_components;
    size_t semantic_component_count;
    mosaic_normalization_mode normalization_mode;
    uint32_t flags;
    uint8_t source_sha256[32];
    uint8_t tokenizer_fingerprint[32];
    mosaic_detection detection;
};

struct mosaic_block_plan {
    mosaic_block_plan_info info;
    mosaic_processing_block *blocks;
    size_t block_count;
    mosaic_macroblock *macroblocks;
    size_t macroblock_count;
};

void mosaic_free(void *pointer) { free(pointer); }
const char *mosaic_version_string(void) { return MOSAIC_RELEASE_VERSION; }
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
        case MOSAIC_ERROR_RESOURCE_LIMIT: return "resource limit";
        case MOSAIC_ERROR_UNSUPPORTED: return "unsupported";
        case MOSAIC_ERROR_NOT_FOUND: return "not found";
        case MOSAIC_ERROR_INTEGRITY: return "integrity check failed";
        case MOSAIC_ERROR_STATE: return "invalid state";
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
        vocab_release_runtime_index(&model->vocab);
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
    vocab_release_runtime_index(&model->vocab);
    free(model->pack);
    free(model);
}

static mosaic_status encode_tokens_internal(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                            const int64_t *adjustments, size_t max_tokens,
                                            mosaic_token **out_tokens, size_t *out_count) {
    if (!model || (!input && input_len) || !out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokens = NULL;
    *out_count = 0;
    Tokenization result = {0};
    if (!tokenize_with_adjustments(&model->vocab, (Slice){input, input_len}, adjustments, &result)) return MOSAIC_ERROR_INTERNAL;
    if (result.count > max_tokens) { tokenization_free(&result); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    size_t count = result.count;
    if (count > SIZE_MAX / sizeof(mosaic_token)) { tokenization_free(&result); return MOSAIC_ERROR_OVERFLOW; }
    mosaic_token *tokens = count ? (mosaic_token *)malloc(count * sizeof *tokens) : NULL;
    if (count && !tokens) { tokenization_free(&result); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    size_t cursor = result.input_len, pos = count;
    while (cursor) {
        uint32_t entry_index = result.back[cursor];
        VocabEntry entry; Slice surface;
        if (!pos || entry_index == UINT32_MAX || !vocab_entry(&model->vocab, entry_index, &entry, &surface) || surface.len > cursor) {
            free(tokens); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL;
        }
        size_t token_start = cursor - surface.len;
        tokens[--pos] = (mosaic_token){entry.token_id, (uint64_t)token_start, (uint64_t)surface.len, entry.cost};
        cursor = token_start;
    }
    if (pos) { free(tokens); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL; }
    tokenization_free(&result);
    *out_tokens = tokens;
    *out_count = count;
    return MOSAIC_OK;
}

mosaic_status mosaic_encode_tokens(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                   mosaic_token **out_tokens, size_t *out_count) {
    return encode_tokens_internal(model, input, input_len, NULL, SIZE_MAX, out_tokens, out_count);
}


static mosaic_status encode_internal(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                     const int64_t *adjustments, size_t max_tokens,
                                     uint32_t **out_ids, size_t *out_count) {
    if (!model || (!input && input_len) || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL;
    *out_count = 0;
    Tokenization result = {0};
    if (!tokenize_with_adjustments(&model->vocab, (Slice){input, input_len}, adjustments, &result)) return MOSAIC_ERROR_INTERNAL;
    if (result.count > max_tokens) { tokenization_free(&result); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    size_t count = result.count;
    if (count > SIZE_MAX / sizeof(uint32_t)) { tokenization_free(&result); return MOSAIC_ERROR_OVERFLOW; }
    uint32_t *ids = count ? (uint32_t *)malloc(count * sizeof *ids) : NULL;
    if (count && !ids) { tokenization_free(&result); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    size_t cursor = result.input_len, pos = count;
    while (cursor) {
        uint32_t entry_index = result.back[cursor];
        VocabEntry entry; Slice surface;
        if (!pos || entry_index == UINT32_MAX || !vocab_entry(&model->vocab, entry_index, &entry, &surface) || surface.len > cursor) {
            free(ids); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL;
        }
        ids[--pos] = entry.token_id;
        cursor -= surface.len;
    }
    if (pos) { free(ids); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL; }
    tokenization_free(&result);
    *out_ids = ids;
    *out_count = count;
    return MOSAIC_OK;
}

mosaic_status mosaic_encode(const mosaic_model *model, const uint8_t *input, size_t input_len,
                            uint32_t **out_ids, size_t *out_count) {
    return encode_internal(model, input, input_len, NULL, SIZE_MAX, out_ids, out_count);
}


static mosaic_status decode_internal(const mosaic_model *model, const uint32_t *ids, size_t count, size_t max_bytes,
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
        if (total > max_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
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

mosaic_status mosaic_decode(const mosaic_model *model, const uint32_t *ids, size_t count,
                            uint8_t **out_bytes, size_t *out_len) {
    return decode_internal(model, ids, count, SIZE_MAX, out_bytes, out_len);
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
    stream->max_buffer_bytes = SIZE_MAX;
    *out_stream = stream;
    return MOSAIC_OK;
}

mosaic_status mosaic_stream_push(mosaic_stream *stream, const uint8_t *bytes, size_t len) {
    if (!stream || (!bytes && len) || stream->finished) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (len > SIZE_MAX - stream->len) return MOSAIC_ERROR_OVERFLOW;
    size_t needed = stream->len + len;
    if (needed > stream->max_buffer_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
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
        status = encode_internal(stream->model, stream->buffer, stream->len, stream->adjustments, SIZE_MAX, out_ids, out_count);
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
    document->max_buffer_bytes=SIZE_MAX;
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
    if (new_len > document->max_buffer_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
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
    return encode_internal(document->model, document->buffer, document->len, document->adjustments, SIZE_MAX, out_ids, out_count);
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

static mosaic_status incremental_document_allocate(const mosaic_model *model, const int64_t *adjustments,
                                                   const uint8_t *input, size_t input_len,
                                                   mosaic_incremental_document **out_document) {
    if (!model || (!input && input_len) || !out_document) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    if (model->vocab.algorithm != 0u) return MOSAIC_ERROR_UNSUPPORTED;
    mosaic_incremental_document *document = (mosaic_incremental_document *)calloc(1, sizeof *document);
    if (!document) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model->pack, model->pack_len, &document->model);
    if (status != MOSAIC_OK) { free(document); return status; }
    if (adjustments) {
        size_t count = (size_t)document->model->vocab.count;
        if (count > SIZE_MAX / sizeof *document->adjustments) { mosaic_model_free(document->model); free(document); return MOSAIC_ERROR_OVERFLOW; }
        document->adjustments = count ? (int64_t *)malloc(count * sizeof *document->adjustments) : NULL;
        if (count && !document->adjustments) { mosaic_model_free(document->model); free(document); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        if (count) memcpy(document->adjustments, adjustments, count * sizeof *document->adjustments);
    }
    if (input_len) {
        size_t headroom = input_len / 16u;
        if (headroom > 1048576u) headroom = 1048576u;
        document->capacity = headroom <= SIZE_MAX - input_len ? input_len + headroom : input_len;
        document->buffer = (uint8_t *)malloc(document->capacity);
        if (!document->buffer) { mosaic_model_free(document->model); free(document->adjustments); free(document); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        memcpy(document->buffer, input, input_len);
    }
    document->len = input_len;
    status = encode_tokens_internal(document->model, document->buffer, document->len, document->adjustments, SIZE_MAX,
                                    &document->tokens, &document->token_count);
    if (status != MOSAIC_OK) { mosaic_incremental_document_free(document); return status; }
    document->token_capacity = document->token_count;
    if (document->token_count) {
        size_t headroom = document->token_count / 16u;
        if (headroom > 65536u) headroom = 65536u;
        if (headroom <= SIZE_MAX - document->token_count && document->token_count + headroom <= SIZE_MAX / sizeof *document->tokens) {
            size_t capacity = document->token_count + headroom;
            mosaic_token *grown = (mosaic_token *)realloc(document->tokens, capacity * sizeof *grown);
            if (grown) { document->tokens = grown; document->token_capacity = capacity; }
        }
    }
    document->last_reprocessed_bytes = input_len;
    *out_document = document;
    return MOSAIC_OK;
}

mosaic_status mosaic_incremental_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                                 mosaic_incremental_document **out_document) {
    mosaic_status st=incremental_document_allocate(model,NULL,input,input_len,out_document);if(st==MOSAIC_OK)(*out_document)->max_document_bytes=SIZE_MAX;return st;
}

mosaic_status mosaic_tokenizer_incremental_document_create(const mosaic_tokenizer *tokenizer,
                                                           const uint8_t *input, size_t input_len,
                                                           mosaic_incremental_document **out_document) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if ((uint64_t)input_len > tokenizer->limits.max_input_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    mosaic_status st=incremental_document_allocate(tokenizer->model,tokenizer->adjustments,input,input_len,out_document);if(st==MOSAIC_OK)(*out_document)->max_document_bytes=(size_t)(tokenizer->limits.max_input_bytes>SIZE_MAX?SIZE_MAX:tokenizer->limits.max_input_bytes);return st;
}

static size_t incremental_safe_restart(const mosaic_incremental_document *document, size_t edit_start, size_t *out_prefix_tokens) {
    size_t guard = document->model->vocab.max_surface_len ? (size_t)document->model->vocab.max_surface_len - 1u : 0u;
    size_t target = edit_start > guard ? edit_start - guard : 0u;
    size_t restart = 0, prefix = 0;
    for (size_t i = 0; i < document->token_count; ++i) {
        const mosaic_token *token = &document->tokens[i];
        if (token->start > SIZE_MAX || token->length > SIZE_MAX || (size_t)token->start > SIZE_MAX - (size_t)token->length) break;
        size_t end = (size_t)token->start + (size_t)token->length;
        if (end > target) break;
        restart = end; prefix = i + 1u;
    }
    *out_prefix_tokens = prefix;
    return restart;
}

mosaic_status mosaic_incremental_document_apply_edit(mosaic_incremental_document *document,
                                                      uint64_t start64, uint64_t delete64,
                                                      const uint8_t *replacement, size_t replacement_len) {
    if (!document || (!replacement && replacement_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (start64 > SIZE_MAX || delete64 > SIZE_MAX) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t start = (size_t)start64, delete_len = (size_t)delete64;
    if (start > document->len || delete_len > document->len - start) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t kept = document->len - delete_len;
    if (replacement_len > SIZE_MAX - kept) return MOSAIC_ERROR_OVERFLOW;
    size_t new_len = kept + replacement_len;
    if (new_len > document->max_document_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;

    size_t prefix_tokens = 0;
    size_t restart = incremental_safe_restart(document, start, &prefix_tokens);
    size_t old_tail = start + delete_len;
    size_t tail_len = document->len - old_tail;
    size_t unchanged = start - restart;
    size_t suffix_len = new_len - restart;
    uint8_t *suffix_source = suffix_len ? (uint8_t *)malloc(suffix_len) : NULL;
    if (suffix_len && !suffix_source) return MOSAIC_ERROR_OUT_OF_MEMORY;
    size_t write = 0;
    if (unchanged) { memcpy(suffix_source + write, document->buffer + restart, unchanged); write += unchanged; }
    if (replacement_len) { memcpy(suffix_source + write, replacement, replacement_len); write += replacement_len; }
    if (tail_len) { memcpy(suffix_source + write, document->buffer + old_tail, tail_len); write += tail_len; }
    if (write != suffix_len) { free(suffix_source); return MOSAIC_ERROR_INTERNAL; }

    mosaic_token *suffix = NULL; size_t suffix_count = 0;
    mosaic_status status = encode_tokens_internal(document->model, suffix_source, suffix_len,
                                                  document->adjustments, SIZE_MAX, &suffix, &suffix_count);
    if (status != MOSAIC_OK) { free(suffix_source); return status; }
    if (prefix_tokens > SIZE_MAX - suffix_count || prefix_tokens + suffix_count > SIZE_MAX / sizeof(mosaic_token)) {
        free(suffix); free(suffix_source); return MOSAIC_ERROR_OVERFLOW;
    }
    size_t total = prefix_tokens + suffix_count;

    if (total > document->token_capacity) {
        size_t headroom = total / 16u;
        if (headroom > 65536u) headroom = 65536u;
        size_t capacity = headroom <= SIZE_MAX - total ? total + headroom : total;
        if (capacity > SIZE_MAX / sizeof *document->tokens) { free(suffix); free(suffix_source); return MOSAIC_ERROR_OVERFLOW; }
        mosaic_token *grown = (mosaic_token *)realloc(document->tokens, capacity * sizeof *grown);
        if (!grown) { free(suffix); free(suffix_source); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        document->tokens = grown; document->token_capacity = capacity;
    }
    if (new_len > document->capacity) {
        size_t headroom = new_len / 16u;
        if (headroom > 1048576u) headroom = 1048576u;
        size_t capacity = headroom <= SIZE_MAX - new_len ? new_len + headroom : new_len;
        if (capacity > document->max_document_bytes) capacity = new_len;
        uint8_t *grown = (uint8_t *)realloc(document->buffer, capacity);
        if (!grown) { free(suffix); free(suffix_source); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        document->buffer = grown; document->capacity = capacity;
    }

    if (suffix_len) memcpy(document->buffer + restart, suffix_source, suffix_len);
    for (size_t i = 0; i < suffix_count; ++i) {
        suffix[i].start += (uint64_t)restart;
        document->tokens[prefix_tokens + i] = suffix[i];
    }
    free(suffix); free(suffix_source);
    document->len = new_len; document->token_count = total;
    document->last_reprocessed_bytes = suffix_len;
    document->last_reused_prefix_bytes = restart;
    return MOSAIC_OK;
}

mosaic_status mosaic_incremental_document_encode(const mosaic_incremental_document *document,
                                                  uint32_t **out_ids, size_t *out_count) {
    if (!document || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL; *out_count = 0;
    if (document->token_count > SIZE_MAX / sizeof(uint32_t)) return MOSAIC_ERROR_OVERFLOW;
    uint32_t *ids = document->token_count ? (uint32_t *)malloc(document->token_count * sizeof *ids) : NULL;
    if (document->token_count && !ids) return MOSAIC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < document->token_count; ++i) ids[i] = document->tokens[i].id;
    *out_ids = ids; *out_count = document->token_count; return MOSAIC_OK;
}

mosaic_status mosaic_incremental_document_copy_bytes(const mosaic_incremental_document *document,
                                                      uint8_t **out_bytes, size_t *out_len) {
    if (!document || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL; *out_len = 0;
    mosaic_status status = copy_bytes(document->buffer, document->len, out_bytes);
    if (status == MOSAIC_OK) *out_len = document->len;
    return status;
}

size_t mosaic_incremental_document_last_reprocessed_bytes(const mosaic_incremental_document *document) {
    return document ? document->last_reprocessed_bytes : 0u;
}
size_t mosaic_incremental_document_last_reused_prefix_bytes(const mosaic_incremental_document *document) {
    return document ? document->last_reused_prefix_bytes : 0u;
}
void mosaic_incremental_document_free(mosaic_incremental_document *document) {
    if (!document) return;
    mosaic_model_free(document->model); free(document->adjustments); free(document->buffer); free(document->tokens); free(document);
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
mosaic_status mosaic_security_visit(const mosaic_security *security,const uint8_t *input,size_t input_len,mosaic_security_visitor visitor,void *context,size_t *out_count){if(!security||(!input&&input_len)||!visitor||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;return security_visit_internal(&security->view,(Slice){input,input_len},visitor,context,out_count);}

mosaic_status mosaic_normalization_load_memory(const uint8_t *pack,size_t pack_len,mosaic_normalization **out_normalization){
    if(!out_normalization||(!pack&&pack_len))return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_normalization=NULL;
    mosaic_normalization*n=(mosaic_normalization*)calloc(1,sizeof*n);if(!n)return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status=copy_bytes(pack,pack_len,&n->pack);if(status!=MOSAIC_OK){free(n);return status;}n->pack_len=pack_len;
    if(!load_normalization_pack(n->pack,n->pack_len,&n->view)||!sha256_bytes(n->pack,n->pack_len,n->hash)){free(n->pack);free(n);return MOSAIC_ERROR_INVALID_PACK;}
    *out_normalization=n;return MOSAIC_OK;
}
mosaic_status mosaic_normalization_load_file(const char *path,mosaic_normalization **out_normalization){if(!path||!out_normalization)return MOSAIC_ERROR_INVALID_ARGUMENT;uint8_t*data=NULL;size_t len=0;if(!read_file(path,&data,&len))return MOSAIC_ERROR_IO;mosaic_status status=mosaic_normalization_load_memory(data,len,out_normalization);free(data);return status;}
void mosaic_normalization_free(mosaic_normalization*n){if(!n)return;free(n->pack);free(n);}
mosaic_status mosaic_normalization_unicode_version(const mosaic_normalization*n,uint16_t*major,uint16_t*minor,uint16_t*micro){if(!n||!major||!minor||!micro)return MOSAIC_ERROR_INVALID_ARGUMENT;*major=n->view.unicode_major;*minor=n->view.unicode_minor;*micro=n->view.unicode_micro;return MOSAIC_OK;}
void mosaic_normalized_view_free(mosaic_normalized_view*view){if(!view)return;free(view->bytes);free(view->units);free(view->source_spans);memset(view,0,sizeof*view);}
mosaic_status mosaic_normalize(const mosaic_normalization*n,mosaic_normalization_mode mode,const uint8_t*input,size_t input_len,mosaic_normalized_view*out_view){if(!n||(!input&&input_len)||!out_view||mode<MOSAIC_NORMALIZE_PRESERVE||mode>MOSAIC_NORMALIZE_NFKC_CASEFOLD)return MOSAIC_ERROR_INVALID_ARGUMENT;memset(out_view,0,sizeof*out_view);if(!normalize_internal(&n->view,mode,(Slice){input,input_len},out_view))return MOSAIC_ERROR_OUT_OF_MEMORY;return MOSAIC_OK;}

mosaic_status mosaic_lexer_load_memory(const uint8_t *pack,size_t pack_len,mosaic_lexer **out_lexer){
    if(!out_lexer||(!pack&&pack_len))return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_lexer=NULL;
    mosaic_lexer*l=(mosaic_lexer*)calloc(1,sizeof*l);if(!l)return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status st=copy_bytes(pack,pack_len,&l->pack);if(st!=MOSAIC_OK){free(l);return st;}l->pack_len=pack_len;
    if(!load_lexer_pack(l->pack,l->pack_len,&l->view)||!sha256_bytes(l->pack,l->pack_len,l->hash)){free(l->pack);free(l);return MOSAIC_ERROR_INVALID_PACK;}
    *out_lexer=l;return MOSAIC_OK;
}
mosaic_status mosaic_lexer_load_file(const char *path,mosaic_lexer **out_lexer){if(!path||!out_lexer)return MOSAIC_ERROR_INVALID_ARGUMENT;uint8_t*d=NULL;size_t n=0;if(!read_file(path,&d,&n))return MOSAIC_ERROR_IO;mosaic_status st=mosaic_lexer_load_memory(d,n,out_lexer);free(d);return st;}
void mosaic_lexer_free(mosaic_lexer *lexer){if(!lexer)return;free(lexer->pack);free(lexer);}
mosaic_status mosaic_lexer_profile_name(const mosaic_lexer *lexer,char *buffer,size_t capacity,size_t *out_required){if(!lexer||!out_required)return MOSAIC_ERROR_INVALID_ARGUMENT;size_t need=(size_t)lexer->view.name_len+1u;*out_required=need;if(!buffer)return capacity?MOSAIC_ERROR_INVALID_ARGUMENT:MOSAIC_OK;if(capacity<need)return MOSAIC_ERROR_OVERFLOW;memcpy(buffer,lexer->view.section+lexer->view.blob_offset+lexer->view.name_offset,lexer->view.name_len);buffer[lexer->view.name_len]='\0';return MOSAIC_OK;}
mosaic_status mosaic_lex(const mosaic_lexer *lexer,const uint8_t *input,size_t input_len,mosaic_lex_token **out_tokens,size_t *out_count){if(!lexer||(!input&&input_len)||!out_tokens||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_tokens=NULL;*out_count=0;if(!lex_view(&lexer->view,input,input_len,out_tokens,out_count))return MOSAIC_ERROR_OUT_OF_MEMORY;return MOSAIC_OK;}

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
    if (tokenizer->normalization) {
        static const uint8_t normalization_domain[] = "NORMALIZATION\0";
        (void)sha256_update(&ctx, normalization_domain, sizeof normalization_domain - 1u);
        (void)sha256_update(&ctx, tokenizer->normalization->hash, 32u);
    }
    if (tokenizer->lexer) {
        static const uint8_t lexer_domain[] = "LEXER\0";
        (void)sha256_update(&ctx, lexer_domain, sizeof lexer_domain - 1u);
        (void)sha256_update(&ctx, tokenizer->lexer->hash, 32u);
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
    mosaic_runtime_limits_default(&tokenizer->limits);
    atomic_init(&tokenizer->sealed, 0);
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
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
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
    if (source->normalization) {
        status = mosaic_tokenizer_set_normalization_memory(copy, source->normalization->pack, source->normalization->pack_len);
        if (status != MOSAIC_OK) { mosaic_tokenizer_free(copy); return status; }
    }
    if (source->lexer) {
        status = mosaic_tokenizer_set_lexer_memory(copy, source->lexer->pack, source->lexer->pack_len);
        if (status != MOSAIC_OK) { mosaic_tokenizer_free(copy); return status; }
    }
    copy->limits = source->limits;
    copy->observer = source->observer;
    if (atomic_load_explicit(&source->sealed, memory_order_acquire)) atomic_store_explicit(&copy->sealed, 1, memory_order_release);
    *out_tokenizer = copy;
    return MOSAIC_OK;
}

static int online_parent_position(const Vocab *v, const uint32_t *back, size_t position, size_t *out_parent) {
    if (!position) { *out_parent = 0; return 1; }
    uint32_t entry_index = back[position];
    VocabEntry e; Slice surface;
    if (entry_index == UINT32_MAX || !vocab_entry(v, entry_index, &e, &surface) || !surface.len || surface.len > position) return 0;
    *out_parent = position - surface.len;
    return 1;
}

static int online_lca(const Vocab *v, const uint32_t *back, const size_t *depth, size_t a, size_t b, size_t *out) {
    while (depth[a] > depth[b]) { if (!online_parent_position(v, back, a, &a)) return 0; }
    while (depth[b] > depth[a]) { if (!online_parent_position(v, back, b, &b)) return 0; }
    while (a != b) {
        if (!online_parent_position(v, back, a, &a) || !online_parent_position(v, back, b, &b)) return 0;
    }
    *out = a; return 1;
}

static int online_ids_to_end(const Vocab *v, const uint32_t *back, size_t end, size_t count,
                             uint32_t **out_ids, size_t *out_count) {
    if (count > SIZE_MAX / sizeof(uint32_t)) return 0;
    uint32_t *ids = count ? (uint32_t *)malloc(count * sizeof *ids) : NULL;
    if (count && !ids) return 0;
    size_t cursor = end, pos = count;
    while (cursor) {
        uint32_t entry_index = back[cursor]; VocabEntry e; Slice surface; size_t parent;
        if (!online_parent_position(v, back, cursor, &parent) || !vocab_entry(v, entry_index, &e, &surface) || !pos) { free(ids); return 0; }
        ids[--pos] = e.token_id; cursor = parent;
    }
    if (pos) { free(ids); return 0; }
    *out_ids = ids; *out_count = count; return 1;
}

static int append_ids(uint32_t **items, size_t *count, size_t *capacity, const uint32_t *more, size_t more_count) {
    if (!more_count) return 1;
    if (more_count > SIZE_MAX - *count) return 0;
    size_t needed = *count + more_count;
    if (needed > *capacity) {
        size_t next = *capacity ? *capacity : 64;
        while (next < needed) { if (next > SIZE_MAX / 2) { next = needed; break; } next *= 2; }
        if (next > SIZE_MAX / sizeof(uint32_t)) return 0;
        uint32_t *grown = (uint32_t *)realloc(*items, next * sizeof *grown);
        if (!grown) return 0;
        *items = grown; *capacity = next;
    }
    memcpy(*items + *count, more, more_count * sizeof *more);
    *count = needed; return 1;
}

static mosaic_status online_commit_prefix(mosaic_online_stream *stream, int eof, uint32_t **out_ids, size_t *out_count, size_t *out_committed_bytes) {
    *out_ids = NULL; *out_count = 0; *out_committed_bytes = 0;
    if (!stream->pending_len) return MOSAIC_OK;
    if (stream->model->vocab.algorithm != 0u) return MOSAIC_ERROR_UNSUPPORTED;
    Tokenization result = {0}; size_t *depth = NULL;
    if (!build_viterbi_prefix_tree(&stream->model->vocab, (Slice){stream->pending, stream->pending_len}, stream->adjustments, &result, &depth))
        return MOSAIC_ERROR_INTERNAL;
    size_t commit_end = stream->pending_len;
    if (!eof) {
        size_t n = stream->pending_len;
        size_t max_len = stream->model->vocab.max_surface_len;
        size_t frontier_start = n >= max_len - 1u ? n - (max_len - 1u) : 0u;
        commit_end = frontier_start;
        for (size_t pos = frontier_start + 1u; pos <= n && commit_end; ++pos) {
            size_t lca;
            if (!online_lca(&stream->model->vocab, result.back, depth, commit_end, pos, &lca)) {
                free(depth); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL;
            }
            commit_end = lca;
        }
    }
    mosaic_status status = MOSAIC_OK;
    if (commit_end && !online_ids_to_end(&stream->model->vocab, result.back, commit_end, depth[commit_end], out_ids, out_count)) status = MOSAIC_ERROR_OUT_OF_MEMORY;
    free(depth); tokenization_free(&result);
    if (status != MOSAIC_OK) return status;
    if (commit_end) {
        memmove(stream->pending, stream->pending + commit_end, stream->pending_len - commit_end);
        stream->pending_len -= commit_end;
        *out_committed_bytes = commit_end;
    }
    return MOSAIC_OK;
}

static mosaic_status online_stream_allocate(const mosaic_model *model, const int64_t *adjustments, size_t max_pending_bytes,
                                            mosaic_online_stream **out_stream) {
    if (!model || !out_stream || !max_pending_bytes) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (model->vocab.algorithm != 0u) return MOSAIC_ERROR_UNSUPPORTED;
    if (max_pending_bytes < model->vocab.max_surface_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_stream = NULL;
    mosaic_online_stream *stream = (mosaic_online_stream *)calloc(1, sizeof *stream);
    if (!stream) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model->pack, model->pack_len, &stream->model);
    if (status != MOSAIC_OK) { free(stream); return status; }
    if (adjustments) {
        stream->adjustments = (int64_t *)malloc((size_t)stream->model->vocab.count * sizeof *stream->adjustments);
        if (!stream->adjustments) { mosaic_model_free(stream->model); free(stream); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        memcpy(stream->adjustments, adjustments, (size_t)stream->model->vocab.count * sizeof *stream->adjustments);
    }
    stream->max_pending_bytes = max_pending_bytes;
    *out_stream = stream; return MOSAIC_OK;
}

mosaic_status mosaic_online_stream_create(const mosaic_model *model, size_t max_pending_bytes, mosaic_online_stream **out_stream) {
    return online_stream_allocate(model, NULL, max_pending_bytes, out_stream);
}

mosaic_status mosaic_tokenizer_online_stream_create(const mosaic_tokenizer *tokenizer, size_t max_pending_bytes,
                                                     mosaic_online_stream **out_stream) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    return online_stream_allocate(tokenizer->model, tokenizer->adjustments, max_pending_bytes, out_stream);
}

mosaic_status mosaic_online_stream_push(mosaic_online_stream *stream, const uint8_t *bytes, size_t len,
                                        size_t *out_consumed, uint32_t **out_ids, size_t *out_count) {
    if (!stream || (!bytes && len) || !out_consumed || !out_ids || !out_count || stream->finished) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_consumed = 0; *out_ids = NULL; *out_count = 0;
    uint32_t *all = NULL; size_t all_count = 0, all_capacity = 0;
    while (*out_consumed < len) {
        if (stream->pending_len == stream->max_pending_bytes) {
            uint32_t *ids = NULL; size_t count = 0, committed = 0;
            mosaic_status status = online_commit_prefix(stream, 0, &ids, &count, &committed);
            if (status != MOSAIC_OK) { free(ids); free(all); return status; }
            if (!append_ids(&all, &all_count, &all_capacity, ids, count)) { free(ids); free(all); return MOSAIC_ERROR_OUT_OF_MEMORY; }
            free(ids);
            if (!committed) { *out_ids = all; *out_count = all_count; return MOSAIC_ERROR_RESOURCE_LIMIT; }
        }
        size_t room = stream->max_pending_bytes - stream->pending_len;
        size_t remaining = len - *out_consumed;
        size_t step = remaining < room ? remaining : room;
        if (step > 65536u) step = 65536u;
        if (stream->pending_len + step > stream->pending_capacity) {
            size_t needed = stream->pending_len + step;
            size_t next = stream->pending_capacity ? stream->pending_capacity : 4096u;
            while (next < needed) { if (next > stream->max_pending_bytes / 2u) { next = stream->max_pending_bytes; break; } next *= 2u; }
            if (next < needed) next = needed;
            uint8_t *grown = (uint8_t *)realloc(stream->pending, next);
            if (!grown) { free(all); return MOSAIC_ERROR_OUT_OF_MEMORY; }
            stream->pending = grown; stream->pending_capacity = next;
        }
        memcpy(stream->pending + stream->pending_len, bytes + *out_consumed, step);
        stream->pending_len += step; *out_consumed += step;
        uint32_t *ids = NULL; size_t count = 0, committed = 0;
        mosaic_status status = online_commit_prefix(stream, 0, &ids, &count, &committed);
        if (status != MOSAIC_OK) { free(ids); free(all); return status; }
        if (!append_ids(&all, &all_count, &all_capacity, ids, count)) { free(ids); free(all); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        free(ids);
    }
    *out_ids = all; *out_count = all_count; return MOSAIC_OK;
}

mosaic_status mosaic_online_stream_finish(mosaic_online_stream *stream, uint32_t **out_ids, size_t *out_count) {
    if (!stream || !out_ids || !out_count || stream->finished) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t committed = 0;
    mosaic_status status = online_commit_prefix(stream, 1, out_ids, out_count, &committed);
    if (status == MOSAIC_OK) stream->finished = 1;
    return status;
}

size_t mosaic_online_stream_pending_bytes(const mosaic_online_stream *stream) { return stream ? stream->pending_len : 0u; }

void mosaic_online_stream_free(mosaic_online_stream *stream) {
    if (!stream) return;
    mosaic_model_free(stream->model); free(stream->adjustments); free(stream->pending); free(stream);
}

static void resync_checkpoint_free(ResyncCheckpoint *cp) {
    if (!cp) return;
    free(cp->pending);
    *cp = (ResyncCheckpoint){0};
}

static void resync_checkpoint_array_free(ResyncCheckpoint *items, size_t count) {
    if (!items) return;
    for (size_t i = 0; i < count; ++i) resync_checkpoint_free(&items[i]);
    free(items);
}

static int resync_checkpoint_append(ResyncCheckpoint **items, size_t *count, size_t *capacity,
                                    size_t consumed, size_t committed_end, size_t prefix_token_count,
                                    const uint8_t *pending, size_t pending_len) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2u : 16u;
        if (next < *capacity || next > SIZE_MAX / sizeof **items) return 0;
        ResyncCheckpoint *grown = (ResyncCheckpoint *)realloc(*items, next * sizeof *grown);
        if (!grown) return 0;
        *items = grown; *capacity = next;
    }
    uint8_t *copy = pending_len ? (uint8_t *)malloc(pending_len) : NULL;
    if (pending_len && !copy) return 0;
    if (pending_len) memcpy(copy, pending, pending_len);
    (*items)[(*count)++] = (ResyncCheckpoint){consumed, committed_end, prefix_token_count, copy, pending_len};
    return 1;
}

static int resync_token_append(ResyncToken **items, size_t *count, size_t *capacity, ResyncToken value) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2u : 64u;
        if (next < *capacity || next > SIZE_MAX / sizeof **items) return 0;
        ResyncToken *grown = (ResyncToken *)realloc(*items, next * sizeof *grown);
        if (!grown) return 0;
        *items = grown; *capacity = next;
    }
    (*items)[(*count)++] = value; return 1;
}


static int resync_append_ids_as_tokens(const mosaic_model *model, const uint8_t *source, size_t source_len,
                                       size_t *cursor, const uint32_t *ids, size_t id_count,
                                       ResyncToken **items, size_t *count, size_t *capacity) {
    if (!model || !cursor || !items || !count || !capacity || (!ids && id_count) || (!source && (source_len || id_count)) || *cursor > source_len) return 0;
    for (size_t i = 0; i < id_count; ++i) {
        VocabEntry e; Slice surface;
        if (!vocab_by_id(&model->vocab, ids[i], &e, &surface) || surface.len > source_len - *cursor || surface.len > UINT16_MAX) return 0;
        if (surface.len && memcmp(source + *cursor, surface.bytes, surface.len) != 0) return 0;
        if (!resync_token_append(items, count, capacity, (ResyncToken){e.token_id, (uint16_t)surface.len, 0u})) return 0;
        *cursor += surface.len;
    }
    return 1;
}

static int resync_compare_ids_to_cached(const mosaic_resync_document *doc, const uint32_t *ids, size_t id_count,
                                        size_t *index, size_t *byte_cursor) {
    if (id_count > doc->token_count - *index) return 0;
    for (size_t i = 0; i < id_count; ++i) {
        const ResyncToken token = doc->tokens[*index + i];
        if (token.id != ids[i] || token.length > SIZE_MAX - *byte_cursor) return 0;
        *byte_cursor += token.length;
    }
    *index += id_count; return 1;
}

static mosaic_status resync_encode_compact(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                           const int64_t *adjustments, ResyncToken **out_tokens, size_t *out_count) {
    if (!model || (!input && input_len) || !out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokens = NULL; *out_count = 0;
    Tokenization result = {0};
    if (!tokenize_with_adjustments(&model->vocab, (Slice){input, input_len}, adjustments, &result)) return MOSAIC_ERROR_INTERNAL;
    if (result.count > SIZE_MAX / sizeof(ResyncToken)) { tokenization_free(&result); return MOSAIC_ERROR_OVERFLOW; }
    ResyncToken *tokens = result.count ? (ResyncToken *)malloc(result.count * sizeof *tokens) : NULL;
    if (result.count && !tokens) { tokenization_free(&result); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    size_t pos = input_len, out = result.count;
    while (pos) {
        uint32_t entry_index = result.back[pos]; VocabEntry entry; Slice surface;
        if (entry_index == UINT32_MAX || !vocab_entry(&model->vocab, entry_index, &entry, &surface) ||
            !surface.len || surface.len > pos || surface.len > UINT16_MAX || !out) {
            free(tokens); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL;
        }
        tokens[--out] = (ResyncToken){entry.token_id, (uint16_t)surface.len, 0u};
        pos -= surface.len;
    }
    if (out != 0u) { free(tokens); tokenization_free(&result); return MOSAIC_ERROR_INTERNAL; }
    size_t count = result.count; tokenization_free(&result); *out_tokens = tokens; *out_count = count; return MOSAIC_OK;
}

static mosaic_status resync_build_checkpoints(mosaic_resync_document *doc) {
    mosaic_online_stream *stream = NULL;
    mosaic_status status = online_stream_allocate(doc->model, doc->adjustments, doc->max_pending_bytes, &stream);
    if (status != MOSAIC_OK) return status;
    ResyncCheckpoint *items = NULL; size_t count = 0, capacity = 0;
    if (!resync_checkpoint_append(&items, &count, &capacity, 0, 0, 0, NULL, 0)) { mosaic_online_stream_free(stream); return MOSAIC_ERROR_OUT_OF_MEMORY; }
    size_t pos = 0, token_index = 0, token_bytes = 0;
    while (pos < doc->len) {
        size_t target = doc->len - pos > doc->checkpoint_bytes ? pos + doc->checkpoint_bytes : doc->len;
        size_t consumed = 0; uint32_t *ids = NULL; size_t id_count = 0;
        status = mosaic_online_stream_push(stream, doc->buffer + pos, target - pos, &consumed, &ids, &id_count);
        if (status != MOSAIC_OK || consumed != target - pos || !resync_compare_ids_to_cached(doc, ids, id_count, &token_index, &token_bytes)) {
            mosaic_free(ids); resync_checkpoint_array_free(items, count); mosaic_online_stream_free(stream);
            return status == MOSAIC_OK ? MOSAIC_ERROR_INTERNAL : status;
        }
        mosaic_free(ids); pos += consumed;
        size_t committed_end = pos - stream->pending_len;
        if (token_bytes != committed_end) {
            resync_checkpoint_array_free(items, count); mosaic_online_stream_free(stream); return MOSAIC_ERROR_INTERNAL;
        }
        if (!resync_checkpoint_append(&items, &count, &capacity, pos, committed_end, token_index, stream->pending, stream->pending_len)) {
            resync_checkpoint_array_free(items, count); mosaic_online_stream_free(stream); return MOSAIC_ERROR_OUT_OF_MEMORY;
        }
    }
    uint32_t *tail = NULL; size_t tail_count = 0;
    status = mosaic_online_stream_finish(stream, &tail, &tail_count);
    if (status != MOSAIC_OK || !resync_compare_ids_to_cached(doc, tail, tail_count, &token_index, &token_bytes) || token_index != doc->token_count || token_bytes != doc->len) {
        mosaic_free(tail); resync_checkpoint_array_free(items, count); mosaic_online_stream_free(stream);
        return status == MOSAIC_OK ? MOSAIC_ERROR_INTERNAL : status;
    }
    mosaic_free(tail); mosaic_online_stream_free(stream);
    doc->checkpoints = items; doc->checkpoint_count = count; doc->checkpoint_capacity = capacity;
    return MOSAIC_OK;
}

static mosaic_status resync_document_allocate(const mosaic_model *model, const int64_t *adjustments,
                                               const uint8_t *input, size_t input_len,
                                               size_t checkpoint_bytes, size_t max_pending_bytes,
                                               mosaic_resync_document **out_document) {
    if (!model || (!input && input_len) || !checkpoint_bytes || !max_pending_bytes || !out_document) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    if (model->vocab.algorithm != 0u) return MOSAIC_ERROR_UNSUPPORTED;
    if (max_pending_bytes < model->vocab.max_surface_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    mosaic_resync_document *doc = (mosaic_resync_document *)calloc(1, sizeof *doc);
    if (!doc) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = mosaic_model_load_memory(model->pack, model->pack_len, &doc->model);
    if (status != MOSAIC_OK) { free(doc); return status; }
    if (adjustments) {
        size_t n = (size_t)doc->model->vocab.count;
        if (n > SIZE_MAX / sizeof *doc->adjustments) { mosaic_resync_document_free(doc); return MOSAIC_ERROR_OVERFLOW; }
        doc->adjustments = n ? (int64_t *)malloc(n * sizeof *doc->adjustments) : NULL;
        if (n && !doc->adjustments) { mosaic_resync_document_free(doc); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        if (n) memcpy(doc->adjustments, adjustments, n * sizeof *doc->adjustments);
    }
    if (input_len) {
        doc->buffer = (uint8_t *)malloc(input_len);
        if (!doc->buffer) { mosaic_resync_document_free(doc); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        memcpy(doc->buffer, input, input_len);
    }
    doc->len = input_len; doc->checkpoint_bytes = checkpoint_bytes; doc->max_pending_bytes = max_pending_bytes;
    status = resync_encode_compact(doc->model, doc->buffer, doc->len, doc->adjustments, &doc->tokens, &doc->token_count);
    if (status != MOSAIC_OK) { mosaic_resync_document_free(doc); return status; }
    doc->token_capacity = doc->token_count;
    if (doc->token_count) {
        size_t headroom = doc->token_count / 16u;
        if (headroom > 65536u) headroom = 65536u;
        if (headroom <= SIZE_MAX - doc->token_count && doc->token_count + headroom <= SIZE_MAX / sizeof *doc->tokens) {
            size_t capacity = doc->token_count + headroom;
            ResyncToken *grown = (ResyncToken *)realloc(doc->tokens, capacity * sizeof *grown);
            if (grown) { doc->tokens = grown; doc->token_capacity = capacity; }
        }
    }
    status = resync_build_checkpoints(doc);
    if (status != MOSAIC_OK) { mosaic_resync_document_free(doc); return status; }
    doc->last_reprocessed_bytes = input_len;
    *out_document = doc; return MOSAIC_OK;
}

mosaic_status mosaic_resync_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                            size_t checkpoint_bytes, size_t max_pending_bytes,
                                            mosaic_resync_document **out_document) {
    mosaic_status st=resync_document_allocate(model,NULL,input,input_len,checkpoint_bytes,max_pending_bytes,out_document);if(st==MOSAIC_OK)(*out_document)->max_document_bytes=SIZE_MAX;return st;
}

mosaic_status mosaic_tokenizer_resync_document_create(const mosaic_tokenizer *tokenizer,
                                                      const uint8_t *input, size_t input_len,
                                                      size_t checkpoint_bytes, size_t max_pending_bytes,
                                                      mosaic_resync_document **out_document) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if ((uint64_t)input_len > tokenizer->limits.max_input_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    mosaic_status st=resync_document_allocate(tokenizer->model,tokenizer->adjustments,input,input_len,checkpoint_bytes,max_pending_bytes,out_document);if(st==MOSAIC_OK)(*out_document)->max_document_bytes=(size_t)(tokenizer->limits.max_input_bytes>SIZE_MAX?SIZE_MAX:tokenizer->limits.max_input_bytes);return st;
}

static int resync_map_old_position(size_t old_pos, size_t delete_len, size_t replacement_len, size_t *new_pos) {
    if (replacement_len >= delete_len) {
        size_t add = replacement_len - delete_len;
        if (old_pos > SIZE_MAX - add) return 0;
        *new_pos = old_pos + add;
    } else {
        size_t sub = delete_len - replacement_len;
        if (old_pos < sub) return 0;
        *new_pos = old_pos - sub;
    }
    return 1;
}

static int resync_pending_equal(const mosaic_online_stream *stream, const ResyncCheckpoint *old) {
    return stream->pending_len == old->pending_len && (!old->pending_len || memcmp(stream->pending, old->pending, old->pending_len) == 0);
}

mosaic_status mosaic_resync_document_apply_edit(mosaic_resync_document *doc,
                                                 uint64_t start64, uint64_t delete64,
                                                 const uint8_t *replacement, size_t replacement_len) {
    if (!doc || (!replacement && replacement_len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (start64 > SIZE_MAX || delete64 > SIZE_MAX) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t start = (size_t)start64, delete_len = (size_t)delete64;
    if (start > doc->len || delete_len > doc->len - start) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t kept = doc->len - delete_len;
    if (replacement_len > SIZE_MAX - kept) return MOSAIC_ERROR_OVERFLOW;
    size_t new_len = kept + replacement_len, old_tail = start + delete_len;
    if (new_len > doc->max_document_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;

    /* Keep the existing document immutable until re-tokenization and resynchronization succeed. */
    uint8_t *next_buffer = new_len ? (uint8_t *)malloc(new_len) : NULL;
    if (new_len && !next_buffer) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (new_len) {
        if (start) memcpy(next_buffer, doc->buffer, start);
        if (replacement_len) memcpy(next_buffer + start, replacement, replacement_len);
        size_t tail_len = doc->len - old_tail;
        if (tail_len) memcpy(next_buffer + start + replacement_len, doc->buffer + old_tail, tail_len);
    }

    size_t base_cp = 0;
    for (size_t i = 1; i < doc->checkpoint_count && doc->checkpoints[i].consumed <= start; ++i) base_cp = i;
    const ResyncCheckpoint *base = &doc->checkpoints[base_cp];
    const size_t base_committed_end = base->committed_end;
    const size_t base_prefix_tokens = base->prefix_token_count;
    if (base_prefix_tokens > doc->token_count) { free(next_buffer); return MOSAIC_ERROR_INTERNAL; }

    mosaic_online_stream *stream = NULL;
    mosaic_status status = online_stream_allocate(doc->model, doc->adjustments, doc->max_pending_bytes, &stream);
    if (status != MOSAIC_OK) { free(next_buffer); return status; }
    if (base->pending_len) {
        stream->pending = (uint8_t *)malloc(base->pending_len);
        if (!stream->pending) { mosaic_online_stream_free(stream); free(next_buffer); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        memcpy(stream->pending, base->pending, base->pending_len);
        stream->pending_len = base->pending_len; stream->pending_capacity = base->pending_len;
    }

    /* Only tokens generated between the reusable prefix and a future resynchronization point are materialized. */
    ResyncToken *changed_tokens = NULL; size_t changed_count = 0, changed_capacity = 0;
    size_t token_cursor = base_committed_end;
    ResyncCheckpoint *new_cps = NULL; size_t new_cp_count = 0, new_cp_capacity = 0;
    for (size_t i = 0; i <= base_cp; ++i) {
        const ResyncCheckpoint *cp = &doc->checkpoints[i];
        if (!resync_checkpoint_append(&new_cps, &new_cp_count, &new_cp_capacity, cp->consumed, cp->committed_end,
                                      cp->prefix_token_count, cp->pending, cp->pending_len)) {
            resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); mosaic_online_stream_free(stream); free(next_buffer);
            return MOSAIC_ERROR_OUT_OF_MEMORY;
        }
    }

    size_t old_candidate = base_cp + 1u;
    while (old_candidate < doc->checkpoint_count && doc->checkpoints[old_candidate].consumed < old_tail) ++old_candidate;
    size_t pos = base->consumed, matched_old = SIZE_MAX;
    while (pos < new_len) {
        size_t candidate_target = new_len;
        if (old_candidate < doc->checkpoint_count) {
            if (!resync_map_old_position(doc->checkpoints[old_candidate].consumed, delete_len, replacement_len, &candidate_target)) {
                status = MOSAIC_ERROR_OVERFLOW; break;
            }
            if (candidate_target < pos) { ++old_candidate; continue; }
            if (candidate_target > new_len) candidate_target = new_len;
        }
        size_t target = candidate_target;
        if (target - pos > doc->checkpoint_bytes) target = pos + doc->checkpoint_bytes;
        if (target == pos && target < new_len) { status = MOSAIC_ERROR_INTERNAL; break; }
        size_t consumed = 0; uint32_t *ids = NULL; size_t id_count = 0;
        status = mosaic_online_stream_push(stream, next_buffer + pos, target - pos, &consumed, &ids, &id_count);
        if (status != MOSAIC_OK || consumed != target - pos ||
            !resync_append_ids_as_tokens(doc->model, next_buffer, new_len, &token_cursor, ids, id_count,
                                         &changed_tokens, &changed_count, &changed_capacity)) {
            mosaic_free(ids); if (status == MOSAIC_OK) status = MOSAIC_ERROR_INTERNAL; break;
        }
        mosaic_free(ids); pos += consumed;
        size_t committed_end = pos - stream->pending_len;
        if (token_cursor != committed_end || changed_count > SIZE_MAX - base_prefix_tokens) { status = MOSAIC_ERROR_INTERNAL; break; }
        size_t global_token_count = base_prefix_tokens + changed_count;
        if (!resync_checkpoint_append(&new_cps, &new_cp_count, &new_cp_capacity, pos, committed_end, global_token_count,
                                      stream->pending, stream->pending_len)) { status = MOSAIC_ERROR_OUT_OF_MEMORY; break; }
        if (old_candidate < doc->checkpoint_count && pos == candidate_target) {
            const ResyncCheckpoint *oldcp = &doc->checkpoints[old_candidate];
            if (oldcp->consumed >= old_tail && resync_pending_equal(stream, oldcp)) { matched_old = old_candidate; break; }
            ++old_candidate;
        }
    }

    size_t reused_suffix = 0, reused_suffix_tokens = 0, old_suffix_start = doc->token_count;
    size_t changed_end = new_len;
    int resynchronized = 0;
    if (status == MOSAIC_OK && matched_old != SIZE_MAX) {
        const ResyncCheckpoint *match = &doc->checkpoints[matched_old];
        size_t new_match_committed = pos - stream->pending_len;
        size_t mapped_committed;
        if (match->prefix_token_count > doc->token_count ||
            !resync_map_old_position(match->committed_end, delete_len, replacement_len, &mapped_committed) ||
            mapped_committed != new_match_committed) status = MOSAIC_ERROR_INTERNAL;
        if (status == MOSAIC_OK) {
            if (changed_count > SIZE_MAX - base_prefix_tokens) status = MOSAIC_ERROR_OVERFLOW;
            size_t before_suffix = base_prefix_tokens + changed_count;
            old_suffix_start = match->prefix_token_count;
            reused_suffix_tokens = doc->token_count - old_suffix_start;
            for (size_t i = matched_old + 1u; status == MOSAIC_OK && i < doc->checkpoint_count; ++i) {
                const ResyncCheckpoint *oldcp = &doc->checkpoints[i]; size_t mapped_consumed, mapped_end;
                if (!resync_map_old_position(oldcp->consumed, delete_len, replacement_len, &mapped_consumed) ||
                    !resync_map_old_position(oldcp->committed_end, delete_len, replacement_len, &mapped_end)) { status = MOSAIC_ERROR_OVERFLOW; break; }
                if (oldcp->prefix_token_count < match->prefix_token_count) { status = MOSAIC_ERROR_INTERNAL; break; }
                size_t suffix_token_offset = oldcp->prefix_token_count - match->prefix_token_count;
                if (suffix_token_offset > SIZE_MAX - before_suffix) { status = MOSAIC_ERROR_OVERFLOW; break; }
                size_t pc = before_suffix + suffix_token_offset;
                if (pc > base_prefix_tokens + changed_count + reused_suffix_tokens ||
                    !resync_checkpoint_append(&new_cps, &new_cp_count, &new_cp_capacity,
                                              mapped_consumed, mapped_end, pc,
                                              oldcp->pending, oldcp->pending_len)) { status = MOSAIC_ERROR_INTERNAL; break; }
            }
            if (status == MOSAIC_OK) {
                reused_suffix = new_len - new_match_committed;
                changed_end = new_match_committed;
                resynchronized = 1;
            }
        }
    } else if (status == MOSAIC_OK) {
        uint32_t *tail = NULL; size_t tail_count = 0;
        status = mosaic_online_stream_finish(stream, &tail, &tail_count);
        if (status == MOSAIC_OK &&
            !resync_append_ids_as_tokens(doc->model, next_buffer, new_len, &token_cursor, tail, tail_count,
                                         &changed_tokens, &changed_count, &changed_capacity)) status = MOSAIC_ERROR_INTERNAL;
        mosaic_free(tail);
        if (status == MOSAIC_OK && token_cursor != new_len) status = MOSAIC_ERROR_INTERNAL;
    }
    mosaic_online_stream_free(stream);
    if (status != MOSAIC_OK) {
        resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return status;
    }

    /* The changed token segment must exactly partition the bytes between reusable prefix and suffix. */
    size_t changed_bytes = 0;
    for (size_t i = 0; i < changed_count; ++i) {
        if (!changed_tokens[i].length || changed_tokens[i].length > changed_end - base_committed_end - changed_bytes) {
            resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return MOSAIC_ERROR_INTERNAL;
        }
        changed_bytes += changed_tokens[i].length;
    }
    if (base_committed_end > changed_end || changed_bytes != changed_end - base_committed_end ||
        reused_suffix != new_len - changed_end) {
        resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return MOSAIC_ERROR_INTERNAL;
    }

    if (base_prefix_tokens > SIZE_MAX - changed_count || base_prefix_tokens + changed_count > SIZE_MAX - reused_suffix_tokens) {
        resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return MOSAIC_ERROR_OVERFLOW;
    }
    size_t final_count = base_prefix_tokens + changed_count + reused_suffix_tokens;
    if (final_count > doc->token_capacity) {
        size_t headroom = final_count / 16u;
        if (headroom > 65536u) headroom = 65536u;
        size_t capacity = headroom <= SIZE_MAX - final_count ? final_count + headroom : final_count;
        if (capacity > SIZE_MAX / sizeof *doc->tokens) {
            resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return MOSAIC_ERROR_OVERFLOW;
        }
        ResyncToken *grown = (ResyncToken *)realloc(doc->tokens, capacity * sizeof *grown);
        if (!grown) {
            resync_checkpoint_array_free(new_cps, new_cp_count); free(changed_tokens); free(next_buffer); return MOSAIC_ERROR_OUT_OF_MEMORY;
        }
        doc->tokens = grown; doc->token_capacity = capacity;
    }

    /* Commit only the changed token interval; reusable prefix/suffix remain in the document allocation. */
    if (reused_suffix_tokens) {
        memmove(doc->tokens + base_prefix_tokens + changed_count,
                doc->tokens + old_suffix_start,
                reused_suffix_tokens * sizeof *doc->tokens);
    }
    if (changed_count) memcpy(doc->tokens + base_prefix_tokens, changed_tokens, changed_count * sizeof *doc->tokens);
    free(changed_tokens);

    size_t reprocessed = resynchronized ? (pos - base_committed_end) : (new_len - base_committed_end);
    free(doc->buffer); resync_checkpoint_array_free(doc->checkpoints, doc->checkpoint_count);
    doc->buffer = next_buffer; doc->len = new_len; doc->token_count = final_count;
    doc->checkpoints = new_cps; doc->checkpoint_count = new_cp_count; doc->checkpoint_capacity = new_cp_capacity;
    doc->last_reprocessed_bytes = reprocessed; doc->last_reused_prefix_bytes = base_committed_end;
    doc->last_reused_suffix_bytes = reused_suffix; doc->last_resynchronized = resynchronized;
    return MOSAIC_OK;
}

mosaic_status mosaic_resync_document_encode(const mosaic_resync_document *doc, uint32_t **out_ids, size_t *out_count) {
    if (!doc || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL; *out_count = 0;
    if (doc->token_count > SIZE_MAX / sizeof(uint32_t)) return MOSAIC_ERROR_OVERFLOW;
    uint32_t *ids = doc->token_count ? (uint32_t *)malloc(doc->token_count * sizeof *ids) : NULL;
    if (doc->token_count && !ids) return MOSAIC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < doc->token_count; ++i) ids[i] = doc->tokens[i].id;
    *out_ids = ids; *out_count = doc->token_count; return MOSAIC_OK;
}

mosaic_status mosaic_resync_document_copy_bytes(const mosaic_resync_document *doc, uint8_t **out_bytes, size_t *out_len) {
    if (!doc || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL; *out_len = 0; mosaic_status status = copy_bytes(doc->buffer, doc->len, out_bytes);
    if (status == MOSAIC_OK) *out_len = doc->len;
    return status;
}
size_t mosaic_resync_document_last_reprocessed_bytes(const mosaic_resync_document *doc) { return doc ? doc->last_reprocessed_bytes : 0u; }
size_t mosaic_resync_document_last_reused_prefix_bytes(const mosaic_resync_document *doc) { return doc ? doc->last_reused_prefix_bytes : 0u; }
size_t mosaic_resync_document_last_reused_suffix_bytes(const mosaic_resync_document *doc) { return doc ? doc->last_reused_suffix_bytes : 0u; }
int mosaic_resync_document_last_resynchronized(const mosaic_resync_document *doc) { return doc && doc->last_resynchronized; }
void mosaic_resync_document_free(mosaic_resync_document *doc) {
    if (!doc) return;
    mosaic_model_free(doc->model);
    free(doc->adjustments);
    free(doc->buffer);
    free(doc->tokens);
    resync_checkpoint_array_free(doc->checkpoints, doc->checkpoint_count);
    free(doc);
}

mosaic_status mosaic_tokenizer_set_detector_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *detector_pack, size_t detector_pack_len) {
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
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
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
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
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
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
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
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
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_SECURITY, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_security_scan(tokenizer->security, input, input_len, out_findings, out_count);
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_SECURITY,
                      status, input_len, status == MOSAIC_OK && out_count ? *out_count : 0u, 0u);
    return status;
}
mosaic_status mosaic_tokenizer_security_visit(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                               mosaic_security_visitor visitor, void *context, size_t *out_count) {
    if (!tokenizer || !tokenizer->security) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_SECURITY, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_security_visit(tokenizer->security, input, input_len, visitor, context, out_count);
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_SECURITY,
                      status, input_len, status == MOSAIC_OK && out_count ? *out_count : 0u, 0u);
    return status;
}

mosaic_status mosaic_tokenizer_set_normalization_memory(mosaic_tokenizer *tokenizer,const uint8_t *pack,size_t pack_len){
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    if(!tokenizer||(!pack&&pack_len))return MOSAIC_ERROR_INVALID_ARGUMENT;
    if(tokenizer->normalization)return MOSAIC_ERROR_CONFLICT;
    mosaic_normalization*n=NULL;
    mosaic_status status=mosaic_normalization_load_memory(pack,pack_len,&n);
    if(status!=MOSAIC_OK)return status;
    tokenizer->normalization=n;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}
mosaic_status mosaic_tokenizer_set_normalization_file(mosaic_tokenizer *tokenizer, const char *path) {
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    if (!tokenizer || !path) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->normalization) return MOSAIC_ERROR_CONFLICT;
    mosaic_normalization *normalization = NULL;
    mosaic_status status = mosaic_normalization_load_file(path, &normalization);
    if (status != MOSAIC_OK) return status;
    tokenizer->normalization = normalization;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

int mosaic_tokenizer_lexer_loaded(const mosaic_tokenizer *tokenizer) {
    return tokenizer && tokenizer->lexer ? 1 : 0;
}

mosaic_status mosaic_tokenizer_set_lexer_memory(mosaic_tokenizer *tokenizer, const uint8_t *pack, size_t len) {
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    if (!tokenizer || (!pack && len)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->lexer) return MOSAIC_ERROR_CONFLICT;
    mosaic_lexer *lexer = NULL;
    mosaic_status status = mosaic_lexer_load_memory(pack, len, &lexer);
    if (status != MOSAIC_OK) return status;
    tokenizer->lexer = lexer;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_set_lexer_file(mosaic_tokenizer *tokenizer, const char *path) {
    if (tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    if (!tokenizer || !path) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (tokenizer->lexer) return MOSAIC_ERROR_CONFLICT;
    mosaic_lexer *lexer = NULL;
    mosaic_status status = mosaic_lexer_load_file(path, &lexer);
    if (status != MOSAIC_OK) return status;
    tokenizer->lexer = lexer;
    tokenizer_compute_fingerprint(tokenizer);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_lex(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t len,
                                  mosaic_lex_token **out, size_t *count) {
    if (!tokenizer || !tokenizer->lexer) return MOSAIC_ERROR_UNSUPPORTED;
    if (!tokenizer_input_allowed(tokenizer, len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_LEX, MOSAIC_ERROR_RESOURCE_LIMIT,
                          len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_lex(tokenizer->lexer, input, len, out, count);
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_LEX,
                      status, len, status == MOSAIC_OK && count ? *count : 0u, 0u);
    return status;
}

int mosaic_tokenizer_normalization_loaded(const mosaic_tokenizer*tokenizer){return tokenizer&&tokenizer->normalization;}
mosaic_status mosaic_tokenizer_normalize(const mosaic_tokenizer *tokenizer, mosaic_normalization_mode mode,
                                          const uint8_t *input, size_t input_len, mosaic_normalized_view *out_view) {
    if (!tokenizer || !tokenizer->normalization) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_NORMALIZE, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_normalize(tokenizer->normalization, mode, input, input_len, out_view);
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_NORMALIZE,
                      status, input_len, status == MOSAIC_OK && out_view ? out_view->byte_length : 0u, 0u);
    return status;
}

static const LanguagePack *tokenizer_language_for_detection(const mosaic_tokenizer *tokenizer,
                                                             const mosaic_detection *detection) {
    if (!detection->matched) return NULL;
    for (size_t i = 0; i < tokenizer->language_count; ++i)
        if (language_matches_cstr(&tokenizer->languages[i], detection->language)) return &tokenizer->languages[i];
    return NULL;
}

typedef struct {
    mosaic_span_route *items;
    size_t len;
    size_t cap;
} SpanRouteVec;

static mosaic_status span_route_vec_push(SpanRouteVec *vec, size_t start, size_t length,
                                         const mosaic_detection *detection) {
    if (!vec || !detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (vec->len == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2u : 8u;
        if (cap < vec->cap || cap > SIZE_MAX / sizeof *vec->items) return MOSAIC_ERROR_OVERFLOW;
        mosaic_span_route *next = (mosaic_span_route *)realloc(vec->items, cap * sizeof *next);
        if (!next) return MOSAIC_ERROR_OUT_OF_MEMORY;
        vec->items = next; vec->cap = cap;
    }
    vec->items[vec->len++] = (mosaic_span_route){(uint64_t)start, (uint64_t)length, *detection};
    return MOSAIC_OK;
}

static mosaic_status u32_vec_append(uint32_t **items, size_t *len, size_t *cap,
                                    const uint32_t *src, size_t count) {
    if (!items || !len || !cap || (!src && count)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (count > SIZE_MAX - *len) return MOSAIC_ERROR_OVERFLOW;
    size_t required = *len + count;
    if (required > *cap) {
        size_t next_cap = *cap ? *cap : 16u;
        while (next_cap < required) {
            if (next_cap > SIZE_MAX / 2u) { next_cap = required; break; }
            next_cap *= 2u;
        }
        if (next_cap > SIZE_MAX / sizeof **items) return MOSAIC_ERROR_OVERFLOW;
        uint32_t *next = (uint32_t *)realloc(*items, next_cap * sizeof *next);
        if (!next) return MOSAIC_ERROR_OUT_OF_MEMORY;
        *items = next; *cap = next_cap;
    }
    if (count) memcpy(*items + *len, src, count * sizeof *src);
    *len = required;
    return MOSAIC_OK;
}

static size_t span_utf8_advance(const uint8_t *input, size_t remaining, uint32_t *out_cp) {
    if (!input || !remaining || !out_cp) return 0;
    uint8_t b0 = input[0];
    if (b0 < 0x80u) { *out_cp = b0; return 1u; }
    if ((b0 & 0xe0u) == 0xc0u && remaining >= 2u && (input[1] & 0xc0u) == 0x80u) {
        uint32_t cp = ((uint32_t)(b0 & 0x1fu) << 6) | (uint32_t)(input[1] & 0x3fu);
        if (cp >= 0x80u) { *out_cp = cp; return 2u; }
    } else if ((b0 & 0xf0u) == 0xe0u && remaining >= 3u &&
               (input[1] & 0xc0u) == 0x80u && (input[2] & 0xc0u) == 0x80u) {
        uint32_t cp = ((uint32_t)(b0 & 0x0fu) << 12) | ((uint32_t)(input[1] & 0x3fu) << 6) |
                      (uint32_t)(input[2] & 0x3fu);
        if (cp >= 0x800u && !(cp >= 0xd800u && cp <= 0xdfffu)) { *out_cp = cp; return 3u; }
    } else if ((b0 & 0xf8u) == 0xf0u && remaining >= 4u &&
               (input[1] & 0xc0u) == 0x80u && (input[2] & 0xc0u) == 0x80u &&
               (input[3] & 0xc0u) == 0x80u) {
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(input[1] & 0x3fu) << 12) |
                      ((uint32_t)(input[2] & 0x3fu) << 6) | (uint32_t)(input[3] & 0x3fu);
        if (cp >= 0x10000u && cp <= 0x10ffffu) { *out_cp = cp; return 4u; }
    }
    *out_cp = 0xfffdu;
    return 1u;
}

static int span_route_class(uint32_t cp) {
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') || cp == '_')
        return 1;
    if (cp >= 0x0900u && cp <= 0x097fu) return 2;       /* Devanagari */
    if ((cp >= 0x3040u && cp <= 0x30ffu) ||              /* Japanese kana */
        (cp >= 0x3400u && cp <= 0x9fffu)) return 3;      /* CJK ideographs */
    if (cp >= 0x80u) return 4;
    return 0;
}

static mosaic_status tokenizer_detect_span(const mosaic_tokenizer *tokenizer,
                                           const uint8_t *input, size_t input_len, int route_class,
                                           mosaic_detection *out_detection) {
    if (!tokenizer || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    memset(out_detection, 0, sizeof *out_detection);
    if (!route_class || !input_len) return MOSAIC_OK;
    mosaic_status status = mosaic_detector_detect(tokenizer->detector, input, input_len, out_detection);
    if (status == MOSAIC_OK)
        out_detection->available = tokenizer_language_for_detection(tokenizer, out_detection) ? 1u : 0u;
    return status;
}

static mosaic_status tokenizer_detect_spans_internal(const mosaic_tokenizer *tokenizer,
                                                     const uint8_t *input, size_t input_len,
                                                     SpanRouteVec *out_vec) {
    if (!tokenizer || !tokenizer->detector || (!input && input_len) || !out_vec) return MOSAIC_ERROR_INVALID_ARGUMENT;
    size_t pos = 0;
    while (pos < input_len) {
        uint32_t cp = 0;
        size_t step = span_utf8_advance(input + pos, input_len - pos, &cp);
        if (!step) return MOSAIC_ERROR_INTERNAL;
        int klass = span_route_class(cp);
        size_t start = pos;
        pos += step;
        while (pos < input_len) {
            uint32_t next_cp = 0;
            size_t next_step = span_utf8_advance(input + pos, input_len - pos, &next_cp);
            if (!next_step) return MOSAIC_ERROR_INTERNAL;
            if (span_route_class(next_cp) != klass) break;
            pos += next_step;
        }
        mosaic_detection detection;
        mosaic_status status = tokenizer_detect_span(tokenizer, input + start, pos - start, klass, &detection);
        if (status != MOSAIC_OK) return status;
        status = span_route_vec_push(out_vec, start, pos - start, &detection);
        if (status != MOSAIC_OK) return status;
    }
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_detect_language(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_detection *out_detection) {
    if (!tokenizer || !tokenizer->detector || (!input && input_len) || !out_detection)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_DETECT, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_detector_detect(tokenizer->detector, input, input_len, out_detection);
    if (status == MOSAIC_OK) out_detection->available = tokenizer_language_for_detection(tokenizer, out_detection) ? 1u : 0u;
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_DETECT,
                      status, input_len, status == MOSAIC_OK && out_detection->matched ? 1u : 0u, 0u);
    return status;
}

mosaic_status mosaic_tokenizer_detect_spans(const mosaic_tokenizer *tokenizer,
                                            const uint8_t *input, size_t input_len,
                                            mosaic_span_route **out_routes, size_t *out_count) {
    if (!tokenizer || !tokenizer->detector || (!input && input_len) || !out_routes || !out_count)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_routes = NULL; *out_count = 0;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_DETECT, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    SpanRouteVec routes = {0};
    mosaic_status status = tokenizer_detect_spans_internal(tokenizer, input, input_len, &routes);
    if (status == MOSAIC_OK) {
        *out_routes = routes.items; *out_count = routes.len;
    } else {
        free(routes.items);
    }
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_DETECT,
                      status, input_len, status == MOSAIC_OK ? *out_count : 0u, 0u);
    return status;
}

mosaic_status mosaic_tokenizer_encode_auto(const mosaic_tokenizer *tokenizer,
                                           const uint8_t *input, size_t input_len,
                                           uint32_t **out_ids, size_t *out_count,
                                           mosaic_detection *out_detection) {
    if (!tokenizer || !out_ids || !out_count || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) { tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_ENCODE, MOSAIC_ERROR_RESOURCE_LIMIT, input_len, 0u, tokenizer->limits.max_input_bytes); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    TokenizerTelemetry *m = &((mosaic_tokenizer *)tokenizer)->telemetry;
    atomic_fetch_add_explicit(&m->encode_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&m->bytes_in, input_len, memory_order_relaxed);
    mosaic_status status = mosaic_tokenizer_detect_language(tokenizer, input, input_len, out_detection);
    if (status == MOSAIC_OK) {
        const LanguagePack *pack = tokenizer_language_for_detection(tokenizer, out_detection);
        status = encode_internal(tokenizer->model, input, input_len, pack ? pack->adjustments : NULL,
                                 (size_t)(tokenizer->limits.max_output_tokens > SIZE_MAX ? SIZE_MAX : tokenizer->limits.max_output_tokens),
                                 out_ids, out_count);
    }
    if (status == MOSAIC_OK && (uint64_t)*out_count > tokenizer->limits.max_output_tokens) {
        mosaic_free(*out_ids); *out_ids = NULL; *out_count = 0; status = MOSAIC_ERROR_RESOURCE_LIMIT;
        atomic_fetch_add_explicit(&m->resource_rejections, 1, memory_order_relaxed);
    }
    if (status == MOSAIC_OK) {
        atomic_fetch_add_explicit(&m->tokens_out, *out_count, memory_order_relaxed);
        tokenizer_observe(tokenizer, MOSAIC_EVENT_SUCCESS, MOSAIC_OPERATION_ENCODE, status, input_len, *out_count, 0u);
    } else {
        atomic_fetch_add_explicit(&m->failures, 1, memory_order_relaxed);
        tokenizer_observe(tokenizer, status == MOSAIC_ERROR_RESOURCE_LIMIT ? MOSAIC_EVENT_RESOURCE_REJECTED : MOSAIC_EVENT_FAILURE,
                          MOSAIC_OPERATION_ENCODE, status, input_len, 0u,
                          status == MOSAIC_ERROR_RESOURCE_LIMIT ? tokenizer->limits.max_output_tokens : 0u);
    }
    return status;
}

mosaic_status mosaic_tokenizer_encode_span_auto(const mosaic_tokenizer *tokenizer,
                                                const uint8_t *input, size_t input_len,
                                                uint32_t **out_ids, size_t *out_count,
                                                mosaic_span_route **out_routes, size_t *out_route_count) {
    if (!tokenizer || !tokenizer->detector || (!input && input_len) || !out_ids || !out_count ||
        !out_routes || !out_route_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ids = NULL; *out_count = 0; *out_routes = NULL; *out_route_count = 0;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_ENCODE, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }

    TokenizerTelemetry *m = &((mosaic_tokenizer *)tokenizer)->telemetry;
    atomic_fetch_add_explicit(&m->encode_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&m->bytes_in, input_len, memory_order_relaxed);

    SpanRouteVec routes = {0};
    uint32_t *ids = NULL;
    size_t id_count = 0, id_cap = 0;
    mosaic_status status = tokenizer_detect_spans_internal(tokenizer, input, input_len, &routes);
    size_t token_limit = (size_t)(tokenizer->limits.max_output_tokens > SIZE_MAX ? SIZE_MAX : tokenizer->limits.max_output_tokens);
    for (size_t i = 0; status == MOSAIC_OK && i < routes.len; ++i) {
        if (id_count > token_limit) { status = MOSAIC_ERROR_RESOURCE_LIMIT; break; }
        mosaic_span_route *route = &routes.items[i];
        const LanguagePack *pack = tokenizer_language_for_detection(tokenizer, &route->detection);
        uint32_t *span_ids = NULL;
        size_t span_count = 0;
        size_t remaining = token_limit - id_count;
        status = encode_internal(tokenizer->model, input + (size_t)route->start, (size_t)route->length,
                                 pack ? pack->adjustments : NULL, remaining, &span_ids, &span_count);
        if (status == MOSAIC_OK) {
            status = u32_vec_append(&ids, &id_count, &id_cap, span_ids, span_count);
        }
        mosaic_free(span_ids);
    }

    if (status == MOSAIC_OK) {
        *out_ids = ids; *out_count = id_count; *out_routes = routes.items; *out_route_count = routes.len;
        atomic_fetch_add_explicit(&m->tokens_out, id_count, memory_order_relaxed);
        tokenizer_observe(tokenizer, MOSAIC_EVENT_SUCCESS, MOSAIC_OPERATION_ENCODE, status, input_len, id_count, 0u);
    } else {
        free(ids); free(routes.items);
        atomic_fetch_add_explicit(&m->failures, 1, memory_order_relaxed);
        if (status == MOSAIC_ERROR_RESOURCE_LIMIT) atomic_fetch_add_explicit(&m->resource_rejections, 1, memory_order_relaxed);
        tokenizer_observe(tokenizer, status == MOSAIC_ERROR_RESOURCE_LIMIT ? MOSAIC_EVENT_RESOURCE_REJECTED : MOSAIC_EVENT_FAILURE,
                          MOSAIC_OPERATION_ENCODE, status, input_len, 0u,
                          status == MOSAIC_ERROR_RESOURCE_LIMIT ? tokenizer->limits.max_output_tokens : 0u);
    }
    return status;
}

mosaic_status mosaic_tokenizer_encode_tokens_auto(const mosaic_tokenizer *tokenizer,
                                                  const uint8_t *input, size_t input_len,
                                                  mosaic_token **out_tokens, size_t *out_count,
                                                  mosaic_detection *out_detection) {
    if (!tokenizer || !out_tokens || !out_count || !out_detection) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_ENCODE, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    TokenizerTelemetry *m = &((mosaic_tokenizer *)tokenizer)->telemetry;
    atomic_fetch_add_explicit(&m->encode_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&m->bytes_in, input_len, memory_order_relaxed);
    mosaic_status status = mosaic_tokenizer_detect_language(tokenizer, input, input_len, out_detection);
    if (status == MOSAIC_OK) {
        const LanguagePack *pack = tokenizer_language_for_detection(tokenizer, out_detection);
        status = encode_tokens_internal(tokenizer->model, input, input_len, pack ? pack->adjustments : NULL,
                                        (size_t)(tokenizer->limits.max_output_tokens > SIZE_MAX ? SIZE_MAX : tokenizer->limits.max_output_tokens),
                                        out_tokens, out_count);
    }
    if (status == MOSAIC_OK && (uint64_t)*out_count > tokenizer->limits.max_output_tokens) {
        mosaic_free(*out_tokens); *out_tokens = NULL; *out_count = 0; status = MOSAIC_ERROR_RESOURCE_LIMIT;
        atomic_fetch_add_explicit(&m->resource_rejections, 1, memory_order_relaxed);
    }
    if (status == MOSAIC_OK) {
        atomic_fetch_add_explicit(&m->tokens_out, *out_count, memory_order_relaxed);
        tokenizer_observe(tokenizer, MOSAIC_EVENT_SUCCESS, MOSAIC_OPERATION_ENCODE, status, input_len, *out_count, 0u);
    } else {
        atomic_fetch_add_explicit(&m->failures, 1, memory_order_relaxed);
        tokenizer_observe(tokenizer, status == MOSAIC_ERROR_RESOURCE_LIMIT ? MOSAIC_EVENT_RESOURCE_REJECTED : MOSAIC_EVENT_FAILURE,
                          MOSAIC_OPERATION_ENCODE, status, input_len, 0u,
                          status == MOSAIC_ERROR_RESOURCE_LIMIT ? tokenizer->limits.max_output_tokens : 0u);
    }
    return status;
}

void mosaic_tokenizer_free(mosaic_tokenizer *tokenizer) {
    if (!tokenizer) return;
    mosaic_model_free(tokenizer->model);
    mosaic_unicode_free(tokenizer->unicode_data);
    language_pack_array_free(tokenizer->languages, tokenizer->language_count);
    mosaic_detector_free(tokenizer->detector);
    mosaic_security_free(tokenizer->security);
    mosaic_normalization_free(tokenizer->normalization);
    mosaic_lexer_free(tokenizer->lexer);
    free(tokenizer->adjustments);
    free(tokenizer);
}

mosaic_status mosaic_tokenizer_set_observer(mosaic_tokenizer *tokenizer, const mosaic_observer_config *config) {
    if (!tokenizer || !config || config->struct_size < sizeof *config || config->flags || config->reserved ||
        (config->event_mask & ~(MOSAIC_OBSERVE_SUCCESS | MOSAIC_OBSERVE_FAILURE | MOSAIC_OBSERVE_RESOURCE)) ||
        (config->event_mask && !config->callback)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    tokenizer->observer = *config;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_get_observer(const mosaic_tokenizer *tokenizer, mosaic_observer_config *out_config) {
    if (!tokenizer || !out_config) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_config = tokenizer->observer;
    return MOSAIC_OK;
}

void mosaic_runtime_limits_default(mosaic_runtime_limits *out_limits) {
    if (!out_limits) return;
    *out_limits = (mosaic_runtime_limits){sizeof *out_limits, 0u,
        1024ull * 1024ull * 1024ull, 1024ull * 1024ull * 1024ull,
        1024ull * 1024ull * 1024ull};
}

void mosaic_runtime_limits_low_memory_default(mosaic_runtime_limits *out_limits) {
    if (!out_limits) return;
    *out_limits = (mosaic_runtime_limits){sizeof *out_limits, 0u,
        64ull * 1024ull * 1024ull, 64ull * 1024ull * 1024ull,
        32ull * 1024ull * 1024ull};
}

mosaic_status mosaic_tokenizer_set_runtime_limits(mosaic_tokenizer *tokenizer, const mosaic_runtime_limits *limits) {
    if (!tokenizer || !limits || limits->struct_size < sizeof *limits || limits->flags ||
        !limits->max_input_bytes || !limits->max_output_tokens || !limits->max_token_document_bytes)
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (atomic_load_explicit(&tokenizer->sealed, memory_order_acquire)) return MOSAIC_ERROR_STATE;
    tokenizer->limits = *limits;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_get_runtime_limits(const mosaic_tokenizer *tokenizer, mosaic_runtime_limits *out_limits) {
    if (!tokenizer || !out_limits) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_limits = tokenizer->limits;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_seal(mosaic_tokenizer *tokenizer) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    atomic_store_explicit(&tokenizer->sealed, 1, memory_order_release);
    return MOSAIC_OK;
}

int mosaic_tokenizer_is_sealed(const mosaic_tokenizer *tokenizer) {
    return tokenizer && atomic_load_explicit(&tokenizer->sealed, memory_order_acquire);
}

mosaic_status mosaic_tokenizer_get_metrics(const mosaic_tokenizer *tokenizer, mosaic_runtime_metrics *out_metrics) {
    if (!tokenizer || !out_metrics) return MOSAIC_ERROR_INVALID_ARGUMENT;
    TokenizerTelemetry *m = (TokenizerTelemetry *)&tokenizer->telemetry;
    *out_metrics = (mosaic_runtime_metrics){
        atomic_load_explicit(&m->encode_calls, memory_order_relaxed),
        atomic_load_explicit(&m->decode_calls, memory_order_relaxed),
        atomic_load_explicit(&m->bytes_in, memory_order_relaxed),
        atomic_load_explicit(&m->bytes_out, memory_order_relaxed),
        atomic_load_explicit(&m->tokens_out, memory_order_relaxed),
        atomic_load_explicit(&m->failures, memory_order_relaxed),
        atomic_load_explicit(&m->resource_rejections, memory_order_relaxed)};
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_reset_metrics(mosaic_tokenizer *tokenizer) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    TokenizerTelemetry *m = &tokenizer->telemetry;
    atomic_store_explicit(&m->encode_calls, 0, memory_order_relaxed);
    atomic_store_explicit(&m->decode_calls, 0, memory_order_relaxed);
    atomic_store_explicit(&m->bytes_in, 0, memory_order_relaxed);
    atomic_store_explicit(&m->bytes_out, 0, memory_order_relaxed);
    atomic_store_explicit(&m->tokens_out, 0, memory_order_relaxed);
    atomic_store_explicit(&m->failures, 0, memory_order_relaxed);
    atomic_store_explicit(&m->resource_rejections, 0, memory_order_relaxed);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_runtime_identity(const mosaic_tokenizer *tokenizer, uint8_t out_sha256[32]) {
    if (!tokenizer || !out_sha256) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint8_t buf[32 + 8 * 3 + 8];
    memcpy(buf, tokenizer->fingerprint, 32u);
    uint8_t *p = buf + 32u;
    wr64(p, tokenizer->limits.max_input_bytes); p += 8u;
    wr64(p, tokenizer->limits.max_output_tokens); p += 8u;
    wr64(p, tokenizer->limits.max_token_document_bytes); p += 8u;
    wr32(p, tokenizer->limits.flags); p += 4u;
    wr32(p, 1u);
    return sha256_bytes(buf, sizeof buf, out_sha256) ? MOSAIC_OK : MOSAIC_ERROR_INTERNAL;
}


static int tokenizer_input_allowed(const mosaic_tokenizer *tokenizer, size_t input_len) {
    if ((uint64_t)input_len <= tokenizer->limits.max_input_bytes) return 1;
    atomic_fetch_add_explicit(&((mosaic_tokenizer *)tokenizer)->telemetry.resource_rejections, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&((mosaic_tokenizer *)tokenizer)->telemetry.failures, 1, memory_order_relaxed);
    return 0;
}

mosaic_status mosaic_tokenizer_fingerprint(const mosaic_tokenizer *tokenizer, uint8_t out_sha256[32]) {
    if (!tokenizer || !out_sha256) return MOSAIC_ERROR_INVALID_ARGUMENT;
    memcpy(out_sha256, tokenizer->fingerprint, 32);
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_get_capabilities(const mosaic_tokenizer *tokenizer, mosaic_tokenizer_capabilities *out_capabilities) {
    if (!tokenizer || !out_capabilities || out_capabilities->struct_size < sizeof(*out_capabilities)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    uint64_t caps = MOSAIC_CAP_MODEL | MOSAIC_CAP_GRAPHEMES | MOSAIC_CAP_STREAMING | MOSAIC_CAP_EDITABLE_DOCUMENT |
                    MOSAIC_CAP_INCREMENTAL_DOCUMENT | MOSAIC_CAP_RESYNC_DOCUMENT | MOSAIC_CAP_TOKEN_DOCUMENT;
    if (tokenizer->model && tokenizer->model->vocab.algorithm == 0u) caps |= MOSAIC_CAP_ONLINE_STREAMING;
    if (tokenizer->language_count) caps |= MOSAIC_CAP_LANGUAGE_PACKS;
    if (tokenizer->detector) caps |= MOSAIC_CAP_DETECTOR | MOSAIC_CAP_SPAN_ROUTING;
    if (tokenizer->security) caps |= MOSAIC_CAP_SECURITY;
    if (tokenizer->normalization) caps |= MOSAIC_CAP_NORMALIZATION;
    if (tokenizer->lexer) caps |= MOSAIC_CAP_LEXER | MOSAIC_CAP_SEMANTIC;
    caps |= MOSAIC_CAP_SUBBYTE | MOSAIC_CAP_BLOCK_PLAN | MOSAIC_CAP_PACKED_MODEL | MOSAIC_CAP_CONTENT_CACHE | MOSAIC_CAP_CACHE_BACKEND | MOSAIC_CAP_RUNTIME_POLICY | MOSAIC_CAP_TOKEN_DOCUMENT_SERIALIZATION | MOSAIC_CAP_PARALLEL_BATCH | MOSAIC_CAP_OBSERVABILITY;
    out_capabilities->available = caps; out_capabilities->reserved = 0u;
    return MOSAIC_OK;
}

mosaic_status mosaic_tokenizer_encode(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                      uint32_t **out_ids, size_t *out_count) {
    if (!tokenizer || !out_ids || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) { tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_ENCODE, MOSAIC_ERROR_RESOURCE_LIMIT, input_len, 0u, tokenizer->limits.max_input_bytes); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    TokenizerTelemetry *m = &((mosaic_tokenizer *)tokenizer)->telemetry;
    atomic_fetch_add_explicit(&m->encode_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&m->bytes_in, input_len, memory_order_relaxed);
    mosaic_status st = encode_internal(tokenizer->model, input, input_len, tokenizer->adjustments, (size_t)(tokenizer->limits.max_output_tokens > SIZE_MAX ? SIZE_MAX : tokenizer->limits.max_output_tokens), out_ids, out_count);
    if (st == MOSAIC_OK && (uint64_t)*out_count > tokenizer->limits.max_output_tokens) { mosaic_free(*out_ids); *out_ids=NULL; *out_count=0; st=MOSAIC_ERROR_RESOURCE_LIMIT; atomic_fetch_add_explicit(&m->resource_rejections,1,memory_order_relaxed); }
    if (st == MOSAIC_OK) {
        atomic_fetch_add_explicit(&m->tokens_out, *out_count, memory_order_relaxed);
        tokenizer_observe(tokenizer, MOSAIC_EVENT_SUCCESS, MOSAIC_OPERATION_ENCODE, st, input_len, *out_count, 0u);
    } else {
        atomic_fetch_add_explicit(&m->failures,1,memory_order_relaxed);
        tokenizer_observe(tokenizer, st == MOSAIC_ERROR_RESOURCE_LIMIT ? MOSAIC_EVENT_RESOURCE_REJECTED : MOSAIC_EVENT_FAILURE,
                          MOSAIC_OPERATION_ENCODE, st, input_len, 0u,
                          st == MOSAIC_ERROR_RESOURCE_LIMIT ? tokenizer->limits.max_output_tokens : 0u);
    }
    return st;
}

mosaic_status mosaic_tokenizer_encode_tokens(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                             mosaic_token **out_tokens, size_t *out_count) {
    if (!tokenizer || !out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) { tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_ENCODE, MOSAIC_ERROR_RESOURCE_LIMIT, input_len, 0u, tokenizer->limits.max_input_bytes); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    TokenizerTelemetry *m=&((mosaic_tokenizer*)tokenizer)->telemetry; atomic_fetch_add_explicit(&m->encode_calls,1,memory_order_relaxed); atomic_fetch_add_explicit(&m->bytes_in,input_len,memory_order_relaxed);
    mosaic_status st=encode_tokens_internal(tokenizer->model,input,input_len,tokenizer->adjustments,(size_t)(tokenizer->limits.max_output_tokens>SIZE_MAX?SIZE_MAX:tokenizer->limits.max_output_tokens),out_tokens,out_count);
    if(st==MOSAIC_OK && (uint64_t)*out_count>tokenizer->limits.max_output_tokens){mosaic_free(*out_tokens);*out_tokens=NULL;*out_count=0;st=MOSAIC_ERROR_RESOURCE_LIMIT;atomic_fetch_add_explicit(&m->resource_rejections,1,memory_order_relaxed);}
    if(st==MOSAIC_OK){atomic_fetch_add_explicit(&m->tokens_out,*out_count,memory_order_relaxed);tokenizer_observe(tokenizer,MOSAIC_EVENT_SUCCESS,MOSAIC_OPERATION_ENCODE,st,input_len,*out_count,0u);}
    else{atomic_fetch_add_explicit(&m->failures,1,memory_order_relaxed);tokenizer_observe(tokenizer,st==MOSAIC_ERROR_RESOURCE_LIMIT?MOSAIC_EVENT_RESOURCE_REJECTED:MOSAIC_EVENT_FAILURE,MOSAIC_OPERATION_ENCODE,st,input_len,0u,st==MOSAIC_ERROR_RESOURCE_LIMIT?tokenizer->limits.max_output_tokens:0u);}
    return st;
}

mosaic_status mosaic_tokenizer_decode(const mosaic_tokenizer *tokenizer, const uint32_t *ids, size_t count,
                                      uint8_t **out_bytes, size_t *out_len) {
    if (!tokenizer || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    TokenizerTelemetry *m=&((mosaic_tokenizer*)tokenizer)->telemetry; atomic_fetch_add_explicit(&m->decode_calls,1,memory_order_relaxed);
    mosaic_status st=decode_internal(tokenizer->model,ids,count,
                                     (size_t)(tokenizer->limits.max_input_bytes>SIZE_MAX?SIZE_MAX:tokenizer->limits.max_input_bytes),
                                     out_bytes,out_len);
    if(st==MOSAIC_OK && (uint64_t)*out_len>tokenizer->limits.max_input_bytes){mosaic_free(*out_bytes);*out_bytes=NULL;*out_len=0;st=MOSAIC_ERROR_RESOURCE_LIMIT;atomic_fetch_add_explicit(&m->resource_rejections,1,memory_order_relaxed);}
    if(st==MOSAIC_OK){atomic_fetch_add_explicit(&m->bytes_out,*out_len,memory_order_relaxed);tokenizer_observe(tokenizer,MOSAIC_EVENT_SUCCESS,MOSAIC_OPERATION_DECODE,st,count,*out_len,0u);}
    else{atomic_fetch_add_explicit(&m->failures,1,memory_order_relaxed);tokenizer_observe(tokenizer,st==MOSAIC_ERROR_RESOURCE_LIMIT?MOSAIC_EVENT_RESOURCE_REJECTED:MOSAIC_EVENT_FAILURE,MOSAIC_OPERATION_DECODE,st,count,0u,st==MOSAIC_ERROR_RESOURCE_LIMIT?tokenizer->limits.max_input_bytes:0u);}
    return st;
}

mosaic_status mosaic_tokenizer_grapheme_ranges(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_range **out_ranges, size_t *out_count) {
    if (!tokenizer) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (!tokenizer_input_allowed(tokenizer, input_len)) {
        tokenizer_observe(tokenizer, MOSAIC_EVENT_RESOURCE_REJECTED, MOSAIC_OPERATION_GRAPHEMES, MOSAIC_ERROR_RESOURCE_LIMIT,
                          input_len, 0u, tokenizer->limits.max_input_bytes);
        return MOSAIC_ERROR_RESOURCE_LIMIT;
    }
    mosaic_status status = mosaic_grapheme_ranges(tokenizer->unicode_data, input, input_len, out_ranges, out_count);
    tokenizer_observe(tokenizer, status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS : MOSAIC_EVENT_FAILURE, MOSAIC_OPERATION_GRAPHEMES,
                      status, input_len, status == MOSAIC_OK && out_count ? *out_count : 0u, 0u);
    return status;
}

static int normalization_mode_valid(mosaic_normalization_mode mode) {
    return mode >= MOSAIC_NORMALIZE_PRESERVE && mode <= MOSAIC_NORMALIZE_NFKC_CASEFOLD;
}

static mosaic_status token_document_create_internal(const mosaic_tokenizer *tokenizer,
                                                    const uint8_t *input, size_t input_len, uint32_t flags,
                                                    mosaic_normalization_mode normalization_mode, int auto_mode,
                                                    mosaic_token_document **out_document) {
    const uint32_t supported = MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES |
                               MOSAIC_TOKEN_DOCUMENT_SECURITY | MOSAIC_TOKEN_DOCUMENT_NORMALIZATION | MOSAIC_TOKEN_DOCUMENT_LEXICAL | MOSAIC_TOKEN_DOCUMENT_SEMANTIC;
    if (!tokenizer || (!input && input_len) || !out_document || (flags & ~supported) || !normalization_mode_valid(normalization_mode))
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    if ((uint64_t)input_len > tokenizer->limits.max_token_document_bytes || (uint64_t)input_len > tokenizer->limits.max_input_bytes) { atomic_fetch_add_explicit(&((mosaic_tokenizer *)tokenizer)->telemetry.resource_rejections,1,memory_order_relaxed); return MOSAIC_ERROR_RESOURCE_LIMIT; }
    if ((flags & MOSAIC_TOKEN_DOCUMENT_SECURITY) && !tokenizer->security) return MOSAIC_ERROR_UNSUPPORTED;
    if ((flags & MOSAIC_TOKEN_DOCUMENT_NORMALIZATION) && !tokenizer->normalization) return MOSAIC_ERROR_UNSUPPORTED;
    if ((flags & (MOSAIC_TOKEN_DOCUMENT_LEXICAL | MOSAIC_TOKEN_DOCUMENT_SEMANTIC)) && !tokenizer->lexer) return MOSAIC_ERROR_UNSUPPORTED;
    *out_document = NULL;
    mosaic_token_document *doc = (mosaic_token_document *)calloc(1, sizeof *doc);
    if (!doc) return MOSAIC_ERROR_OUT_OF_MEMORY;
    doc->flags = flags; doc->source_len = input_len; doc->normalization_mode = normalization_mode;
    if (input_len) {
        doc->source = (uint8_t *)malloc(input_len);
        if (!doc->source) { mosaic_token_document_free(doc); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        memcpy(doc->source, input, input_len);
    }
    if (!sha256_bytes(input, input_len, doc->source_sha256) ||
        mosaic_tokenizer_fingerprint(tokenizer, doc->tokenizer_fingerprint) != MOSAIC_OK) {
        mosaic_token_document_free(doc); return MOSAIC_ERROR_INTERNAL;
    }
    if (flags & MOSAIC_TOKEN_DOCUMENT_MODEL) {
        mosaic_token *tokens = NULL; size_t count = 0; mosaic_status status;
        if (auto_mode) status = mosaic_tokenizer_encode_tokens_auto(tokenizer, input, input_len, &tokens, &count, &doc->detection);
        else status = mosaic_tokenizer_encode_tokens(tokenizer, input, input_len, &tokens, &count);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
        if (count > SIZE_MAX / sizeof *doc->model_tokens) { mosaic_free(tokens); mosaic_token_document_free(doc); return MOSAIC_ERROR_OVERFLOW; }
        doc->model_tokens = count ? (ResyncToken *)malloc(count * sizeof *doc->model_tokens) : NULL;
        if (count && !doc->model_tokens) { mosaic_free(tokens); mosaic_token_document_free(doc); return MOSAIC_ERROR_OUT_OF_MEMORY; }
        size_t cursor = 0;
        for (size_t i = 0; i < count; ++i) {
            if (tokens[i].start != cursor || !tokens[i].length || tokens[i].length > UINT16_MAX || tokens[i].length > input_len - cursor) {
                mosaic_free(tokens); mosaic_token_document_free(doc); return MOSAIC_ERROR_INTERNAL;
            }
            doc->model_tokens[i] = (ResyncToken){tokens[i].id, (uint16_t)tokens[i].length, 0u};
            cursor += (size_t)tokens[i].length;
        }
        mosaic_free(tokens);
        if (cursor != input_len) { mosaic_token_document_free(doc); return MOSAIC_ERROR_INTERNAL; }
        doc->model_token_count = count;
    } else if (auto_mode && tokenizer->detector) {
        mosaic_status status = mosaic_tokenizer_detect_language(tokenizer, input, input_len, &doc->detection);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
    }
    if (flags & MOSAIC_TOKEN_DOCUMENT_GRAPHEMES) {
        mosaic_status status = mosaic_tokenizer_grapheme_ranges(tokenizer, input, input_len, &doc->graphemes, &doc->grapheme_count);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
    }
    if (flags & MOSAIC_TOKEN_DOCUMENT_SECURITY) {
        mosaic_status status = mosaic_tokenizer_security_scan(tokenizer, input, input_len, &doc->security_findings, &doc->security_finding_count);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
    }
    if (flags & MOSAIC_TOKEN_DOCUMENT_NORMALIZATION) {
        mosaic_status status = mosaic_tokenizer_normalize(tokenizer, normalization_mode, input, input_len, &doc->normalized);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
    }
    if (flags & (MOSAIC_TOKEN_DOCUMENT_LEXICAL | MOSAIC_TOKEN_DOCUMENT_SEMANTIC)) {
        mosaic_status status = mosaic_tokenizer_lex(tokenizer, input, input_len, &doc->lexical_tokens, &doc->lexical_token_count);
        if (status != MOSAIC_OK) { mosaic_token_document_free(doc); return status; }
        if ((flags & MOSAIC_TOKEN_DOCUMENT_SEMANTIC) &&
            !semantic_enrich(&tokenizer->lexer->view, input, input_len, doc->lexical_tokens, doc->lexical_token_count,
                             &doc->semantic_components, &doc->semantic_component_count)) {
            mosaic_token_document_free(doc); return MOSAIC_ERROR_OUT_OF_MEMORY;
        }
    }
    *out_document = doc; return MOSAIC_OK;
}

static size_t token_document_projection_count(uint32_t flags) {
    size_t count = 0u;
    for (; flags; flags &= flags - 1u) ++count;
    return count;
}

static void tokenizer_observe_token_document(const mosaic_tokenizer *tokenizer, mosaic_status status,
                                              size_t input_len, uint32_t flags) {
    if (!tokenizer) return;
    const uint32_t kind = status == MOSAIC_OK ? MOSAIC_EVENT_SUCCESS :
                          status == MOSAIC_ERROR_RESOURCE_LIMIT ? MOSAIC_EVENT_RESOURCE_REJECTED : MOSAIC_EVENT_FAILURE;
    uint64_t limit = 0u;
    if (status == MOSAIC_ERROR_RESOURCE_LIMIT) {
        limit = tokenizer->limits.max_input_bytes < tokenizer->limits.max_token_document_bytes ?
                tokenizer->limits.max_input_bytes : tokenizer->limits.max_token_document_bytes;
    }
    tokenizer_observe(tokenizer, kind, MOSAIC_OPERATION_TOKEN_DOCUMENT, status, input_len,
                      status == MOSAIC_OK ? token_document_projection_count(flags) : 0u, limit);
}

mosaic_status mosaic_tokenizer_token_document_create(const mosaic_tokenizer *tokenizer,
                                                       const uint8_t *input, size_t input_len, uint32_t flags,
                                                       mosaic_token_document **out_document) {
    mosaic_status status = token_document_create_internal(tokenizer, input, input_len, flags, MOSAIC_NORMALIZE_PRESERVE, 0, out_document);
    tokenizer_observe_token_document(tokenizer, status, input_len, flags);
    return status;
}

mosaic_status mosaic_tokenizer_token_document_create_auto(const mosaic_tokenizer *tokenizer,
                                                            const uint8_t *input, size_t input_len, uint32_t flags,
                                                            mosaic_token_document **out_document) {
    mosaic_status status = token_document_create_internal(tokenizer, input, input_len, flags, MOSAIC_NORMALIZE_PRESERVE, 1, out_document);
    tokenizer_observe_token_document(tokenizer, status, input_len, flags);
    return status;
}

mosaic_status mosaic_tokenizer_token_document_create_ex(const mosaic_tokenizer *tokenizer,
                                                          const uint8_t *input, size_t input_len,
                                                          const mosaic_token_document_options *options,
                                                          mosaic_token_document **out_document) {
    if (!options || options->struct_size < sizeof(*options) || options->reserved) {
        tokenizer_observe_token_document(tokenizer, MOSAIC_ERROR_INVALID_ARGUMENT, input_len, options ? options->flags : 0u);
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    }
    mosaic_status status = token_document_create_internal(tokenizer, input, input_len, options->flags, options->normalization_mode, 0, out_document);
    tokenizer_observe_token_document(tokenizer, status, input_len, options->flags);
    return status;
}

mosaic_status mosaic_tokenizer_token_document_create_auto_ex(const mosaic_tokenizer *tokenizer,
                                                               const uint8_t *input, size_t input_len,
                                                               const mosaic_token_document_options *options,
                                                               mosaic_token_document **out_document) {
    if (!options || options->struct_size < sizeof(*options) || options->reserved) {
        tokenizer_observe_token_document(tokenizer, MOSAIC_ERROR_INVALID_ARGUMENT, input_len, options ? options->flags : 0u);
        return MOSAIC_ERROR_INVALID_ARGUMENT;
    }
    mosaic_status status = token_document_create_internal(tokenizer, input, input_len, options->flags, options->normalization_mode, 1, out_document);
    tokenizer_observe_token_document(tokenizer, status, input_len, options->flags);
    return status;
}

mosaic_status mosaic_token_document_get_info(const mosaic_token_document *document, mosaic_token_document_info *out_info) {
    if (!document || !out_info) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_info = (mosaic_token_document_info){0};
    out_info->flags = document->flags; out_info->source_length = (uint64_t)document->source_len;
    out_info->model_token_count = (uint64_t)document->model_token_count; out_info->grapheme_count = (uint64_t)document->grapheme_count;
    out_info->security_finding_count = (uint64_t)document->security_finding_count;
    out_info->normalized_byte_length = (uint64_t)document->normalized.byte_length;
    out_info->normalized_unit_count = (uint64_t)document->normalized.unit_count;
    out_info->lexical_token_count = (document->flags & MOSAIC_TOKEN_DOCUMENT_LEXICAL) ? (uint64_t)document->lexical_token_count : 0u;
    out_info->semantic_component_count = (uint64_t)document->semantic_component_count;
    out_info->normalization_mode = document->normalization_mode;
    memcpy(out_info->source_sha256, document->source_sha256, 32);
    memcpy(out_info->tokenizer_fingerprint_sha256, document->tokenizer_fingerprint, 32);
    out_info->detection = document->detection; return MOSAIC_OK;
}

#define MOSAIC_TOKEN_IR_MAGIC "MSTIRD01"
#define MOSAIC_TOKEN_IR_VERSION 1u
#define MOSAIC_TOKEN_IR_HEADER 256u
#define MOSAIC_TOKEN_IR_SECTIONS 9u
#define MOSAIC_TOKEN_IR_DIR_ENTRY 32u
#define MOSAIC_TOKEN_IR_DATA_START (MOSAIC_TOKEN_IR_HEADER + MOSAIC_TOKEN_IR_SECTIONS * MOSAIC_TOKEN_IR_DIR_ENTRY)
#define MOSAIC_TOKEN_IR_HASH_OFFSET 112u

typedef struct {
    uint32_t kind;
    uint64_t offset;
    uint64_t length;
    uint64_t count;
} TokenIRSection;

static int token_ir_align8(size_t value, size_t *out) {
    if (value > SIZE_MAX - 7u) return 0;
    *out = (value + 7u) & ~(size_t)7u;
    return 1;
}

static int token_ir_add_section(TokenIRSection *section, uint32_t kind, size_t length, size_t count, size_t *cursor) {
    section->kind = kind; section->count = (uint64_t)count; section->length = (uint64_t)length; section->offset = 0u;
    if (!length) { if (count) return 0; return 1; }
    size_t aligned;
    if (!token_ir_align8(*cursor, &aligned) || length > SIZE_MAX - aligned) return 0;
    section->offset = (uint64_t)aligned; *cursor = aligned + length; return 1;
}

static void token_ir_write_section(uint8_t *out, size_t index, const TokenIRSection *section) {
    uint8_t *p = out + MOSAIC_TOKEN_IR_HEADER + index * MOSAIC_TOKEN_IR_DIR_ENTRY;
    wr32(p, section->kind); wr32(p + 4u, 0u); wr64(p + 8u, section->offset);
    wr64(p + 16u, section->length); wr64(p + 24u, section->count);
}

static int token_ir_hash(const uint8_t *bytes, size_t len, uint8_t out[32]) {
    return mosaic_internal_sha256_zero_range(bytes, len, MOSAIC_TOKEN_IR_HASH_OFFSET, 32u, out);
}

mosaic_status mosaic_token_document_serialize(const mosaic_token_document *document, uint8_t **out_bytes, size_t *out_len) {
    if (!document || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL; *out_len = 0;
    const uint32_t supported = MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES |
        MOSAIC_TOKEN_DOCUMENT_SECURITY | MOSAIC_TOKEN_DOCUMENT_NORMALIZATION | MOSAIC_TOKEN_DOCUMENT_LEXICAL |
        MOSAIC_TOKEN_DOCUMENT_SEMANTIC;
    if (document->flags & ~supported) return MOSAIC_ERROR_INTERNAL;
    if (document->source_len > UINT64_MAX) return MOSAIC_ERROR_OVERFLOW;

    TokenIRSection sec[MOSAIC_TOKEN_IR_SECTIONS] = {0};
    size_t cursor = MOSAIC_TOKEN_IR_DATA_START;
    size_t model_bytes=0,grapheme_bytes=0,security_bytes=0,norm_unit_bytes=0,norm_span_bytes=0,lex_bytes=0,semantic_bytes=0;
    if (document->model_token_count > SIZE_MAX / 24u || document->grapheme_count > SIZE_MAX / 16u ||
        document->security_finding_count > SIZE_MAX / 24u || document->normalized.unit_count > SIZE_MAX / 24u ||
        document->normalized.source_span_count > SIZE_MAX / 16u || document->lexical_token_count > SIZE_MAX / 24u ||
        document->semantic_component_count > SIZE_MAX / 32u) return MOSAIC_ERROR_OVERFLOW;
    model_bytes=document->model_token_count*24u;grapheme_bytes=document->grapheme_count*16u;
    security_bytes=document->security_finding_count*24u;norm_unit_bytes=document->normalized.unit_count*24u;
    norm_span_bytes=document->normalized.source_span_count*16u;lex_bytes=document->lexical_token_count*24u;
    semantic_bytes=document->semantic_component_count*32u;
    if (!token_ir_add_section(&sec[0],1u,document->source_len,document->source_len,&cursor) ||
        !token_ir_add_section(&sec[1],2u,(document->flags&MOSAIC_TOKEN_DOCUMENT_MODEL)?model_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_MODEL)?document->model_token_count:0u,&cursor) ||
        !token_ir_add_section(&sec[2],3u,(document->flags&MOSAIC_TOKEN_DOCUMENT_GRAPHEMES)?grapheme_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_GRAPHEMES)?document->grapheme_count:0u,&cursor) ||
        !token_ir_add_section(&sec[3],4u,(document->flags&MOSAIC_TOKEN_DOCUMENT_SECURITY)?security_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_SECURITY)?document->security_finding_count:0u,&cursor) ||
        !token_ir_add_section(&sec[4],5u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?document->normalized.byte_length:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?document->normalized.byte_length:0u,&cursor) ||
        !token_ir_add_section(&sec[5],6u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?norm_unit_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?document->normalized.unit_count:0u,&cursor) ||
        !token_ir_add_section(&sec[6],7u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?norm_span_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)?document->normalized.source_span_count:0u,&cursor) ||
        !token_ir_add_section(&sec[7],8u,(document->flags&(MOSAIC_TOKEN_DOCUMENT_LEXICAL|MOSAIC_TOKEN_DOCUMENT_SEMANTIC))?lex_bytes:0u,(document->flags&(MOSAIC_TOKEN_DOCUMENT_LEXICAL|MOSAIC_TOKEN_DOCUMENT_SEMANTIC))?document->lexical_token_count:0u,&cursor) ||
        !token_ir_add_section(&sec[8],9u,(document->flags&MOSAIC_TOKEN_DOCUMENT_SEMANTIC)?semantic_bytes:0u,(document->flags&MOSAIC_TOKEN_DOCUMENT_SEMANTIC)?document->semantic_component_count:0u,&cursor)) return MOSAIC_ERROR_OVERFLOW;

    uint8_t *out=(uint8_t*)calloc(cursor?cursor:1u,1u); if(!out)return MOSAIC_ERROR_OUT_OF_MEMORY;
    memcpy(out,MOSAIC_TOKEN_IR_MAGIC,8u);wr32(out+8u,MOSAIC_TOKEN_IR_VERSION);wr32(out+12u,MOSAIC_TOKEN_IR_HEADER);
    wr32(out+16u,document->flags);wr32(out+20u,(uint32_t)document->normalization_mode);wr64(out+24u,(uint64_t)cursor);
    wr64(out+32u,(uint64_t)document->source_len);wr32(out+40u,mosaic_tokenizer_semantics_version());wr32(out+44u,MOSAIC_TOKEN_IR_SECTIONS);
    memcpy(out+48u,document->source_sha256,32u);memcpy(out+80u,document->tokenizer_fingerprint,32u);
    wr32(out+144u,document->detection.matched);wr32(out+148u,document->detection.available);wr64(out+152u,(uint64_t)document->detection.score);
    wr64(out+160u,(uint64_t)document->detection.margin);memcpy(out+168u,document->detection.language,64u);
    for(size_t i=0;i<MOSAIC_TOKEN_IR_SECTIONS;++i)token_ir_write_section(out,i,&sec[i]);
    if(sec[0].length)memcpy(out+(size_t)sec[0].offset,document->source,document->source_len);
    if(sec[1].length){uint8_t*p=out+(size_t)sec[1].offset;size_t start=0;for(size_t i=0;i<document->model_token_count;++i){ResyncToken t=document->model_tokens[i];wr32(p, t.id);wr32(p+4u,0u);wr64(p+8u,start);wr64(p+16u,t.length);start+=t.length;p+=24u;}}
    if(sec[2].length){uint8_t*p=out+(size_t)sec[2].offset;for(size_t i=0;i<document->grapheme_count;++i){wr64(p,document->graphemes[i].start);wr64(p+8u,document->graphemes[i].length);p+=16u;}}
    if(sec[3].length){uint8_t*p=out+(size_t)sec[3].offset;for(size_t i=0;i<document->security_finding_count;++i){mosaic_security_finding f=document->security_findings[i];wr32(p,f.kind);wr16(p+4u,f.script_id);wr16(p+6u,0u);wr64(p+8u,f.start);wr64(p+16u,f.length);p+=24u;}}
    if(sec[4].length)memcpy(out+(size_t)sec[4].offset,document->normalized.bytes,document->normalized.byte_length);
    if(sec[5].length){uint8_t*p=out+(size_t)sec[5].offset;for(size_t i=0;i<document->normalized.unit_count;++i){mosaic_normalized_unit u=document->normalized.units[i];wr64(p,u.output_start);wr64(p+8u,u.output_length);wr32(p+16u,u.source_span_index);wr32(p+20u,u.source_span_count);p+=24u;}}
    if(sec[6].length){uint8_t*p=out+(size_t)sec[6].offset;for(size_t i=0;i<document->normalized.source_span_count;++i){wr64(p,document->normalized.source_spans[i].start);wr64(p+8u,document->normalized.source_spans[i].length);p+=16u;}}
    if(sec[7].length){uint8_t*p=out+(size_t)sec[7].offset;for(size_t i=0;i<document->lexical_token_count;++i){mosaic_lex_token t=document->lexical_tokens[i];wr32(p,t.kind);wr32(p+4u,t.flags);wr64(p+8u,t.start);wr64(p+16u,t.length);p+=24u;}}
    if(sec[8].length){uint8_t*p=out+(size_t)sec[8].offset;for(size_t i=0;i<document->semantic_component_count;++i){mosaic_semantic_component c=document->semantic_components[i];wr32(p,c.kind);wr32(p+4u,c.flags);wr64(p+8u,c.lexical_index);wr64(p+16u,c.start);wr64(p+24u,c.length);p+=32u;}}
    uint8_t digest[32];if(!token_ir_hash(out,cursor,digest)){free(out);return MOSAIC_ERROR_INTERNAL;}memcpy(out+MOSAIC_TOKEN_IR_HASH_OFFSET,digest,32u);
    *out_bytes=out;*out_len=cursor;return MOSAIC_OK;
}

static int token_ir_range_ok(uint64_t start,uint64_t length,size_t total){return start<=total&&length<=total-(size_t)start;}

void mosaic_token_ir_limits_default(mosaic_token_ir_limits *out_limits) {
    if (!out_limits) return;
    *out_limits = (mosaic_token_ir_limits){sizeof *out_limits, 0u, 1024ull * 1024ull * 1024ull,
                                           1024ull * 1024ull * 1024ull, 1000000000ull};
}

static mosaic_status token_document_deserialize_internal(const uint8_t *bytes, size_t len,
                                                         const mosaic_token_ir_limits *limits,
                                                         mosaic_token_document **out_document) {
    if ((!bytes && len) || !out_document) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_document = NULL;
    if (!limits || limits->struct_size < sizeof *limits || limits->flags || !limits->max_record_bytes ||
        !limits->max_source_bytes || !limits->max_projection_items) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if ((uint64_t)len > limits->max_record_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    if(len<MOSAIC_TOKEN_IR_DATA_START||memcmp(bytes,MOSAIC_TOKEN_IR_MAGIC,8u)||rd32(bytes+8u)!=MOSAIC_TOKEN_IR_VERSION||
       rd32(bytes+12u)!=MOSAIC_TOKEN_IR_HEADER||rd64(bytes+24u)!=len||rd32(bytes+40u)!=mosaic_tokenizer_semantics_version()||rd32(bytes+44u)!=MOSAIC_TOKEN_IR_SECTIONS||
       !all_zero(bytes+232u,24u)) return MOSAIC_ERROR_INTEGRITY;
    uint32_t flags=rd32(bytes+16u), supported=MOSAIC_TOKEN_DOCUMENT_MODEL|MOSAIC_TOKEN_DOCUMENT_GRAPHEMES|MOSAIC_TOKEN_DOCUMENT_SECURITY|MOSAIC_TOKEN_DOCUMENT_NORMALIZATION|MOSAIC_TOKEN_DOCUMENT_LEXICAL|MOSAIC_TOKEN_DOCUMENT_SEMANTIC;
    mosaic_normalization_mode mode=(mosaic_normalization_mode)rd32(bytes+20u);if(flags&~supported||!normalization_mode_valid(mode))return MOSAIC_ERROR_INTEGRITY;
    uint8_t digest[32];
    if (!token_ir_hash(bytes, len, digest) || memcmp(digest, bytes + MOSAIC_TOKEN_IR_HASH_OFFSET, 32u) != 0) return MOSAIC_ERROR_INTEGRITY;
    uint64_t source_len64=rd64(bytes+32u);
    if (source_len64 > limits->max_source_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
    if (source_len64 > SIZE_MAX) return MOSAIC_ERROR_OVERFLOW;
    size_t source_len = (size_t)source_len64;
    if(!memchr(bytes+168u,0,64u))return MOSAIC_ERROR_INTEGRITY;
    if (rd32(bytes + 144u) > 1u || rd32(bytes + 148u) > 1u ||
        (rd32(bytes + 148u) && !rd32(bytes + 144u))) return MOSAIC_ERROR_INTEGRITY;
    TokenIRSection sec[MOSAIC_TOKEN_IR_SECTIONS];size_t expected=MOSAIC_TOKEN_IR_DATA_START;
    static const size_t widths[MOSAIC_TOKEN_IR_SECTIONS]={1u,24u,16u,24u,1u,24u,16u,24u,32u};
    for(size_t i=0;i<MOSAIC_TOKEN_IR_SECTIONS;++i){const uint8_t*p=bytes+MOSAIC_TOKEN_IR_HEADER+i*MOSAIC_TOKEN_IR_DIR_ENTRY;sec[i].kind=rd32(p);uint32_t sf=rd32(p+4u);sec[i].offset=rd64(p+8u);sec[i].length=rd64(p+16u);sec[i].count=rd64(p+24u);if(sec[i].kind!=i+1u||sf)return MOSAIC_ERROR_INTEGRITY;if(!sec[i].length){if(sec[i].offset||sec[i].count)return MOSAIC_ERROR_INTEGRITY;continue;}if(sec[i].count>SIZE_MAX||sec[i].length>SIZE_MAX)return MOSAIC_ERROR_OVERFLOW;
        if (i != 0u && i != 4u && sec[i].count > limits->max_projection_items) return MOSAIC_ERROR_RESOURCE_LIMIT;
        size_t a;if(!token_ir_align8(expected,&a)||sec[i].offset!=a||!token_ir_range_ok(sec[i].offset,sec[i].length,len))return MOSAIC_ERROR_INTEGRITY;for(size_t z=expected;z<a;++z)if(bytes[z])return MOSAIC_ERROR_INTEGRITY;expected=a+(size_t)sec[i].length;if(widths[i]>1u){if(sec[i].count>UINT64_MAX/widths[i]||sec[i].length!=sec[i].count*widths[i])return MOSAIC_ERROR_INTEGRITY;}else if(sec[i].length!=sec[i].count)return MOSAIC_ERROR_INTEGRITY;}
    if(expected!=len||sec[0].length!=source_len64)return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&MOSAIC_TOKEN_DOCUMENT_MODEL)&&sec[1].length)return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&MOSAIC_TOKEN_DOCUMENT_GRAPHEMES)&&sec[2].length)return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&MOSAIC_TOKEN_DOCUMENT_SECURITY)&&sec[3].length)return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)&&(sec[4].length||sec[5].length||sec[6].length))return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&(MOSAIC_TOKEN_DOCUMENT_LEXICAL|MOSAIC_TOKEN_DOCUMENT_SEMANTIC))&&sec[7].length)return MOSAIC_ERROR_INTEGRITY;
    if(!(flags&MOSAIC_TOKEN_DOCUMENT_SEMANTIC)&&sec[8].length)return MOSAIC_ERROR_INTEGRITY;
    mosaic_token_document *doc=(mosaic_token_document*)calloc(1,sizeof*doc);if(!doc)return MOSAIC_ERROR_OUT_OF_MEMORY;doc->flags=flags;doc->normalization_mode=mode;doc->source_len=source_len;
    memcpy(doc->source_sha256,bytes+48u,32u);memcpy(doc->tokenizer_fingerprint,bytes+80u,32u);doc->detection.matched=rd32(bytes+144u);doc->detection.available=rd32(bytes+148u);doc->detection.score=(int64_t)rd64(bytes+152u);doc->detection.margin=(int64_t)rd64(bytes+160u);memcpy(doc->detection.language,bytes+168u,64u);
    if(source_len){doc->source=(uint8_t*)malloc(source_len);if(!doc->source)goto oom;memcpy(doc->source,bytes+(size_t)sec[0].offset,source_len);}uint8_t source_hash[32];if(!sha256_bytes(doc->source,source_len,source_hash)||memcmp(source_hash,doc->source_sha256,32u))goto integrity;
    if(sec[1].count){if(sec[1].count>SIZE_MAX/sizeof*doc->model_tokens)goto overflow;doc->model_token_count=(size_t)sec[1].count;doc->model_tokens=(ResyncToken*)malloc(doc->model_token_count*sizeof*doc->model_tokens);if(!doc->model_tokens)goto oom;const uint8_t*p=bytes+(size_t)sec[1].offset;size_t cursor=0;for(size_t i=0;i<doc->model_token_count;++i,p+=24u){uint32_t id=rd32(p);if(rd32(p+4u))goto integrity;uint64_t st=rd64(p+8u),ln=rd64(p+16u);if(st!=cursor||!ln||ln>UINT16_MAX||ln>source_len-cursor)goto integrity;doc->model_tokens[i]=(ResyncToken){id,(uint16_t)ln,0u};cursor+=(size_t)ln;}if(cursor!=source_len)goto integrity;}
    if(sec[2].count){if(sec[2].count>SIZE_MAX/sizeof*doc->graphemes)goto overflow;doc->grapheme_count=(size_t)sec[2].count;doc->graphemes=(mosaic_range*)malloc(doc->grapheme_count*sizeof*doc->graphemes);if(!doc->graphemes)goto oom;const uint8_t*p=bytes+(size_t)sec[2].offset;size_t cursor=0;for(size_t i=0;i<doc->grapheme_count;++i,p+=16u){uint64_t st=rd64(p),ln=rd64(p+8u);if(st!=cursor||!ln||!token_ir_range_ok(st,ln,source_len))goto integrity;doc->graphemes[i]=(mosaic_range){st,ln};cursor+=(size_t)ln;}if(cursor!=source_len)goto integrity;}
    if(sec[3].count){if(sec[3].count>SIZE_MAX/sizeof*doc->security_findings)goto overflow;doc->security_finding_count=(size_t)sec[3].count;doc->security_findings=(mosaic_security_finding*)malloc(doc->security_finding_count*sizeof*doc->security_findings);if(!doc->security_findings)goto oom;const uint8_t*p=bytes+(size_t)sec[3].offset;for(size_t i=0;i<doc->security_finding_count;++i,p+=24u){uint32_t kind=rd32(p);uint16_t script=rd16(p+4u);if(rd16(p+6u) || kind < MOSAIC_SECURITY_BIDI_CONTROL || kind > MOSAIC_SECURITY_MIXED_SCRIPT)goto integrity;uint64_t st=rd64(p+8u),ln=rd64(p+16u);if(!ln||!token_ir_range_ok(st,ln,source_len))goto integrity;doc->security_findings[i]=(mosaic_security_finding){kind,script,0u,st,ln};}}
    if(flags&MOSAIC_TOKEN_DOCUMENT_NORMALIZATION){doc->normalized.byte_length=(size_t)sec[4].count;if(doc->normalized.byte_length){doc->normalized.bytes=(uint8_t*)malloc(doc->normalized.byte_length);if(!doc->normalized.bytes)goto oom;memcpy(doc->normalized.bytes,bytes+(size_t)sec[4].offset,doc->normalized.byte_length);}if(sec[6].count){if(sec[6].count>SIZE_MAX/sizeof*doc->normalized.source_spans)goto overflow;doc->normalized.source_span_count=(size_t)sec[6].count;doc->normalized.source_spans=(mosaic_range*)malloc(doc->normalized.source_span_count*sizeof*doc->normalized.source_spans);if(!doc->normalized.source_spans)goto oom;const uint8_t*p=bytes+(size_t)sec[6].offset;for(size_t i=0;i<doc->normalized.source_span_count;++i,p+=16u){uint64_t st=rd64(p),ln=rd64(p+8u);if(!token_ir_range_ok(st,ln,source_len))goto integrity;doc->normalized.source_spans[i]=(mosaic_range){st,ln};}}if(sec[5].count){if(sec[5].count>SIZE_MAX/sizeof*doc->normalized.units)goto overflow;doc->normalized.unit_count=(size_t)sec[5].count;doc->normalized.units=(mosaic_normalized_unit*)malloc(doc->normalized.unit_count*sizeof*doc->normalized.units);if(!doc->normalized.units)goto oom;const uint8_t*p=bytes+(size_t)sec[5].offset;for(size_t i=0;i<doc->normalized.unit_count;++i,p+=24u){uint64_t os=rd64(p),ol=rd64(p+8u);uint32_t si=rd32(p+16u),sc=rd32(p+20u);if(!token_ir_range_ok(os,ol,doc->normalized.byte_length)||si>doc->normalized.source_span_count||sc>doc->normalized.source_span_count-si)goto integrity;doc->normalized.units[i]=(mosaic_normalized_unit){os,ol,si,sc};}}}
    if(sec[7].count){if(sec[7].count>SIZE_MAX/sizeof*doc->lexical_tokens)goto overflow;doc->lexical_token_count=(size_t)sec[7].count;doc->lexical_tokens=(mosaic_lex_token*)malloc(doc->lexical_token_count*sizeof*doc->lexical_tokens);if(!doc->lexical_tokens)goto oom;const uint8_t*p=bytes+(size_t)sec[7].offset;size_t cursor=0;for(size_t i=0;i<doc->lexical_token_count;++i,p+=24u){uint32_t kind=rd32(p),lf=rd32(p+4u);uint64_t st=rd64(p+8u),ln=rd64(p+16u);if(kind < MOSAIC_LEX_WHITESPACE || kind > MOSAIC_LEX_ERROR || st!=cursor||!ln||!token_ir_range_ok(st,ln,source_len))goto integrity;doc->lexical_tokens[i]=(mosaic_lex_token){kind,lf,st,ln};cursor+=(size_t)ln;}if(cursor!=source_len)goto integrity;}
    if(sec[8].count){if(sec[8].count>SIZE_MAX/sizeof*doc->semantic_components)goto overflow;doc->semantic_component_count=(size_t)sec[8].count;doc->semantic_components=(mosaic_semantic_component*)malloc(doc->semantic_component_count*sizeof*doc->semantic_components);if(!doc->semantic_components)goto oom;const uint8_t*p=bytes+(size_t)sec[8].offset;for(size_t i=0;i<doc->semantic_component_count;++i,p+=32u){uint32_t kind=rd32(p),sf=rd32(p+4u);uint64_t li=rd64(p+8u),st=rd64(p+16u),ln=rd64(p+24u);if(kind < MOSAIC_SEM_IDENTIFIER_PART || kind > MOSAIC_SEM_STRING_CONTENT || li>=doc->lexical_token_count||!ln||!token_ir_range_ok(st,ln,source_len))goto integrity;mosaic_lex_token lt=doc->lexical_tokens[(size_t)li];if(st<lt.start || ln>lt.length || st-lt.start>lt.length-ln)goto integrity;doc->semantic_components[i]=(mosaic_semantic_component){kind,sf,li,st,ln};}}
    *out_document=doc;return MOSAIC_OK;
oom:mosaic_token_document_free(doc);return MOSAIC_ERROR_OUT_OF_MEMORY;
overflow:mosaic_token_document_free(doc);return MOSAIC_ERROR_OVERFLOW;
integrity:mosaic_token_document_free(doc);return MOSAIC_ERROR_INTEGRITY;
}

mosaic_status mosaic_token_document_deserialize_with_limits(const uint8_t *bytes, size_t len,
                                                             const mosaic_token_ir_limits *limits,
                                                             mosaic_token_document **out_document) {
    return token_document_deserialize_internal(bytes, len, limits, out_document);
}

mosaic_status mosaic_token_document_deserialize(const uint8_t *bytes, size_t len, mosaic_token_document **out_document) {
    mosaic_token_ir_limits limits;
    mosaic_token_ir_limits_default(&limits);
    return token_document_deserialize_internal(bytes, len, &limits, out_document);
}

mosaic_status mosaic_token_document_copy_source(const mosaic_token_document *document, uint8_t **out_bytes, size_t *out_len) {
    if (!document || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL; *out_len = 0;
    mosaic_status status = copy_bytes(document->source, document->source_len, out_bytes);
    if (status == MOSAIC_OK) *out_len = document->source_len;
    return status;
}

mosaic_status mosaic_token_document_model_tokens(const mosaic_token_document *document,
                                                  mosaic_document_token **out_tokens, size_t *out_count) {
    if (!document || !out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokens = NULL; *out_count = 0;
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_MODEL)) return MOSAIC_ERROR_UNSUPPORTED;
    if (document->model_token_count > SIZE_MAX / sizeof(mosaic_document_token)) return MOSAIC_ERROR_OVERFLOW;
    mosaic_document_token *out = document->model_token_count ? (mosaic_document_token *)malloc(document->model_token_count * sizeof *out) : NULL;
    if (document->model_token_count && !out) return MOSAIC_ERROR_OUT_OF_MEMORY;
    size_t cursor = 0;
    for (size_t i = 0; i < document->model_token_count; ++i) {
        ResyncToken token = document->model_tokens[i];
        out[i] = (mosaic_document_token){token.id, (uint64_t)cursor, (uint64_t)token.length}; cursor += token.length;
    }
    *out_tokens = out; *out_count = document->model_token_count; return MOSAIC_OK;
}

mosaic_status mosaic_token_document_graphemes(const mosaic_token_document *document,
                                               mosaic_range **out_ranges, size_t *out_count) {
    if (!document || !out_ranges || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_ranges = NULL; *out_count = 0;
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_GRAPHEMES)) return MOSAIC_ERROR_UNSUPPORTED;
    if (document->grapheme_count > SIZE_MAX / sizeof(mosaic_range)) return MOSAIC_ERROR_OVERFLOW;
    mosaic_range *out = document->grapheme_count ? (mosaic_range *)malloc(document->grapheme_count * sizeof *out) : NULL;
    if (document->grapheme_count && !out) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (document->grapheme_count) memcpy(out, document->graphemes, document->grapheme_count * sizeof *out);
    *out_ranges = out; *out_count = document->grapheme_count; return MOSAIC_OK;
}

mosaic_status mosaic_token_document_security_findings(const mosaic_token_document *document,
                                                       mosaic_security_finding **out_findings, size_t *out_count) {
    if (!document || !out_findings || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_findings = NULL; *out_count = 0;
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_SECURITY)) return MOSAIC_ERROR_UNSUPPORTED;
    if (document->security_finding_count > SIZE_MAX / sizeof **out_findings) return MOSAIC_ERROR_OVERFLOW;
    mosaic_security_finding *copy = document->security_finding_count ?
        (mosaic_security_finding *)malloc(document->security_finding_count * sizeof *copy) : NULL;
    if (document->security_finding_count && !copy) return MOSAIC_ERROR_OUT_OF_MEMORY;
    if (document->security_finding_count) memcpy(copy, document->security_findings, document->security_finding_count * sizeof *copy);
    *out_findings = copy; *out_count = document->security_finding_count; return MOSAIC_OK;
}

mosaic_status mosaic_token_document_normalized_view(const mosaic_token_document *document, mosaic_normalized_view *out_view) {
    if (!document || !out_view) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_view = (mosaic_normalized_view){0};
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_NORMALIZATION)) return MOSAIC_ERROR_UNSUPPORTED;
    const mosaic_normalized_view *src = &document->normalized;
    if (src->byte_length) { out_view->bytes = (uint8_t *)malloc(src->byte_length); if (!out_view->bytes) goto oom; memcpy(out_view->bytes, src->bytes, src->byte_length); }
    if (src->unit_count) {
        if (src->unit_count > SIZE_MAX / sizeof *out_view->units) goto overflow;
        out_view->units = (mosaic_normalized_unit *)malloc(src->unit_count * sizeof *out_view->units); if (!out_view->units) goto oom;
        memcpy(out_view->units, src->units, src->unit_count * sizeof *out_view->units);
    }
    if (src->source_span_count) {
        if (src->source_span_count > SIZE_MAX / sizeof *out_view->source_spans) goto overflow;
        out_view->source_spans = (mosaic_range *)malloc(src->source_span_count * sizeof *out_view->source_spans); if (!out_view->source_spans) goto oom;
        memcpy(out_view->source_spans, src->source_spans, src->source_span_count * sizeof *out_view->source_spans);
    }
    out_view->byte_length = src->byte_length; out_view->unit_count = src->unit_count; out_view->source_span_count = src->source_span_count;
    return MOSAIC_OK;
overflow: mosaic_normalized_view_free(out_view); return MOSAIC_ERROR_OVERFLOW;
oom: mosaic_normalized_view_free(out_view); return MOSAIC_ERROR_OUT_OF_MEMORY;
}

mosaic_status mosaic_token_document_lexical_tokens(const mosaic_token_document *document,mosaic_lex_token **out_tokens,size_t *out_count){if(!document||!out_tokens||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_tokens=NULL;*out_count=0;if(!(document->flags&MOSAIC_TOKEN_DOCUMENT_LEXICAL))return MOSAIC_ERROR_UNSUPPORTED;if(document->lexical_token_count>SIZE_MAX/sizeof**out_tokens)return MOSAIC_ERROR_OVERFLOW;mosaic_lex_token*copy=document->lexical_token_count?(mosaic_lex_token*)malloc(document->lexical_token_count*sizeof*copy):NULL;if(document->lexical_token_count&&!copy)return MOSAIC_ERROR_OUT_OF_MEMORY;if(document->lexical_token_count)memcpy(copy,document->lexical_tokens,document->lexical_token_count*sizeof*copy);*out_tokens=copy;*out_count=document->lexical_token_count;return MOSAIC_OK;}

mosaic_status mosaic_token_document_semantic_components(const mosaic_token_document *document,mosaic_semantic_component **out_components,size_t *out_count){
    if (!document || !out_components || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_components = NULL; *out_count = 0;
    if(!(document->flags&MOSAIC_TOKEN_DOCUMENT_SEMANTIC))return MOSAIC_ERROR_UNSUPPORTED;
    if(document->semantic_component_count>SIZE_MAX/sizeof**out_components)return MOSAIC_ERROR_OVERFLOW;
    mosaic_semantic_component*copy=document->semantic_component_count?(mosaic_semantic_component*)malloc(document->semantic_component_count*sizeof*copy):NULL;
    if(document->semantic_component_count&&!copy)return MOSAIC_ERROR_OUT_OF_MEMORY;
    if(document->semantic_component_count)memcpy(copy,document->semantic_components,document->semantic_component_count*sizeof*copy);
    *out_components=copy;*out_count=document->semantic_component_count;return MOSAIC_OK;
}
mosaic_status mosaic_subbyte_extract_u64(const uint8_t *source,size_t source_len,mosaic_subbyte_span span,uint64_t *out_value){
    if(!source||!out_value||span.reserved||span.start_bit>7u||!span.bit_length||span.bit_length>64u||(span.bit_order!=MOSAIC_BIT_MSB0&&span.bit_order!=MOSAIC_BIT_LSB0))return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (span.byte_start > SIZE_MAX) return MOSAIC_ERROR_OVERFLOW;
    size_t byte_start = (size_t)span.byte_start;
    uint64_t last=(uint64_t)span.start_bit+(uint64_t)span.bit_length-1u;uint64_t bytes_needed=last/8u+1u;
    if(byte_start>source_len||bytes_needed>source_len-byte_start)return MOSAIC_ERROR_OVERFLOW;
    uint64_t value = 0;
    for (uint32_t i = 0; i < span.bit_length; ++i) {
        uint64_t logical = (uint64_t)span.start_bit + i;
        size_t bi = byte_start + (size_t)(logical / 8u);
        uint32_t within = (uint32_t)(logical % 8u);
        if (span.bit_order == MOSAIC_BIT_MSB0) value = (value << 1u) | ((source[bi] >> (7u - within)) & 1u);
        else value |= (uint64_t)((source[bi] >> within) & 1u) << i;
    }
    *out_value = value; return MOSAIC_OK;
}


#define MOSAIC_PACKED_MODEL_HEADER 160u
static const uint8_t MOSAIC_PACKED_MODEL_MAGIC[8] = {'M','S','T','K','P','K','0','1'};

static int append_processing_block(mosaic_block_plan *plan, mosaic_processing_block block, size_t limit) {
    if (plan->block_count >= limit) return 0;
    size_t next = plan->block_count + 1u;
    if (next > SIZE_MAX / sizeof *plan->blocks) return 0;
    mosaic_processing_block *grown = (mosaic_processing_block *)realloc(plan->blocks, next * sizeof *grown);
    if (!grown) return 0;
    plan->blocks = grown; plan->blocks[plan->block_count++] = block; return 1;
}
static int append_macroblock(mosaic_block_plan *plan, mosaic_macroblock block, size_t limit) {
    if (plan->macroblock_count >= limit) return 0;
    size_t next = plan->macroblock_count + 1u;
    if (next > SIZE_MAX / sizeof *plan->macroblocks) return 0;
    mosaic_macroblock *grown = (mosaic_macroblock *)realloc(plan->macroblocks, next * sizeof *grown);
    if (!grown) return 0;
    plan->macroblocks = grown; plan->macroblocks[plan->macroblock_count++] = block; return 1;
}

void mosaic_block_policy_default(mosaic_block_policy *out_policy) {
    if (!out_policy) return;
    *out_policy = (mosaic_block_policy){sizeof *out_policy, 0u, 16u*1024u, 64u*1024u, 256u*1024u,
                                        4u*1024u*1024u, 1000000u, 100000u};
}

static int block_policy_valid(const mosaic_block_policy *p) {
    return p && p->struct_size >= sizeof *p && !p->flags && p->min_bytes &&
           p->min_bytes <= p->preferred_bytes && p->preferred_bytes <= p->max_bytes &&
           p->max_bytes <= SIZE_MAX && p->macroblock_bytes >= p->max_bytes &&
           p->max_blocks && p->max_blocks <= SIZE_MAX && p->max_macroblocks && p->max_macroblocks <= SIZE_MAX;
}

static int hash_block_identity(const uint8_t fingerprint[32], const uint8_t *source, size_t len, uint8_t out[32]) {
    static const uint8_t domain[] = "MOSAIC-BLOCK-v1"; Sha256Ctx c; sha256_init(&c);
    return sha256_update(&c,domain,sizeof domain-1u) && sha256_update(&c,fingerprint,32u) &&
           sha256_update(&c,source,len) && sha256_final(&c,out);
}
static int hash_macro_identity(const uint8_t fingerprint[32], const mosaic_processing_block *blocks, size_t count, uint8_t out[32]) {
    static const uint8_t domain[] = "MOSAIC-MACROBLOCK-v1"; Sha256Ctx c; sha256_init(&c);
    if (!sha256_update(&c,domain,sizeof domain-1u) || !sha256_update(&c,fingerprint,32u)) return 0;
    for (size_t i=0;i<count;++i) if (!sha256_update(&c,blocks[i].identity_sha256,32u)) return 0;
    return sha256_final(&c,out);
}

mosaic_status mosaic_token_document_block_plan(const mosaic_token_document *document, const mosaic_block_policy *policy, mosaic_block_plan **out_plan) {
    if (!document || !out_plan) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_plan = NULL;
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_MODEL)) return MOSAIC_ERROR_UNSUPPORTED;
    mosaic_block_policy p; if (policy) p=*policy; else mosaic_block_policy_default(&p);
    if (!block_policy_valid(&p)) return MOSAIC_ERROR_INVALID_ARGUMENT;
    if (document->source_len && !document->model_token_count) return MOSAIC_ERROR_INTERNAL;
    size_t sum=0; for(size_t i=0;i<document->model_token_count;++i){ if(document->model_tokens[i].reserved || !document->model_tokens[i].length || document->model_tokens[i].length>SIZE_MAX-sum) return MOSAIC_ERROR_INTERNAL; sum+=document->model_tokens[i].length; }
    if(sum!=document->source_len) return MOSAIC_ERROR_INTERNAL;
    mosaic_block_plan *plan=(mosaic_block_plan*)calloc(1,sizeof*plan); if(!plan)return MOSAIC_ERROR_OUT_OF_MEMORY;
    plan->info.source_length=document->source_len; plan->info.model_token_count=document->model_token_count; plan->info.policy=p;
    memcpy(plan->info.source_sha256,document->source_sha256,32); memcpy(plan->info.tokenizer_fingerprint_sha256,document->tokenizer_fingerprint,32);
    size_t ti=0, source_start=0;
    while(ti<document->model_token_count){
        size_t j=ti, accum=0, best_j=ti, best_len=0; uint64_t best_dist=UINT64_MAX; uint32_t flags=0;
        while(j<document->model_token_count){
            size_t tl=document->model_tokens[j].length;
            if(tl>SIZE_MAX-accum){mosaic_block_plan_free(plan);return MOSAIC_ERROR_OVERFLOW;}
            if(accum && (uint64_t)accum+(uint64_t)tl>p.max_bytes)break;
            accum+=tl; ++j;
            if((uint64_t)accum>=p.min_bytes && (uint64_t)accum<=p.max_bytes){
                uint64_t dist=(uint64_t)accum>p.preferred_bytes?(uint64_t)accum-p.preferred_bytes:p.preferred_bytes-(uint64_t)accum;
                if(dist<best_dist || (dist==best_dist && accum>best_len)){best_dist=dist;best_len=accum;best_j=j;}
            }
            if((uint64_t)accum>=p.max_bytes)break;
        }
        if(best_j==ti){
            best_j=ti+1u; best_len=document->model_tokens[ti].length;
            if((uint64_t)best_len>p.max_bytes)flags|=MOSAIC_BLOCK_OVERSIZE_TOKEN;
        }
        mosaic_processing_block b={0}; b.source_start=source_start;b.source_length=best_len;b.first_model_token=ti;b.model_token_count=best_j-ti;b.flags=flags;
        if(!sha256_bytes(document->source+source_start,best_len,b.source_sha256)||!hash_block_identity(document->tokenizer_fingerprint,document->source+source_start,best_len,b.identity_sha256)){mosaic_block_plan_free(plan);return MOSAIC_ERROR_INTERNAL;}
        if (!append_processing_block(plan, b, (size_t)p.max_blocks)) {
            mosaic_status error = plan->block_count >= (size_t)p.max_blocks ? MOSAIC_ERROR_RESOURCE_LIMIT : MOSAIC_ERROR_OUT_OF_MEMORY;
            mosaic_block_plan_free(plan);
            return error;
        }
        source_start+=best_len;ti=best_j;
    }
    size_t bi=0; while(bi<plan->block_count){
        size_t first=bi; uint64_t total=0;
        do { if(UINT64_MAX-total<plan->blocks[bi].source_length){mosaic_block_plan_free(plan);return MOSAIC_ERROR_OVERFLOW;} total+=plan->blocks[bi].source_length; ++bi; } while(bi<plan->block_count && total<p.macroblock_bytes);
        mosaic_macroblock m={0};m.first_block=first;m.block_count=bi-first;m.source_start=plan->blocks[first].source_start;m.source_length=total;
        if(!hash_macro_identity(document->tokenizer_fingerprint,plan->blocks+first,bi-first,m.identity_sha256)){mosaic_block_plan_free(plan);return MOSAIC_ERROR_INTERNAL;}
        if (!append_macroblock(plan, m, (size_t)p.max_macroblocks)) {
            mosaic_status error = plan->macroblock_count >= (size_t)p.max_macroblocks ? MOSAIC_ERROR_RESOURCE_LIMIT : MOSAIC_ERROR_OUT_OF_MEMORY;
            mosaic_block_plan_free(plan);
            return error;
        }
    }
    plan->info.block_count=plan->block_count;plan->info.macroblock_count=plan->macroblock_count;*out_plan=plan;return MOSAIC_OK;
}

mosaic_status mosaic_block_plan_get_info(const mosaic_block_plan *plan,mosaic_block_plan_info *out_info){if(!plan||!out_info)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_info=plan->info;return MOSAIC_OK;}
mosaic_status mosaic_block_plan_blocks(const mosaic_block_plan *plan,mosaic_processing_block **out_blocks,size_t *out_count){if(!plan||!out_blocks||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_blocks=NULL;*out_count=0;if(plan->block_count>SIZE_MAX/sizeof**out_blocks)return MOSAIC_ERROR_OVERFLOW;mosaic_processing_block*c=plan->block_count?(mosaic_processing_block*)malloc(plan->block_count*sizeof*c):NULL;if(plan->block_count&&!c)return MOSAIC_ERROR_OUT_OF_MEMORY;if(plan->block_count)memcpy(c,plan->blocks,plan->block_count*sizeof*c);*out_blocks=c;*out_count=plan->block_count;return MOSAIC_OK;}
mosaic_status mosaic_block_plan_macroblocks(const mosaic_block_plan *plan,mosaic_macroblock **out_macroblocks,size_t *out_count){if(!plan||!out_macroblocks||!out_count)return MOSAIC_ERROR_INVALID_ARGUMENT;*out_macroblocks=NULL;*out_count=0;if(plan->macroblock_count>SIZE_MAX/sizeof**out_macroblocks)return MOSAIC_ERROR_OVERFLOW;mosaic_macroblock*c=plan->macroblock_count?(mosaic_macroblock*)malloc(plan->macroblock_count*sizeof*c):NULL;if(plan->macroblock_count&&!c)return MOSAIC_ERROR_OUT_OF_MEMORY;if(plan->macroblock_count)memcpy(c,plan->macroblocks,plan->macroblock_count*sizeof*c);*out_macroblocks=c;*out_count=plan->macroblock_count;return MOSAIC_OK;}
void mosaic_block_plan_free(mosaic_block_plan *plan){if(!plan)return;free(plan->blocks);free(plan->macroblocks);free(plan);}

mosaic_status mosaic_processing_block_cache_key(const mosaic_processing_block *block,uint32_t projection_namespace,uint32_t schema_version,uint8_t out_sha256[32]){
    if(!block||!out_sha256||!projection_namespace||!schema_version||block->reserved||!block->source_length||!block->model_token_count||(block->flags&~MOSAIC_BLOCK_OVERSIZE_TOKEN))return MOSAIC_ERROR_INVALID_ARGUMENT;
    static const uint8_t domain[]="MOSAIC-CACHE-KEY-v1";uint8_t meta[8];wr32(meta,projection_namespace);wr32(meta+4,schema_version);Sha256Ctx c;sha256_init(&c);
    if(!sha256_update(&c,domain,sizeof domain-1u)||!sha256_update(&c,block->identity_sha256,32u)||!sha256_update(&c,meta,sizeof meta)||!sha256_final(&c,out_sha256))return MOSAIC_ERROR_INTERNAL;
    return MOSAIC_OK;
}

static size_t uleb_size_u64(uint64_t v){size_t n=1;while(v>=128u){v>>=7u;++n;}return n;}
static void uleb_write_u64(uint8_t *p,uint64_t v,size_t *used){size_t n=0;do{uint8_t b=(uint8_t)(v&127u);v>>=7u;if(v)b|=128u;p[n++]=b;}while(v);*used=n;}
static int uleb_read_u64(const uint8_t *p,size_t len,size_t *used,uint64_t *out){
    uint64_t v=0;unsigned shift=0;
    for(size_t i=0;i<len&&i<10u;++i){
        uint8_t b=p[i];uint64_t part=(uint64_t)(b&127u);
        if(shift>63u||(shift==63u&&part>1u))return 0;
        v|=part<<shift;
        if(!(b&128u)){if(i&&b==0u)return 0;*used=i+1u;*out=v;return 1;}
        shift+=7u;
    }
    return 0;
}
static uint32_t bits_needed_u32(uint32_t v){uint32_t n=0;do{++n;v>>=1u;}while(v);return n;}
static int packed_model_content_hash(const uint8_t *bytes,size_t len,uint8_t out[32]){
    if(!bytes||len<MOSAIC_PACKED_MODEL_HEADER)return 0;
    static const uint8_t zeros[32]={0};Sha256Ctx c;sha256_init(&c);
    return sha256_update(&c,bytes,120u)&&sha256_update(&c,zeros,sizeof zeros)&&
           sha256_update(&c,bytes+152u,len-152u)&&sha256_final(&c,out);
}

mosaic_status mosaic_token_document_pack_model(const mosaic_token_document *document,uint8_t **out_bytes,size_t *out_len){
    if (!document || !out_bytes || !out_len) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_bytes = NULL;
    *out_len = 0;
    if (!(document->flags & MOSAIC_TOKEN_DOCUMENT_MODEL)) return MOSAIC_ERROR_UNSUPPORTED;
    size_t lengths=0;uint32_t max_id=0;uint64_t source_sum=0;for(size_t i=0;i<document->model_token_count;++i){ResyncToken t=document->model_tokens[i];if(!t.length||t.reserved)return MOSAIC_ERROR_INTERNAL;size_t n=uleb_size_u64(t.length);if(n>SIZE_MAX-lengths)return MOSAIC_ERROR_OVERFLOW;lengths+=n;if(t.id>max_id)max_id=t.id;if(UINT64_MAX-source_sum<t.length)return MOSAIC_ERROR_OVERFLOW;source_sum+=t.length;}if(source_sum!=document->source_len)return MOSAIC_ERROR_INTERNAL;
    uint32_t id_bits=document->model_token_count?bits_needed_u32(max_id):0u;if(id_bits&&document->model_token_count>UINT64_MAX/id_bits)return MOSAIC_ERROR_OVERFLOW;uint64_t bit_count=(uint64_t)id_bits*(uint64_t)document->model_token_count;if(bit_count>UINT64_MAX-7u)return MOSAIC_ERROR_OVERFLOW;uint64_t ids64=(bit_count+7u)/8u;if(ids64>SIZE_MAX)return MOSAIC_ERROR_OVERFLOW;size_t ids=(size_t)ids64,payload; if(!add_size(lengths,ids,&payload))return MOSAIC_ERROR_OVERFLOW;size_t total;if(!add_size(MOSAIC_PACKED_MODEL_HEADER,payload,&total))return MOSAIC_ERROR_OVERFLOW;uint8_t*out=(uint8_t*)calloc(total?total:1u,1u);if(!out)return MOSAIC_ERROR_OUT_OF_MEMORY;
    memcpy(out,MOSAIC_PACKED_MODEL_MAGIC,8);wr16(out+8,1u);wr16(out+10,MOSAIC_PACKED_MODEL_HEADER);wr32(out+12,0u);wr64(out+16,document->model_token_count);wr64(out+24,document->source_len);wr32(out+32,id_bits);wr32(out+36,0u);wr64(out+40,lengths);wr64(out+48,ids);memcpy(out+56,document->source_sha256,32);memcpy(out+88,document->tokenizer_fingerprint,32);
    size_t pos=MOSAIC_PACKED_MODEL_HEADER;for(size_t i=0;i<document->model_token_count;++i){size_t used=0;uleb_write_u64(out+pos,document->model_tokens[i].length,&used);pos+=used;}uint8_t*idp=out+MOSAIC_PACKED_MODEL_HEADER+lengths;uint64_t bitpos=0;for(size_t i=0;i<document->model_token_count;++i){uint32_t id=document->model_tokens[i].id;for(uint32_t b=0;b<id_bits;++b,++bitpos)if((id>>b)&1u)idp[bitpos/8u]|=(uint8_t)(1u<<(bitpos%8u));}
    if(!packed_model_content_hash(out,total,out+120)){free(out);return MOSAIC_ERROR_INTERNAL;}*out_bytes=out;*out_len=total;return MOSAIC_OK;
}

static mosaic_status packed_model_validate(const uint8_t *bytes,size_t len,mosaic_packed_model_info *out,size_t *out_lengths,size_t *out_ids){
    if (!bytes || len < MOSAIC_PACKED_MODEL_HEADER || memcmp(bytes, MOSAIC_PACKED_MODEL_MAGIC, 8) ||
        rd16(bytes + 8) != 1u || rd16(bytes + 10) != MOSAIC_PACKED_MODEL_HEADER || rd32(bytes + 12) || rd32(bytes + 36))
        return MOSAIC_ERROR_INVALID_PACK;
    for (size_t i = 152; i < 160; ++i) if (bytes[i]) return MOSAIC_ERROR_INVALID_PACK;
    uint64_t count=rd64(bytes+16),source_len=rd64(bytes+24),lengths64=rd64(bytes+40),ids64=rd64(bytes+48);uint32_t bits=rd32(bytes+32);if((count==0u)!=(bits==0u)||bits>32u||count>SIZE_MAX||source_len>SIZE_MAX||lengths64>SIZE_MAX||ids64>SIZE_MAX)return MOSAIC_ERROR_INVALID_PACK;if(bits&&count>UINT64_MAX/bits)return MOSAIC_ERROR_INVALID_PACK;uint64_t expected_bits=count*(uint64_t)bits;if(expected_bits>UINT64_MAX-7u)return MOSAIC_ERROR_INVALID_PACK;uint64_t expected_ids=(expected_bits+7u)/8u;if(ids64!=expected_ids)return MOSAIC_ERROR_INVALID_PACK;size_t lengths=(size_t)lengths64,ids=(size_t)ids64,payload,total;if(!add_size(lengths,ids,&payload)||!add_size(MOSAIC_PACKED_MODEL_HEADER,payload,&total)||total!=len)return MOSAIC_ERROR_INVALID_PACK;uint8_t digest[32];if(!packed_model_content_hash(bytes,len,digest)||memcmp(digest,bytes+120,32))return MOSAIC_ERROR_INVALID_PACK;
    size_t pos=MOSAIC_PACKED_MODEL_HEADER;uint64_t sum=0;for(uint64_t i=0;i<count;++i){size_t used=0;uint64_t tl=0;if(!uleb_read_u64(bytes+pos,MOSAIC_PACKED_MODEL_HEADER+lengths-pos,&used,&tl)||!tl||tl>UINT64_MAX-sum)return MOSAIC_ERROR_INVALID_PACK;sum+=tl;pos+=used;}if(pos!=MOSAIC_PACKED_MODEL_HEADER+lengths||sum!=source_len)return MOSAIC_ERROR_INVALID_PACK;if(ids&&expected_bits%8u){uint8_t mask=(uint8_t)(0xffu<<(expected_bits%8u));if(bytes[len-1u]&mask)return MOSAIC_ERROR_INVALID_PACK;}
    if(out){*out=(mosaic_packed_model_info){1u,bits,count,source_len,len,{0},{0},{0}};memcpy(out->source_sha256,bytes+56,32);memcpy(out->tokenizer_fingerprint_sha256,bytes+88,32);memcpy(out->content_sha256,bytes+120,32);}if(out_lengths)*out_lengths=lengths;if(out_ids)*out_ids=ids;return MOSAIC_OK;
}

mosaic_status mosaic_packed_model_inspect(const uint8_t *bytes,size_t len,mosaic_packed_model_info *out_info){if(!out_info)return MOSAIC_ERROR_INVALID_ARGUMENT;memset(out_info,0,sizeof*out_info);return packed_model_validate(bytes,len,out_info,NULL,NULL);}

mosaic_status mosaic_packed_model_decode(const uint8_t *bytes,size_t len,mosaic_document_token **out_tokens,size_t *out_count){
    if (!out_tokens || !out_count) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_tokens = NULL;
    *out_count = 0;
    mosaic_packed_model_info info;
    size_t lengths = 0, ids = 0;
    mosaic_status st = packed_model_validate(bytes, len, &info, &lengths, &ids);
    if (st != MOSAIC_OK) return st;
    if (info.token_count > SIZE_MAX / sizeof **out_tokens) return MOSAIC_ERROR_OVERFLOW;
    size_t count = (size_t)info.token_count;
    mosaic_document_token *out = count ? (mosaic_document_token *)malloc(count * sizeof *out) : NULL;
    if (count && !out) return MOSAIC_ERROR_OUT_OF_MEMORY;
    size_t pos = MOSAIC_PACKED_MODEL_HEADER;
    uint64_t start = 0, bitpos = 0;
    const uint8_t *idp = bytes + MOSAIC_PACKED_MODEL_HEADER + lengths;
    for (size_t i = 0; i < count; ++i) {
        size_t used = 0; uint64_t tl = 0;
        if (!uleb_read_u64(bytes + pos, lengths - (pos - MOSAIC_PACKED_MODEL_HEADER), &used, &tl)) { free(out); return MOSAIC_ERROR_INVALID_PACK; }
        pos += used;
        uint32_t id = 0;
        for (uint32_t b = 0; b < info.id_bit_width; ++b, ++bitpos)
            if (idp[bitpos / 8u] & (uint8_t)(1u << (bitpos % 8u))) id |= 1u << b;
        out[i] = (mosaic_document_token){id, start, tl};
        start += tl;
    }
    *out_tokens = out;
    *out_count = count;
    (void)ids;
    return MOSAIC_OK;
}

void mosaic_token_document_free(mosaic_token_document *document) {
    if (!document) return;
    free(document->source);
    free(document->model_tokens);
    free(document->graphemes);
    free(document->security_findings);
    free(document->lexical_tokens);
    free(document->semantic_components);
    mosaic_normalized_view_free(&document->normalized);
    free(document);
}

mosaic_status mosaic_tokenizer_stream_create(const mosaic_tokenizer *tokenizer, mosaic_stream **out_stream) {
    if (!tokenizer || !out_stream) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_stream = NULL;
    mosaic_stream *stream = (mosaic_stream *)calloc(1, sizeof *stream);
    if (!stream) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_status status = tokenizer_clone(tokenizer, &stream->tokenizer);
    if (status != MOSAIC_OK) { free(stream); return status; }
    stream->max_buffer_bytes=(size_t)(tokenizer->limits.max_input_bytes>SIZE_MAX?SIZE_MAX:tokenizer->limits.max_input_bytes);
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
    if ((uint64_t)input_len > tokenizer->limits.max_input_bytes) return MOSAIC_ERROR_RESOURCE_LIMIT;
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
    document->max_buffer_bytes = (size_t)(tokenizer->limits.max_input_bytes > SIZE_MAX ? SIZE_MAX : tokenizer->limits.max_input_bytes);
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
            "       %s analyze-span-auto MODEL_PACK UNICODE_PACK DETECTOR_PACK INPUT LANGUAGE_PACK...\n"
            "       %s roundtrip-auto MODEL_PACK UNICODE_PACK DETECTOR_PACK INPUT LANGUAGE_PACK...\n"
            "       %s security SECURITY_PACK INPUT\n"
            "       %s fingerprint-security MODEL_PACK UNICODE_PACK SECURITY_PACK\n"
            "       %s analyze-security MODEL_PACK UNICODE_PACK SECURITY_PACK INPUT\n"
            "       %s lexer LEXER_PACK INPUT\n"
            "       %s fingerprint-lexer MODEL_PACK UNICODE_PACK LEXER_PACK\n"
            "       %s analyze-lexer MODEL_PACK UNICODE_PACK LEXER_PACK INPUT\n"
            "       %s normalize NORMALIZATION_PACK MODE INPUT OUTPUT\n"
            "       %s normalize-map NORMALIZATION_PACK MODE INPUT\n"
            "       %s fingerprint-normalization MODEL_PACK UNICODE_PACK NORMALIZATION_PACK\n"
            "       %s analyze-normalization MODEL_PACK UNICODE_PACK NORMALIZATION_PACK MODE INPUT\n",
            argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
            argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
            argv0, argv0, argv0);
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
            if (spans[i].script_id == 0) memcpy(name, "OPAQUE", sizeof "OPAQUE");
            else if (mosaic_security_script_name(security, spans[i].script_id, name, sizeof name, &need) != MOSAIC_OK)
                memcpy(name, "UNKNOWN", sizeof "UNKNOWN");
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
    int span_only = !strcmp(argv[1], "analyze-span-auto");
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
    if (span_only) {
        uint32_t *ids = NULL; size_t id_count = 0; mosaic_span_route *routes = NULL; size_t route_count = 0;
        uint8_t *decoded = NULL; size_t decoded_len = 0;
        mosaic_status ed = mosaic_tokenizer_detect_language(tokenizer, input, input_len, &detection);
        mosaic_status es = mosaic_tokenizer_encode_span_auto(tokenizer, input, input_len, &ids, &id_count, &routes, &route_count);
        mosaic_status ds = es == MOSAIC_OK ? mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) : es;
        int ok = ed == MOSAIC_OK && es == MOSAIC_OK && ds == MOSAIC_OK && decoded_len == input_len &&
                 (!input_len || memcmp(decoded, input, input_len) == 0);
        if (ok) {
            printf("fingerprint="); static const char hex[]="0123456789abcdef";
            for(size_t i=0;i<32;++i)printf("%c%c",hex[fingerprint[i]>>4],hex[fingerprint[i]&15]);
            printf(" bytes=%zu tokens=%zu spans=%zu route=%s available=%u score=%" PRId64 " margin=%" PRId64 " languages=%zu\n",
                   input_len, id_count, route_count, detection.matched ? detection.language : "none", detection.available,
                   detection.score, detection.margin, mosaic_tokenizer_language_count(tokenizer));
            for (size_t i = 0; i < route_count; ++i) {
                const mosaic_span_route *route = &routes[i];
                printf("span start=%" PRIu64 " length=%" PRIu64 " route=%s available=%u score=%" PRId64 " margin=%" PRId64 "\n",
                       route->start, route->length, route->detection.matched ? route->detection.language : "none",
                       route->detection.available, route->detection.score, route->detection.margin);
            }
        }
        mosaic_free(ids); mosaic_free(routes); mosaic_free(decoded); free(input); mosaic_tokenizer_free(tokenizer); return ok ? 1 : 0;
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


static const char *lex_kind_name(uint16_t kind) {
    switch ((mosaic_lex_kind)kind) {
        case MOSAIC_LEX_WHITESPACE: return "whitespace";
        case MOSAIC_LEX_NEWLINE: return "newline";
        case MOSAIC_LEX_IDENTIFIER: return "identifier";
        case MOSAIC_LEX_KEYWORD: return "keyword";
        case MOSAIC_LEX_NUMBER: return "number";
        case MOSAIC_LEX_STRING: return "string";
        case MOSAIC_LEX_COMMENT: return "comment";
        case MOSAIC_LEX_PUNCTUATION: return "punctuation";
        case MOSAIC_LEX_ERROR: return "error";
        default: return "unknown";
    }
}

static void print_lex_tokens(const mosaic_lex_token *tokens, size_t count) {
    printf("lexical_tokens=%zu\n", count);
    for (size_t i = 0; i < count; ++i) {
        printf("lex kind=%s kind_id=%u flags=%u start=%" PRIu64 " length=%" PRIu64 "\n",
               lex_kind_name((uint16_t)tokens[i].kind), (unsigned)tokens[i].kind, (unsigned)tokens[i].flags,
               tokens[i].start, tokens[i].length);
    }
}

static int lexer_cli(int argc, char **argv) {
    if (argc != 4) return -1;
    mosaic_lexer *lexer = NULL;
    if (mosaic_lexer_load_file(argv[2], &lexer) != MOSAIC_OK) return 0;
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[3], &input, &input_len)) { mosaic_lexer_free(lexer); return 0; }
    mosaic_lex_token *tokens = NULL; size_t count = 0;
    mosaic_status st = mosaic_lex(lexer, input, input_len, &tokens, &count);
    if (st == MOSAIC_OK) {
        char profile[128] = {0}; size_t required = 0;
        if (mosaic_lexer_profile_name(lexer, profile, sizeof profile, &required) != MOSAIC_OK) {
            mosaic_free(tokens); free(input); mosaic_lexer_free(lexer); return 0;
        }
        printf("profile=%s bytes=%zu ", profile, input_len);
        print_lex_tokens(tokens, count);
    }
    mosaic_free(tokens); free(input); mosaic_lexer_free(lexer);
    return st == MOSAIC_OK ? 1 : 0;
}

static int integrated_lexer_cli(int argc, char **argv) {
    int fingerprint_only = !strcmp(argv[1], "fingerprint-lexer");
    if ((fingerprint_only && argc != 5) || (!fingerprint_only && argc != 6)) return -1;
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[2], argv[3], &tokenizer) != MOSAIC_OK) return 0;
    if (mosaic_tokenizer_set_lexer_file(tokenizer, argv[4]) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    uint8_t fingerprint[32];
    if (mosaic_tokenizer_fingerprint(tokenizer, fingerprint) != MOSAIC_OK) { mosaic_tokenizer_free(tokenizer); return 0; }
    if (fingerprint_only) { print_hex32(fingerprint); mosaic_tokenizer_free(tokenizer); return 1; }
    uint8_t *input = NULL; size_t input_len = 0;
    if (!read_file(argv[5], &input, &input_len)) { mosaic_tokenizer_free(tokenizer); return 0; }
    mosaic_lex_token *tokens = NULL; size_t count = 0;
    mosaic_status st = mosaic_tokenizer_lex(tokenizer, input, input_len, &tokens, &count);
    if (st == MOSAIC_OK) {
        printf("fingerprint=");
        static const char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < 32; ++i) printf("%c%c", hex[fingerprint[i] >> 4], hex[fingerprint[i] & 15]);
        printf(" bytes=%zu ", input_len);
        print_lex_tokens(tokens, count);
    }
    mosaic_free(tokens); free(input); mosaic_tokenizer_free(tokenizer);
    return st == MOSAIC_OK ? 1 : 0;
}


static int parse_normalization_mode(const char *name,mosaic_normalization_mode *mode){
    if(!strcmp(name,"preserve"))*mode=MOSAIC_NORMALIZE_PRESERVE;
    else if(!strcmp(name,"nfd"))*mode=MOSAIC_NORMALIZE_NFD;
    else if(!strcmp(name,"nfc"))*mode=MOSAIC_NORMALIZE_NFC;
    else if(!strcmp(name,"nfkd"))*mode=MOSAIC_NORMALIZE_NFKD;
    else if(!strcmp(name,"nfkc"))*mode=MOSAIC_NORMALIZE_NFKC;
    else if(!strcmp(name,"nfkc-casefold"))*mode=MOSAIC_NORMALIZE_NFKC_CASEFOLD;
    else return 0;
    return 1;
}
static int write_bytes_file(const char *path,const uint8_t *bytes,size_t len){FILE*f=fopen(path,"wb");if(!f){perror(path);return 0;}int ok=!len||fwrite(bytes,1,len,f)==len;if(fclose(f)!=0)ok=0;return ok;}
static void print_normalization_map(const mosaic_normalized_view *v){
    printf("normalized_bytes=%zu units=%zu source_spans=%zu\n",v->byte_length,v->unit_count,v->source_span_count);
    for(size_t i=0;i<v->unit_count;++i){const mosaic_normalized_unit*u=&v->units[i];printf("unit output=%" PRIu64 ":%" PRIu64 " sources=",u->output_start,u->output_start+u->output_length);for(uint32_t j=0;j<u->source_span_count;++j){mosaic_range r=v->source_spans[u->source_span_index+j];printf("%s%" PRIu64 ":%" PRIu64,j?",":"",r.start,r.start+r.length);}putchar('\n');}
}
static int normalization_cli(int argc,char **argv){
    int map_only=!strcmp(argv[1],"normalize-map");if((map_only&&argc!=5)||(!map_only&&argc!=6))return -1;mosaic_normalization_mode mode;if(!parse_normalization_mode(argv[3],&mode))return -1;
    mosaic_normalization*n=NULL;if(mosaic_normalization_load_file(argv[2],&n)!=MOSAIC_OK)return 0;uint8_t*input=NULL;size_t input_len=0;if(!read_file(argv[4],&input,&input_len)){mosaic_normalization_free(n);return 0;}mosaic_normalized_view v={0};mosaic_status status=mosaic_normalize(n,mode,input,input_len,&v);int ok=status==MOSAIC_OK;if(ok&&map_only)print_normalization_map(&v);else if(ok)ok=write_bytes_file(argv[5],v.bytes,v.byte_length);mosaic_normalized_view_free(&v);free(input);mosaic_normalization_free(n);return ok?1:0;
}
static int integrated_normalization_cli(int argc,char **argv){
    int fingerprint_only=!strcmp(argv[1],"fingerprint-normalization");if((fingerprint_only&&argc!=5)||(!fingerprint_only&&argc!=7))return -1;mosaic_tokenizer*t=NULL;if(mosaic_tokenizer_load_files(argv[2],argv[3],&t)!=MOSAIC_OK)return 0;if(mosaic_tokenizer_set_normalization_file(t,argv[4])!=MOSAIC_OK){mosaic_tokenizer_free(t);return 0;}uint8_t hash[32];if(mosaic_tokenizer_fingerprint(t,hash)!=MOSAIC_OK){mosaic_tokenizer_free(t);return 0;}if(fingerprint_only){print_hex32(hash);mosaic_tokenizer_free(t);return 1;}mosaic_normalization_mode mode;if(!parse_normalization_mode(argv[5],&mode)){mosaic_tokenizer_free(t);return -1;}uint8_t*input=NULL;size_t input_len=0;if(!read_file(argv[6],&input,&input_len)){mosaic_tokenizer_free(t);return 0;}mosaic_normalized_view v={0};mosaic_status st=mosaic_tokenizer_normalize(t,mode,input,input_len,&v);if(st==MOSAIC_OK){printf("fingerprint=");static const char hex[]="0123456789abcdef";for(size_t i=0;i<32;++i)printf("%c%c",hex[hash[i]>>4],hex[hash[i]&15]);putchar('\n');print_normalization_map(&v);}mosaic_normalized_view_free(&v);free(input);mosaic_tokenizer_free(t);return st==MOSAIC_OK?1:0;
}

int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "version"))) {
        printf("mosaic-tokenizer %s\n", mosaic_version_string());
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "lexer")) {
        int result=lexer_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint-lexer") || !strcmp(argv[1], "analyze-lexer"))) {
        int result=integrated_lexer_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "normalize") || !strcmp(argv[1], "normalize-map"))) {
        int result=normalization_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
    }
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint-normalization") || !strcmp(argv[1], "analyze-normalization"))) {
        int result=integrated_normalization_cli(argc,argv);if(result<0){usage(argv[0]);return 2;}return result?0:1;
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
    if (argc >= 2 && (!strcmp(argv[1], "fingerprint-auto") || !strcmp(argv[1], "analyze-auto") ||
                      !strcmp(argv[1], "analyze-span-auto") || !strcmp(argv[1], "roundtrip-auto"))) {
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
    Vocab v = {0}; if (!load_model_pack(pack, pack_len, &v)) { vocab_release_runtime_index(&v); free(pack); return 1; }
    if (!strcmp(argv[1], "validate")) { printf("OK vocabulary_entries=%" PRIu32 " pack_bytes=%zu\n", v.count, pack_len); vocab_release_runtime_index(&v); free(pack); return 0; }
    if (!strcmp(argv[1], "decode-u32")) {
        if (argc != 5) { usage(argv[0]); vocab_release_runtime_index(&v); free(pack); return 2; }
        int ok = decode_u32_file(&v, argv[3], argv[4]);
        vocab_release_runtime_index(&v); free(pack);
        return ok ? 0 : 1;
    }
    if (!strcmp(argv[1], "encode-u32")) {
        if (argc != 5) { usage(argv[0]); vocab_release_runtime_index(&v); free(pack); return 2; }
        uint8_t *input = NULL; size_t input_len = 0;
        if (!read_file(argv[3], &input, &input_len)) { vocab_release_runtime_index(&v); free(pack); return 1; }
        Tokenization result = {0};
        if (!tokenize_with_adjustments(&v, (Slice){input, input_len}, NULL, &result)) { free(input); vocab_release_runtime_index(&v); free(pack); return 1; }
        int ok = roundtrip(&v, (Slice){input, input_len}, &result)
            && write_encoded_u32(&v, &result, argv[4]);
        tokenization_free(&result); free(input); vocab_release_runtime_index(&v); free(pack);
        return ok ? 0 : 1;
    }
    if (argc != 4) { usage(argv[0]); vocab_release_runtime_index(&v); free(pack); return 2; }
    uint8_t *input = NULL; size_t input_len = 0; if (!read_file(argv[3], &input, &input_len)) { vocab_release_runtime_index(&v); free(pack); return 1; }
    Tokenization result = {0}; if (!tokenize_with_adjustments(&v, (Slice){input, input_len}, NULL, &result)) { free(input); vocab_release_runtime_index(&v); free(pack); return 1; }
    int ok = roundtrip(&v, (Slice){input, input_len}, &result);
    if (!ok) { fail("internal round-trip mismatch"); tokenization_free(&result); free(input); vocab_release_runtime_index(&v); free(pack); return 1; }
    if (!strcmp(argv[1], "encode")) print_encoding(&v, (Slice){input, input_len}, &result);
    else if (!strcmp(argv[1], "roundtrip")) printf("OK bytes=%zu tokens=%zu cost=%" PRId64 "\n", input_len, result.count, result.cost);
    else { usage(argv[0]); tokenization_free(&result); free(input); vocab_release_runtime_index(&v); free(pack); return 2; }
    tokenization_free(&result); free(input); vocab_release_runtime_index(&v); free(pack); return 0;
}
#endif /* MOSAIC_LIBRARY_ONLY */
