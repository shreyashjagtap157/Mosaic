#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <compressapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MosaicArchiveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t algorithm;
    uint32_t level;
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum32;
} MosaicArchiveHeader;

static uint32_t crc32_bytes(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int)(crc & 1u));
        }
    }
    return ~crc;
}

static int read_file(const char *path, uint8_t **out_bytes, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n ? (size_t)n : 1);
    if (!buf) { fclose(f); return 0; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return 0; }
    fclose(f);
    *out_bytes = buf;
    *out_len = (size_t)n;
    return 1;
}

static int write_file(const char *path, const uint8_t *bytes, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = !len || fwrite(bytes, 1, len, f) == len;
    fclose(f);
    return ok;
}

static int file_equals(const char *left_path, const char *right_path) {
    uint8_t *left = NULL, *right = NULL;
    size_t left_len = 0, right_len = 0;
    int ok = 0;
    if (!read_file(left_path, &left, &left_len) || !read_file(right_path, &right, &right_len)) goto done;
    ok = left_len == right_len && (!left_len || memcmp(left, right, left_len) == 0);
done:
    free(left);
    free(right);
    return ok;
}

static int roundtrip(const char *input_path, const char *archive_path, const char *output_path) {
    uint8_t *input = NULL, *compressed = NULL, *payload = NULL, *decompressed = NULL;
    size_t input_len = 0, compressed_len = 0;
    COMPRESSOR_HANDLE ch = NULL;
    DECOMPRESSOR_HANDLE dh = NULL;
    MosaicArchiveHeader header;
    DWORD err = ERROR_SUCCESS;
    int ok = 0;
    if (!read_file(input_path, &input, &input_len)) goto done;
    if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &ch)) goto done;
    compressed_len = input_len + input_len / 16u + 1024u;
    if (compressed_len < 4096u) compressed_len = 4096u;
    compressed = (uint8_t *)malloc(compressed_len);
    if (!compressed) goto done;
    if (!Compress(ch, input, (SIZE_T)input_len, compressed, (SIZE_T)compressed_len, (SIZE_T *)&compressed_len)) goto done;
    CloseCompressor(ch); ch = NULL;
    header.magic = 0x31434D5A;
    header.version = 1;
    header.algorithm = COMPRESS_ALGORITHM_XPRESS_HUFF;
    header.level = 3;
    header.original_size = (uint64_t)input_len;
    header.compressed_size = (uint64_t)compressed_len;
    header.checksum32 = crc32_bytes(input, input_len);
    payload = (uint8_t *)malloc(sizeof(header) + compressed_len);
    if (!payload) goto done;
    memcpy(payload, &header, sizeof(header));
    memcpy(payload + sizeof(header), compressed, compressed_len);
    if (!write_file(archive_path, payload, sizeof(header) + compressed_len)) goto done;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &dh)) goto done;
    decompressed = (uint8_t *)malloc((size_t)header.original_size ? (size_t)header.original_size : 1u);
    if (!decompressed) goto done;
    if (!Decompress(dh, payload + sizeof(header), (SIZE_T)compressed_len, decompressed, (SIZE_T)header.original_size, (SIZE_T *)&err)) goto done;
    CloseDecompressor(dh); dh = NULL;
    if (crc32_bytes(decompressed, (size_t)header.original_size) != header.checksum32) goto done;
    if (!write_file(output_path, decompressed, (size_t)header.original_size)) goto done;
    ok = file_equals(input_path, output_path);
done:
    if (ch) CloseCompressor(ch);
    if (dh) CloseDecompressor(dh);
    free(input);
    free(compressed);
    free(payload);
    free(decompressed);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: mosaic-desktop-selftest INPUT ARCHIVE OUTPUT\n");
        return 2;
    }
    if (!roundtrip(argv[1], argv[2], argv[3])) {
        fprintf(stderr, "self-test failed\n");
        return 3;
    }
    printf("OK self-test\n");
    return 0;
}
