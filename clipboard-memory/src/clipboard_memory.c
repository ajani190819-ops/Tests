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

#define RES_ICON_APP        1

#define WM_TRAYICON         (WM_APP + 1)
#define WM_CLIPUPDATE       (WM_APP + 2)

#define IDM_TRAY_OPEN       200
#define IDM_TRAY_CLEAR      201
#define IDM_TRAY_QUIT       202
#define ID_HOTKEY_SHOW      300
#define ID_BTN_PASTE_BASE   1000
#define ID_BTN_DELETE_BASE  2000
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
    if (hist_find_dup(text, len) >= 0) return;  /* no consecutive/any dup */

    /* Shift everything down to make room at index 0 (most recent first) */
    if (g_hist_count == MAX_HISTORY) {
        /* Drop the oldest */
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
    /* Alt+Space to dismiss any menu, then Ctrl+V */
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
static int S(int x) { return MulDiv(x, g_dpi, 96); }

static void draw_suite_emblem(HDC hdc, int x, int y, int size) {
    HPEN hPen = CreatePen(PS_SOLID, 2, COL_ACCENT);
    HBRUSH hBrush = CreateSolidBrush(COL_BG);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    SetBkMode(hdc, TRANSPARENT);

    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2;

    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    for (int d = 0; d < 3; d++) {
        double rad = (-90.0 + d * 120.0) * 3.14159265 / 180.0;
        int x2 = cx + (int)(cos(rad) * r * 0.85);
        int y2 = cy + (int)(sin(rad) * r * 0.85);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
    }

    Ellipse(hdc, cx - r/4, cy - r/4, cx + r/4, cy + r/4);

    POINT tri[3] = {
        { cx, cy - r/5 },
        { cx + r/6, cy + r/8 },
        { cx - r/6, cy + r/8 }
    };
    SelectObject(hdc, GetStockBrush(DC_BRUSH));
    SetDCBrushColor(hdc, COL_BG);
    Polygon(hdc, tri, 3);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

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
    int row_h = S(66);
    int row_w = S(600);

    for (int i = 0; i < g_hist_count; i++) {
        int base_y = y;

        /* Paste button (left) - clicking pastes this item */
        g_item_hwnds[i][0] = CreateWindowExW(
            0, L"BUTTON", L"Paste",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER,
            12, base_y + S(8), S(72), S(24),
            g_scroll, (HMENU)(INT_PTR)(ID_BTN_PASTE_BASE + i), g_hInst, NULL);

        /* Delete button (right) */
        g_item_hwnds[i][1] = CreateWindowExW(
            0, L"BUTTON", L"×",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER,
            row_w - S(40), base_y + S(8), S(28), S(24),
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
            S(96), base_y + S(8), row_w - S(96) - S(48), S(24),
            g_scroll, NULL, g_hInst, NULL);

        /* Separator line */
        {
            HDC hdc = GetDC(g_scroll);
            HPEN pen = CreatePen(PS_SOLID, 1, COL_BORDER);
            SelectObject(hdc, pen);
            MoveToEx(hdc, 12, base_y + row_h - 2, NULL);
            LineTo(hdc, row_w - 12, base_y + row_h - 2);
            DeleteObject(pen);
            ReleaseDC(g_scroll, hdc);
        }

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

        /* Header area */
        CreateWindowExW(0, L"BUTTON", L"\\u21BB Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            S(20), S(16), S(96), S(28),
            hwnd, (HMENU)4001, g_hInst, NULL);

        CreateWindowExW(0, L"BUTTON", L"Clear All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            S(20), S(56), S(96), S(28),
            hwnd, (HMENU)ID_BTN_CLEAR_ALL, g_hInst, NULL);

        /* Scrollable container for history items */
        g_scroll = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
            S(20), S(104), S(660), S(440),
            hwnd, NULL, g_hInst, NULL);

        ui_rebuild();

        return 0;
    }

    case WM_DESTROY:
        g_quitting = TRUE;
        clip_stop_listening(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        hist_save();
        PostQuitMessage(0);
        return 0;

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
        if (id == 4001) {  /* Refresh */
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

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, CreateSolidBrush(COL_BG));

        RECT hdr = { 0, 0, rc.right, S(60) };
        FillRect(hdc, &hdr, CreateSolidBrush(COL_FOOTER_BG));
        SetBkColor(hdc, COL_FOOTER_BG);
        SetTextColor(hdc, COL_TEXT);
        DrawTextW(hdc, APP_TITLE, -1, &(RECT){ S(20), S(8), rc.right, S(32) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_suite_emblem(hdc, rc.right - S(56), S(6), S(48));

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

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
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
