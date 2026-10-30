#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 *  Clipboard Memory - Native Win32 Suite App
 *
 *  Disk-persisted clipboard history (last 20 items, survives reboot).
 *  Tray icon + single instance + hotkey-triggered paste from history.
 *
 *  Matches the unified Soft Slate Suite UI shared with ProjectHub.exe,
 *  BluetoothGuard.exe, KeyboardLighting.exe and AudioSwitcher.exe.
 * ============================================================================ */

/* --- Soft Slate palette (docs/UI_STYLE_GUIDE.md) --- */
#define COL_BG              RGB(24, 27, 36)    /* #181B24 canvas */
#define COL_FOOTER_BG       RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER         RGB(48, 55, 72)    /* #303748 */
#define COL_CARD            RGB(34, 39, 52)    /* #222734 card */
#define COL_BORDER          RGB(56, 64, 84)    /* #384054 */
#define COL_SEC_BTN         RGB(43, 49, 66)    /* #2B3142 */
#define COL_SEC_HOV         RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER      RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_RIM       RGB(96, 112, 144)  /* #607090 */
#define COL_TEXT            RGB(236, 239, 246) /* #ECEFF6 */
#define COL_MUTED           RGB(154, 163, 184) /* #9AA3B8 */
#define COL_GOOD            RGB(52, 211, 153)  /* #34D399 */
#define COL_WARN            RGB(251, 191, 36)  /* #FBBF24 */
#define COL_ACCENT          RGB(56, 189, 248)  /* #38BDF8 */
#define COL_ACCENT_STRONG   RGB(37, 99, 235)   /* #2563EB */
#define COL_DANGER          RGB(248, 113, 113) /* #F87171 */

#define APP_TITLE           L"Clipboard Memory"
#define SINGLE_MUTEX_NAME   L"Local\\Tests_ClipboardMemory_SingleInstance_v1"
#define SHOW_EVENT_NAME     L"Local\\Tests_ClipboardMemory_ShowWindow_v1"
#define REPO_OWNER_W        L"ajani190819-ops"
#define REPO_NAME_W         L"Tests"
#define FALLBACK_BRANCH_W   L"arena/e895b94f-tests"

#define MAX_HISTORY         20
#define MAX_ITEM_CHARS      4000

/* Tri-Spoke Core Emblem - same SDF renderer as Project Hub */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

#define RES_ICON_APP        1

#define WM_TRAYICON         (WM_APP + 1)
#define WM_CLIPUPDATE       (WM_APP + 2)

#define IDM_TRAY_OPEN       200
#define IDM_TRAY_CLEAR      201
#define IDM_TRAY_QUIT       202
#define ID_HOTKEY_SHOW      300
#define ID_BTN_PASTE_BASE   1000
#define ID_BTN_DELETE_BASE  2000
#define ID_BTN_REFRESH      4001
#define ID_BTN_CLEAR_ALL    3000

/* ============================================================================
 *  Clipboard history storage (disk-persisted)
 * ============================================================================ */

typedef struct {
    wchar_t  text[MAX_ITEM_CHARS];
    size_t   len;            /* char count, not including NUL */
    time_t   tstamp;
} HistItem;

static HistItem g_hist[MAX_HISTORY];
static int      g_hist_count = 0;
static wchar_t  g_hist_path[MAX_PATH];
static BOOL     g_hist_dirty = FALSE;

static void hist_path_init(void) {
    wchar_t appdata[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    if (n == 0 || n > MAX_PATH) wcscpy(appdata, L"C:\\");
    _snwprintf(g_hist_path, MAX_PATH, L"%s\\ClipboardMemory\\history.bin", appdata);
}

static void hist_ensure_dir(void) {
    wchar_t dir[MAX_PATH];
    wcsncpy(dir, g_hist_path, MAX_PATH - 1);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = L'\0';
    CreateDirectoryW(dir, NULL);
}

static void hist_save(void) {
    if (!g_hist_dirty) return;
    hist_ensure_dir();
    HANDLE h = CreateFileW(g_hist_path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    int count = g_hist_count;
    WriteFile(h, &count, sizeof(count), &written, NULL);
    for (int i = 0; i < g_hist_count; i++) {
        WriteFile(h, &g_hist[i].len, sizeof(g_hist[i].len), &written, NULL);
        WriteFile(h, g_hist[i].text, (DWORD)(g_hist[i].len * sizeof(wchar_t)), &written, NULL);
        WriteFile(h, &g_hist[i].tstamp, sizeof(g_hist[i].tstamp), &written, NULL);
    }
    CloseHandle(h);
    g_hist_dirty = FALSE;
}

static void hist_load(void) {
    HANDLE h = CreateFileW(g_hist_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD read = 0;
    int count = 0;
    if (!ReadFile(h, &count, sizeof(count), &read, NULL) || read != sizeof(count)) {
        CloseHandle(h); return;
    }
    if (count < 0 || count > MAX_HISTORY * 4) { CloseHandle(h); return; }
    g_hist_count = 0;
    for (int i = 0; i < count && i < MAX_HISTORY; i++) {
        size_t len = 0;
        if (!ReadFile(h, &len, sizeof(len), &read, NULL) || read != sizeof(len)) break;
        if (len > MAX_ITEM_CHARS - 1) len = MAX_ITEM_CHARS - 1;
        if (len > 0) {
            if (!ReadFile(h, g_hist[g_hist_count].text, (DWORD)(len * sizeof(wchar_t)), &read, NULL)) break;
            g_hist[g_hist_count].text[len] = L'\0';
        } else {
            g_hist[g_hist_count].text[0] = L'\0';
        }
        if (!ReadFile(h, &g_hist[g_hist_count].tstamp, sizeof(g_hist[g_hist_count].tstamp), &read, NULL)) break;
        g_hist[g_hist_count].len = len;
        g_hist_count++;
    }
    CloseHandle(h);
}

static int hist_find_dup(const wchar_t *text, size_t len) {
    for (int i = 0; i < g_hist_count; i++) {
        if (g_hist[i].len == len && wcscmp(g_hist[i].text, text) == 0)
            return i;
    }
    return -1;
}

static void hist_add(const wchar_t *text, size_t len) {
    if (len == 0) return;
    if (hist_find_dup(text, len) >= 0) return;  /* no dup */

    /* Shift everything down to make room at index 0 (most recent first) */
    if (g_hist_count == MAX_HISTORY) {
        memmove(&g_hist[1], &g_hist[0], (size_t)(MAX_HISTORY - 1) * sizeof(HistItem));
        g_hist_count = MAX_HISTORY - 1;
    }
    memmove(&g_hist[1], &g_hist[0], (size_t)g_hist_count * sizeof(HistItem));
    g_hist_count++;

    HistItem *it = &g_hist[0];
    wcsncpy(it->text, text, MAX_ITEM_CHARS - 1);
    it->text[MAX_ITEM_CHARS - 1] = L'\0';
    it->len = len;
    it->tstamp = time(NULL);
    g_hist_dirty = TRUE;
}

static void hist_delete(int idx) {
    if (idx < 0 || idx >= g_hist_count) return;
    memmove(&g_hist[idx], &g_hist[idx + 1],
            (size_t)(g_hist_count - idx - 1) * sizeof(HistItem));
    g_hist_count--;
    g_hist_dirty = TRUE;
}

static void hist_clear_all(void) {
    g_hist_count = 0;
    g_hist_dirty = TRUE;
}

static const wchar_t *hist_preview(const wchar_t *text, wchar_t *out, size_t outsz) {
    /* Single-line preview: collapse newlines/tabs to spaces */
    size_t j = 0;
    for (size_t i = 0; text[i] && j < outsz - 1; i++) {
        wchar_t c = text[i];
        if (c == L'\n' || c == L'\r' || c == L'\t') c = L' ';
        out[j++] = c;
    }
    out[j] = L'\0';
    if (wcslen(out) > 60) { out[57] = L'.'; out[58] = L'.'; out[59] = L'.'; out[60] = L'\0'; }
    return out;
}

static void fmt_time(time_t t, wchar_t *out, size_t outsz) {
    struct tm tm;
    localtime_s(&tm, &t);
    wcsftime(out, outsz, L"%H:%M:%S", &tm);
}

/* ============================================================================
 *  Clipboard monitoring (AddClipboardFormatListener)
 * ============================================================================ */

static BOOL g_clip_listener = FALSE;

static void clip_start_listening(HWND hwnd) {
    if (AddClipboardFormatListener(hwnd)) g_clip_listener = TRUE;
}

static void clip_stop_listening(HWND hwnd) {
    if (g_clip_listener) {
        RemoveClipboardFormatListener(hwnd);
        g_clip_listener = FALSE;
    }
}

static BOOL clip_get_text(wchar_t *out, size_t outsz) {
    if (!OpenClipboard(NULL)) return FALSE;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) { CloseClipboard(); return FALSE; }
    wchar_t *data = (wchar_t*)GlobalLock(h);
    if (!data) { CloseClipboard(); return FALSE; }
    size_t len = wcslen(data);
    if (len > outsz - 1) len = outsz - 1;
    memcpy(out, data, len * sizeof(wchar_t));
    out[len] = L'\0';
    GlobalUnlock(h);
    CloseClipboard();
    return TRUE;
}

static void clip_set_text(const wchar_t *text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    size_t len = wcslen(text);
    HANDLE h = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    if (h) {
        wchar_t *data = (wchar_t*)GlobalLock(h);
        if (data) {
            memcpy(data, text, (len + 1) * sizeof(wchar_t));
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
}

/* Paste the current clipboard into the active window using SendInput */
static void clip_paste(void) {
    INPUT inputs[2] = {0};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[0].ki.dwFlags = 0;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0x56;  /* 'V' */
    inputs[1].ki.dwFlags = 0;
    SendInput(2, inputs, sizeof(INPUT));

    INPUT inputs2[2] = {0};
    inputs2[0].type = INPUT_KEYBOARD;
    inputs2[0].ki.wVk = 0x56;
    inputs2[0].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs2[1].type = INPUT_KEYBOARD;
    inputs2[1].ki.wVk = VK_CONTROL;
    inputs2[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs2, sizeof(INPUT));
}

/* ============================================================================
 *  Win32 window + UI + tray
 * ============================================================================ */

static HINSTANCE g_hInst = NULL;
static HWND   g_hwnd = NULL;
static HICON  g_app_icon = NULL;
static NOTIFYICONDATAW g_nid = {0};
static BOOL   g_quitting = FALSE;
static BOOL   g_silent = FALSE;
static HWND   g_scroll = NULL;      /* scrollable history container */
static HWND   g_item_hwnds[MAX_HISTORY][3];  /* [idx][0]=pasteBtn [1]=delBtn [2]=label */
static int    g_item_hwnd_count = 0;
static int    g_dpi = 96;
static int    g_hover_row = -1;     /* which history row is hovered */

/* Spinning emblem */
static RECT   g_rcEmblem   = {0, 0, 0, 0};
static int S(int x) { return MulDiv(x, g_dpi, 96); }

/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1 from Project Hub */
static void EmbRasterize(uint32_t *dst, int n, COLORREF bg) {
    (void)bg;
    const double scale = EMB_SDF_SCALE, b = EMB_SDF_BOOST;
    const double pxSize = 2.0 / (double)n;
    const double aa = (pxSize * 1.35 > 1e-4) ? (pxSize * 1.35) : 1e-4;
    const double deg = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double nrm[3] = { 90.0, -150.0, -30.0 };
    for (int j = 0; j < n; j++) {
        double y0 = ((double)j + 0.5) / (double)n;
        double y = -1.0 + 2.0 * y0;
        for (int i = 0; i < n; i++) {
            double x0 = ((double)i + 0.5) / (double)n;
            double x = -1.0 + 2.0 * x0;
            double r = sqrt(x * x + y * y) / scale;
            double ang = atan2(y, x) / deg;
            double dOuter = fabs(r - 0.895) - (0.022 + b * 0.45);
            double dRad   = fabs(r - 0.742) - (0.056 + b * 0.5);
            double dGap = 180.0;
            for (int k = 0; k < 3; k++) {
                double m = fmod(ang - gaps[k] + 180.0, 360.0);
                if (m < 0) m += 360.0;
                double diff = fabs(m - 180.0);
                if (diff < dGap) dGap = diff;
            }
            double dCut  = (12.5 - dGap) * deg * 0.742;
            double dArcs = (dRad > dCut) ? dRad : dCut;
            double dSpokes = 10.0;
            double pMid = 0.052 + b * 0.12, pHw = 0.024 + b * 0.42;
            double sMid = 0.5 * (0.258 + 0.722), sHw = 0.5 * (0.722 - 0.258);
            for (int k = 0; k < 3; k++) {
                double rad = gaps[k] * deg;
                double ux = cos(rad), uy = sin(rad);
                double proj = (x * ux + y * uy) / scale;
                double perp = fabs(-x * uy + y * ux) / scale;
                double dAlong = fabs(proj - sMid) - sHw;
                double dBars  = fabs(perp - pMid) - pHw;
                double dThis  = (dAlong > dBars) ? dAlong : dBars;
                if (dThis < dSpokes) dSpokes = dThis;
            }
            double dHubOut = r - (0.224 + b * 0.25);
            double dTri = -10.0;
            for (int k = 0; k < 3; k++) {
                double rad = nrm[k] * deg;
                double dHalf = (x * cos(rad) + y * sin(rad)) / scale - 0.074;
                if (dHalf > dTri) dTri = dHalf;
            }
            double dHub = (dHubOut > -dTri) ? dHubOut : -dTri;
            double u1 = (dOuter < dArcs) ? dOuter : dArcs;
            double u2 = (dSpokes < dHub) ? dSpokes : dHub;
            double u  = (u1 < u2) ? u1 : u2;
            double alpha = 0.5 - (u * scale) / aa;
            if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
            double t = x0 * 0.3 + y0 * 0.7;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            double fr = 246.0 * (1 - t) + 196.0 * t;
            double fg = 249.0 * (1 - t) + 212.0 * t;
            double fb = 254.0 * (1 - t) + 234.0 * t;
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

typedef struct EmbCache {
    int      sz;
    int      srcSz;
    uint32_t *src;
} EmbCache;
static EmbCache g_embCache = { 0, 0, NULL };

static void EmbCacheEnsure(int sz, COLORREF bg) {
    if (g_embCache.src && g_embCache.sz == sz) return;
    free(g_embCache.src);
    g_embCache.src = NULL;
    g_embCache.sz = 0;
    if (sz < 8 || sz > 512) return;
    int srcSz = sz * 4;
    uint32_t *src = (uint32_t *)malloc((size_t)srcSz * srcSz * sizeof(uint32_t));
    if (!src) return;
    EmbRasterize(src, srcSz, bg);
    g_embCache.sz    = sz;
    g_embCache.srcSz = srcSz;
    g_embCache.src   = src;
}

static void EmbSpinDraw(uint32_t *px, int W, int H, int x, int y, int sz, COLORREF bg) {
    EmbCacheEnsure(sz, bg);
    const uint32_t *src = g_embCache.src;
    if (!src) return;
    int srcSz = g_embCache.srcSz;
    double ang = ((double)(GetTickCount64() % 30000ULL) / 30000.0) * 6.283185307179586;
    double ca = cos(ang), sa = sin(ang);
    double c = (sz - 1) * 0.5;
    double sc = (double)(srcSz - 1) / (double)(sz > 1 ? sz : 1);
    int maxs = srcSz - 1;
    for (int dy = 0; dy < sz; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= H) continue;
        double v = dy - c;
        for (int dx = 0; dx < sz; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= W) continue;
            double u = dx - c;
            double fx = (ca * u + sa * v + c) * sc;
            double fy = (-sa * u + ca * v + c) * sc;
            if (fx < 0) fx = 0; else if (fx > maxs) fx = maxs;
            if (fy < 0) fy = 0; else if (fy > maxs) fy = maxs;
            int x0 = (int)fx, y0 = (int)fy;
            double ax = fx - x0, ay = fy - y0;
            double w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay);
            double w01 = (1 - ax) * ay,       w11 = ax * ay;
            const uint32_t *p00 = &src[y0 * srcSz + x0];
            uint32_t c00 = p00[0];
            uint32_t c10 = (x0 < maxs) ? p00[1] : c00;
            const uint32_t *p01 = (y0 < maxs) ? p00 + srcSz : p00;
            uint32_t c01 = p01[0];
            uint32_t c11 = (x0 < maxs) ? p01[1] : c01;
            double r = (c00 & 255) * w00 + (c10 & 255) * w10 + (c01 & 255) * w01 + (c11 & 255) * w11;
            double g = ((c00 >> 8) & 255) * w00 + ((c10 >> 8) & 255) * w10 +
                       ((c01 >> 8) & 255) * w01 + ((c11 >> 8) & 255) * w11;
            double b = ((c00 >> 16) & 255) * w00 + ((c10 >> 16) & 255) * w10 +
                       ((c01 >> 16) & 255) * w01 + ((c11 >> 16) & 255) * w11;
            double a = ((c00 >> 24) & 255) * w00 + ((c10 >> 24) & 255) * w10 +
                       ((c01 >> 24) & 255) * w01 + ((c11 >> 24) & 255) * w11;
            int ai = (int)(a + 0.5);
            uint32_t d = px[ty * W + tx];
            if (ai >= 255) {
                px[ty * W + tx] = RGB((int)(r + 0.5), (int)(g + 0.5), (int)(b + 0.5));
            } else if (ai > 0) {
                int dr = (int)(d & 255), dg = (int)((d >> 8) & 255), db = (int)((d >> 16) & 255);
                px[ty * W + tx] = RGB(((int)(r + 0.5) * ai + dr * (255 - ai) + 127) / 255,
                                      ((int)(g + 0.5) * ai + dg * (255 - ai) + 127) / 255,
                                      ((int)(b + 0.5) * ai + db * (255 - ai) + 127) / 255);
            }
        }
    }
}

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). */
static void PaintEmblemBlit(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    uint32_t bgP = ((uint32_t)GetRValue(COL_BG) << 16) |
                   ((uint32_t)GetGValue(COL_BG) << 8) |
                   (uint32_t)GetBValue(COL_BG);
    for (int i = 0; i < sz * sz; i++) px[i] = bgP;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

/* Draw a rounded card background for a history item */
static int GetWinDpi(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi ? dpi : 96;
}

static void ui_rebuild(void) {
    if (!g_hwnd || !g_scroll) return;

    /* Destroy existing item rows */
    for (int i = 0; i < g_item_hwnd_count; i++) {
        for (int j = 0; j < 3; j++) {
            if (g_item_hwnds[i][j]) DestroyWindow(g_item_hwnds[i][j]);
            g_item_hwnds[i][j] = NULL;
        }
    }
    g_item_hwnd_count = 0;

    int y = 12;
    int row_h = S(72);
    int row_w = S(620);
    int card_x = 12;
    int card_w = row_w - 24;

    for (int i = 0; i < g_hist_count; i++) {
        int base_y = y;

        /* Draw card background (handled in WM_PAINT of scroll, but we
           also draw per-row here via owner-draw style on the label) */

        /* Paste button (left side of card) */
        g_item_hwnds[i][0] = CreateWindowExW(
            0, L"BUTTON", L"Paste",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER | BS_VCENTER,
            card_x + S(16), base_y + S(20), S(80), S(32),
            g_scroll, (HMENU)(INT_PTR)(ID_BTN_PASTE_BASE + i), g_hInst, NULL);

        /* Delete button (right side of card) */
        g_item_hwnds[i][1] = CreateWindowExW(
            0, L"BUTTON", L"\u00D7",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER | BS_VCENTER,
            card_x + card_w - S(96), base_y + S(20), S(80), S(32),
            g_scroll, (HMENU)(INT_PTR)(ID_BTN_DELETE_BASE + i), g_hInst, NULL);

        /* Label: preview text + timestamp */
        wchar_t prev[128];
        wchar_t tbuf[32];
        fmt_time(g_hist[i].tstamp, tbuf, 32);
        const wchar_t *pv = hist_preview(g_hist[i].text, prev, 128);
        wchar_t label[256];
        _snwprintf(label, 256, L"%s    [%s]", pv, tbuf);

        g_item_hwnds[i][2] = CreateWindowExW(
            0, L"STATIC", label,
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            card_x + S(112), base_y + S(24), card_w - S(112) - S(96), S(24),
            g_scroll, NULL, g_hInst, NULL);

        g_item_hwnd_count++;
        y += row_h;
    }

    /* Set scroll range */
    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = max(0, y - 12);
    si.nPage = S(400);
    SetScrollInfo(g_scroll, SB_VERT, &si, TRUE);

    InvalidateRect(g_scroll, NULL, FALSE);
}

static void ui_scroll_to_top(void) {
    if (!g_scroll) return;
    SendMessageW(g_scroll, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
}

static void do_paste(int idx) {
    if (idx < 0 || idx >= g_hist_count) return;
    clip_set_text(g_hist[idx].text);
    /* Move to most recent */
    HistItem it = g_hist[idx];
    memmove(&g_hist[1], &g_hist[0], (size_t)idx * sizeof(HistItem));
    g_hist[0] = it;
    g_hist_dirty = TRUE;
    hist_save();
    ui_rebuild();

    ShowWindow(g_hwnd, SW_HIDE);
    clip_paste();
}

static void do_delete(int idx) {
    if (idx < 0 || idx >= g_hist_count) return;
    hist_delete(idx);
    hist_save();
    ui_rebuild();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_app_icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP));
        if (!g_app_icon) g_app_icon = LoadIconW(NULL, IDI_APPLICATION);

        ZeroMemory(&g_nid, sizeof(g_nid));
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_app_icon;
        wcscpy(g_nid.szTip, L"Clipboard Memory - disk-persisted clipboard history");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        /* Register hotkey: Win+V shows the history window */
        RegisterHotKey(hwnd, ID_HOTKEY_SHOW, MOD_WIN, 0x56);

        /* Start clipboard monitoring */
        clip_start_listening(hwnd);

        /* Header buttons - right side, side by side before the emblem */
        int hdrBtnY = S(16);
        int hdrBtnW = S(96);
        int hdrBtnH = S(28);
        int emblemX = S(720) - S(48) - S(8);  /* emblem x-position, matches WM_PAINT */

        CreateWindowExW(0, L"BUTTON", L"\u21BB Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            emblemX - hdrBtnW - S(100), hdrBtnY, hdrBtnW, hdrBtnH,
            hwnd, (HMENU)ID_BTN_REFRESH, g_hInst, NULL);

        CreateWindowExW(0, L"BUTTON", L"Clear All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            emblemX - hdrBtnW, hdrBtnY, hdrBtnW, hdrBtnH,
            hwnd, (HMENU)ID_BTN_CLEAR_ALL, g_hInst, NULL);

        /* Scrollable container for history items */
        g_scroll = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | WS_CLIPCHILDREN,
            S(20), S(104), S(660), S(440),
            hwnd, NULL, g_hInst, NULL);

        ui_rebuild();

        /* Emblem spin timer - 50ms for smooth rotation */
        SetTimer(hwnd, 2, 50, NULL);

        return 0;
    }

    case WM_DESTROY:
        g_quitting = TRUE;
        KillTimer(hwnd, 2);
        clip_stop_listening(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        hist_save();
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        if (wp == 2) {
            /* Emblem spin only - invalidate just the emblem square */
            if (g_rcEmblem.right > g_rcEmblem.left)
                InvalidateRect(hwnd, &g_rcEmblem, FALSE);
            return 0;
        }
        break;

    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Clipboard Memory");
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLEAR, L"Clear History");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Clipboard Memory");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                           pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lp == WM_LBUTTONDBLCLK) {
            if (!IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
            } else {
                ShowWindow(hwnd, SW_HIDE);
            }
        }
        return 0;
    }

    case WM_HOTKEY: {
        if ((int)wp == ID_HOTKEY_SHOW) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            ui_scroll_to_top();
        }
        return 0;
    }

    case WM_CLIPUPDATE: {
        wchar_t text[MAX_ITEM_CHARS];
        if (clip_get_text(text, MAX_ITEM_CHARS)) {
            size_t len = wcslen(text);
            hist_add(text, len);
            hist_save();
            if (IsWindowVisible(hwnd)) ui_rebuild();
        }
        return 0;
    }

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_BTN_CLEAR_ALL) {
            hist_clear_all();
            hist_save();
            ui_rebuild();
            return 0;
        }
        if (id == ID_BTN_REFRESH) {  /* Refresh */
            wchar_t text[MAX_ITEM_CHARS];
            if (clip_get_text(text, MAX_ITEM_CHARS)) {
                size_t len = wcslen(text);
                hist_add(text, len);
                hist_save();
                ui_rebuild();
            }
            return 0;
        }
        if (id >= ID_BTN_PASTE_BASE && id < ID_BTN_PASTE_BASE + MAX_HISTORY) {
            do_paste(id - ID_BTN_PASTE_BASE);
            return 0;
        }
        if (id >= ID_BTN_DELETE_BASE && id < ID_BTN_DELETE_BASE + MAX_HISTORY) {
            do_delete(id - ID_BTN_DELETE_BASE);
            return 0;
        }
        if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (id == IDM_TRAY_CLEAR) {
            hist_clear_all();
            hist_save();
            ui_rebuild();
            return 0;
        }
        if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_VSCROLL: {
        if ((HWND)lp == g_scroll) {
            int code = LOWORD(wp);
            SCROLLINFO si = {0};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(g_scroll, SB_VERT, &si);
            int pos = si.nPos;
            switch (code) {
            case SB_LINEUP:   pos -= S(24); break;
            case SB_LINEDOWN: pos += S(24); break;
            case SB_PAGEUP:   pos -= (int)si.nPage; break;
            case SB_PAGEDOWN: pos += (int)si.nPage; break;
            case SB_THUMBTRACK: pos = HIWORD(wp); break;
            case SB_TOP: pos = si.nMin; break;
            case SB_BOTTOM: pos = si.nMax; break;
            }
            if (pos < si.nMin) pos = si.nMin;
            if (pos > si.nMax - (int)si.nPage) pos = si.nMax - (int)si.nPage;
            si.fMask = SIF_POS;
            si.nPos = pos;
            SetScrollInfo(g_scroll, SB_VERT, &si, TRUE);
            int old = GetScrollPos(g_scroll, SB_VERT);
            ScrollWindow(g_scroll, 0, old - pos, NULL, NULL);
            UpdateWindow(g_scroll);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = (short)(GetMessagePos() & 0xFFFF);
        pt.y = (short)((GetMessagePos() >> 16) & 0xFFFF);
        HWND hwndChild = WindowFromPoint(pt);
        while (hwndChild && hwndChild != g_hwnd) {
            if (hwndChild == g_scroll) break;
            hwndChild = GetParent(hwndChild);
        }
        if (hwndChild == g_scroll) {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
            SCROLLINFO si = {0};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(g_scroll, SB_VERT, &si);
            int pos = si.nPos - (zDelta / WHEEL_DELTA) * S(24);
            if (pos < si.nMin) pos = si.nMin;
            if (pos > si.nMax - (int)si.nPage) pos = si.nMax - (int)si.nPage;
            si.fMask = SIF_POS;
            si.nPos = pos;
            SetScrollInfo(g_scroll, SB_VERT, &si, TRUE);
            int old = GetScrollPos(g_scroll, SB_VERT);
            ScrollWindow(g_scroll, 0, old - pos, NULL, NULL);
            UpdateWindow(g_scroll);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        /* Detect which row the mouse is over for hover effects */
        if (!g_scroll) break;
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g_scroll, &pt);
        SCROLLINFO si = {0};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        GetScrollInfo(g_scroll, SB_VERT, &si);
        int row_h = S(72);
        int row_idx = (pt.y + si.nPos - 12) / row_h;
        if (row_idx >= 0 && row_idx < g_hist_count) {
            if (row_idx != g_hover_row) {
                g_hover_row = row_idx;
                InvalidateRect(g_scroll, NULL, FALSE);
            }
        } else if (g_hover_row != -1) {
            g_hover_row = -1;
            InvalidateRect(g_scroll, NULL, FALSE);
        }
        break;
    }

    case WM_MOUSELEAVE: {
        if (g_hover_row != -1) {
            g_hover_row = -1;
            InvalidateRect(g_scroll, NULL, FALSE);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, CreateSolidBrush(COL_BG));

        RECT hdr = { 0, 0, rc.right, S(60) };
        FillRect(hdc, &hdr, CreateSolidBrush(COL_FOOTER_BG));
        SetBkColor(hdc, COL_FOOTER_BG);
        SetTextColor(hdc, COL_TEXT);

        /* Spinning emblem in top-right corner of header */
        int embSz = S(48);
        int embX = rc.right - embSz - S(8);
        int embY = S(6);
        SetRect(&g_rcEmblem, embX, embY, embX + embSz, embY + embSz);

        DrawTextW(hdc, APP_TITLE, -1, &(RECT){ S(20), S(8), embX - S(120), S(32) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        PaintEmblemBlit(hdc, embX, embY, embSz);

        RECT div = { 0, S(60), rc.right, S(62) };
        FillRect(hdc, &div, CreateSolidBrush(COL_DIVIDER));

        wchar_t status[128];
        _snwprintf(status, 128, L"%d item(s) in history", g_hist_count);
        SetTextColor(hdc, COL_MUTED);
        DrawTextW(hdc, status, -1, &(RECT){ S(20), S(68), S(300), S(88) },
                  DT_LEFT | DT_TOP | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        (void)lp;
        SetBkColor(hdc, COL_CARD);
        SetTextColor(hdc, COL_TEXT);
        return (INT_PTR)CreateSolidBrush(COL_CARD);
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, COL_CARD);
        return (INT_PTR)CreateSolidBrush(COL_CARD);
    }

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;

    g_silent = (cmdLine && (wcsstr(cmdLine, L"--silent") != NULL || wcsstr(cmdLine, L"-Silent") != NULL));

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!g_silent) {
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        CloseHandle(hMutex);
        return 0;
    }

    g_hInst = hInst;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    hist_path_init();
    hist_load();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TestsClipboardMemoryWnd";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    RegisterClassExW(&wc);

    g_dpi = GetWinDpi(NULL);

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, S(720), S(620),
        NULL, NULL, hInst, NULL);

    /* Start hidden in tray - user opens from tray icon or Win+V hotkey */
    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
