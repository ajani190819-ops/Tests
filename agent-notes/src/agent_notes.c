/* AgentNotes - quick notes from the owner straight to the agent workspace.
 *
 * No Google middleman: this little suite app POSTs notes directly to the
 * agent session's notes server (tools/notes_server.py behind the Arena
 * preview link). While the agent is offline the notes queue locally in
 * %LOCALAPPDATA%\AgentNotes\outbox\ and are delivered automatically as
 * soon as the session link answers again.
 *
 * UI: always-spinning Tri-Spoke Core emblem (the hub's SDF renderer, one
 * turn per 30 s idle / 8 s while sending), guided note box with a live
 * hint, a recent-notes list whose Sent/Queued state is derived from the
 * outbox, and a connection status strip.
 */
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define APP_TITLE_W   L"Agent Notes"
#define MUTEX_NAME    L"Local\\Tests_AgentNotes_SingleInstance_v1"
#define SHOW_EVENT    L"Local\\Tests_AgentNotes_Show_v1"

/* Soft Slate palette - exact tokens from docs/UI_STYLE_GUIDE.md */
#define COL_BG        RGB(24, 27, 36)     /* Canvas Slate   #181B24 */
#define COL_CARD      RGB(34, 39, 52)     /* Card Surface   #222734 */
#define COL_INPUT     RGB(43, 49, 66)     /* Control Surface #2B3142 */
#define COL_TXT       RGB(236, 239, 246)  /* Primary Text   #ECEFF6 */
#define COL_MUTED     RGB(154, 163, 184)  /* Secondary Text #9AA3B8 */
#define COL_ACCENT    RGB(56, 189, 248)   /* Primary Sky    #38BDF8 */
#define COL_WARN      RGB(251, 191, 36)   /* Warm Amber     #FBBF24 */
#define COL_GOOD      RGB(52, 211, 153)   /* Active Mint    #34D399 */
#define COL_DIV       RGB(58, 66, 86)     /* Soft Hairline  #3A4256 */
#define COL_HOT       RGB(82, 94, 122)    /* Hover Border   #525E7A */
#define COL_CARD_BD   RGB(56, 64, 84)     /* Card border    #384054 */

#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR (WM_USER + 67)   /* rich edit background color */
#endif

enum { BTN_SAVE = 0, BTN_SEND, BTN_COUNT };
enum { TIMER_FLUSH = 1, TIMER_SPIN = 2 };
#define ID_EDIT_LINK   101
#define ID_EDIT_NOTE   102

#define HINT_TEXT  L"Type your note here, then press Send to Agent - it lands in the agent's inbox."
#define HIST_MAX   20
#define HIST_SHOW  3

static HINSTANCE g_hInst;
static HWND      g_hwnd, g_hNote, g_hLink;
static HFONT     g_fontUI, g_fontTitle, g_fontBtn;
static HBRUSH    g_bgBrush;
static UINT      g_dpi = 96;
static int       g_fit = 100;      /* uniform shrink so the window always fits the screen */
static int       g_hoverBtn = -1;
static HANDLE    g_showEvent = NULL;

static wchar_t   g_link[256] = L"";
static wchar_t   g_cfgDir[MAX_PATH] = L"";
static wchar_t   g_cfgPath[MAX_PATH] = L"";
static wchar_t   g_histPath[MAX_PATH] = L"";
static wchar_t   g_outboxDir[MAX_PATH] = L"";
static wchar_t   g_status[256] = L"Paste the agent's session link above, then type a note.";
static int       g_lastOk = -1;          /* -1 unknown, 0 failed, 1 delivered */
static int       g_sending = 0;
static int       g_noteHint = 1;         /* note box is showing the hint */
static wchar_t   g_lastSent[16] = L"";

static int S(int px) {
    return MulDiv(MulDiv(px, (int)g_dpi, 96), g_fit, 100);
}

/* Dark title bar + full Windows 11 rounded corners (same as the hub). */
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD cornerPref = 2;   /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIV;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &cornerPref, sizeof(cornerPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static UINT GetWinDpi(HWND h) {
    UINT (WINAPI *pfn)(HWND) = (UINT (WINAPI *)(HWND))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return (pfn && h) ? pfn(h) : 96;
}

/* ---------------- config + outbox + history ---------------- */

static void InitPaths(void) {
    wchar_t appdata[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    _snwprintf(g_cfgDir, MAX_PATH - 1, L"%s\\AgentNotes", appdata);
    CreateDirectoryW(g_cfgDir, NULL);
    _snwprintf(g_cfgPath, MAX_PATH - 1, L"%s\\config.ini", g_cfgDir);
    _snwprintf(g_histPath, MAX_PATH - 1, L"%s\\history.txt", g_cfgDir);
    _snwprintf(g_outboxDir, MAX_PATH - 1, L"%s\\outbox", g_cfgDir);
    CreateDirectoryW(g_outboxDir, NULL);
}

static void LoadConfig(void) {
    GetPrivateProfileStringW(L"AgentNotes", L"Link", L"", g_link, 255, g_cfgPath);
}

static void SaveConfig(void) {
    WritePrivateProfileStringW(L"AgentNotes", L"Link", g_link, g_cfgPath);
}

static int OutboxCount(void) {
    int n = 0;
    wchar_t pat[MAX_PATH + 16];
    WIN32_FIND_DATAW fd;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return n;
}

static int FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

/* Recent notes: newest first. Each entry remembers its outbox file name -
   the file still existing means "Queued", gone means "Sent". The state is
   always live, with no bookkeeping to drift. */
typedef struct {
    wchar_t name[80];
    wchar_t stamp[16];    /* "MM-DD HH:MM" */
    wchar_t snip[80];
} HistEnt;
static HistEnt g_hist[HIST_MAX];
static int     g_histN = 0;

static void HistSave(void) {
    HANDLE h = CreateFileW(g_histPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < g_histN; i++) {
        /* Convert each field to UTF-8 explicitly - %ls in narrow printf
           would route through the ANSI codepage and mangle non-ASCII. */
        char n8[256], s8[64], p8[256];
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].name, -1, n8, sizeof(n8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].stamp, -1, s8, sizeof(s8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].snip, -1, p8, sizeof(p8) - 1, NULL, NULL);
        n8[sizeof(n8) - 1] = s8[sizeof(s8) - 1] = p8[sizeof(p8) - 1] = '\0';
        char line[640];
        int n = snprintf(line, sizeof(line) - 1, "%s\t%s\t%s\r\n", n8, s8, p8);
        if (n > 0) {
            DWORD wrote = 0;
            WriteFile(h, line, (DWORD)n, &wrote, NULL);
        }
    }
    CloseHandle(h);
}

static void HistLoad(void) {
    HANDLE h = CreateFileW(g_histPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), rd = 0;
    if (sz > 200000) sz = 200000;
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { CloseHandle(h); return; }
    ReadFile(h, buf, sz, &rd, NULL);
    buf[rd] = '\0';
    CloseHandle(h);
    g_histN = 0;
    char *p = buf;
    while (*p && g_histN < HIST_MAX) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        char *t1 = strchr(p, '\t');
        if (t1) {
            *t1 = '\0';
            char *t2 = strchr(t1 + 1, '\t');
            if (t2) {
                *t2 = '\0';
                HistEnt *e = &g_hist[g_histN];
                MultiByteToWideChar(CP_UTF8, 0, p, -1, e->name, 79);
                MultiByteToWideChar(CP_UTF8, 0, t1 + 1, -1, e->stamp, 15);
                MultiByteToWideChar(CP_UTF8, 0, t2 + 1, -1, e->snip, 79);
                e->name[79] = e->stamp[15] = e->snip[79] = L'\0';
                g_histN++;
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}

static void HistAdd(const wchar_t *name, const wchar_t *stamp, const wchar_t *snip) {
    if (g_histN == HIST_MAX) g_histN--;
    memmove(&g_hist[1], &g_hist[0], (size_t)g_histN * sizeof(HistEnt));
    HistEnt *e = &g_hist[0];
    wcsncpy(e->name, name, 79);   e->name[79] = L'\0';
    wcsncpy(e->stamp, stamp, 15); e->stamp[15] = L'\0';
    wcsncpy(e->snip, snip, 79);   e->snip[79] = L'\0';
    g_histN++;
    HistSave();
}

static void HistSnip(const wchar_t *text, wchar_t *out, int cap) {
    int n = 0;
    while (text[n] && n + 1 < cap) n++;
    for (int i = 0; i < n; i++) {
        wchar_t c = text[i];
        out[i] = (c == L'\r' || c == L'\n' || c == L'\t') ? L' ' : c;
    }
    out[n] = L'\0';
}

static int OutboxAdd(const wchar_t *text, wchar_t *nameOut, int nameCap) {
    wchar_t path[MAX_PATH + 32];
    static LONG seq = 0;
    LONG i = InterlockedIncrement(&seq);
    wchar_t name[64];
    _snwprintf(name, 63, L"n-%lu-%lu.txt",
               (unsigned long)GetTickCount(), (unsigned long)i);
    name[63] = L'\0';
    _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, name);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *u = (char *)malloc((size_t)n);
    if (u) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, u, n, NULL, NULL);
        DWORD wrote = 0;
        WriteFile(h, u, (DWORD)(n - 1), &wrote, NULL);
        free(u);
    }
    CloseHandle(h);
    if (nameOut && nameCap > 0) {
        wcsncpy(nameOut, name, nameCap - 1);
        nameOut[nameCap - 1] = L'\0';
    }
    return 1;
}

/* ---------------- HTTP to the agent session ---------------- */

static int HttpPostNote(const wchar_t *link, const wchar_t *path,
                        const char *body, int bodyLen) {
    URL_COMPONENTS uc;
    wchar_t host[128] = L"", urlPath[256] = L"";
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 127;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 255;
    if (!WinHttpCrackUrl(link, 0, 0, &uc)) return 0;

    HINTERNET ses = WinHttpOpen(L"AgentNotes/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 6000, 6000, 10000, 15000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, L"POST", path, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (uc.nScheme == INTERNET_SCHEME_HTTPS)
                                           ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return 0; }
    int ok = 0;
    if (WinHttpSendRequest(req, L"Content-Type: text/plain; charset=utf-8", (DWORD)-1L,
                           (LPVOID)body, (DWORD)bodyLen, (DWORD)bodyLen, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(req,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
                            WINHTTP_NO_HEADER_INDEX);
        ok = (code == 200);
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static int FlushOutbox(int *delivered) {
    wchar_t pat[MAX_PATH + 16], path[MAX_PATH + 32];
    WIN32_FIND_DATAW fd;
    int sent = 0, failed = 0;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, fd.cFileName);
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) { failed++; continue; }
        DWORD sz = GetFileSize(f, NULL), rd = 0;
        if (sz > 60000) sz = 60000;
        char *buf = (char *)malloc((size_t)sz + 1);
        if (!buf) { CloseHandle(f); failed++; continue; }
        ReadFile(f, buf, sz, &rd, NULL);
        buf[rd] = '\0';
        CloseHandle(f);
        if (rd > 0 && g_link[0] && HttpPostNote(g_link, L"/note", buf, (int)rd)) {
            DeleteFileW(path);
            sent++;
        } else {
            failed++;
        }
        free(buf);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (delivered) *delivered = sent;
    return failed;
}

static DWORD WINAPI FlushThread(LPVOID unused) {
    (void)unused;
    int sent = 0;
    int failed = FlushOutbox(&sent);
    int left = OutboxCount();
    g_sending = 0;
    if (sent > 0 && failed == 0) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        _snwprintf(g_lastSent, 15, L"%02d:%02d", st.wHour, st.wMinute);
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d note%s  |  last note sent %s  |  inbox is clear.",
                   sent, sent == 1 ? L"" : L"s", g_lastSent);
    } else if (sent > 0) {
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d  |  %d still queued - will retry automatically.",
                   sent, left);
    } else if (failed > 0) {
        g_lastOk = 0;
        _snwprintf(g_status, 255,
                   L"Session link not answering - re-paste the current link if it changed  |  %d queued.",
                   left);
    } else {
        g_lastOk = 1;
        wcsncpy(g_status, L"Nothing to send.", 255);
    }
    g_status[255] = L'\0';
    InvalidateRect(g_hwnd, NULL, FALSE);
    return 0;
}

static void StartFlush(void) {
    if (g_sending) return;
    g_sending = 1;
    HANDLE h = CreateThread(NULL, 0, FlushThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else g_sending = 0;
}

/* ---------------- Tri-Spoke Core emblem (hub's SDF renderer) -------------
   Analytic sub-pixel SDF, rasterized once at 4x; every frame only rotates
   the cache with bilinear sampling - nothing is allocated per frame and the
   rest of the window is never touched by the spin timer. */

#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

static void EmbRasterize(uint32_t *dst, int n) {
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
static double   g_spinAngle = 0.0;

static void EmbCacheEnsure(int sz) {
    if (g_embCache.src && g_embCache.sz == sz) return;
    free(g_embCache.src);
    g_embCache.src = NULL;
    g_embCache.sz = 0;
    if (sz < 8 || sz > 512) return;
    int srcSz = sz * 4;
    uint32_t *src = (uint32_t *)malloc((size_t)srcSz * srcSz * sizeof(uint32_t));
    if (!src) return;
    EmbRasterize(src, srcSz);
    g_embCache.sz    = sz;
    g_embCache.srcSz = srcSz;
    g_embCache.src   = src;
}

static void EmbSpinDraw(uint32_t *px, int W, int H, int x, int y, int sz, double ang) {
    EmbCacheEnsure(sz);
    const uint32_t *src = g_embCache.src;
    if (!src) return;
    int srcSz = g_embCache.srcSz;
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

/* ---------------- UI ---------------- */

static void EmbRect(RECT *r) {
    SetRect(r, S(20), S(22), S(20) + S(56), S(22) + S(56));
}

static void BtnRect(int i, RECT *r) {
    if (i == BTN_SAVE) SetRect(r, S(470), S(98), S(470) + S(120), S(98) + S(32));
    else SetRect(r, S(20), S(380), S(20) + S(190), S(380) + S(38));
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : COL_INPUT;
    COLORREF edge = (g_hoverBtn == idx) ? COL_HOT : COL_DIV;
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, S(10), S(10));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, primary ? COL_BG : COL_TXT);
    SelectObject(dc, g_fontBtn);
    DrawTextW(dc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static int HitBtn(int x, int y) {
    RECT r;
    POINT pt = { x, y };
    for (int i = 0; i < BTN_COUNT; i++) {
        BtnRect(i, &r);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void DrawDot(HDC dc, int cx, int cy, int rad, COLORREF col) {
    HBRUSH b = CreateSolidBrush(col);
    HPEN p = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    Ellipse(dc, cx - rad, cy - rad, cx + rad + 1, cy + rad + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void PaintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    /* Spin fast-path: only the emblem square is dirty - repaint just it. */
    RECT er;
    EmbRect(&er);
    if (ps.rcPaint.left >= er.left && ps.rcPaint.top >= er.top &&
        ps.rcPaint.right <= er.right && ps.rcPaint.bottom <= er.bottom) {
        int sz = er.right - er.left;
        uint32_t *bits = NULL;
        BITMAPINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = sz;
        bi.bmiHeader.biHeight = -sz;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        if (dib && bits) {
            for (int i = 0; i < sz * sz; i++) bits[i] = RGB(24, 27, 36);
            EmbSpinDraw(bits, sz, sz, 0, 0, sz, g_spinAngle);
            HDC mdc = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mdc, dib);
            BitBlt(dc, er.left, er.top, sz, sz, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            DeleteDC(mdc);
            DeleteObject(dib);
        }
        EndPaint(hwnd, &ps);
        return;
    }

    /* Full paint into a top-down 32-bit DIB (GDI text + emblem pixels). */
    uint32_t *bits = NULL;
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = NULL;
    if (dib && bits) old = SelectObject(mem, dib);

    RECT full = { 0, 0, W, H };
    FillRect(mem, &full, g_bgBrush);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, COL_TXT);
    SelectObject(mem, g_fontTitle);
    RECT t = { S(92), S(20), W - S(20), S(52) };
    DrawTextW(mem, L"Agent Notes", -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTED);
    RECT s = { S(92), S(52), W - S(20), S(84) };
    DrawTextW(mem,
              L"Type a note and press Send to Agent - it lands in the agent's session inbox.",
              -1, &s, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT lbl = { S(20), S(98), S(126), S(130) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Session link", -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT nlbl = { S(20), S(152), W - S(20), S(178) };
    DrawTextW(mem, L"New note", -1, &nlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Soft-slate rounded control surfaces. The RichEdit children paint on
       top of these; the rounded fill shows at the corners. */
    {
        HBRUSH b = CreateSolidBrush(COL_INPUT);
        HPEN p = CreatePen(PS_SOLID, 1, COL_DIV);
        HGDIOBJ ob = SelectObject(mem, b);
        HGDIOBJ op = SelectObject(mem, p);
        RECT fr1 = { S(127), S(95), S(463), S(133) };
        RoundRect(mem, fr1.left, fr1.top, fr1.right, fr1.bottom, S(10), S(10));
        RECT fr2 = { S(17), S(175), S(603), S(371) };
        RoundRect(mem, fr2.left, fr2.top, fr2.right, fr2.bottom, S(14), S(14));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(b);
        DeleteObject(p);
    }

    DrawBtn(mem, BTN_SAVE, L"Save Link", 0);
    DrawBtn(mem, BTN_SEND, L"Send to Agent", 1);

    /* Recent notes: live Sent/Queued state (outbox file exists = queued). */
    RECT rl = { S(20), S(438), W - S(20), S(462) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Recent notes", -1, &rl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    {
        HBRUSH b = CreateSolidBrush(COL_CARD);
        HPEN p = CreatePen(PS_SOLID, 1, COL_CARD_BD);
        HGDIOBJ ob = SelectObject(mem, b);
        HGDIOBJ op = SelectObject(mem, p);
        RECT cardRc = { S(17), S(462), W - S(17), S(550) };
        RoundRect(mem, cardRc.left, cardRc.top, cardRc.right, cardRc.bottom, S(14), S(14));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(b);
        DeleteObject(p);
    }
    SelectObject(mem, g_fontUI);
    for (int i = 0; i < HIST_SHOW && i < g_histN; i++) {
        int y = S(466) + i * S(26);
        wchar_t qp[MAX_PATH + 96];
        _snwprintf(qp, MAX_PATH + 95, L"%s\\%s", g_outboxDir, g_hist[i].name);
        int queued = FileExistsW(qp);
        DrawDot(mem, S(28), y + S(10), S(3), queued ? COL_WARN : COL_GOOD);
        RECT st = { S(42), y, S(42) + S(78), y + S(21) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, g_hist[i].stamp, -1, &st, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT chip = { S(124), y + S(1), S(124) + S(62), y + S(20) };
        HPEN p = CreatePen(PS_SOLID, 1, queued ? COL_WARN : COL_GOOD);
        HGDIOBJ op = SelectObject(mem, p);
        HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
        RoundRect(mem, chip.left, chip.top, chip.right, chip.bottom, S(10), S(10));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(p);
        SetTextColor(mem, queued ? COL_WARN : COL_GOOD);
        DrawTextW(mem, queued ? L"Queued" : L"Sent", -1, &chip,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT sn = { S(198), y, W - S(24), y + S(21) };
        SetTextColor(mem, COL_TXT);
        DrawTextW(mem, g_hist[i].snip, -1, &sn,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (g_histN == 0) {
        RECT e = { S(42), S(466), W - S(24), S(487) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, L"Notes you send will show up here.", -1, &e,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* Status strip: dot + what is going on right now. */
    int sy = H - S(36);
    DrawDot(mem, S(26), sy + S(14), S(4),
            g_lastOk == 0 ? COL_WARN : (g_lastOk == 1 ? COL_GOOD : COL_MUTED));
    RECT fr = { S(42), sy, W - S(20), sy + S(28) };
    SetTextColor(mem, (g_lastOk == 0) ? COL_WARN : ((g_lastOk == 1) ? COL_GOOD : COL_MUTED));
    DrawTextW(mem, g_status, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (bits) {
        /* GDI batches its draws - flush before touching the DIB pixels, or
           the batched fills can land on top of the emblem as a ghost square. */
        GdiFlush();
        EmbSpinDraw(bits, W, H, er.left, er.top, er.right - er.left, g_spinAngle);
    }

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    if (dib) {
        SelectObject(mem, old);
        DeleteObject(dib);
    }
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void NoteSetHint(int on) {
    if (!g_hNote) return;
    if (on) SetWindowTextW(g_hNote, HINT_TEXT);
    else SetWindowTextW(g_hNote, L"");
    g_noteHint = on;
}

static void NowStamp(wchar_t *out, int cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(out, cap - 1, L"%02d-%02d %02d:%02d",
               st.wMonth, st.wDay, st.wHour, st.wMinute);
    out[cap - 1] = L'\0';
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_PAINT:
        PaintAll(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b != g_hoverBtn) {
            g_hoverBtn = b;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b == BTN_SAVE) {
            GetWindowTextW(g_hLink, g_link, 255);
            SaveConfig();
            if (g_link[0]) {
                wcsncpy(g_status, L"Link saved - testing the connection...", 255);
                StartFlush();
            } else {
                wcsncpy(g_status, L"Link cleared.", 255);
            }
            g_status[255] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_SEND) {
            int empty = (g_noteHint || GetWindowTextLengthW(g_hNote) <= 0);
            if (empty) {
                wcsncpy(g_status, L"Nothing to send - type a note first.", 255);
                g_status[255] = L'\0';
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            int n = GetWindowTextLengthW(g_hNote);
            wchar_t *text = (wchar_t *)malloc(((size_t)n + 1) * sizeof(wchar_t));
            if (!text) return 0;
            GetWindowTextW(g_hNote, text, n + 1);
            wchar_t name[80], stamp[16], snip[80];
            if (OutboxAdd(text, name, 80)) {
                NowStamp(stamp, 16);
                HistSnip(text, snip, 80);
                HistAdd(name, stamp, snip);
            }
            free(text);
            NoteSetHint(1);
            SetFocus(g_hNote);
            wcsncpy(g_status, L"Sending...", 255);
            g_status[255] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
            StartFlush();
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == ID_EDIT_NOTE) {
            if (code == EN_SETFOCUS && g_noteHint) {
                NoteSetHint(0);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (code == EN_KILLFOCUS && GetWindowTextLengthW(g_hNote) <= 0) {
                NoteSetHint(1);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC h = (HDC)wp;
        if ((HWND)lp == g_hNote && g_noteHint) {
            SetTextColor(h, COL_MUTED);
        } else {
            SetTextColor(h, COL_TXT);
        }
        SetBkColor(h, COL_INPUT);
        return (LRESULT)g_bgBrush;
    }
    case WM_TIMER:
        if (wp == TIMER_SPIN) {
            /* One turn per 30 s idle; per 8 s while a send is in flight. */
            double rate = g_sending ? (6.283185307179586 / 8.0)
                                    : (6.283185307179586 / 30.0);
            g_spinAngle += rate * 0.05;
            if (g_spinAngle > 6.283185307179586) g_spinAngle -= 6.283185307179586;
            RECT er;
            EmbRect(&er);
            InvalidateRect(hwnd, &er, FALSE);
        } else if (wp == TIMER_FLUSH) {
            if (OutboxCount() > 0 && !g_sending) StartFlush();
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_FLUSH);
        KillTimer(hwnd, TIMER_SPIN);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---------------- entry ---------------- */

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;
    g_hInst = hInst;

    CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        g_showEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT);
        if (g_showEvent) SetEvent(g_showEvent);
        return 0;
    }
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT);

    InitPaths();
    LoadConfig();
    HistLoad();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"AgentNotesWnd";
    RegisterClassExW(&wc);

    /* Create first at a nominal size, then size to the true DPI: S() still
       uses the default 96 dpi at this point, so sizing the window before
       GetWinDpi would cut the layout off on any scaled display. */
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE_W, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             S(620), S(600),
                             NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    g_dpi = GetWinDpi(g_hwnd);
    {
        /* Size to the true DPI, then shrink uniformly if that would not fit
           the work area - the layout can never be cut off on any display. */
        RECT wrc = { 0, 0, S(620), S(600) };
        AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        int workW = (wa.right - wa.left) - S(32);
        int workH = (wa.bottom - wa.top) - S(32);
        int winW = wrc.right - wrc.left, winH = wrc.bottom - wrc.top;
        if (winW > workW || winH > workH) {
            int fx = MulDiv(100, workW, winW);
            int fy = MulDiv(100, workH, winH);
            g_fit = (fx < fy) ? fx : fy;
            if (g_fit < 50) g_fit = 50;
            if (g_fit > 100) g_fit = 100;
            RECT w2 = { 0, 0, S(620), S(600) };
            AdjustWindowRectEx(&w2, WS_OVERLAPPEDWINDOW, FALSE, 0);
            wrc = w2;
        }
        SetWindowPos(g_hwnd, NULL, 0, 0,
                     wrc.right - wrc.left, wrc.bottom - wrc.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ApplyModernWindowFrame(g_hwnd);
    g_bgBrush = CreateSolidBrush(COL_BG);
    g_fontUI = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(22), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontBtn = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

    /* rich-edits give us dark, standard multiline/single-line inputs */
    LoadLibraryW(L"msftedit.dll");
    g_hLink = CreateWindowExW(0, L"RichEdit50W", g_link,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              S(130), S(98), S(330), S(32),
                              g_hwnd, (HMENU)(INT_PTR)ID_EDIT_LINK, hInst, NULL);
    g_hNote = CreateWindowExW(0, L"RichEdit50W", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                              ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                              S(20), S(178), S(580), S(190),
                              g_hwnd, (HMENU)(INT_PTR)ID_EDIT_NOTE, hInst, NULL);
    if (g_hLink) {
        SendMessageW(g_hLink, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_INPUT);
        SendMessageW(g_hLink, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hLink, EM_SETLIMITTEXT, 0, 255);
    }
    if (g_hNote) {
        SendMessageW(g_hNote, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_INPUT);
        SendMessageW(g_hNote, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hNote, EM_SETLIMITTEXT, 0, 4000);
        NoteSetHint(1);
    }

    if (OutboxCount() > 0)
        _snwprintf(g_status, 255, L"%d queued note(s) found - delivering when reachable.",
                   OutboxCount());
    else if (g_link[0])
        wcsncpy(g_status, L"Ready - type a note and press Send to Agent.", 255);

    SetTimer(g_hwnd, TIMER_FLUSH, 60000, NULL);
    SetTimer(g_hwnd, TIMER_SPIN, 50, NULL);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    for (;;) {
        DWORD w = MsgWaitForMultipleObjects(1, &g_showEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (w == WAIT_OBJECT_0) {
            /* second-launch nudge: bring the window forward */
            if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
            SetForegroundWindow(g_hwnd);
            continue;
        }
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            if (!IsDialogMessageW(g_hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
}
