#include "mosaic.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define MAX_TOOLS 8
#define MAX_TEMPLATE 1024

typedef struct {
    char name[32];
    char compress[MAX_TEMPLATE];
    char decompress[MAX_TEMPLATE];
} ToolSpec;

typedef struct {
    ToolSpec tools[MAX_TOOLS];
    size_t count;
} ToolList;

static double seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *buf = n ? (uint8_t *)malloc((size_t)n) : NULL;
    if (n && (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_len = (size_t)n;
    return buf;
}

static int file_size(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f);
    fclose(f);
    if (n < 0) return 0;
    *out_size = (size_t)n;
    return 1;
}

static void stem_from_path(const char *path, char *stem, size_t stem_size) {
    const char *base = strrchr(path, '/');
#ifdef _WIN32
    const char *base2 = strrchr(path, '\\');
    if (!base || (base2 && base2 > base)) base = base2;
#endif
    base = base ? base + 1 : path;
    size_t n = strcspn(base, ".");
    if (n >= stem_size) n = stem_size - 1;
    memcpy(stem, base, n);
    stem[n] = '\0';
    if (!stem[0]) snprintf(stem, stem_size, "mosaic");
}

static int replace_tokens(const char *tmpl, const char *input, const char *archive, const char *roundtrip, char *buf, size_t buflen) {
    size_t used = 0;
    for (const char *p = tmpl; *p; ++p) {
        const char *rep = NULL;
        size_t rep_len = 0;
        if (p[0] == '%' && p[1] == 'I') { rep = input; rep_len = strlen(input); ++p; }
        else if (p[0] == '%' && p[1] == 'A') { rep = archive; rep_len = strlen(archive); ++p; }
        else if (p[0] == '%' && p[1] == 'R') { rep = roundtrip; rep_len = strlen(roundtrip); ++p; }
        else {
            if (used + 1 >= buflen) return 0;
            buf[used++] = *p;
            continue;
        }
        if (used + rep_len >= buflen) return 0;
        memcpy(buf + used, rep, rep_len);
        used += rep_len;
    }
    if (used + 1 >= buflen) return 0;
    buf[used] = '\0';
    return 1;
}

static int run_command(const char *cmd) {
    int rc = system(cmd);
#ifdef _WIN32
    return rc == 0;
#else
    return rc == 0;
#endif
}

static int command_available(const char *name) {
#ifdef _WIN32
    char path[MAX_TEMPLATE];
    DWORD n = SearchPathA(NULL, name, ".exe;.cmd;.bat", (DWORD)sizeof path, path, NULL);
    return n > 0 && n < sizeof path;
#else
    char probe[128];
    snprintf(probe, sizeof probe, "%s", name);
    return access(probe, X_OK) == 0;
#endif
}

static void add_tool(ToolList *tools, const char *name, const char *compress, const char *decompress) {
    if (tools->count >= MAX_TOOLS) return;
    ToolSpec *t = &tools->tools[tools->count++];
    snprintf(t->name, sizeof t->name, "%s", name);
    snprintf(t->compress, sizeof t->compress, "%s", compress);
    snprintf(t->decompress, sizeof t->decompress, "%s", decompress);
}

static void default_tools(ToolList *tools) {
    tools->count = 0;
    add_tool(tools, "zip", "zip -j -q \"%A\" \"%I\"", "unzip -p \"%A\" > \"%R\"");
    add_tool(tools, "7z", "7z a -t7z -bd -y -mx=9 \"%A\" \"%I\"", "7z e -bd -y -so \"%A\" > \"%R\"");
    add_tool(tools, "rar", "rar a -idq \"%A\" \"%I\"", "rar p -inul \"%A\" > \"%R\"");
}

static int compare_external(const char *input_path, const char *archive_path, const char *roundtrip_path, const ToolSpec *tool) {
    char cmd[MAX_TEMPLATE * 2];
    if (!command_available(tool->name)) return 0;
    if (!replace_tokens(tool->compress, input_path, archive_path, roundtrip_path, cmd, sizeof cmd)) return 0;
    if (!run_command(cmd)) return 0;
    if (!replace_tokens(tool->decompress, input_path, archive_path, roundtrip_path, cmd, sizeof cmd)) return 0;
    if (!run_command(cmd)) return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    if (mosaic_tokenizer_semantics_version() != 2u) return 60;

    const char *model_pack = argv[1];
    const char *unicode_pack = argv[2];
    const char *input_path = argv[3];

    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(model_pack, unicode_pack, &tokenizer) != MOSAIC_OK) return 3;

    size_t input_len = 0;
    uint8_t *input = read_file(input_path, &input_len);
    if (input_len && !input) { mosaic_tokenizer_free(tokenizer); return 4; }

    uint32_t *ids = NULL;
    size_t id_count = 0;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    double t0 = seconds();
    if (mosaic_tokenizer_encode(tokenizer, input, input_len, &ids, &id_count) != MOSAIC_OK) { free(input); mosaic_tokenizer_free(tokenizer); return 5; }
    if (mosaic_tokenizer_decode(tokenizer, ids, id_count, &decoded, &decoded_len) != MOSAIC_OK) { mosaic_free(ids); free(input); mosaic_tokenizer_free(tokenizer); return 6; }
    double t1 = seconds();
    if (decoded_len != input_len || (input_len && memcmp(decoded, input, input_len) != 0)) {
        mosaic_free(ids);
        mosaic_free(decoded);
        free(input);
        mosaic_tokenizer_free(tokenizer);
        return 7;
    }

    char stem[256];
    stem_from_path(input_path, stem, sizeof stem);
    char archive_path[512];
    char roundtrip_path[512];
    snprintf(archive_path, sizeof archive_path, "%s.%s.7z", input_path, stem);
    snprintf(roundtrip_path, sizeof roundtrip_path, "%s.%s.roundtrip", input_path, stem);

    ToolList tools;
    default_tools(&tools);

    printf("mosaic bytes=%zu tokens=%zu roundtrip=PASS encode_decode=%.6f\n", input_len, id_count, t1 - t0);
    size_t archive_size = 0;
    for (size_t i = 0; i < tools.count; ++i) {
        const ToolSpec *tool = &tools.tools[i];
        double c0 = seconds();
        int ok = compare_external(input_path, archive_path, roundtrip_path, tool);
        double c1 = seconds();
        if (!ok) {
            printf("%s=SKIP\n", tool->name);
            continue;
        }
        if (!file_size(archive_path, &archive_size)) {
            printf("%s=FAIL size\n", tool->name);
            continue;
        }
        size_t roundtrip_size = 0;
        if (!file_size(roundtrip_path, &roundtrip_size)) {
            printf("%s=FAIL roundtrip-size\n", tool->name);
            continue;
        }
        uint8_t *roundtrip = read_file(roundtrip_path, &roundtrip_size);
        if (!roundtrip && roundtrip_size) {
            printf("%s=FAIL roundtrip-read\n", tool->name);
            continue;
        }
        int match = roundtrip_size == input_len && (!input_len || memcmp(roundtrip, input, input_len) == 0);
        free(roundtrip);
        printf("%s=%s archive_bytes=%zu ratio=%.3f elapsed=%.6f\n",
               tool->name, match ? "PASS" : "FAIL", archive_size,
               input_len ? (double)archive_size / (double)input_len : 0.0, c1 - c0);
        remove(archive_path);
        remove(roundtrip_path);
    }

    mosaic_free(ids);
    mosaic_free(decoded);
    free(input);
    mosaic_tokenizer_free(tokenizer);
    return 0;
}
