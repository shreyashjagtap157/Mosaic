#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <compressapi.h>
#include <uxtheme.h>
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
#define ID_SOURCE_KIND 1012
#define ID_VERIFY_CHECK 1013
#define ID_MODE_COMBO 1015

#define APP_BG RGB(247, 249, 252)
#define CARD_BG RGB(255, 255, 255)
#define TEXT_PRIMARY RGB(25, 32, 44)
#define TEXT_SECONDARY RGB(92, 101, 118)
#define BORDER_COLOR RGB(216, 223, 233)
#define ACCENT_COLOR RGB(38, 99, 235)
#define LOG_BG RGB(248, 250, 252)
#define LOG_FG RGB(17, 24, 39)

typedef enum SourceKind {
    SOURCE_KIND_FILE = 0,
    SOURCE_KIND_FOLDER = 1
} SourceKind;

typedef enum ArchiveMode {
    ARCHIVE_MODE_ADD_REPLACE = 0,
    ARCHIVE_MODE_ADD_SKIP = 1,
    ARCHIVE_MODE_UPDATE_NEWER = 2
} ArchiveMode;

typedef struct AppState {
    HWND hwnd;
    HWND input_edit;
    HWND output_edit;
    HWND source_combo;
    HWND mode_combo;
    HWND algo_combo;
    HWND level_track;
    HWND verify_check;
    HWND safety_note;
    HWND status;
    HWND log_edit;
    DWORD algorithm;
    DWORD level;
    SourceKind source_kind;
    ArchiveMode archive_mode;
    int verify_after;
    char input_path[MAX_PATH];
    char output_path[MAX_PATH];
} AppState;

typedef struct UiState {
    HFONT title_font;
    HFONT header_font;
    HFONT body_font;
    HBRUSH bg_brush;
    HBRUSH card_brush;
    HBRUSH log_brush;
    HBRUSH edit_brush;
    HBRUSH border_brush;
    COLORREF text_color;
    COLORREF secondary_color;
} UiState;

static UiState g_ui;

typedef struct ArchiveConfig {
    DWORD algorithm;
    DWORD level;
    const char *input_path;
    const char *archive_path;
    const char *output_path;
    int roundtrip;
} ArchiveConfig;

typedef struct MosaicEntry {
    int is_dir;
    char path[MAX_PATH * 2];
    uint8_t *bytes;
    size_t size;
} MosaicEntry;

typedef struct MosaicArchiveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t algorithm;
    uint32_t level;
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t checksum32;
} MosaicArchiveHeader;

static int append_entry(MosaicEntry **entries, size_t *count, size_t *cap, const MosaicEntry *entry);
static int collect_folder_entries(const char *root, const char *rel, MosaicEntry **entries, size_t *count, size_t *cap);
static int serialize_entries(const MosaicEntry *entries, size_t count, const char *root_name, uint8_t **out, size_t *out_len);
static DWORD compress_buffer(DWORD algorithm, const uint8_t *input, size_t input_len, uint8_t **out_bytes, size_t *out_len);
static int decompress_blob_exact(DWORD algorithm, const uint8_t *input, size_t input_len, size_t output_len, uint8_t **out_bytes, DWORD *error_out);
static uint32_t crc32_bytes(const uint8_t *data, size_t len);
static int utf8_from_wide(const wchar_t *input, char **out_text);
static int write_archive_v2(const char *source_path, int is_folder, DWORD algorithm, uint8_t **archive_blob, size_t *archive_blob_len, size_t *serialized_len_out, char *error_buf, size_t error_cap);
static int parse_archive_v2(const uint8_t *blob, size_t blob_len, char *root_name, size_t root_cap, MosaicEntry **entries, size_t *entry_count, char *error_buf, size_t error_cap);
static int restore_archive_entries(const char *output_path, MosaicEntry *entries, size_t count);

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

static void set_input_path(AppState *state, const char *path) {
    if (!state) return;
    strncpy(state->input_path, path ? path : "", sizeof(state->input_path) - 1);
    state->input_path[sizeof(state->input_path) - 1] = '\0';
    set_text(state->input_edit, state->input_path);
}

static void set_output_path(AppState *state, const char *path) {
    if (!state) return;
    strncpy(state->output_path, path ? path : "", sizeof(state->output_path) - 1);
    state->output_path[sizeof(state->output_path) - 1] = '\0';
    set_text(state->output_edit, state->output_path);
}

static int utf8_to_wide_alloc(const char *input, wchar_t **out_text) {
    int need;
    if (!input || !out_text) return 0;
    need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, NULL, 0);
    if (need <= 0) return 0;
    *out_text = (wchar_t *)malloc((size_t)need * sizeof(wchar_t));
    if (!*out_text) return 0;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, *out_text, need)) {
        free(*out_text);
        *out_text = NULL;
        return 0;
    }
    return 1;
}

static void ui_init(HWND hwnd) {
    if (g_ui.body_font) return;
    NONCLIENTMETRICSA ncm;
    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    strncpy(ncm.lfMessageFont.lfFaceName, "Segoe UI", LF_FACESIZE - 1);
    ncm.lfMessageFont.lfFaceName[LF_FACESIZE - 1] = '\0';
    ncm.lfMessageFont.lfHeight = -16;
    g_ui.body_font = CreateFontIndirectA(&ncm.lfMessageFont);
    LOGFONTA lf = ncm.lfMessageFont;
    lf.lfHeight = -26;
    lf.lfWeight = FW_SEMIBOLD;
    g_ui.title_font = CreateFontIndirectA(&lf);
    lf.lfHeight = -18;
    lf.lfWeight = FW_BOLD;
    g_ui.header_font = CreateFontIndirectA(&lf);
    g_ui.bg_brush = CreateSolidBrush(APP_BG);
    g_ui.card_brush = CreateSolidBrush(CARD_BG);
    g_ui.log_brush = CreateSolidBrush(LOG_BG);
    g_ui.edit_brush = CreateSolidBrush(RGB(255, 255, 255));
    g_ui.border_brush = CreateSolidBrush(BORDER_COLOR);
    g_ui.text_color = TEXT_PRIMARY;
    g_ui.secondary_color = TEXT_SECONDARY;
    (void)hwnd;
}

static void ui_apply_fonts(HWND control, HFONT font) {
    if (control && font) SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static void ui_theme_common(HWND control) {
    if (!control) return;
    SetWindowTheme(control, L"Explorer", NULL);
}

static void draw_card(HDC hdc, const RECT *rc);
static void draw_section_label(HDC hdc, int x, int y, const char *text);

static void ui_layout(HWND hwnd, AppState *state) {
    RECT rc;
    int margin = 24;
    int content_w;
    int x_left, x_right, card_w, card_h;
    int y = 108;
    GetClientRect(hwnd, &rc);
    content_w = rc.right - rc.left;
    card_w = (content_w - margin * 3) / 2;
    if (card_w < 280) card_w = content_w - margin * 2;
    x_left = margin;
    x_right = x_left + card_w + margin;
    card_h = 114;
    MoveWindow(state->input_edit, x_left, y + 36, card_w - 104, 28, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_LOAD_INPUT), x_left + card_w - 92, y + 36, 92, 28, TRUE);
    MoveWindow(state->output_edit, x_left, y + card_h + 36, card_w - 104, 28, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_CHOOSE_OUTPUT), x_left + card_w - 92, y + card_h + 36, 92, 28, TRUE);
    MoveWindow(state->source_combo, x_left, y + 290, 150, 28, TRUE);
    MoveWindow(state->mode_combo, x_left + 166, y + 290, card_w - 166, 28, TRUE);
    MoveWindow(state->algo_combo, x_right, y + 36, card_w - 20, 28, TRUE);
    MoveWindow(state->level_track, x_right, y + 96, card_w - 20, 34, TRUE);
    MoveWindow(state->verify_check, x_right, y + 150, card_w - 20, 24, TRUE);
    MoveWindow(state->safety_note, x_right, y + 178, card_w - 20, 40, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_COMPRESS), margin, rc.bottom - 124, 120, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_DECOMPRESS), margin + 132, rc.bottom - 124, 120, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_CLEAR), margin + 264, rc.bottom - 124, 120, 34, TRUE);
    MoveWindow(state->status, margin, rc.bottom - 84, content_w - margin * 2, 22, TRUE);
    MoveWindow(state->log_edit, margin, rc.bottom - 58, content_w - margin * 2, 38, TRUE);
}

static void paint_ui(HWND hwnd, HDC hdc) {
    RECT rc;
    RECT left_card = {24, 100, 24 + 520, 100 + 334};
    RECT right_card = {24 + 536, 100, 24 + 536 + 520, 100 + 334};
    char title[] = "Mosaic Compressor";
    char subtitle[] = "Modern desktop compression workspace";
    char hero[] = "Create, test, and extract archives with a cleaner Windows-native workflow.";
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, g_ui.bg_brush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_ui.text_color);
    SelectObject(hdc, g_ui.title_font);
    rc.left = 24; rc.top = 24; rc.right = 700; rc.bottom = 64;
    DrawTextA(hdc, title, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, g_ui.header_font);
    SetTextColor(hdc, g_ui.secondary_color);
    rc.top = 64; rc.bottom = 90;
    DrawTextA(hdc, subtitle, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, g_ui.body_font);
    rc.top = 86; rc.bottom = 108;
    DrawTextA(hdc, hero, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    draw_card(hdc, &left_card);
    draw_card(hdc, &right_card);
    draw_section_label(hdc, 44, 116, "Archive source");
    draw_section_label(hdc, 44, 196, "Destination");
    draw_section_label(hdc, 44, 282, "Workflow");
    draw_section_label(hdc, 580, 116, "Compression profile");
    draw_section_label(hdc, 580, 176, "Compression level");
    draw_section_label(hdc, 580, 230, "Safety note");
}

static void draw_card(HDC hdc, const RECT *rc) {
    FillRect(hdc, rc, g_ui.card_brush);
    FrameRect(hdc, rc, g_ui.border_brush);
}

static void draw_section_label(HDC hdc, int x, int y, const char *text) {
    RECT rc = { x, y, x + 1000, y + 22 };
    SetTextColor(hdc, g_ui.secondary_color);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_ui.header_font);
    DrawTextA(hdc, text, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
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
    FILE *f = NULL;
    wchar_t *wide = NULL;
    if (!utf8_to_wide_alloc(path, &wide)) return 0;
    f = _wfopen(wide, L"rb");
    free(wide);
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
    FILE *f = NULL;
    wchar_t *wide = NULL;
    if (!utf8_to_wide_alloc(path, &wide)) return 0;
    f = _wfopen(wide, L"wb");
    free(wide);
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

static int modern_file_dialog(HWND owner, char *out_path, size_t out_cap, int save_mode, int folder_mode) {
    HRESULT hr;
    IFileDialog *dlg = NULL;
    COMDLG_FILTERSPEC spec[] = {
        {L"Mosaic Archive", L"*.mzc"},
        {L"All Files", L"*.*"},
    };
    DWORD opts = 0;
    ZeroMemory(out_path, out_cap);
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0;
    hr = folder_mode ? CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dlg)
                     : (save_mode ? CoCreateInstance(&CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dlg)
                                  : CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dlg));
    if (FAILED(hr) || !dlg) {
        if (hr == S_OK) CoUninitialize();
        return 0;
    }
    dlg->lpVtbl->SetTitle(dlg, folder_mode ? L"Select a source folder" : (save_mode ? L"Select archive output" : L"Select a source file"));
    dlg->lpVtbl->SetOkButtonLabel(dlg, L"Select");
    if (save_mode) {
        dlg->lpVtbl->SetFileTypes(dlg, (UINT)(sizeof(spec) / sizeof(spec[0])), spec);
    }
    if (folder_mode) {
        dlg->lpVtbl->GetOptions(dlg, &opts);
        dlg->lpVtbl->SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOVALIDATE);
    } else if (save_mode) {
        dlg->lpVtbl->GetOptions(dlg, &opts);
        dlg->lpVtbl->SetOptions(dlg, opts | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);
    } else {
        dlg->lpVtbl->GetOptions(dlg, &opts);
        dlg->lpVtbl->SetOptions(dlg, opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    }
    hr = dlg->lpVtbl->Show(dlg, owner);
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        hr = dlg->lpVtbl->GetResult(dlg, &item);
        if (SUCCEEDED(hr) && item) {
            PWSTR wide = NULL;
            hr = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &wide);
            if (SUCCEEDED(hr) && wide) {
                char *utf8 = NULL;
                if (utf8_from_wide(wide, &utf8)) {
                    size_t len = strlen(utf8);
                    if (len < out_cap) {
                        memcpy(out_path, utf8, len + 1);
                        free(utf8);
                        CoTaskMemFree(wide);
                        item->lpVtbl->Release(item);
                        dlg->lpVtbl->Release(dlg);
                        CoUninitialize();
                        return 1;
                    }
                    free(utf8);
                }
                CoTaskMemFree(wide);
            }
            item->lpVtbl->Release(item);
        }
    }
    dlg->lpVtbl->Release(dlg);
    CoUninitialize();
    return 0;
}

static int folder_dialog_open(HWND owner, char *out_path, size_t out_cap) {
    return modern_file_dialog(owner, out_path, out_cap, 0, 1);
}

static int file_dialog_save(HWND owner, char *out_path, size_t out_cap, const char *filter) {
    (void)filter;
    return modern_file_dialog(owner, out_path, out_cap, 1, 0);
}

static int file_dialog_open(HWND owner, char *out_path, size_t out_cap, const char *filter) {
    (void)filter;
    return modern_file_dialog(owner, out_path, out_cap, 0, 0);
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

static int path_exists_dir(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int path_exists_file(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int get_file_write_time(const char *path, FILETIME *out_time) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0;
    *out_time = data.ftLastWriteTime;
    return 1;
}

static int filetime_compare(const FILETIME *left, const FILETIME *right) {
    if (left->dwHighDateTime != right->dwHighDateTime) return left->dwHighDateTime < right->dwHighDateTime ? -1 : 1;
    if (left->dwLowDateTime != right->dwLowDateTime) return left->dwLowDateTime < right->dwLowDateTime ? -1 : 1;
    return 0;
}

static void basename_from_path(const char *path, char *out, size_t cap) {
    const char *base = path;
    const char *slash = strrchr(path, '\\');
    const char *fslash = strrchr(path, '/');
    if (slash && fslash) base = slash > fslash ? slash + 1 : fslash + 1;
    else if (slash) base = slash + 1;
    else if (fslash) base = fslash + 1;
    strncpy(out, base, cap - 1);
    out[cap - 1] = '\0';
}

static int ensure_parent_dirs(const char *path) {
    char buf[MAX_PATH * 2];
    size_t i;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (i = 3; buf[i] != '\0'; ++i) {
        if (buf[i] == '\\' || buf[i] == '/') {
            char save = buf[i];
            buf[i] = '\0';
            CreateDirectoryA(buf, NULL);
            buf[i] = save;
        }
    }
    return 1;
}

static int append_entry(MosaicEntry **entries, size_t *count, size_t *cap, const MosaicEntry *entry) {
    if (*count >= *cap) {
        size_t next = (*cap == 0) ? 16u : (*cap * 2u);
        MosaicEntry *grown = (MosaicEntry *)realloc(*entries, next * sizeof(MosaicEntry));
        if (!grown) return 0;
        *entries = grown;
        *cap = next;
    }
    (*entries)[(*count)++] = *entry;
    return 1;
}

static int collect_folder_entries(const char *root, const char *rel, MosaicEntry **entries, size_t *count, size_t *cap) {
    char search[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (rel && *rel) snprintf(search, sizeof(search), "%s\\%s\\*", root, rel);
    else snprintf(search, sizeof(search), "%s\\*", root);
    h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        MosaicEntry entry;
        char child_rel[MAX_PATH * 2];
        char child_abs[MAX_PATH * 2];
        ZeroMemory(&entry, sizeof(entry));
        if (rel && *rel) snprintf(child_rel, sizeof(child_rel), "%s\\%s", rel, fd.cFileName);
        else snprintf(child_rel, sizeof(child_rel), "%s", fd.cFileName);
        if (rel && *rel) snprintf(child_abs, sizeof(child_abs), "%s\\%s\\%s", root, rel, fd.cFileName);
        else snprintf(child_abs, sizeof(child_abs), "%s\\%s", root, fd.cFileName);
        entry.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        strncpy(entry.path, child_rel, sizeof(entry.path) - 1);
        if (entry.is_dir) {
            if (!append_entry(entries, count, cap, &entry)) { FindClose(h); return 0; }
            if (!collect_folder_entries(root, child_rel, entries, count, cap)) { FindClose(h); return 0; }
        } else {
            if (!read_file(child_abs, &entry.bytes, &entry.size)) { FindClose(h); return 0; }
            if (!append_entry(entries, count, cap, &entry)) { free(entry.bytes); FindClose(h); return 0; }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 1;
}

static int write_u32(uint8_t **buf, size_t *len, size_t *cap, uint32_t v) {
    if (*len + sizeof(v) > *cap) {
        size_t next = (*cap == 0) ? 1024u : (*cap * 2u);
        while (*len + sizeof(v) > next) next *= 2u;
        uint8_t *grown = (uint8_t *)realloc(*buf, next);
        if (!grown) return 0;
        *buf = grown;
        *cap = next;
    }
    memcpy(*buf + *len, &v, sizeof(v));
    *len += sizeof(v);
    return 1;
}

static int write_u64(uint8_t **buf, size_t *len, size_t *cap, uint64_t v) {
    return write_u32(buf, len, cap, (uint32_t)(v & 0xFFFFFFFFu)) &&
           write_u32(buf, len, cap, (uint32_t)(v >> 32));
}

static int write_bytes(uint8_t **buf, size_t *len, size_t *cap, const void *src, size_t src_len) {
    if (*len + src_len > *cap) {
        size_t next = (*cap == 0) ? 1024u : (*cap * 2u);
        while (*len + src_len > next) next *= 2u;
        uint8_t *grown = (uint8_t *)realloc(*buf, next);
        if (!grown) return 0;
        *buf = grown;
        *cap = next;
    }
    memcpy(*buf + *len, src, src_len);
    *len += src_len;
    return 1;
}

static int serialize_entries(const MosaicEntry *entries, size_t count, const char *root_name, uint8_t **out, size_t *out_len) {
    uint8_t *buf = NULL;
    size_t len = 0, cap = 0;
    size_t i;
    size_t root_len = root_name ? strlen(root_name) : 0;
    if (!write_u32(&buf, &len, &cap, 0x31434D5A) || !write_u32(&buf, &len, &cap, 1)) goto fail;
    if (!write_u32(&buf, &len, &cap, (uint32_t)root_len) || !write_bytes(&buf, &len, &cap, root_name ? root_name : "", root_len)) goto fail;
    if (!write_u32(&buf, &len, &cap, (uint32_t)count)) goto fail;
    for (i = 0; i < count; ++i) {
        uint32_t path_len = (uint32_t)strlen(entries[i].path);
        if (!write_u32(&buf, &len, &cap, (uint32_t)entries[i].is_dir)) goto fail;
        if (!write_u32(&buf, &len, &cap, path_len) || !write_bytes(&buf, &len, &cap, entries[i].path, path_len)) goto fail;
        if (!write_u64(&buf, &len, &cap, (uint64_t)entries[i].size)) goto fail;
        if (entries[i].size && !write_bytes(&buf, &len, &cap, entries[i].bytes, entries[i].size)) goto fail;
    }
    *out = buf;
    *out_len = len;
    return 1;
fail:
    free(buf);
    return 0;
}

static int read_u32(const uint8_t **p, size_t *remain, uint32_t *out) {
    if (*remain < sizeof(uint32_t)) return 0;
    memcpy(out, *p, sizeof(uint32_t));
    *p += sizeof(uint32_t);
    *remain -= sizeof(uint32_t);
    return 1;
}

static int read_u64(const uint8_t **p, size_t *remain, uint64_t *out) {
    uint32_t lo, hi;
    if (!read_u32(p, remain, &lo) || !read_u32(p, remain, &hi)) return 0;
    *out = ((uint64_t)hi << 32) | lo;
    return 1;
}

static int read_bytes(const uint8_t **p, size_t *remain, void *dst, size_t dst_len) {
    if (*remain < dst_len) return 0;
    memcpy(dst, *p, dst_len);
    *p += dst_len;
    *remain -= dst_len;
    return 1;
}

static int is_safe_relative_path(const char *path) {
    const char *p = path;
    if (!path || !*path) return 0;
    if (path[0] == '\\' || path[0] == '/' || strchr(path, ':')) return 0;
    while (*p) {
        if ((p[0] == '.' && p[1] == '.' && (p[2] == '\\' || p[2] == '/' || p[2] == '\0'))) return 0;
        ++p;
    }
    return 1;
}

static int join_under_root(const char *root, const char *rel, char *out, size_t cap) {
    if (!is_safe_relative_path(rel)) return 0;
    snprintf(out, cap, "%s\\%s", root, rel);
    return 1;
}

static int write_archive_v2(const char *source_path, int is_folder, DWORD algorithm, uint8_t **archive_blob, size_t *archive_blob_len, size_t *serialized_len_out, char *error_buf, size_t error_cap) {
    MosaicEntry *entries = NULL;
    size_t entry_count = 0, entry_cap = 0;
    uint8_t *serialized = NULL;
    size_t serialized_len = 0;
    char root_name[MAX_PATH];
    if (is_folder) {
        if (!collect_folder_entries(source_path, NULL, &entries, &entry_count, &entry_cap)) { snprintf(error_buf, error_cap, "scan folder failed"); goto fail; }
        basename_from_path(source_path, root_name, sizeof(root_name));
    } else {
        MosaicEntry one;
        ZeroMemory(&one, sizeof(one));
        basename_from_path(source_path, root_name, sizeof(root_name));
        strncpy(one.path, root_name, sizeof(one.path) - 1);
        if (!read_file(source_path, &one.bytes, &one.size)) { snprintf(error_buf, error_cap, "read input failed"); goto fail; }
        if (!append_entry(&entries, &entry_count, &entry_cap, &one)) { free(one.bytes); snprintf(error_buf, error_cap, "out of memory"); goto fail; }
    }
    if (!serialize_entries(entries, entry_count, root_name, &serialized, &serialized_len)) { snprintf(error_buf, error_cap, "serialize failed"); goto fail; }
    if (compress_buffer(algorithm, serialized, serialized_len, archive_blob, archive_blob_len) != ERROR_SUCCESS) { snprintf(error_buf, error_cap, "compress failed"); goto fail; }
    if (serialized_len_out) *serialized_len_out = serialized_len;
    free(serialized);
    for (size_t i = 0; i < entry_count; ++i) free(entries[i].bytes);
    free(entries);
    return 1;
fail:
    free(serialized);
    for (size_t i = 0; i < entry_count; ++i) free(entries[i].bytes);
    free(entries);
    return 0;
}

static int parse_archive_v2(const uint8_t *blob, size_t blob_len, char *root_name, size_t root_cap, MosaicEntry **entries, size_t *entry_count, char *error_buf, size_t error_cap) {
    const uint8_t *p = blob;
    size_t remain = blob_len;
    uint32_t magic = 0, version = 0, root_len = 0, count = 0;
    if (!read_u32(&p, &remain, &magic) || !read_u32(&p, &remain, &version) || !read_u32(&p, &remain, &root_len)) goto bad;
    if (magic != 0x31434D5A || version != 1 || root_len >= root_cap || !read_bytes(&p, &remain, root_name, root_len)) goto bad;
    root_name[root_len] = '\0';
    if (!read_u32(&p, &remain, &count)) goto bad;
    *entries = (MosaicEntry *)calloc(count ? count : 1, sizeof(MosaicEntry));
    if (!*entries) goto bad;
    *entry_count = count;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t is_dir = 0, path_len = 0;
        uint64_t data_len = 0;
        if (!read_u32(&p, &remain, &is_dir) || !read_u32(&p, &remain, &path_len)) goto bad;
        if (path_len >= sizeof((*entries)[i].path) || !read_bytes(&p, &remain, (*entries)[i].path, path_len)) goto bad;
        (*entries)[i].path[path_len] = '\0';
        if (!read_u64(&p, &remain, &data_len)) goto bad;
        (*entries)[i].is_dir = (int)is_dir;
        (*entries)[i].size = (size_t)data_len;
        if (data_len) {
            (*entries)[i].bytes = (uint8_t *)malloc((size_t)data_len);
            if (!(*entries)[i].bytes || !read_bytes(&p, &remain, (*entries)[i].bytes, (size_t)data_len)) goto bad;
        }
    }
    return 1;
bad:
    snprintf(error_buf, error_cap, "parse failed");
    if (*entries) {
        for (uint32_t i = 0; i < *entry_count; ++i) free((*entries)[i].bytes);
        free(*entries);
        *entries = NULL;
    }
    return 0;
}

static int restore_archive_entries(const char *output_path, MosaicEntry *entries, size_t count) {
    char target[MAX_PATH * 2];
    size_t i;
    if (count == 1 && !entries[0].is_dir) {
        return write_file(output_path, entries[0].bytes, entries[0].size);
    }
    CreateDirectoryA(output_path, NULL);
    for (i = 0; i < count; ++i) {
        if (!join_under_root(output_path, entries[i].path, target, sizeof(target))) return 0;
        if (entries[i].is_dir) {
            CreateDirectoryA(target, NULL);
        } else {
            ensure_parent_dirs(target);
            if (!write_file(target, entries[i].bytes, entries[i].size)) return 0;
        }
    }
    return 1;
}

static int compare_files_exact(const char *left_path, const char *right_path) {
    return file_equals_path(left_path, right_path);
}

static int compare_directory_trees(const char *left_root, const char *right_root) {
    char search_left[MAX_PATH * 2];
    WIN32_FIND_DATAA left_fd;
    HANDLE left_h;
    snprintf(search_left, sizeof(search_left), "%s\\*", left_root);
    left_h = FindFirstFileA(search_left, &left_fd);
    if (left_h == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        char left_child[MAX_PATH * 2];
        char right_child[MAX_PATH * 2];
        if (strcmp(left_fd.cFileName, ".") == 0 || strcmp(left_fd.cFileName, "..") == 0) continue;
        snprintf(left_child, sizeof(left_child), "%s\\%s", left_root, left_fd.cFileName);
        snprintf(right_child, sizeof(right_child), "%s\\%s", right_root, left_fd.cFileName);
        if ((left_fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            DWORD attr = GetFileAttributesA(right_child);
            if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                FindClose(left_h);
                return 0;
            }
            if (!compare_directory_trees(left_child, right_child)) {
                FindClose(left_h);
                return 0;
            }
        } else {
            DWORD attr = GetFileAttributesA(right_child);
            if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                FindClose(left_h);
                return 0;
            }
            if (!compare_files_exact(left_child, right_child)) {
                FindClose(left_h);
                return 0;
            }
        }
    } while (FindNextFileA(left_h, &left_fd));
    FindClose(left_h);
    return 1;
}

static int verify_archive_roundtrip(const char *source_path, int is_folder, const char *archive_path, const char *output_path, DWORD algorithm, char *error_buf, size_t error_cap) {
    uint8_t *archive = NULL;
    uint8_t *payload = NULL;
    MosaicEntry *entries = NULL;
    size_t archive_len = 0, payload_len = 0, entry_count = 0;
    MosaicArchiveHeader header;
    char root_name[MAX_PATH];
    int ok = 0;
    if (!read_file(archive_path, &archive, &archive_len) || archive_len < sizeof(header)) {
        snprintf(error_buf, error_cap, "verify read failed");
        goto done;
    }
    memcpy(&header, archive, sizeof(header));
    if (header.magic != 0x31434D5A || header.version != 1 || header.compressed_size + sizeof(header) != archive_len) {
        snprintf(error_buf, error_cap, "verify archive header invalid");
        goto done;
    }
    if (crc32_bytes(archive + sizeof(header), (size_t)header.compressed_size) != header.checksum32) {
        snprintf(error_buf, error_cap, "verify archive checksum failed");
        goto done;
    }
    if (header.algorithm != algorithm) {
        snprintf(error_buf, error_cap, "verify algorithm mismatch");
        goto done;
    }
    if (!decompress_blob_exact(header.algorithm, archive + sizeof(header), (size_t)header.compressed_size, (size_t)header.original_size, &payload, NULL)) {
        snprintf(error_buf, error_cap, "verify decompress failed");
        goto done;
    }
    payload_len = (size_t)header.original_size;
    if (!parse_archive_v2(payload, payload_len, root_name, sizeof(root_name), &entries, &entry_count, error_buf, error_cap)) {
        goto done;
    }
    if (!restore_archive_entries(output_path, entries, entry_count)) {
        snprintf(error_buf, error_cap, "verify restore failed");
        goto done;
    }
    if (is_folder) {
        ok = compare_directory_trees(source_path, output_path);
    } else {
        ok = compare_files_exact(source_path, output_path);
    }
    if (!ok) snprintf(error_buf, error_cap, "verify roundtrip mismatch");
done:
    free(archive);
    free(payload);
    if (entries) {
        for (size_t i = 0; i < entry_count; ++i) free(entries[i].bytes);
        free(entries);
    }
    return ok;
}

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

static int decompress_blob_exact(DWORD algorithm, const uint8_t *input, size_t input_len, size_t output_len, uint8_t **out_bytes, DWORD *error_out) {
    DECOMPRESSOR_HANDLE handle = NULL;
    uint8_t *buf = NULL;
    SIZE_T written = 0;
    DWORD err = ERROR_SUCCESS;
    if (error_out) *error_out = ERROR_SUCCESS;
    if (!CreateDecompressor(algorithm, NULL, &handle)) {
        err = GetLastError();
        if (error_out) *error_out = err;
        return 0;
    }
    buf = (uint8_t *)malloc(output_len ? output_len : 1u);
    if (!buf) {
        CloseDecompressor(handle);
        err = ERROR_OUTOFMEMORY;
        if (error_out) *error_out = err;
        return 0;
    }
    if (!Decompress(handle, input, (SIZE_T)input_len, buf, (SIZE_T)output_len, &written)) {
        err = GetLastError();
        free(buf);
        CloseDecompressor(handle);
        if (error_out) *error_out = err;
        return 0;
    }
    CloseDecompressor(handle);
    *out_bytes = buf;
    if (error_out) *error_out = ERROR_SUCCESS;
    return 1;
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
        snprintf(error_buf, error_cap, "read input failed: %s", cfg->input_path);
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
    if (!decompress_blob_exact(header.algorithm, payload + sizeof(header), compressed_len, (size_t)header.original_size, &output, &err)) {
        snprintf(error_buf, error_cap, "decompress failed: %lu", (unsigned long)err);
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
    uint8_t *archive_blob = NULL;
    size_t archive_blob_len = 0;
    size_t serialized_len = 0;
    DWORD err = ERROR_SUCCESS;
    char errbuf[160];
    char temp_dir[MAX_PATH];
    char verify_output[MAX_PATH];
    MosaicArchiveHeader header;
    int archive_exists;
    FILETIME source_time, archive_time;
    if (!get_window_text_alloc(state->input_edit, &input_path, &in_len) || !get_window_text_alloc(state->output_edit, &output_path, &out_len)) {
        log_append(state->log_edit, "Read paths failed.");
        goto done;
    }
    if (input_path[0] == '\0' || output_path[0] == '\0') { log_append(state->log_edit, "Choose both input and output paths."); goto done; }
    if (!has_suffix(output_path, ".mzc")) {
        log_append(state->log_edit, "Output archive should use .mzc.");
        goto done;
    }
    state->algorithm = algorithm_from_selection(state->algo_combo);
    state->level = level_from_track(state->level_track);
    if (state->source_kind == SOURCE_KIND_FOLDER && !path_exists_dir(input_path)) {
        log_append(state->log_edit, "Pick a folder source first.");
        goto done;
    }
    if (state->source_kind == SOURCE_KIND_FILE && !path_exists_file(input_path)) {
        log_append(state->log_edit, "Pick a file source first.");
        goto done;
    }
    archive_exists = path_exists_file(output_path);
    if (archive_exists && state->archive_mode == ARCHIVE_MODE_ADD_SKIP) {
        log_append(state->log_edit, "Archive already exists; add-and-skip left it untouched.");
        set_status(state, "Archive skipped");
        goto done;
    }
    if (archive_exists && state->archive_mode == ARCHIVE_MODE_UPDATE_NEWER) {
        if (get_file_write_time(input_path, &source_time) && get_file_write_time(output_path, &archive_time) && filetime_compare(&archive_time, &source_time) >= 0) {
            log_append(state->log_edit, "Archive is already newer than the source; no update was needed.");
            set_status(state, "Archive up to date");
            goto done;
        }
    }
    err = write_archive_v2(input_path, state->source_kind == SOURCE_KIND_FOLDER, state->algorithm, &archive_blob, &archive_blob_len, &serialized_len, errbuf, sizeof(errbuf));
    if (err != ERROR_SUCCESS) {
        log_append(state->log_edit, errbuf[0] ? errbuf : "Compression failed.");
        goto done;
    }
    header.magic = 0x31434D5A;
    header.version = 1;
    header.algorithm = state->algorithm;
    header.level = state->level;
    header.original_size = (uint64_t)serialized_len;
    header.compressed_size = (uint64_t)archive_blob_len;
    header.checksum32 = crc32_bytes(archive_blob, archive_blob_len);
    {
        uint8_t *payload = (uint8_t *)malloc(sizeof(header) + archive_blob_len);
        if (!payload) { log_append(state->log_edit, "Out of memory."); goto done; }
        memcpy(payload, &header, sizeof(header));
        memcpy(payload + sizeof(header), archive_blob, archive_blob_len);
        if (!write_file(output_path, payload, sizeof(header) + archive_blob_len)) {
            free(payload);
            log_append(state->log_edit, "Could not write compressed file.");
            goto done;
        }
        free(payload);
    }
    if (state->verify_after) {
        DWORD temp_len = GetTempPathA(sizeof(temp_dir), temp_dir);
        if (temp_len == 0 || temp_len >= sizeof(temp_dir) || !GetTempFileNameA(temp_dir, "mzc", 0, verify_output)) {
            log_append(state->log_edit, "Could not prepare verification temp output.");
            goto done;
        }
        if (!verify_archive_roundtrip(input_path, state->source_kind == SOURCE_KIND_FOLDER, output_path, verify_output, state->algorithm, errbuf, sizeof(errbuf))) {
            log_append(state->log_edit, errbuf[0] ? errbuf : "Verification failed.");
            goto done;
        }
    }
    {
        char line[256];
        _snprintf(line, sizeof(line), "Compressed %s using %s level %lu", state->source_kind == SOURCE_KIND_FOLDER ? "folder" : "file", algorithm_name(state->algorithm), (unsigned long)state->level);
        log_append(state->log_edit, line);
        set_status(state, "Archive written");
    }
done:
    free(archive_blob);
    free(output_path);
    free(input_path);
}

static void decompress_selected(AppState *state) {
    char *input_path = NULL;
    char *output_path = NULL;
    size_t in_len = 0, out_len = 0;
    uint8_t *input = NULL, *output = NULL;
    MosaicEntry *entries = NULL;
    size_t entry_count = 0;
    char root_name[MAX_PATH];
    char errbuf[160];
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
    if (header.magic != 0x31434D5A || header.version != 1 || header.compressed_size + sizeof(header) != in_len) {
        log_append(state->log_edit, "Not a Mosaic archive.");
        goto done;
    }
    if (header.compressed_size == 0 || header.original_size == 0) {
        log_append(state->log_edit, "Archive header is invalid.");
        goto done;
    }
    if (crc32_bytes(input + sizeof(header), (size_t)header.compressed_size) != header.checksum32) {
        log_append(state->log_edit, "Archive integrity check failed.");
        goto done;
    }
    if (!decompress_blob_exact(header.algorithm, input + sizeof(header), (size_t)header.compressed_size, (size_t)header.original_size, &output, NULL)) {
        log_append(state->log_edit, "Decompression failed.");
        goto done;
    }
    if (!parse_archive_v2(output, (size_t)header.original_size, root_name, sizeof(root_name), &entries, &entry_count, errbuf, sizeof(errbuf))) {
        log_append(state->log_edit, errbuf[0] ? errbuf : "Not a Mosaic archive.");
        goto done;
    }
    if (!restore_archive_entries(output_path, entries, entry_count)) {
        log_append(state->log_edit, "Could not write decompressed output.");
        goto done;
    }
    {
        char line[256];
        _snprintf(line, sizeof(line), "Decompressed archive to %s", output_path);
        log_append(state->log_edit, line);
        set_status(state, "Archive extracted");
    }
done:
    if (entries) {
        for (size_t i = 0; i < entry_count; ++i) free(entries[i].bytes);
        free(entries);
    }
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
        state->source_kind = SOURCE_KIND_FILE;
        state->archive_mode = ARCHIVE_MODE_ADD_REPLACE;
        state->verify_after = 1;
        ui_init(hwnd);
        HFONT font = g_ui.body_font;
        CreateWindowA("STATIC", "Source file or folder", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->input_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)ID_INPUT_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Browse source", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_LOAD_INPUT, NULL, NULL);
        CreateWindowA("STATIC", "Destination archive", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->output_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)ID_OUTPUT_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Browse output", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_CHOOSE_OUTPUT, NULL, NULL);
        CreateWindowA("STATIC", "Source type", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->source_combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, hwnd, (HMENU)ID_SOURCE_KIND, NULL, NULL);
        SendMessageA(state->source_combo, CB_ADDSTRING, 0, (LPARAM)"File");
        SendMessageA(state->source_combo, CB_ADDSTRING, 0, (LPARAM)"Folder");
        SendMessageA(state->source_combo, CB_SETCURSEL, 0, 0);
        CreateWindowA("STATIC", "Archive mode", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->mode_combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, hwnd, (HMENU)ID_MODE_COMBO, NULL, NULL);
        SendMessageA(state->mode_combo, CB_ADDSTRING, 0, (LPARAM)"Add and replace");
        SendMessageA(state->mode_combo, CB_ADDSTRING, 0, (LPARAM)"Add and skip");
        SendMessageA(state->mode_combo, CB_ADDSTRING, 0, (LPARAM)"Update newer");
        SendMessageA(state->mode_combo, CB_SETCURSEL, 0, 0);
        CreateWindowA("STATIC", "Algorithm", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->algo_combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, hwnd, (HMENU)ID_ALGO_COMBO, NULL, NULL);
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"XPRESS_HUFF");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"XPRESS");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"MSZIP");
        SendMessageA(state->algo_combo, CB_ADDSTRING, 0, (LPARAM)"LZMS");
        SendMessageA(state->algo_combo, CB_SETCURSEL, 0, 0);
        CreateWindowA("STATIC", "Level", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        state->level_track = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd, (HMENU)ID_LEVEL_TRACK, NULL, NULL);
        SendMessageA(state->level_track, TBM_SETRANGE, TRUE, MAKELPARAM(0, 5));
        SendMessageA(state->level_track, TBM_SETPOS, TRUE, 3);
        state->verify_check = CreateWindowA("BUTTON", "Verify after archive", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd, (HMENU)ID_VERIFY_CHECK, NULL, NULL);
        SendMessageA(state->verify_check, BM_SETCHECK, BST_CHECKED, 0);
        state->safety_note = CreateWindowA("STATIC", "Source deletion is disabled in this build.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
        CreateWindowA("BUTTON", "Archive", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_COMPRESS, NULL, NULL);
        CreateWindowA("BUTTON", "Extract", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_DECOMPRESS, NULL, NULL);
        CreateWindowA("BUTTON", "Clear log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_CLEAR, NULL, NULL);
        state->status = CreateWindowA("STATIC", "Ready", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        state->log_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)ID_LOG_EDIT, NULL, NULL);
        ui_apply_fonts(state->input_edit, font);
        ui_apply_fonts(state->output_edit, font);
        ui_apply_fonts(state->source_combo, font);
        ui_apply_fonts(state->mode_combo, font);
        ui_apply_fonts(state->algo_combo, font);
        ui_apply_fonts(state->verify_check, font);
        ui_apply_fonts(state->safety_note, font);
        ui_apply_fonts(state->status, font);
        ui_apply_fonts(state->log_edit, font);
        ui_theme_common(state->input_edit);
        ui_theme_common(state->output_edit);
        ui_theme_common(state->source_combo);
        ui_theme_common(state->mode_combo);
        ui_theme_common(state->algo_combo);
        ui_theme_common(state->verify_check);
        ui_theme_common(state->safety_note);
        ui_theme_common(state->log_edit);
        DragAcceptFiles(hwnd, TRUE);
        log_append(state->log_edit, "Mosaic Desktop ready.");
        log_append(state->log_edit, "Uses Windows Compression API for file compression.");
        ui_layout(hwnd, state);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_LOAD_INPUT: {
            char path[MAX_PATH];
            if (state->source_kind == SOURCE_KIND_FOLDER) {
                if (folder_dialog_open(hwnd, path, sizeof(path))) {
                    SendMessageA(state->source_combo, CB_SETCURSEL, 1, 0);
                    set_input_path(state, path);
                    set_output_for_input(state, path, ".mzc");
                    log_append(state->log_edit, "Folder selected.");
                    set_status(state, "Folder loaded");
                }
            } else {
                if (file_dialog_open(hwnd, path, sizeof(path), NULL)) {
                    SendMessageA(state->source_combo, CB_SETCURSEL, 0, 0);
                    set_input_path(state, path);
                    set_output_for_input(state, path, ".mzc");
                    log_append(state->log_edit, "Input selected.");
                    set_status(state, "Input loaded");
                }
            }
            return 0;
        }
        case ID_CHOOSE_OUTPUT: {
            char path[MAX_PATH];
            if (file_dialog_save(hwnd, path, sizeof(path), NULL)) {
                set_output_path(state, path);
                log_append(state->log_edit, "Output selected.");
                set_status(state, "Archive target selected");
            }
            return 0;
        }
        case ID_SOURCE_KIND:
            state->source_kind = (SourceKind)SendMessageA(state->source_combo, CB_GETCURSEL, 0, 0);
            log_append(state->log_edit, state->source_kind == SOURCE_KIND_FOLDER ? "Folder mode selected." : "File mode selected.");
            return 0;
        case ID_MODE_COMBO:
            state->archive_mode = (ArchiveMode)SendMessageA(state->mode_combo, CB_GETCURSEL, 0, 0);
            return 0;
        case ID_VERIFY_CHECK:
            state->verify_after = (IsDlgButtonChecked(hwnd, ID_VERIFY_CHECK) == BST_CHECKED);
            return 0;
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
    case WM_SIZE:
        if (state) ui_layout(hwnd, state);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_ui(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wparam;
        HWND child = (HWND)lparam;
        if (child == state->log_edit) {
            SetTextColor(hdc, LOG_FG);
            SetBkColor(hdc, LOG_BG);
            return (INT_PTR)g_ui.log_brush;
        }
        SetTextColor(hdc, g_ui.text_color);
        SetBkColor(hdc, APP_BG);
        return (INT_PTR)g_ui.edit_brush;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wparam;
        UINT count = DragQueryFileA(drop, 0xFFFFFFFF, NULL, 0);
        if (count > 0) {
            char path[MAX_PATH];
            DragQueryFileA(drop, 0, path, MAX_PATH);
            set_input_path(state, path);
            state->source_kind = path_exists_dir(path) ? SOURCE_KIND_FOLDER : SOURCE_KIND_FILE;
            SendMessageA(state->source_combo, CB_SETCURSEL, (WPARAM)state->source_kind, 0);
            if (state->source_kind == SOURCE_KIND_FOLDER) {
                set_output_for_input(state, path, ".mzc");
            }
            log_append(state->log_edit, "Input dropped.");
            set_status(state, "Input loaded");
        }
        DragFinish(drop);
        return 0;
    }
    case WM_DESTROY:
        DeleteObject(g_ui.title_font);
        DeleteObject(g_ui.header_font);
        DeleteObject(g_ui.body_font);
        DeleteObject(g_ui.bg_brush);
        DeleteObject(g_ui.card_brush);
        DeleteObject(g_ui.log_brush);
        DeleteObject(g_ui.edit_brush);
        DeleteObject(g_ui.border_brush);
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
