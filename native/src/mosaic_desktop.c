#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <compressapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ID_INPUT_EDIT 1001
#define ID_OUTPUT_EDIT 1002
#define ID_ALGO_COMBO 1003
#define ID_LEVEL_TRACK 1004
#define ID_STATUS 1005
#define ID_LOG_EDIT 1006
#define ID_LOAD_INPUT 1007
#define ID_CHOOSE_OUTPUT 1008
#define ID_COMPRESS 1009
#define ID_DECOMPRESS 1010
#define ID_CLEAR 1011

typedef struct AppState {
    HWND hwnd;
    HWND input_edit;
    HWND output_edit;
    HWND algo_combo;
    HWND level_track;
    HWND status;
    HWND log_edit;
    DWORD algorithm;
    DWORD level;
    char input_path[MAX_PATH];
    char output_path[MAX_PATH];
} AppState;

typedef struct ArchiveConfig {
    DWORD algorithm;
    DWORD level;
    const char *input_path;
    const char *archive_path;
    const char *output_path;
    int roundtrip;
} ArchiveConfig;

static void log_append(HWND edit, const char *text) {
    int len = GetWindowTextLengthA(edit);
    SendMessageA(edit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

static void set_text(HWND edit, const char *text) { SetWindowTextA(edit, text ? text : ""); }

static void set_status(AppState *state, const char *text) {
    if (state && state->status) SetWindowTextA(state->status, text ? text : "");
}

static int get_window_text_alloc(HWND hwnd, char **out_text, size_t *out_len) {
    int len = GetWindowTextLengthA(hwnd);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) return 0;
    GetWindowTextA(hwnd, buf, len + 1);
    *out_text = buf;
    *out_len = (size_t)len;
    return 1;
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

static int file_equals_path(const char *left_path, const char *right_path) {
    uint8_t *left = NULL;
    uint8_t *right = NULL;
    size_t left_len = 0, right_len = 0;
    int ok = 0;
    if (!read_file(left_path, &left, &left_len) || !read_file(right_path, &right, &right_len)) goto done;
    ok = left_len == right_len && (!left_len || memcmp(left, right, left_len) == 0);
done:
    free(left);
    free(right);
    return ok;
}

static int file_dialog_open(HWND owner, char *out_path, size_t out_cap, const char *filter) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(out_path, out_cap);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = out_path;
    ofn.nMaxFile = (DWORD)out_cap;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameA(&ofn);
}

static int file_dialog_save(HWND owner, char *out_path, size_t out_cap, const char *filter) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(out_path, out_cap);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = out_path;
    ofn.nMaxFile = (DWORD)out_cap;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    return GetSaveFileNameA(&ofn);
}

static int has_suffix(const char *path, const char *suffix) {
    size_t lp = strlen(path), ls = strlen(suffix);
    if (lp < ls) return 0;
    return _stricmp(path + lp - ls, suffix) == 0;
}

static void suggest_output_from_input(const char *input_path, char *out_path, size_t out_cap, const char *suffix) {
    const char *base = input_path;
    const char *slash = strrchr(input_path, '\\');
    const char *fslash = strrchr(input_path, '/');
    if (slash && fslash) base = slash > fslash ? slash + 1 : fslash + 1;
    else if (slash) base = slash + 1;
    else if (fslash) base = fslash + 1;
    if (!*base) {
        snprintf(out_path, out_cap, "mosaic-output%s", suffix);
        return;
    }
    snprintf(out_path, out_cap, "%.*s%s", (int)(strcspn(base, ".")), base, suffix);
}

static void set_output_for_input(AppState *state, const char *input_path, const char *suffix) {
    if (!state || !input_path || !*input_path) return;
    suggest_output_from_input(input_path, state->output_path, sizeof(state->output_path), suffix);
    set_text(state->output_edit, state->output_path);
}

typedef struct MosaicArchiveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t algorithm;
    uint32_t level;
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum32;
} MosaicArchiveHeader;

static DWORD algorithm_from_selection(HWND combo) {
    LRESULT idx = SendMessageA(combo, CB_GETCURSEL, 0, 0);
    switch ((int)idx) {
    case 0: return COMPRESS_ALGORITHM_XPRESS_HUFF;
    case 1: return COMPRESS_ALGORITHM_XPRESS;
    case 2: return COMPRESS_ALGORITHM_MSZIP;
    case 3: return COMPRESS_ALGORITHM_LZMS;
    default: return COMPRESS_ALGORITHM_XPRESS_HUFF;
    }
}

static const char *algorithm_name(DWORD algorithm) {
    switch (algorithm) {
    case COMPRESS_ALGORITHM_XPRESS_HUFF: return "XPRESS_HUFF";
    case COMPRESS_ALGORITHM_XPRESS: return "XPRESS";
    case COMPRESS_ALGORITHM_MSZIP: return "MSZIP";
    case COMPRESS_ALGORITHM_LZMS: return "LZMS";
    default: return "UNKNOWN";
    }
}

static int algorithm_supported(DWORD algorithm) {
    return algorithm == COMPRESS_ALGORITHM_XPRESS_HUFF ||
           algorithm == COMPRESS_ALGORITHM_XPRESS ||
           algorithm == COMPRESS_ALGORITHM_MSZIP ||
           algorithm == COMPRESS_ALGORITHM_LZMS;
}

static DWORD level_from_track(HWND track) {
    return (DWORD)SendMessageA(track, TBM_GETPOS, 0, 0);
}

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

static DWORD compress_buffer(DWORD algorithm, const uint8_t *input, size_t input_len, uint8_t **out_bytes, size_t *out_len) {
    COMPRESSOR_HANDLE handle = NULL;
    if (!CreateCompressor(algorithm, NULL, &handle)) return GetLastError();

    size_t cap = input_len + (input_len / 16u) + 1024u;
    if (cap < 4096u) cap = 4096u;
    uint8_t *buf = NULL;
    DWORD err = ERROR_SUCCESS;

    for (;;) {
        buf = (uint8_t *)malloc(cap);
        if (!buf) { err = ERROR_OUTOFMEMORY; break; }
        SIZE_T written = 0;
        if (Compress(handle, input, (SIZE_T)input_len, buf, (SIZE_T)cap, &written)) {
            *out_bytes = buf;
            *out_len = (size_t)written;
            CloseCompressor(handle);
            return ERROR_SUCCESS;
        }
        err = GetLastError();
        free(buf);
        buf = NULL;
        if (err != ERROR_INSUFFICIENT_BUFFER) break;
        cap *= 2u;
        if (cap < input_len) { err = ERROR_BUFFER_OVERFLOW; break; }
    }

    CloseCompressor(handle);
    return err;
}

static DWORD decompress_buffer(DWORD algorithm, const uint8_t *input, size_t input_len, size_t output_len, uint8_t **out_bytes) {
    DECOMPRESSOR_HANDLE handle = NULL;
    if (!CreateDecompressor(algorithm, NULL, &handle)) return GetLastError();
    uint8_t *buf = (uint8_t *)malloc(output_len ? output_len : 1u);
    if (!buf) { CloseDecompressor(handle); return ERROR_OUTOFMEMORY; }
    SIZE_T written = 0;
    if (!Decompress(handle, input, (SIZE_T)input_len, buf, (SIZE_T)output_len, &written)) {
        DWORD err = GetLastError();
        free(buf);
        CloseDecompressor(handle);
        return err;
    }
    CloseDecompressor(handle);
    *out_bytes = buf;
    return ERROR_SUCCESS;
}

static int archive_write_roundtrip(const ArchiveConfig *cfg, char *error_buf, size_t error_cap);
static int utf8_from_wide(const wchar_t *input, char **out_text) {
    int need = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (need <= 0) return 0;
    *out_text = (char *)malloc((size_t)need);
    if (!*out_text) return 0;
    if (!WideCharToMultiByte(CP_UTF8, 0, input, -1, *out_text, need, NULL, NULL)) {
        free(*out_text);
        *out_text = NULL;
        return 0;
    }
    return 1;
}

static int run_self_test(int argc, char **argv) {
    ArchiveConfig cfg;
    char errbuf[128];
    if (argc != 5) {
        fprintf(stderr, "usage: mosaic-desktop --self-test INPUT ARCHIVE OUTPUT\n");
        return 2;
    }
    cfg.algorithm = COMPRESS_ALGORITHM_XPRESS_HUFF;
    cfg.level = 3;
    cfg.input_path = argv[2];
    cfg.archive_path = argv[3];
    cfg.output_path = argv[4];
    cfg.roundtrip = 1;
    if (!archive_write_roundtrip(&cfg, errbuf, sizeof(errbuf))) {
        fprintf(stderr, "self-test failed: %s\n", errbuf);
        return 3;
    }
    fprintf(stdout, "OK self-test roundtrip=%s archive=%s output=%s\n", cfg.input_path, cfg.archive_path, cfg.output_path);
    return 0;
}

static int archive_write_roundtrip(const ArchiveConfig *cfg, char *error_buf, size_t error_cap) {
    uint8_t *input = NULL, *compressed = NULL, *payload = NULL, *output = NULL;
    size_t input_len = 0, compressed_len = 0;
    MosaicArchiveHeader header;
    DWORD err = ERROR_SUCCESS;
    if (!cfg || !cfg->input_path || !cfg->archive_path || !cfg->output_path) return 0;
    if (!read_file(cfg->input_path, &input, &input_len)) {
        snprintf(error_buf, error_cap, "read input failed");
        return 0;
    }
    err = compress_buffer(cfg->algorithm, input, input_len, &compressed, &compressed_len);
    if (err != ERROR_SUCCESS) {
        snprintf(error_buf, error_cap, "compress failed: %lu", (unsigned long)err);
        goto fail;
    }
    header.magic = 0x31434D5A;
    header.version = 1;
    header.algorithm = cfg->algorithm;
    header.level = cfg->level;
    header.original_size = (uint64_t)input_len;
    header.compressed_size = (uint64_t)compressed_len;
    header.checksum32 = crc32_bytes(input, input_len);
    payload = (uint8_t *)malloc(sizeof(header) + compressed_len);
    if (!payload) {
        snprintf(error_buf, error_cap, "out of memory");
        goto fail;
    }
    memcpy(payload, &header, sizeof(header));
    memcpy(payload + sizeof(header), compressed, compressed_len);
    if (!write_file(cfg->archive_path, payload, sizeof(header) + compressed_len)) {
        snprintf(error_buf, error_cap, "write archive failed");
        goto fail;
    }
    if (!decompress_buffer(header.algorithm, payload + sizeof(header), compressed_len, (size_t)header.original_size, &output)) {
        snprintf(error_buf, error_cap, "decompress failed");
        goto fail;
    }
    if (!write_file(cfg->output_path, output, (size_t)header.original_size)) {
        snprintf(error_buf, error_cap, "write output failed");
        goto fail;
    }
    if (!file_equals_path(cfg->input_path, cfg->output_path)) {
        snprintf(error_buf, error_cap, "roundtrip mismatch");
        goto fail;
    }
    free(input);
    free(compressed);
    free(payload);
    free(output);
    return 1;
fail:
    free(input);
    free(compressed);
    free(payload);
    free(output);
    return 0;
}

static void compress_selected(AppState *state) {
    char *input_path = NULL;
    char *output_path = NULL;
    size_t in_len = 0, out_len = 0;
    uint8_t *input = NULL, *compressed = NULL, *payload = NULL;
    MosaicArchiveHeader header;
    DWORD err = ERROR_SUCCESS;
    if (!get_window_text_alloc(state->input_edit, &input_path, &in_len) || !get_window_text_alloc(state->output_edit, &output_path, &out_len)) {
        log_append(state->log_edit, "Read paths failed.");
        goto done;
    }
    if (input_path[0] == '\0' || output_path[0] == '\0') { log_append(state->log_edit, "Choose both input and output paths."); goto done; }
    if (!has_suffix(output_path, ".mzc")) {
        log_append(state->log_edit, "Output archive should use .mzc.");
        goto done;
    }
    if (!read_file(input_path, &input, &in_len)) { log_append(state->log_edit, "Could not read input file."); goto done; }
    state->algorithm = algorithm_from_selection(state->algo_combo);
    state->level = level_from_track(state->level_track);
    err = compress_buffer(state->algorithm, input, in_len, &compressed, &out_len);
    if (err != ERROR_SUCCESS) {
        char line[128];
        _snprintf(line, sizeof(line), "Compression failed: %lu", (unsigned long)err);
        log_append(state->log_edit, line);
        goto done;
    }
    header.magic = 0x31434D5A; /* ZMC1 little-endian */
    header.version = 1;
    header.algorithm = state->algorithm;
    header.level = state->level;
    header.original_size = (uint64_t)in_len;
    header.compressed_size = (uint64_t)out_len;
    payload = (uint8_t *)malloc(sizeof(header) + out_len);
    if (!payload) { log_append(state->log_edit, "Out of memory."); goto done; }
    memcpy(payload, &header, sizeof(header));
    memcpy(payload + sizeof(header), compressed, out_len);
    if (!write_file(output_path, payload, sizeof(header) + out_len)) {
        log_append(state->log_edit, "Could not write compressed file.");
        goto done;
    }
    {
        char line[256];
        _snprintf(line, sizeof(line), "Compressed %zu bytes -> %zu bytes using %s level %lu", in_len, out_len + sizeof(header), algorithm_name(state->algorithm), (unsigned long)state->level);
        log_append(state->log_edit, line);
    }
done:
    free(payload);
    free(compressed);
    free(input);
    free(output_path);
    free(input_path);
}

static void decompress_selected(AppState *state) {
    char *input_path = NULL;
    char *output_path = NULL;
    size_t in_len = 0, out_len = 0;
    uint8_t *input = NULL, *output = NULL;
    MosaicArchiveHeader header;
    if (!get_window_text_alloc(state->input_edit, &input_path, &in_len) || !get_window_text_alloc(state->output_edit, &output_path, &out_len)) {
        log_append(state->log_edit, "Read paths failed.");
        goto done;
    }
    if (!read_file(input_path, &input, &in_len) || in_len < sizeof(header)) {
        log_append(state->log_edit, "Could not read archive.");
        goto done;
    }
    memcpy(&header, input, sizeof(header));
    if (header.magic != 0x31434D5A || header.version != 1 || header.compressed_size + sizeof(header) != in_len || header.original_size == 0 || !algorithm_supported(header.algorithm)) {
        log_append(state->log_edit, "Not a Mosaic archive.");
        goto done;
    }
    DWORD err = decompress_buffer(header.algorithm, input + sizeof(header), (size_t)header.compressed_size, (size_t)header.original_size, &output);
    if (err != ERROR_SUCCESS) {
        char line[128];
        _snprintf(line, sizeof(line), "Decompression failed: %lu", (unsigned long)err);
        log_append(state->log_edit, line);
        goto done;
    }
    if (crc32_bytes(output, (size_t)header.original_size) != header.checksum32) {
        log_append(state->log_edit, "Archive integrity check failed.");
        goto done;
    }
    if (!write_file(output_path, output, (size_t)header.original_size)) {
        log_append(state->log_edit, "Could not write decompressed file.");
        goto done;
    }
    {
        char line[256];
        _snprintf(line, sizeof(line), "Decompressed %zu bytes from %s level %lu", (size_t)header.original_size, algorithm_name(header.algorithm), (unsigned long)header.level);
        log_append(state->log_edit, line);
    }
done:
    free(output);
    free(input);
    free(output_path);
    free(input_path);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    AppState *state = (AppState *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)lparam;
        state = (AppState *)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hwnd = hwnd;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        CreateWindowA("STATIC", "Input file or archive", WS_CHILD | WS_VISIBLE, 12, 12, 150, 20, hwnd, NULL, NULL, NULL);
        state->input_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 12, 32, 460, 24, hwnd, (HMENU)ID_INPUT_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 480, 32, 80, 24, hwnd, (HMENU)ID_LOAD_INPUT, NULL, NULL);
        CreateWindowA("STATIC", "Output file", WS_CHILD | WS_VISIBLE, 12, 64, 150, 20, hwnd, NULL, NULL, NULL);
        state->output_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 12, 84, 460, 24, hwnd, (HMENU)ID_OUTPUT_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Save as", WS_CHILD | WS_VISIBLE, 480, 84, 80, 24, hwnd, (HMENU)ID_CHOOSE_OUTPUT, NULL, NULL);
        CreateWindowA("STATIC", "Algorithm", WS_CHILD | WS_VISIBLE, 12, 118, 80, 18, hwnd, NULL, NULL, NULL);
        state->algo_combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 12, 136, 160, 160, hwnd, (HMENU)ID_ALGO_COMBO, NULL, NULL);
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"XPRESS_HUFF");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"XPRESS");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"MSZIP");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"LZMS");
        SendMessageA(state->algo_combo, CB_SETCURSEL, 0, 0);
        CreateWindowA("STATIC", "Level", WS_CHILD | WS_VISIBLE, 190, 118, 80, 18, hwnd, NULL, NULL, NULL);
        state->level_track = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 190, 136, 180, 30, hwnd, (HMENU)ID_LEVEL_TRACK, NULL, NULL);
        SendMessageA(state->level_track, TBM_SETRANGE, TRUE, MAKELPARAM(0, 5));
        SendMessageA(state->level_track, TBM_SETPOS, TRUE, 3);
        CreateWindowA("BUTTON", "Compress", WS_CHILD | WS_VISIBLE, 12, 176, 100, 28, hwnd, (HMENU)ID_COMPRESS, NULL, NULL);
        CreateWindowA("BUTTON", "Decompress", WS_CHILD | WS_VISIBLE, 120, 176, 100, 28, hwnd, (HMENU)ID_DECOMPRESS, NULL, NULL);
        CreateWindowA("BUTTON", "Clear log", WS_CHILD | WS_VISIBLE, 228, 176, 100, 28, hwnd, (HMENU)ID_CLEAR, NULL, NULL);
        state->status = CreateWindowA("STATIC", "Ready", WS_CHILD | WS_VISIBLE, 12, 212, 548, 18, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        state->log_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 12, 236, 548, 172, hwnd, (HMENU)ID_LOG_EDIT, NULL, NULL);
        SendMessageA(state->input_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->output_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->algo_combo, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->status, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->log_edit, WM_SETFONT, (WPARAM)font, TRUE);
        DragAcceptFiles(hwnd, TRUE);
        log_append(state->log_edit, "Mosaic Desktop ready.");
        log_append(state->log_edit, "Uses Windows Compression API for file compression.");
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_LOAD_INPUT: {
            char path[MAX_PATH];
            const char *filter = "All Files\0*.*\0";
            if (file_dialog_open(hwnd, path, sizeof(path), filter)) {
                set_text(state->input_edit, path);
                set_output_for_input(state, path, ".mzc");
                log_append(state->log_edit, "Input selected.");
                set_status(state, "Input loaded");
            }
            return 0;
        }
        case ID_CHOOSE_OUTPUT: {
            char path[MAX_PATH];
            const char *filter = "Mosaic Archive\0*.mzc\0All Files\0*.*\0";
            if (file_dialog_save(hwnd, path, sizeof(path), filter)) {
                set_text(state->output_edit, path);
                strncpy(state->output_path, path, sizeof(state->output_path) - 1);
                log_append(state->log_edit, "Output selected.");
            }
            return 0;
        }
        case ID_COMPRESS: compress_selected(state); return 0;
        case ID_DECOMPRESS: decompress_selected(state); return 0;
        case ID_CLEAR: SetWindowTextA(state->log_edit, ""); set_status(state, "Ready"); return 0;
        }
        break;
    case WM_HSCROLL:
        if ((HWND)lparam == state->level_track) {
            char lvl[16];
            _snprintf(lvl, sizeof(lvl), "Level %lu", (unsigned long)level_from_track(state->level_track));
            set_status(state, lvl);
        }
        break;
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wparam;
        UINT count = DragQueryFileA(drop, 0xFFFFFFFF, NULL, 0);
        if (count > 0) {
            char path[MAX_PATH];
            DragQueryFileA(drop, 0, path, MAX_PATH);
            set_text(state->input_edit, path);
            log_append(state->log_edit, "Input dropped.");
            set_status(state, "Input loaded");
        }
        DragFinish(drop);
        return 0;
    }
    case WM_DESTROY:
        free(state);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show) {
    LPWSTR *argvw = NULL;
    int argc = 0;
    int rc = 0;
    int i;
    (void)prev;
    (void)cmdline;
    argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvw && argc >= 2 && wcscmp(argvw[1], L"--self-test") == 0) {
        char **argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
        if (argv) {
            for (i = 0; i < argc; ++i) {
                if (!utf8_from_wide(argvw[i], &argv[i])) {
                    rc = 4;
                    break;
                }
            }
            if (i == argc) rc = run_self_test(argc, argv);
            for (i = 0; i < argc; ++i) free(argv[i]);
            free(argv);
        } else {
            rc = 4;
        }
        LocalFree(argvw);
        return rc;
    }
    if (argvw) LocalFree(argvw);
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MosaicDesktopWindow";
    RegisterClassA(&wc);

    AppState *state = (AppState *)calloc(1, sizeof(AppState));
    HWND hwnd = CreateWindowA("MosaicDesktopWindow", "Mosaic Compressor", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 590, 470, NULL, NULL, instance, state);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
