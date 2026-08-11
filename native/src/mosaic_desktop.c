#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mosaic.h"

#define ID_MODEL_EDIT 1001
#define ID_UNICODE_EDIT 1002
#define ID_INPUT_EDIT 1003
#define ID_LOG_EDIT 1004
#define ID_LOAD_MODEL 1005
#define ID_LOAD_UNICODE 1006
#define ID_LOAD_INPUT 1007
#define ID_RUN 1008

typedef struct AppState {
    HWND hwnd;
    HWND model_edit;
    HWND unicode_edit;
    HWND input_edit;
    HWND log_edit;
    mosaic_tokenizer *tokenizer;
    char model_path[MAX_PATH];
    char unicode_path[MAX_PATH];
    char input_path[MAX_PATH];
} AppState;

static void log_append(HWND edit, const char *text) {
    int len = GetWindowTextLengthA(edit);
    SendMessageA(edit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

static void set_text(HWND edit, const char *text) {
    SetWindowTextA(edit, text ? text : "");
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

static int get_window_text_alloc(HWND hwnd, char **out_text, size_t *out_len) {
    int len = GetWindowTextLengthA(hwnd);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) return 0;
    GetWindowTextA(hwnd, buf, len + 1);
    *out_text = buf;
    *out_len = (size_t)len;
    return 1;
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

static void ensure_tokenizer(AppState *state) {
    if (state->tokenizer) return;
    if (state->model_path[0] == '\0' || state->unicode_path[0] == '\0') return;
    if (mosaic_tokenizer_load_files(state->model_path, state->unicode_path, &state->tokenizer) == MOSAIC_OK) {
        log_append(state->log_edit, "Loaded Mosaic packs.");
    } else {
        log_append(state->log_edit, "Failed to load Mosaic packs.");
    }
}

static void run_analysis(AppState *state) {
    char *input = NULL;
    size_t input_len = 0;
    if (!get_window_text_alloc(state->input_edit, &input, &input_len)) {
        log_append(state->log_edit, "Unable to read input text.");
        return;
    }
    ensure_tokenizer(state);
    if (!state->tokenizer) {
        free(input);
        log_append(state->log_edit, "Load model and Unicode packs first.");
        return;
    }

    uint32_t *ids = NULL;
    size_t id_count = 0;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    mosaic_security_finding *findings = NULL;
    size_t finding_count = 0;
    mosaic_normalized_view view;
    ZeroMemory(&view, sizeof(view));

    log_append(state->log_edit, "Running analysis...");
    if (mosaic_tokenizer_encode(state->tokenizer, (const uint8_t *)input, input_len, &ids, &id_count) == MOSAIC_OK) {
        char line[256];
        _snprintf(line, sizeof(line), "Token count: %zu", id_count);
        log_append(state->log_edit, line);
        if (mosaic_tokenizer_decode(state->tokenizer, ids, id_count, &decoded, &decoded_len) == MOSAIC_OK) {
            log_append(state->log_edit, decoded_len == input_len && memcmp(decoded, input, input_len) == 0 ? "Roundtrip: PASS" : "Roundtrip: FAIL");
        }
    } else {
        log_append(state->log_edit, "Encode: FAIL");
    }

    if (mosaic_tokenizer_security_scan(state->tokenizer, (const uint8_t *)input, input_len, &findings, &finding_count) == MOSAIC_OK) {
        char line[256];
        _snprintf(line, sizeof(line), "Security findings: %zu", finding_count);
        log_append(state->log_edit, line);
    } else {
        log_append(state->log_edit, "Security scan unavailable.");
    }

    if (mosaic_tokenizer_normalize(state->tokenizer, MOSAIC_NORMALIZE_NFC, (const uint8_t *)input, input_len, &view) == MOSAIC_OK) {
        char line[256];
        _snprintf(line, sizeof(line), "Normalized bytes: %zu", view.byte_length);
        log_append(state->log_edit, line);
    }

    if (ids) mosaic_free(ids);
    if (decoded) mosaic_free(decoded);
    if (findings) mosaic_free(findings);
    if (view.bytes) mosaic_free(view.bytes);
    if (view.units) mosaic_free(view.units);
    if (view.source_spans) mosaic_free(view.source_spans);
    free(input);
}

static void on_load_path(HWND hwnd, AppState *state, int which) {
    char path[MAX_PATH];
    const char *filter = "Pack Files\0*.mpack;*.bin\0All Files\0*.*\0";
    if (!file_dialog_open(hwnd, path, sizeof(path), filter)) return;
    if (which == ID_MODEL_EDIT) {
        strcpy(state->model_path, path);
        set_text(state->model_edit, path);
        if (state->tokenizer) { mosaic_tokenizer_free(state->tokenizer); state->tokenizer = NULL; }
        log_append(state->log_edit, "Model pack selected.");
    } else if (which == ID_UNICODE_EDIT) {
        strcpy(state->unicode_path, path);
        set_text(state->unicode_edit, path);
        if (state->tokenizer) { mosaic_tokenizer_free(state->tokenizer); state->tokenizer = NULL; }
        log_append(state->log_edit, "Unicode pack selected.");
    } else if (which == ID_INPUT_EDIT) {
        uint8_t *bytes = NULL; size_t len = 0;
        if (read_file(path, &bytes, &len)) {
            set_text(state->input_edit, (const char *)bytes);
            free(bytes);
            strcpy(state->input_path, path);
            log_append(state->log_edit, "Input file loaded into editor.");
        } else {
            log_append(state->log_edit, "Could not load input file.");
        }
    }
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
        CreateWindowA("STATIC", "Model pack", WS_CHILD | WS_VISIBLE, 12, 12, 90, 20, hwnd, NULL, NULL, NULL);
        state->model_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 12, 32, 460, 24, hwnd, (HMENU)ID_MODEL_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 480, 32, 80, 24, hwnd, (HMENU)ID_LOAD_MODEL, NULL, NULL);
        CreateWindowA("STATIC", "Unicode pack", WS_CHILD | WS_VISIBLE, 12, 64, 90, 20, hwnd, NULL, NULL, NULL);
        state->unicode_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 12, 84, 460, 24, hwnd, (HMENU)ID_UNICODE_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 480, 84, 80, 24, hwnd, (HMENU)ID_LOAD_UNICODE, NULL, NULL);
        CreateWindowA("STATIC", "Input text", WS_CHILD | WS_VISIBLE, 12, 116, 90, 20, hwnd, NULL, NULL, NULL);
        state->input_edit = CreateWindowA("EDIT", "Type or load text here.", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL, 12, 136, 548, 160, hwnd, (HMENU)ID_INPUT_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Load file", WS_CHILD | WS_VISIBLE, 12, 304, 80, 26, hwnd, (HMENU)ID_LOAD_INPUT, NULL, NULL);
        CreateWindowA("BUTTON", "Run analysis", WS_CHILD | WS_VISIBLE, 100, 304, 100, 26, hwnd, (HMENU)ID_RUN, NULL, NULL);
        state->log_edit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 12, 344, 548, 180, hwnd, (HMENU)ID_LOG_EDIT, NULL, NULL);
        SendMessageA(state->model_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->unicode_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->input_edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageA(state->log_edit, WM_SETFONT, (WPARAM)font, TRUE);
        log_append(state->log_edit, "Mosaic Desktop ready.");
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_LOAD_MODEL: on_load_path(hwnd, state, ID_MODEL_EDIT); return 0;
        case ID_LOAD_UNICODE: on_load_path(hwnd, state, ID_UNICODE_EDIT); return 0;
        case ID_LOAD_INPUT: on_load_path(hwnd, state, ID_INPUT_EDIT); return 0;
        case ID_RUN: run_analysis(state); return 0;
        }
        break;
    case WM_DESTROY:
        if (state) {
            if (state->tokenizer) mosaic_tokenizer_free(state->tokenizer);
            free(state);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show) {
    (void)prev; (void)cmdline;
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
    HWND hwnd = CreateWindowA("MosaicDesktopWindow", "Mosaic Desktop", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 590, 590, NULL, NULL, instance, state);
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
