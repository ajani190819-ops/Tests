#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define HUB_VERSION       "2.1.0"
#define HUB_VERSION_W     L"2.1.0"
#define APP_TITLE         L"Project Hub"
#define SINGLE_MUTEX_NAME L"Local\\Tests_ProjectHub_SingleInstance_v2"
#define SHOW_EVENT_NAME   L"Local\\Tests_ProjectHub_ShowWindow_v2"

#define REPO_OWNER_W      L"ajani190819-ops"
#define REPO_NAME_W       L"Tests"
#define FALLBACK_BRANCH_W L"arena/e895b94f-tests"

#define WM_HUB_REFRESH    (WM_APP + 10)
#define WM_HUB_STATUS     (WM_APP + 11)
#define WM_HUB_SELFUPD    (WM_APP + 12)

/* Action IDs */
#define ID_BTN_REFRESH_ALL   1001
#define ID_BTN_UPDATE_ALL    1002
#define ID_BTN_MORE_MENU     1003
#define ID_MENU_SHORTCUT     1011
#define ID_MENU_SELF_UPDATE  1012
#define ID_MENU_SUITE_DIR    1013

#define ID_BTG_OPEN          1101
#define ID_BTG_UPDATE        1102
#define ID_BTG_STOP          1103
#define ID_BTG_FOLDER        1104

#define ID_KBL_OPEN          1201
#define ID_KBL_RESTART       1202
#define ID_KBL_UPDATE        1203
#define ID_KBL_FOLDER        1204

#define ID_HUD_UPDATE        1301
#define ID_HUD_PICK          1302
#define ID_HUD_FOLDER        1303

#define ID_ORCA_UPDATE       1401
#define ID_ORCA_MENU         1402
#define ID_ORCA_FOLDER       1403

#define ID_ARENA_OPEN        1501
#define ID_ARENA_SYNC        1502
#define ID_ARENA_FOLDER      1503

/* Modern dark slate palette (matches BluetoothGuard.exe) */
#define COL_BG               RGB(15, 23, 42)    /* #0F172A */
#define COL_HEADER_BG        RGB(15, 23, 42)    /* #0F172A */
#define COL_FOOTER_BG        RGB(12, 19, 35)    /* #0C1323 */
#define COL_CARD_BG          RGB(30, 41, 59)    /* #1E293B */
#define COL_CARD_HOVER       RGB(36, 49, 70)    /* #243146 */
#define COL_CARD_BORDER      RGB(51, 65, 85)    /* #334155 */
#define COL_DIVIDER          RGB(30, 41, 59)    /* #1E293B */
#define COL_TEXT_PRIMARY     RGB(248, 250, 252) /* #F8FAFC */
#define COL_TEXT_SECONDARY   RGB(148, 163, 184) /* #94A3B8 */
#define COL_TEXT_MUTED       RGB(100, 116, 139) /* #64748B */

#define COL_ACCENT           RGB(14, 165, 233)  /* #0EA5E9 */
#define COL_ACCENT_HOVER     RGB(56, 189, 248)  /* #38BDF8 */
#define COL_BTN_BG           RGB(42, 56, 78)    /* #2A384E */
#define COL_BTN_HOVER        RGB(58, 76, 104)   /* #3A4C68 */
#define COL_BTN_BORDER       RGB(71, 85, 105)   /* #475569 */
#define COL_WARN_BTN         RGB(180, 83, 9)    /* #B45309 */
#define COL_WARN_BTN_HOVER   RGB(217, 119, 6)   /* #D97706 */

#define COL_OK_TEXT          RGB(52, 211, 153)  /* #34D399 */
#define COL_OK_BG            RGB(6, 58, 46)
#define COL_WARN_TEXT        RGB(251, 191, 36)  /* #FBBF24 */
#define COL_WARN_BG          RGB(69, 42, 10)
#define COL_ERR_TEXT         RGB(248, 113, 113) /* #F87171 */
#define COL_ERR_BG           RGB(69, 18, 24)
#define COL_IDLE_TEXT        RGB(148, 163, 184) /* #94A3B8 */
#define COL_IDLE_BG          RGB(39, 52, 73)

enum {
    BADGE_NEUTRAL = 0,
    BADGE_OK      = 1,
    BADGE_WARN    = 2,
    BADGE_DANGER  = 3
};

enum {
    BTN_SECONDARY = 0,
    BTN_PRIMARY   = 1,
    BTN_WARNING   = 2
};

enum {
    TASK_NONE = 0,
    TASK_CHECK_REMOTE,
    TASK_INSTALL_BTG,
    TASK_OPEN_OR_INSTALL_BTG,
    TASK_INSTALL_KBL,
    TASK_INSTALL_HUD,
    TASK_INSTALL_ORCA,
    TASK_SYNC_ARENA,
    TASK_UPDATE_ALL,
    TASK_SELF_UPDATE
};

typedef struct {
    int            id;
    const wchar_t *label;
    int            style;
    int            widthPx;
    RECT           rc;
} ActionBtn;

typedef struct {
    const wchar_t *title;
    const wchar_t *subtitle;
    wchar_t        badge[48];
    int            badgeKind;
    wchar_t        detail[256];
    ActionBtn      btns[4];
    int            btnCount;
    RECT           rcCard;
} ProjectCard;

static HWND             g_hwnd        = NULL;
static HFONT            g_fontHero    = NULL;
static HFONT            g_fontTitle   = NULL;
static HFONT            g_fontBody    = NULL;
static HFONT            g_fontBtn     = NULL;
static HFONT            g_fontBadge   = NULL;
static HICON            g_appIcon     = NULL;
static HANDLE           g_showEvent   = NULL;
static CRITICAL_SECTION g_cs;

static ActionBtn        g_topBtns[3] = {
    { ID_BTN_REFRESH_ALL, L"Refresh",    BTN_SECONDARY, 86,  {0,0,0,0} },
    { ID_BTN_UPDATE_ALL,  L"Update All", BTN_PRIMARY,   104, {0,0,0,0} },
    { ID_BTN_MORE_MENU,   L"More \x25BE", BTN_SECONDARY, 74, {0,0,0,0} }
};

static ProjectCard      g_cards[5];
static int              g_hoverId     = 0;
static int              g_scrollY     = 0;
static int              g_maxScrollY  = 0;

static wchar_t          g_exePath[MAX_PATH];
static wchar_t          g_exeDir[MAX_PATH];
static wchar_t          g_localApp[MAX_PATH];
static wchar_t          g_roamApp[MAX_PATH];
static wchar_t          g_suiteDir[MAX_PATH];
static wchar_t          g_hubCfgDir[MAX_PATH];
static wchar_t          g_btgDir[MAX_PATH];
static wchar_t          g_kblDir[MAX_PATH];
static wchar_t          g_modsDir[MAX_PATH];
static wchar_t          g_orcaBatDir[MAX_PATH];
static wchar_t          g_arenaDir[MAX_PATH];

/* Shared remote state protected by g_cs */
static wchar_t          g_activeBranch[64]  = L"";
static char             g_remWaveVer[32]    = "";
static char             g_remInfillVer[32]  = "";
static char             g_remArenaVer[32]   = "";
static wchar_t          g_remHudName[128]   = L"";
static wchar_t          g_remHudPath[512]   = L"";
static DWORD            g_remHudSize        = 0;
static wchar_t          g_statusMsg[512]    = L"Checking local apps and GitHub updates...";
static int              g_progressVal       = 0;

/* Worker queue */
static int              g_taskQueue[32];
static int              g_qHead             = 0;
static int              g_qTail             = 0;
static HANDLE           g_qSem              = NULL;
static volatile LONG    g_running           = 1;

typedef UINT    (WINAPI *PFN_GetDpiForWindow)(HWND);
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static int GetWinDpi(HWND hwnd) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        PFN_GetDpiForWindow fn = (PFN_GetDpiForWindow)(void *)GetProcAddress(u32, "GetDpiForWindow");
        if (fn && hwnd) {
            UINT d = fn(hwnd);
            if (d >= 96) return (int)d;
        }
    }
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi >= 96 ? dpi : 96;
}

static int S(int px, int dpi) { return MulDiv(px, dpi, 96); }

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD roundPref = 2; /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &roundPref, sizeof(roundPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
    }
    FreeLibrary(dwm);
}

static void RebuildFonts(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    if (g_fontHero)  DeleteObject(g_fontHero);
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontBody)  DeleteObject(g_fontBody);
    if (g_fontBtn)   DeleteObject(g_fontBtn);
    if (g_fontBadge) DeleteObject(g_fontBadge);

    g_fontHero  = CreateFontW(-S(19, dpi), 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBody  = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static BOOL FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static BOOL DirExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}

static void EnsureDirTree(const wchar_t *dir) {
    wchar_t tmp[MAX_PATH];
    wcsncpy(tmp, dir, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = L'\0';
    for (wchar_t *p = tmp + 3; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t ch = *p;
            *p = L'\0';
            CreateDirectoryW(tmp, NULL);
            *p = ch;
        }
    }
    CreateDirectoryW(tmp, NULL);
}

static void SetFooterStatus(const wchar_t *msg, int pct) {
    EnterCriticalSection(&g_cs);
    wcsncpy(g_statusMsg, msg, 511);
    g_statusMsg[511] = L'\0';
    g_progressVal = pct;
    LeaveCriticalSection(&g_cs);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_STATUS, 0, 0);
}

static char *ReadTextFileUtf8(const wchar_t *path, DWORD *outLen) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(hf, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 8 * 1024 * 1024) { CloseHandle(hf); return NULL; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { CloseHandle(hf); return NULL; }
    DWORD rd = 0;
    if (!ReadFile(hf, buf, sz, &rd, NULL)) { free(buf); CloseHandle(hf); return NULL; }
    buf[rd] = '\0';
    CloseHandle(hf);
    if (outLen) *outLen = rd;
    return buf;
}

static BOOL WriteBinaryFileAtomic(const wchar_t *dest, const BYTE *data, DWORD len) {
    wchar_t dir[MAX_PATH];
    wcsncpy(dir, dest, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) { *slash = L'\0'; EnsureDirTree(dir); }

    wchar_t tmp[MAX_PATH];
    _snwprintf(tmp, MAX_PATH, L"%s.tmp", dest);
    HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;
    DWORD wr = 0;
    BOOL ok = WriteFile(hf, data, len, &wr, NULL) && (wr == len);
    CloseHandle(hf);
    if (!ok) { DeleteFileW(tmp); return FALSE; }
    if (!MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return FALSE;
    }
    return TRUE;
}

/* HTTPS GET into memory via WinHTTP (automatically follows GitHub 301/302 redirects) */
static BYTE *HttpGetBuffer(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    if (outSize) *outSize = 0;
    if (outStatus) *outStatus = 0;

    HINTERNET hSession = WinHttpOpen(L"Tests-ProjectHub/2.1",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;
    WinHttpSetTimeouts(hSession, 8000, 10000, 15000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }

    BOOL sent = WinHttpSendRequest(hReq,
                                   L"User-Agent: Tests-ProjectHub/2.1\r\nAccept: */*\r\n",
                                   (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD statusCode = 0, scSize = sizeof(statusCode);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);
    if (outStatus) *outStatus = (int)statusCode;
    if (statusCode != 200) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD cap = 65536, len = 0;
    BYTE *buf = (BYTE *)malloc(cap);
    if (!buf) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return NULL;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail) || avail == 0) break;
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            BYTE *nb = (BYTE *)realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
        }
        DWORD rd = 0;
        if (!WinHttpReadData(hReq, buf + len, avail, &rd) || rd == 0) break;
        len += rd;
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (buf) {
        buf[len] = 0;
        if (outSize) *outSize = len;
    }
    return buf;
}

static BOOL DownloadRawRepoFile(const wchar_t *branch, const wchar_t *relUrl, const wchar_t *destPath) {
    wchar_t path[512];
    _snwprintf(path, 511, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, branch, relUrl);
    DWORD sz = 0;
    int st = 0;
    BYTE *data = HttpGetBuffer(L"raw.githubusercontent.com", path, &sz, &st);
    if (!data || st != 200 || sz < 8) {
        free(data);
        return FALSE;
    }
    BOOL ok = WriteBinaryFileAtomic(destPath, data, sz);
    free(data);
    return ok;
}

static void EnsureActiveBranch(wchar_t *outBranch, size_t cap) {
    EnterCriticalSection(&g_cs);
    if (g_activeBranch[0]) {
        wcsncpy(outBranch, g_activeBranch, cap - 1);
        outBranch[cap - 1] = L'\0';
        LeaveCriticalSection(&g_cs);
        return;
    }
    LeaveCriticalSection(&g_cs);

    DWORD sz = 0;
    int st = 0;
    wchar_t probePath[256];
    _snwprintf(probePath, 255, L"/%s/%s/main/bluetooth-guard/README.md", REPO_OWNER_W, REPO_NAME_W);
    BYTE *probe = HttpGetBuffer(L"raw.githubusercontent.com", probePath, &sz, &st);
    free(probe);

    const wchar_t *chosen = (st == 200 && sz > 20) ? L"main" : FALLBACK_BRANCH_W;
    EnterCriticalSection(&g_cs);
    wcsncpy(g_activeBranch, chosen, 63);
    g_activeBranch[63] = L'\0';
    LeaveCriticalSection(&g_cs);

    wcsncpy(outBranch, chosen, cap - 1);
    outBranch[cap - 1] = L'\0';
}

static BOOL ExtractQuotedAfter(const char *hay, const char *key, char *out, size_t cap) {
    const char *p = strstr(hay, key);
    if (!p) return FALSE;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '=')) p++;
    char quote = *p;
    if (quote != '"' && quote != '\'') return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != quote && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

static void InitAllPaths(void) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    wcsncpy(g_exeDir, g_exePath, MAX_PATH - 1);
    wchar_t *sl = wcsrchr(g_exeDir, L'\\');
    if (sl) *sl = L'\0';

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, g_localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", g_localApp, MAX_PATH);
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, g_roamApp)))
        GetEnvironmentVariableW(L"APPDATA", g_roamApp, MAX_PATH);

    _snwprintf(g_hubCfgDir, MAX_PATH, L"%s\\ProjectHub", g_localApp);
    _snwprintf(g_suiteDir,  MAX_PATH, L"%s\\Programs\\TestsSuite", g_localApp);
    EnsureDirTree(g_hubCfgDir);
    EnsureDirTree(g_suiteDir);

    wchar_t parent[MAX_PATH];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    wchar_t *psl = wcsrchr(parent, L'\\');
    if (psl) *psl = L'\0';

    wchar_t cand[MAX_PATH];
    _snwprintf(cand, MAX_PATH, L"%s\\bluetooth-guard\\BluetoothGuard.exe", parent);
    if (FileExistsW(cand))
        _snwprintf(g_btgDir, MAX_PATH, L"%s\\bluetooth-guard", parent);
    else if (FileExistsW(L"BluetoothGuard.exe"))
        wcsncpy(g_btgDir, g_exeDir, MAX_PATH - 1);
    else
        _snwprintf(g_btgDir, MAX_PATH, L"%s\\BluetoothGuard", g_suiteDir);

    _snwprintf(cand, MAX_PATH, L"%s\\keyboard-lighting\\Tray.ps1", parent);
    if (FileExistsW(cand))
        _snwprintf(g_kblDir, MAX_PATH, L"%s\\keyboard-lighting", parent);
    else
        _snwprintf(g_kblDir, MAX_PATH, L"%s\\KeyboardLighting", g_suiteDir);

    _snwprintf(cand, MAX_PATH, L"%s\\Orca-Plugins.bat", parent);
    if (FileExistsW(cand))
        wcsncpy(g_orcaBatDir, parent, MAX_PATH - 1);
    else
        _snwprintf(g_orcaBatDir, MAX_PATH, L"%s\\OrcaPlugins", g_suiteDir);

    _snwprintf(cand, MAX_PATH, L"%s\\arena-link-windows\\package.json", parent);
    if (FileExistsW(cand))
        _snwprintf(g_arenaDir, MAX_PATH, L"%s\\arena-link-windows", parent);
    else
        _snwprintf(g_arenaDir, MAX_PATH, L"%s\\ArenaLink", g_suiteDir);

    wchar_t hudTargetTxt[MAX_PATH];
    _snwprintf(hudTargetTxt, MAX_PATH, L"%s\\SpatialHudUpdater\\target-directory.txt", g_localApp);
    char *savedMods = ReadTextFileUtf8(hudTargetTxt, NULL);
    if (savedMods) {
        unsigned char *s = (unsigned char *)savedMods;
        if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) s += 3;
        size_t n = strlen((char *)s);
        while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = '\0';
        if (n > 2) MultiByteToWideChar(CP_UTF8, 0, (char *)s, -1, g_modsDir, MAX_PATH);
        free(savedMods);
    }
    if (!g_modsDir[0]) {
        _snwprintf(g_modsDir, MAX_PATH, L"%s\\ModrinthApp\\profiles\\F5W\\mods", g_roamApp);
    }
}

/* Check lock file liveness: returns 0 = stopped, 1 = running, 2 = hung */
static int CheckPidLockStatus(const wchar_t *lockFile, double maxAgeSec, int *outField2, int *outField3) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(lockFile, GetFileExInfoStandard, &fad)) return 0;

    char *txt = ReadTextFileUtf8(lockFile, NULL);
    if (!txt) return 0;
    unsigned long pid = 0;
    int f1 = 0, f2 = 0, f3 = 0;
    sscanf(txt, "%lu|%d|%d|%d", &pid, &f1, &f2, &f3);
    free(txt);
    if (outField2) *outField2 = f2;
    if (outField3) *outField3 = f3;
    if (pid == 0) return 0;

    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hp) return 0;
    DWORD exitCode = 0;
    BOOL alive = GetExitCodeProcess(hp, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hp);
    if (!alive) return 0;

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER uNow, uMod;
    uNow.LowPart = nowFt.dwLowDateTime; uNow.HighPart = nowFt.dwHighDateTime;
    uMod.LowPart = fad.ftLastWriteTime.dwLowDateTime; uMod.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    double ageSec = (uNow.QuadPart > uMod.QuadPart)
                  ? (double)(uNow.QuadPart - uMod.QuadPart) / 10000000.0
                  : 0.0;
    return (ageSec > maxAgeSec) ? 2 : 1;
}

static BOOL CreateShortcutLink(const wchar_t *lnkPath, const wchar_t *targetExe,
                               const wchar_t *workDir, const wchar_t *desc) {
    IShellLinkW *psl = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IShellLinkW, (void **)&psl);
    if (FAILED(hr) || !psl) return FALSE;
    IShellLinkW_SetPath(psl, targetExe);
    IShellLinkW_SetWorkingDirectory(psl, workDir);
    IShellLinkW_SetDescription(psl, desc);

    IPersistFile *ppf = NULL;
    BOOL ok = FALSE;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        ok = SUCCEEDED(IPersistFile_Save(ppf, lnkPath, TRUE));
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return ok;
}

static void InitCardDefinitions(void) {
    ZeroMemory(g_cards, sizeof(g_cards));

    g_cards[0].title    = L"Bluetooth Guard";
    g_cards[0].subtitle = L"Phone audio protection";
    g_cards[0].btnCount = 4;
    g_cards[0].btns[0]  = (ActionBtn){ ID_BTG_OPEN,   L"Open",   BTN_PRIMARY,   68, {0,0,0,0} };
    g_cards[0].btns[1]  = (ActionBtn){ ID_BTG_UPDATE, L"Update", BTN_SECONDARY, 72, {0,0,0,0} };
    g_cards[0].btns[2]  = (ActionBtn){ ID_BTG_STOP,   L"Stop",   BTN_SECONDARY, 60, {0,0,0,0} };
    g_cards[0].btns[3]  = (ActionBtn){ ID_BTG_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[1].title    = L"Keyboard Lighting";
    g_cards[1].subtitle = L"ROG Strix G16 RGB tray";
    g_cards[1].btnCount = 4;
    g_cards[1].btns[0]  = (ActionBtn){ ID_KBL_OPEN,    L"Open",     BTN_SECONDARY, 64, {0,0,0,0} };
    g_cards[1].btns[1]  = (ActionBtn){ ID_KBL_RESTART, L"Unfreeze", BTN_SECONDARY, 82, {0,0,0,0} };
    g_cards[1].btns[2]  = (ActionBtn){ ID_KBL_UPDATE,  L"Update",   BTN_SECONDARY, 72, {0,0,0,0} };
    g_cards[1].btns[3]  = (ActionBtn){ ID_KBL_FOLDER,  L"Folder",   BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[2].title    = L"Spatial HUD";
    g_cards[2].subtitle = L"Minecraft 26.3 Fabric mod";
    g_cards[2].btnCount = 3;
    g_cards[2].btns[0]  = (ActionBtn){ ID_HUD_UPDATE, L"Update Mod",  BTN_PRIMARY,   96, {0,0,0,0} };
    g_cards[2].btns[1]  = (ActionBtn){ ID_HUD_PICK,   L"Mods Dir...", BTN_SECONDARY, 92, {0,0,0,0} };
    g_cards[2].btns[2]  = (ActionBtn){ ID_HUD_FOLDER, L"Folder",      BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[3].title    = L"OrcaSlicer Plugins";
    g_cards[3].subtitle = L"Wave Overhangs & Unlayered Infill";
    g_cards[3].btnCount = 3;
    g_cards[3].btns[0]  = (ActionBtn){ ID_ORCA_UPDATE, L"Update",      BTN_PRIMARY,   76,  {0,0,0,0} };
    g_cards[3].btns[1]  = (ActionBtn){ ID_ORCA_MENU,   L"Branch Menu", BTN_SECONDARY, 100, {0,0,0,0} };
    g_cards[3].btns[2]  = (ActionBtn){ ID_ORCA_FOLDER, L"Folder",      BTN_SECONDARY, 68,  {0,0,0,0} };

    g_cards[4].title    = L"Arena Link";
    g_cards[4].subtitle = L"Local Git workspace bridge";
    g_cards[4].btnCount = 3;
    g_cards[4].btns[0]  = (ActionBtn){ ID_ARENA_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[4].btns[1]  = (ActionBtn){ ID_ARENA_SYNC,   L"Sync",   BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[4].btns[2]  = (ActionBtn){ ID_ARENA_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0} };
}

static void RefreshProjectStatuses(void) {
    char remWave[32], remInfill[32], remArena[32];
    wchar_t remHud[128];
    DWORD remHudSz = 0;

    EnterCriticalSection(&g_cs);
    strncpy(remWave, g_remWaveVer, 31); remWave[31] = '\0';
    strncpy(remInfill, g_remInfillVer, 31); remInfill[31] = '\0';
    strncpy(remArena, g_remArenaVer, 31); remArena[31] = '\0';
    wcsncpy(remHud, g_remHudName, 127); remHud[127] = L'\0';
    remHudSz = g_remHudSize;
    LeaveCriticalSection(&g_cs);

    wchar_t path[MAX_PATH];

    /* 1. Bluetooth Guard */
    _snwprintf(path, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    BOOL hasBtg = FileExistsW(path);
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    int blocked = 0, total = 0;
    int btgState = CheckPidLockStatus(btgLock, 12.0, &blocked, &total);
    if (btgState == 1) {
        wcsncpy(g_cards[0].badge, L"Running", 47);
        g_cards[0].badgeKind = BADGE_OK;
        g_cards[0].btns[0].label = L"Open";
        g_cards[0].btns[0].style = BTN_SECONDARY;
        _snwprintf(g_cards[0].detail, 255,
                   L"Guarding Bluetooth  \x00B7  %d auto-connect OFF (%d paired)", blocked, total);
    } else if (hasBtg) {
        wcsncpy(g_cards[0].badge, L"Stopped", 47);
        g_cards[0].badgeKind = BADGE_NEUTRAL;
        g_cards[0].btns[0].label = L"Start";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Installed locally  \x00B7  Click Start to launch tray guard", 255);
    } else {
        wcsncpy(g_cards[0].badge, L"Not Installed", 47);
        g_cards[0].badgeKind = BADGE_WARN;
        g_cards[0].btns[0].label = L"Install";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Click Install to download & launch BluetoothGuard.exe", 255);
    }

    /* 2. Keyboard Lighting */
    _snwprintf(path, MAX_PATH, L"%s\\Tray.ps1", g_kblDir);
    BOOL hasKbl = FileExistsW(path);
    wchar_t kblLock[MAX_PATH];
    _snwprintf(kblLock, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    int kblState = CheckPidLockStatus(kblLock, 12.0, NULL, NULL);
    if (kblState == 2) {
        wcsncpy(g_cards[1].badge, L"Frozen", 47);
        g_cards[1].badgeKind = BADGE_DANGER;
        g_cards[1].btns[1].style = BTN_WARNING;
        wcsncpy(g_cards[1].detail, L"Tray stopped responding  \x00B7  Click Unfreeze to restart cleanly", 255);
    } else if (kblState == 1) {
        wcsncpy(g_cards[1].badge, L"Running", 47);
        g_cards[1].badgeKind = BADGE_OK;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"Active in system tray", 255);
    } else if (hasKbl) {
        wcsncpy(g_cards[1].badge, L"Stopped", 47);
        g_cards[1].badgeKind = BADGE_NEUTRAL;
        g_cards[1].btns[0].style = BTN_PRIMARY;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"Installed locally  \x00B7  Click Open to start", 255);
    } else {
        wcsncpy(g_cards[1].badge, L"Not Installed", 47);
        g_cards[1].badgeKind = BADGE_NEUTRAL;
        g_cards[1].btns[2].style = BTN_PRIMARY;
        wcsncpy(g_cards[1].detail, L"Click Update to download & configure Keyboard Lighting", 255);
    }

    /* 3. Spatial HUD */
    wchar_t searchPat[MAX_PATH], foundJar[128] = L"";
    DWORD foundSize = 0;
    _snwprintf(searchPat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(searchPat, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        wcsncpy(foundJar, fd.cFileName, 127);
        foundSize = fd.nFileSizeLow;
        FindClose(hf);
    }
    if (!foundJar[0]) {
        wcsncpy(g_cards[2].badge, L"Not Installed", 47);
        g_cards[2].badgeKind = BADGE_NEUTRAL;
        g_cards[2].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[2].detail, L"No JAR in selected mods folder  \x00B7  Click Update Mod to install", 255);
    } else if (remHudSz > 0 && foundSize != remHudSz) {
        wcsncpy(g_cards[2].badge, L"Update Ready", 47);
        g_cards[2].badgeKind = BADGE_WARN;
        g_cards[2].btns[0].style = BTN_PRIMARY;
        _snwprintf(g_cards[2].detail, 255, L"%s (%lu KB)  \x2192  Latest: %s (%lu KB)",
                   foundJar, (unsigned long)(foundSize / 1024), remHud, (unsigned long)(remHudSz / 1024));
    } else {
        wcsncpy(g_cards[2].badge, L"Up to Date", 47);
        g_cards[2].badgeKind = BADGE_OK;
        g_cards[2].btns[0].style = BTN_SECONDARY;
        _snwprintf(g_cards[2].detail, 255, L"%s (%lu KB)", foundJar, (unsigned long)(foundSize / 1024));
    }

    /* 4. OrcaSlicer Plugins */
    char locWave[32] = "", locInfill[32] = "";
    const wchar_t *probeRel[] = {
        L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py",
        L"%s\\OrcaSlicer\\plugins\\wave_overhangs_orca.py"
    };
    for (int i = 0; i < 2 && !locWave[0]; i++) {
        _snwprintf(path, MAX_PATH, probeRel[i], g_roamApp);
        char *py = ReadTextFileUtf8(path, NULL);
        if (py) { ExtractQuotedAfter(py, "PLUGIN_VERSION", locWave, sizeof(locWave)); free(py); }
    }
    const wchar_t *probeInf[] = {
        L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py",
        L"%s\\OrcaSlicer\\plugins\\unlayered_infill_orca.py"
    };
    for (int i = 0; i < 2 && !locInfill[0]; i++) {
        _snwprintf(path, MAX_PATH, probeInf[i], g_roamApp);
        char *py = ReadTextFileUtf8(path, NULL);
        if (py) { ExtractQuotedAfter(py, "PLUGIN_VERSION", locInfill, sizeof(locInfill)); free(py); }
    }
    if (!locWave[0] && !locInfill[0]) {
        wcsncpy(g_cards[3].badge, L"Not Installed", 47);
        g_cards[3].badgeKind = BADGE_NEUTRAL;
        g_cards[3].btns[0].style = BTN_PRIMARY;
        _snwprintf(g_cards[3].detail, 255, L"Latest on GitHub: Wave Overhangs %S  \x00B7  Unlayered Infill %S",
                   remWave[0] ? remWave : "...", remInfill[0] ? remInfill : "...");
    } else {
        BOOL upd = (remWave[0] && strcmp(locWave, remWave) != 0) ||
                   (remInfill[0] && strcmp(locInfill, remInfill) != 0);
        wcsncpy(g_cards[3].badge, upd ? L"Update Ready" : L"Up to Date", 47);
        g_cards[3].badgeKind = upd ? BADGE_WARN : BADGE_OK;
        g_cards[3].btns[0].style = upd ? BTN_PRIMARY : BTN_SECONDARY;
        _snwprintf(g_cards[3].detail, 255, L"Wave Overhangs %S  \x00B7  Unlayered Infill %S",
                   locWave[0] ? locWave : "none", locInfill[0] ? locInfill : "none");
    }

    /* 5. Arena Link */
    char locArena[32] = "";
    _snwprintf(path, MAX_PATH, L"%s\\package.json", g_arenaDir);
    char *pkg = ReadTextFileUtf8(path, NULL);
    if (pkg) { ExtractQuotedAfter(pkg, "\"version\"", locArena, sizeof(locArena)); free(pkg); }

    int linkedRepos = 0;
    _snwprintf(path, MAX_PATH, L"%s\\arena-link-windows\\arena-link-state.json", g_roamApp);
    char *stJson = ReadTextFileUtf8(path, NULL);
    if (stJson) {
        const char *p = stJson;
        while ((p = strstr(p, "\"localPath\"")) != NULL) { linkedRepos++; p += 11; }
        free(stJson);
    }
    if (locArena[0]) {
        BOOL upd = (remArena[0] && strcmp(locArena, remArena) != 0);
        wcsncpy(g_cards[4].badge, upd ? L"Update Ready" : L"Ready", 47);
        g_cards[4].badgeKind = upd ? BADGE_WARN : BADGE_OK;
        g_cards[4].btns[1].style = upd ? BTN_PRIMARY : BTN_SECONDARY;
        _snwprintf(g_cards[4].detail, 255, L"Source v%S  \x00B7  %d linked Git workspace%s",
                   locArena, linkedRepos, linkedRepos == 1 ? L"" : L"s");
    } else {
        wcsncpy(g_cards[4].badge, L"Sync Available", 47);
        g_cards[4].badgeKind = BADGE_NEUTRAL;
        g_cards[4].btns[1].style = BTN_PRIMARY;
        _snwprintf(g_cards[4].detail, 255, L"%d linked Git workspace%s  \x00B7  Click Sync for latest files",
                   linkedRepos, linkedRepos == 1 ? L"" : L"s");
    }

    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static void WorkerCheckRemote(void) {
    wchar_t branch[64];
    SetFooterStatus(L"Checking GitHub for project versions & updates...", 20);
    EnsureActiveBranch(branch, 64);

    wchar_t p[256];
    DWORD sz = 0; int st = 0;
    _snwprintf(p, 255, L"/%s/%s/%s/plugins.json", REPO_OWNER_W, REPO_NAME_W, branch);
    char *manifest = (char *)HttpGetBuffer(L"raw.githubusercontent.com", p, &sz, &st);
    if (manifest && st == 200) {
        char wv[32] = "", iv[32] = "";
        const char *wpos = strstr(manifest, "\"wave-overhangs\"");
        if (wpos) ExtractQuotedAfter(wpos, "\"version\"", wv, sizeof(wv));
        const char *ipos = strstr(manifest, "\"unlayered-infill\"");
        if (ipos) ExtractQuotedAfter(ipos, "\"version\"", iv, sizeof(iv));
        EnterCriticalSection(&g_cs);
        if (wv[0]) strncpy(g_remWaveVer, wv, 31);
        if (iv[0]) strncpy(g_remInfillVer, iv, 31);
        LeaveCriticalSection(&g_cs);
    }
    free(manifest);
    SetFooterStatus(L"Checking Arena Link & Spatial HUD releases...", 60);

    _snwprintf(p, 255, L"/%s/%s/%s/arena-link-windows/package.json", REPO_OWNER_W, REPO_NAME_W, branch);
    char *apkg = (char *)HttpGetBuffer(L"raw.githubusercontent.com", p, &sz, &st);
    if (apkg && st == 200) {
        char av[32] = "";
        if (ExtractQuotedAfter(apkg, "\"version\"", av, sizeof(av))) {
            EnterCriticalSection(&g_cs);
            strncpy(g_remArenaVer, av, 31);
            LeaveCriticalSection(&g_cs);
        }
    }
    free(apkg);

    _snwprintf(p, 255, L"/repos/%s/%s/releases/tags/spatial-hud-latest", REPO_OWNER_W, REPO_NAME_W);
    char *rel = (char *)HttpGetBuffer(L"api.github.com", p, &sz, &st);
    if (rel && st == 200) {
        const char *dlPos = strstr(rel, "\"browser_download_url\"");
        if (dlPos) {
            char urlUtf8[512] = "";
            if (ExtractQuotedAfter(dlPos, "\"browser_download_url\"", urlUtf8, sizeof(urlUtf8)) &&
                strncmp(urlUtf8, "https://github.com/", 19) == 0) {
                const char *slashPath = urlUtf8 + 18;
                const char *baseName = strrchr(urlUtf8, '/');
                DWORD assetSz = 0;
                const char *szPos = strstr(rel, "\"size\"");
                if (szPos) {
                    const char *colon = strchr(szPos, ':');
                    if (colon) assetSz = (DWORD)strtoul(colon + 1, NULL, 10);
                }
                EnterCriticalSection(&g_cs);
                MultiByteToWideChar(CP_UTF8, 0, slashPath, -1, g_remHudPath, 511);
                if (baseName) MultiByteToWideChar(CP_UTF8, 0, baseName + 1, -1, g_remHudName, 127);
                g_remHudSize = assetSz;
                LeaveCriticalSection(&g_cs);
            }
        }
    }
    free(rel);

    wchar_t doneMsg[256];
    _snwprintf(doneMsg, 255, L"Synced status with GitHub (%s)", branch);
    SetFooterStatus(doneMsg, 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static BOOL WorkerInstallBluetoothGuard(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_btgDir);

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);

    if (FileExistsW(destExe)) {
        SetFooterStatus(L"Stopping running Bluetooth Guard before update...", 20);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = destExe;
        sei.lpParameters = L"--stop";
        sei.lpDirectory = g_btgDir;
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2500);
            CloseHandle(sei.hProcess);
        }
        Sleep(250);
    }

    SetFooterStatus(L"Downloading BluetoothGuard.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"bluetooth-guard/BluetoothGuard.exe", destExe)) {
        SetFooterStatus(L"Could not download BluetoothGuard.exe from GitHub.", 0);
        return FALSE;
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
        CreateShortcutLink(lnk, destExe, g_btgDir, L"Bluetooth Guard - Stop Windows stealing phone audio");
    }

    if (launchAfter) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_btgDir, SW_SHOWNORMAL);
    }
    SetFooterStatus(L"Bluetooth Guard updated and ready.", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
    return TRUE;
}

static void WorkerInstallKeyboardLighting(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_kblDir);

    const wchar_t *files[] = {
        L"Aura-Background.ps1", L"Tray.ps1", L"ui_controls.cs.txt", L"app.ico",
        L"Setup.ps1", L"Setup.bat", L"Restart.bat", L"Zones.ps1", L"Zones.bat",
        L"MyEffect.ps1", L"README.md"
    };
    int count = (int)(sizeof(files) / sizeof(files[0]));
    for (int i = 0; i < count; i++) {
        wchar_t msg[256], rel[128], dest[MAX_PATH];
        _snwprintf(msg, 255, L"Downloading Keyboard Lighting (%d/%d): %s...", i + 1, count, files[i]);
        SetFooterStatus(msg, 10 + (i * 80) / count);
        _snwprintf(rel, 127, L"keyboard-lighting/%s", files[i]);
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        DownloadRawRepoFile(branch, rel, dest);
    }

    wchar_t setupBat[MAX_PATH];
    _snwprintf(setupBat, MAX_PATH, L"%s\\Setup.bat", g_kblDir);
    if (FileExistsW(setupBat)) {
        ShellExecuteW(g_hwnd, L"open", setupBat, NULL, g_kblDir, SW_SHOWNORMAL);
        SetFooterStatus(L"Downloaded Keyboard Lighting and launched Setup.", 0);
    } else {
        SetFooterStatus(L"Could not download Keyboard Lighting files.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallSpatialHud(void) {
    SetFooterStatus(L"Checking latest Spatial HUD release on GitHub...", 20);
    if (!g_remHudPath[0]) WorkerCheckRemote();

    wchar_t dlPath[512], jarName[128];
    EnterCriticalSection(&g_cs);
    wcsncpy(dlPath, g_remHudPath, 511); dlPath[511] = L'\0';
    wcsncpy(jarName, g_remHudName, 127); jarName[127] = L'\0';
    LeaveCriticalSection(&g_cs);

    if (!dlPath[0] || !jarName[0]) {
        SetFooterStatus(L"No published Spatial HUD JAR found under release 'spatial-hud-latest'.", 0);
        return;
    }

    SetFooterStatus(L"Downloading Spatial HUD Fabric JAR...", 60);
    DWORD sz = 0; int st = 0;
    BYTE *jarBytes = HttpGetBuffer(L"github.com", dlPath, &sz, &st);
    if (!jarBytes || st != 200 || sz < 4096 || jarBytes[0] != 'P' || jarBytes[1] != 'K') {
        free(jarBytes);
        SetFooterStatus(L"Spatial HUD download failed or was not a valid JAR archive.", 0);
        return;
    }

    EnsureDirTree(g_modsDir);
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(pat, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            wchar_t oldJar[MAX_PATH];
            _snwprintf(oldJar, MAX_PATH, L"%s\\%s", g_modsDir, fd.cFileName);
            DeleteFileW(oldJar);
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }

    wchar_t destJar[MAX_PATH];
    _snwprintf(destJar, MAX_PATH, L"%s\\%s", g_modsDir, jarName);
    BOOL ok = WriteBinaryFileAtomic(destJar, jarBytes, sz);
    free(jarBytes);

    if (ok) {
        wchar_t msg[300];
        _snwprintf(msg, 299, L"Installed %s into mods folder.", jarName);
        SetFooterStatus(msg, 0);
    } else {
        SetFooterStatus(L"Could not write JAR to mods folder (close Minecraft first).", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallOrcaPlugins(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);

    SetFooterStatus(L"Downloading Wave Overhangs & Unlayered Infill plugins...", 40);
    EnsureDirTree(g_orcaBatDir);

    wchar_t batPath[MAX_PATH], tmpWave[MAX_PATH], tmpInf[MAX_PATH];
    _snwprintf(batPath, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    _snwprintf(tmpWave, MAX_PATH, L"%s\\wave_overhangs_orca.py", g_hubCfgDir);
    _snwprintf(tmpInf,  MAX_PATH, L"%s\\unlayered_infill_orca.py", g_hubCfgDir);

    BOOL ok1 = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", batPath);
    BOOL ok2 = DownloadRawRepoFile(branch, L"plugins/wave-overhangs/wave_overhangs_orca.py", tmpWave);
    BOOL ok3 = DownloadRawRepoFile(branch, L"plugins/unlayered-infill/unlayered_infill_orca.py", tmpInf);
    if (!ok2 || !ok3) {
        SetFooterStatus(L"Failed downloading OrcaSlicer plugin files from GitHub.", 0);
        (void)ok1;
        return;
    }

    wchar_t searchOrca[MAX_PATH];
    _snwprintf(searchOrca, MAX_PATH, L"%s\\OrcaSlicer*", g_roamApp);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(searchOrca, &fd);
    int targetsInstalled = 0;

    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
                _snwprintf(wDir, MAX_PATH, L"%s\\%s\\orca_plugins\\wave-overhangs", g_roamApp, fd.cFileName);
                _snwprintf(iDir, MAX_PATH, L"%s\\%s\\orca_plugins\\unlayered-infill", g_roamApp, fd.cFileName);
                _snwprintf(legacyDir, MAX_PATH, L"%s\\%s\\plugins", g_roamApp, fd.cFileName);
                EnsureDirTree(wDir);
                EnsureDirTree(iDir);
                EnsureDirTree(legacyDir);

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
                const char *stJson = "{\"enabled\": true}\n";
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
                CopyFileW(tmpInf, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
                CopyFileW(tmpInf, dst, FALSE);
                targetsInstalled++;
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }

    if (targetsInstalled == 0) {
        wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
        _snwprintf(wDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs", g_roamApp);
        _snwprintf(iDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill", g_roamApp);
        _snwprintf(legacyDir, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);
        const char *stJson = "{\"enabled\": true}\n";
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
        CopyFileW(tmpInf, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
        CopyFileW(tmpInf, dst, FALSE);
    }

    SetFooterStatus(L"Updated Wave Overhangs + Unlayered Infill! Restart OrcaSlicer to load.", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerSyncArenaLink(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    const wchar_t *files[] = {
        L"package.json", L"main.js", L"core.js", L"preload.js", L"README.md",
        L"renderer/index.html", L"renderer/app.js", L"renderer/styles.css"
    };
    int n = (int)(sizeof(files) / sizeof(files[0]));
    for (int i = 0; i < n; i++) {
        wchar_t rel[128], sub[128], dest[MAX_PATH];
        _snwprintf(rel, 127, L"arena-link-windows/%s", files[i]);
        wcsncpy(sub, files[i], 127);
        for (wchar_t *c = sub; *c; c++) if (*c == L'/') *c = L'\\';
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_arenaDir, sub);
        SetFooterStatus(L"Syncing Arena Link source files from GitHub...", 15 + (i * 80) / n);
        DownloadRawRepoFile(branch, rel, dest);
    }
    SetFooterStatus(L"Arena Link source files synced.", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerSelfUpdateHub(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    SetFooterStatus(L"Downloading latest ProjectHub.exe from GitHub...", 50);

    wchar_t newExe[MAX_PATH], oldExe[MAX_PATH];
    _snwprintf(newExe, MAX_PATH, L"%s.new", g_exePath);
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    if (!DownloadRawRepoFile(branch, L"project-hub/ProjectHub.exe", newExe)) {
        SetFooterStatus(L"Could not download ProjectHub.exe from GitHub.", 0);
        return;
    }

    if (!MoveFileExW(g_exePath, oldExe, MOVEFILE_REPLACE_EXISTING) ||
        !MoveFileExW(newExe, g_exePath, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(newExe);
        SetFooterStatus(L"Could not replace ProjectHub.exe on disk.", 0);
        return;
    }

    SetFooterStatus(L"Updated ProjectHub.exe — restarting...", 100);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_SELFUPD, 0, 0);
}

static void EnqueueTask(int task) {
    EnterCriticalSection(&g_cs);
    int next = (g_qTail + 1) % 32;
    if (next != g_qHead) {
        g_taskQueue[g_qTail] = task;
        g_qTail = next;
        ReleaseSemaphore(g_qSem, 1, NULL);
    }
    LeaveCriticalSection(&g_cs);
}

static DWORD WINAPI HubWorkerThread(LPVOID unused) {
    (void)unused;
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        if (WaitForSingleObject(g_qSem, INFINITE) != WAIT_OBJECT_0) break;
        if (InterlockedCompareExchange(&g_running, 1, 1) == 0) break;

        int task = TASK_NONE;
        EnterCriticalSection(&g_cs);
        if (g_qHead != g_qTail) {
            task = g_taskQueue[g_qHead];
            g_qHead = (g_qHead + 1) % 32;
        }
        LeaveCriticalSection(&g_cs);

        switch (task) {
        case TASK_CHECK_REMOTE:        WorkerCheckRemote(); break;
        case TASK_INSTALL_BTG:         WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_OPEN_OR_INSTALL_BTG: WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_INSTALL_KBL:         WorkerInstallKeyboardLighting(); break;
        case TASK_INSTALL_HUD:         WorkerInstallSpatialHud(); break;
        case TASK_INSTALL_ORCA:        WorkerInstallOrcaPlugins(); break;
        case TASK_SYNC_ARENA:          WorkerSyncArenaLink(); break;
        case TASK_UPDATE_ALL:
            WorkerInstallBluetoothGuard(FALSE);
            WorkerInstallOrcaPlugins();
            WorkerInstallSpatialHud();
            WorkerSyncArenaLink();
            WorkerCheckRemote();
            SetFooterStatus(L"All projects updated.", 0);
            break;
        case TASK_SELF_UPDATE:         WorkerSelfUpdateHub(); break;
        default: break;
        }
    }
    return 0;
}

static void RestartKeyboardLightingCleanly(void) {
    const wchar_t *locks[2] = { L"running.lock", L"engine.pid" };
    for (int i = 0; i < 2; i++) {
        wchar_t lp[MAX_PATH];
        _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\%s", g_localApp, locks[i]);
        char *txt = ReadTextFileUtf8(lp, NULL);
        if (txt) {
            unsigned long pid = strtoul(txt, NULL, 10);
            free(txt);
            if (pid > 0) {
                HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
                if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
            }
        }
        DeleteFileW(lp);
    }
    wchar_t exe[MAX_PATH], rstBat[MAX_PATH];
    _snwprintf(exe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(rstBat, MAX_PATH, L"%s\\Restart.bat", g_kblDir);
    if (FileExistsW(exe))
        ShellExecuteW(g_hwnd, L"open", exe, NULL, g_kblDir, SW_SHOWNORMAL);
    else if (FileExistsW(rstBat))
        ShellExecuteW(g_hwnd, L"open", rstBat, NULL, g_kblDir, SW_SHOWNORMAL);
    else
        EnqueueTask(TASK_INSTALL_KBL);
    SetFooterStatus(L"Restarted Keyboard Lighting cleanly.", 0);
}

static void PickSpatialHudModsFolder(void) {
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = L"Select your Minecraft / Modrinth 'mods' folder:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t picked[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, picked) && picked[0]) {
            wcsncpy(g_modsDir, picked, MAX_PATH - 1);
            wchar_t saveDir[MAX_PATH], saveFile[MAX_PATH];
            _snwprintf(saveDir, MAX_PATH, L"%s\\SpatialHudUpdater", g_localApp);
            EnsureDirTree(saveDir);
            _snwprintf(saveFile, MAX_PATH, L"%s\\target-directory.txt", saveDir);
            char utf8[MAX_PATH * 3];
            int n = WideCharToMultiByte(CP_UTF8, 0, g_modsDir, -1, utf8, sizeof(utf8), NULL, NULL);
            if (n > 1) WriteBinaryFileAtomic(saveFile, (const BYTE *)utf8, (DWORD)(n - 1));
            RefreshProjectStatuses();
        }
        CoTaskMemFree(pidl);
    }
}

/* ============================================================================
 * Modern Double-Buffered GDI Rendering & Layout
 * ============================================================================ */

static void FillRoundBox(HDC hdc, const RECT *rc, int rad, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, rad, rad);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void DrawActionBtn(HDC hdc, const ActionBtn *btn, int dpi) {
    BOOL hot = (g_hoverId == btn->id);
    COLORREF bg, border, fg = COL_TEXT_PRIMARY;
    if (btn->style == BTN_PRIMARY) {
        bg     = hot ? COL_ACCENT_HOVER : COL_ACCENT;
        border = bg;
    } else if (btn->style == BTN_WARNING) {
        bg     = hot ? COL_WARN_BTN_HOVER : COL_WARN_BTN;
        border = bg;
    } else {
        bg     = hot ? COL_BTN_HOVER : COL_BTN_BG;
        border = COL_BTN_BORDER;
    }
    FillRoundBox(hdc, &btn->rc, S(8, dpi), bg, border);
    SelectObject(hdc, g_fontBtn);
    SetTextColor(hdc, fg);
    RECT tr = btn->rc;
    DrawTextW(hdc, btn->label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void ComputeLayout(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    int pad = S(20, dpi);
    int headerH = S(68, dpi);
    int footerH = S(46, dpi);
    int cardH = S(78, dpi);
    int gap = S(10, dpi);

    int bx = w - pad;
    int topBtnH = S(32, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    for (int i = 2; i >= 0; i--) {
        int bw = S(g_topBtns[i].widthPx, dpi);
        bx -= bw;
        SetRect(&g_topBtns[i].rc, bx, topBtnY, bx + bw, topBtnY + topBtnH);
        bx -= S(8, dpi);
    }

    int contentH = 5 * cardH + 4 * gap + S(12, dpi);
    int viewH = h - headerH - footerH;
    g_maxScrollY = (contentH > viewH) ? (contentH - viewH) : 0;
    if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
    if (g_scrollY < 0) g_scrollY = 0;

    int y = headerH + S(4, dpi) - g_scrollY;
    int btnH = S(28, dpi);
    for (int c = 0; c < 5; c++) {
        SetRect(&g_cards[c].rcCard, pad, y, w - pad, y + cardH);
        int rx = w - pad - S(14, dpi);
        int by = y + cardH - S(12, dpi) - btnH;
        for (int b = g_cards[c].btnCount - 1; b >= 0; b--) {
            int bw = S(g_cards[c].btns[b].widthPx, dpi);
            rx -= bw;
            SetRect(&g_cards[c].btns[b].rc, rx, by, rx + bw, by + btnH);
            rx -= S(6, dpi);
        }
        y += cardH + gap;
    }
}

static int HitTestButtons(POINT pt, int headerBottom, int footerTop) {
    for (int i = 0; i < 3; i++) {
        if (PtInRect(&g_topBtns[i].rc, pt)) return g_topBtns[i].id;
    }
    if (pt.y < headerBottom || pt.y > footerTop) return 0;
    for (int c = 0; c < 5; c++) {
        for (int b = 0; b < g_cards[c].btnCount; b++) {
            if (PtInRect(&g_cards[c].btns[b].rc, pt)) return g_cards[c].btns[b].id;
        }
    }
    return 0;
}

static void PaintHubWindow(HWND hwnd, HDC targetDc) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    if (w <= 0 || h <= 0) return;

    ComputeLayout(hwnd);

    HDC hdc = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateCompatibleBitmap(targetDc, w, h);
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);
    SetBkMode(hdc, TRANSPARENT);

    /* Base canvas */
    HBRUSH bgBr = CreateSolidBrush(COL_BG);
    FillRect(hdc, &cli, bgBr);
    DeleteObject(bgBr);

    int pad = S(20, dpi);
    int headerH = S(68, dpi);
    int footerH = S(46, dpi);
    int footerTop = h - footerH;

    /* Cards clipped cleanly between header and footer */
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, 0, headerH, w, footerTop);

    for (int c = 0; c < 5; c++) {
        const ProjectCard *card = &g_cards[c];
        if (card->rcCard.bottom < headerH || card->rcCard.top > footerTop) continue;

        FillRoundBox(hdc, &card->rcCard, S(12, dpi), COL_CARD_BG, COL_CARD_BORDER);

        /* Status pill colors */
        COLORREF pillFg = COL_IDLE_TEXT, pillBg = COL_IDLE_BG, stripeCol = COL_BTN_BORDER;
        if (card->badgeKind == BADGE_OK) {
            pillFg = COL_OK_TEXT; pillBg = COL_OK_BG; stripeCol = COL_OK_TEXT;
        } else if (card->badgeKind == BADGE_WARN) {
            pillFg = COL_WARN_TEXT; pillBg = COL_WARN_BG; stripeCol = COL_WARN_TEXT;
        } else if (card->badgeKind == BADGE_DANGER) {
            pillFg = COL_ERR_TEXT; pillBg = COL_ERR_BG; stripeCol = COL_ERR_TEXT;
        }

        /* Left vertical accent bar */
        RECT accentRc = {
            card->rcCard.left + S(1, dpi),
            card->rcCard.top + S(14, dpi),
            card->rcCard.left + S(5, dpi),
            card->rcCard.bottom - S(14, dpi)
        };
        FillRoundBox(hdc, &accentRc, S(3, dpi), stripeCol, stripeCol);

        /* Measure & draw status pill top-right */
        SelectObject(hdc, g_fontBadge);
        SIZE bsz = {0, 0};
        const wchar_t *badgeTxt = card->badge[0] ? card->badge : L"Checking";
        GetTextExtentPoint32W(hdc, badgeTxt, (int)wcslen(badgeTxt), &bsz);
        int pillW = bsz.cx + S(20, dpi);
        int pillH = S(22, dpi);
        RECT pillRc = {
            card->rcCard.right - S(14, dpi) - pillW,
            card->rcCard.top + S(11, dpi),
            card->rcCard.right - S(14, dpi),
            card->rcCard.top + S(11, dpi) + pillH
        };
        FillRoundBox(hdc, &pillRc, S(11, dpi), pillBg, pillBg);
        SetTextColor(hdc, pillFg);
        DrawTextW(hdc, badgeTxt, -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        /* Card Title + muted subtitle inline */
        SelectObject(hdc, g_fontTitle);
        SIZE tsz = {0, 0};
        GetTextExtentPoint32W(hdc, card->title, (int)wcslen(card->title), &tsz);
        RECT titleRc = {
            card->rcCard.left + S(16, dpi),
            card->rcCard.top + S(11, dpi),
            pillRc.left - S(10, dpi),
            card->rcCard.top + S(34, dpi)
        };
        SetTextColor(hdc, COL_TEXT_PRIMARY);
        DrawTextW(hdc, card->title, -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        int subLeft = titleRc.left + tsz.cx + S(10, dpi);
        if (subLeft + S(60, dpi) < pillRc.left) {
            SelectObject(hdc, g_fontBody);
            SetTextColor(hdc, COL_TEXT_MUTED);
            RECT subRc = { subLeft, card->rcCard.top + S(12, dpi), pillRc.left - S(10, dpi), card->rcCard.top + S(34, dpi) };
            DrawTextW(hdc, card->subtitle, -1, &subRc,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        /* Clean 1-line detail summary on bottom-left */
        int firstBtnLeft = (card->btnCount > 0) ? card->btns[0].rc.left : (card->rcCard.right - S(16, dpi));
        RECT detRc = {
            card->rcCard.left + S(16, dpi),
            card->rcCard.bottom - S(36, dpi),
            firstBtnLeft - S(14, dpi),
            card->rcCard.bottom - S(12, dpi)
        };
        SelectObject(hdc, g_fontBody);
        SetTextColor(hdc, COL_TEXT_SECONDARY);
        DrawTextW(hdc, card->detail, -1, &detRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        /* Action buttons */
        for (int b = 0; b < card->btnCount; b++) {
            DrawActionBtn(hdc, &card->btns[b], dpi);
        }
    }

    RestoreDC(hdc, saved);

    /* Top Header */
    RECT hdrRc = { 0, 0, w, headerH };
    HBRUSH hdrBr = CreateSolidBrush(COL_HEADER_BG);
    FillRect(hdc, &hdrRc, hdrBr);
    DeleteObject(hdrBr);

    int hdrRightLimit = g_topBtns[0].rc.left - S(12, dpi);
    SelectObject(hdc, g_fontHero);
    SetTextColor(hdc, COL_TEXT_PRIMARY);
    RECT hTitle = { pad, S(12, dpi), hdrRightLimit, S(38, dpi) };
    DrawTextW(hdc, L"Project Hub", -1, &hTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, g_fontBody);
    SetTextColor(hdc, COL_TEXT_SECONDARY);
    RECT hSub = { pad, S(38, dpi), hdrRightLimit, S(58, dpi) };
    DrawTextW(hdc, L"Manage apps, status, and one-click updates", -1, &hSub,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    for (int i = 0; i < 3; i++) {
        DrawActionBtn(hdc, &g_topBtns[i], dpi);
    }

    /* Footer bar */
    RECT ftrRc = { 0, footerTop, w, h };
    HBRUSH ftrBr = CreateSolidBrush(COL_FOOTER_BG);
    FillRect(hdc, &ftrRc, ftrBr);
    DeleteObject(ftrBr);

    HPEN divPen = CreatePen(PS_SOLID, 1, COL_CARD_BORDER);
    HGDIOBJ oldPen = SelectObject(hdc, divPen);
    MoveToEx(hdc, 0, footerTop, NULL);
    LineTo(hdc, w, footerTop);
    SelectObject(hdc, oldPen);
    DeleteObject(divPen);

    wchar_t footMsg[512];
    int pct = 0;
    EnterCriticalSection(&g_cs);
    wcsncpy(footMsg, g_statusMsg, 511); footMsg[511] = L'\0';
    pct = g_progressVal;
    LeaveCriticalSection(&g_cs);

    if (pct > 0 && pct < 100) {
        RECT trackRc = { pad, footerTop + S(7, dpi), w - pad, footerTop + S(11, dpi) };
        FillRoundBox(hdc, &trackRc, S(2, dpi), COL_CARD_BG, COL_CARD_BG);
        int fillW = ((w - pad * 2) * pct) / 100;
        if (fillW > S(4, dpi)) {
            RECT fillRc = { pad, footerTop + S(7, dpi), pad + fillW, footerTop + S(11, dpi) };
            FillRoundBox(hdc, &fillRc, S(2, dpi), COL_ACCENT, COL_ACCENT);
        }
    }

    SelectObject(hdc, g_fontBody);
    SetTextColor(hdc, COL_TEXT_SECONDARY);
    RECT fTxt = { pad, footerTop + (pct > 0 && pct < 100 ? S(14, dpi) : S(8, dpi)), w - pad, h - S(6, dpi) };
    DrawTextW(hdc, footMsg, -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    BitBlt(targetDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

static void ShowMoreMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SHORTCUT,    L"Pin Project Hub to Desktop");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SELF_UPDATE, L"Self-Update Project Hub (" HUB_VERSION_W L")");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SUITE_DIR,   L"Open Managed Apps Folder");

    POINT pt = { g_topBtns[2].rc.right, g_topBtns[2].rc.bottom + 4 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static void HandleActionCommand(HWND hwnd, int id) {
    wchar_t p[MAX_PATH];
    switch (id) {
    case ID_BTN_REFRESH_ALL:
        RefreshProjectStatuses();
        EnqueueTask(TASK_CHECK_REMOTE);
        break;
    case ID_BTN_UPDATE_ALL:
        EnqueueTask(TASK_UPDATE_ALL);
        break;
    case ID_BTN_MORE_MENU:
        ShowMoreMenu(hwnd);
        break;
    case ID_MENU_SHORTCUT: {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
            if (CreateShortcutLink(lnk, g_exePath, g_exeDir, L"Project Hub - Status & Updates")) {
                SetFooterStatus(L"Pinned 'Project Hub' shortcut to Desktop.", 0);
            }
        }
        break;
    }
    case ID_MENU_SELF_UPDATE:
        EnqueueTask(TASK_SELF_UPDATE);
        break;
    case ID_MENU_SUITE_DIR:
        EnsureDirTree(g_suiteDir);
        ShellExecuteW(hwnd, L"open", g_suiteDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    /* Bluetooth Guard */
    case ID_BTG_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_btgDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_OPEN_OR_INSTALL_BTG);
        break;
    case ID_BTG_UPDATE:
        EnqueueTask(TASK_INSTALL_BTG);
        break;
    case ID_BTG_STOP:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, L"--stop", g_btgDir, SW_HIDE);
        SetFooterStatus(L"Stopped Bluetooth Guard.", 0);
        break;
    case ID_BTG_FOLDER:
        EnsureDirTree(g_btgDir);
        ShellExecuteW(hwnd, L"open", g_btgDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    /* Keyboard Lighting */
    case ID_KBL_OPEN: {
        wchar_t showFlag[MAX_PATH];
        _snwprintf(showFlag, MAX_PATH, L"%s\\KeyboardLighting\\show.flag", g_localApp);
        WriteBinaryFileAtomic(showFlag, (const BYTE *)"1", 1);
        _snwprintf(p, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_kblDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_KBL);
        break;
    }
    case ID_KBL_RESTART:
        RestartKeyboardLightingCleanly();
        break;
    case ID_KBL_UPDATE:
        EnqueueTask(TASK_INSTALL_KBL);
        break;
    case ID_KBL_FOLDER:
        EnsureDirTree(g_kblDir);
        ShellExecuteW(hwnd, L"open", g_kblDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    /* Spatial HUD */
    case ID_HUD_UPDATE:
        EnqueueTask(TASK_INSTALL_HUD);
        break;
    case ID_HUD_PICK:
        PickSpatialHudModsFolder();
        break;
    case ID_HUD_FOLDER:
        EnsureDirTree(g_modsDir);
        ShellExecuteW(hwnd, L"open", g_modsDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    /* OrcaSlicer */
    case ID_ORCA_UPDATE:
        EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_MENU:
        _snwprintf(p, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_orcaBatDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_FOLDER:
        _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins", g_roamApp);
        if (!DirExistsW(p)) _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(p);
        ShellExecuteW(hwnd, L"open", p, NULL, NULL, SW_SHOWNORMAL);
        break;

    /* Arena Link */
    case ID_ARENA_OPEN: {
        wchar_t installedExe[MAX_PATH];
        _snwprintf(installedExe, MAX_PATH, L"%s\\Programs\\arena-link-windows\\Arena Link.exe", g_localApp);
        if (FileExistsW(installedExe))
            ShellExecuteW(hwnd, L"open", installedExe, NULL, NULL, SW_SHOWNORMAL);
        else {
            EnsureDirTree(g_arenaDir);
            ShellExecuteW(hwnd, L"open", g_arenaDir, NULL, NULL, SW_SHOWNORMAL);
        }
        break;
    }
    case ID_ARENA_SYNC:
        EnqueueTask(TASK_SYNC_ARENA);
        break;
    case ID_ARENA_FOLDER:
        _snwprintf(p, MAX_PATH, L"%s\\arena-link-windows", g_roamApp);
        EnsureDirTree(p);
        ShellExecuteW(hwnd, L"open", p, NULL, NULL, SW_SHOWNORMAL);
        break;
    }
}

static LRESULT CALLBACK HubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        RebuildFonts(hwnd);
        InitCardDefinitions();
        RefreshProjectStatuses();
        SetTimer(hwnd, 1, 2000, NULL);
        EnqueueTask(TASK_CHECK_REMOTE);
        return 0;

    case WM_DPICHANGED: {
        RebuildFonts(hwnd);
        const RECT *suggested = (const RECT *)lp;
        if (suggested) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        int dpi = GetWinDpi(hwnd);
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = S(640, dpi);
        mmi->ptMinTrackSize.y = S(420, dpi);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintHubWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int dpi = GetWinDpi(hwnd);
        g_scrollY -= (delta > 0) ? S(36, dpi) : -S(36, dpi);
        if (g_scrollY < 0) g_scrollY = 0;
        if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(68, dpi), cli.bottom - S(46, dpi));
        if (hit != g_hoverId) {
            g_hoverId = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hoverId != 0) {
            g_hoverId = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hoverId != 0) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONUP: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(68, dpi), cli.bottom - S(46, dpi));
        if (hit != 0) HandleActionCommand(hwnd, hit);
        return 0;
    }

    case WM_COMMAND:
        HandleActionCommand(hwnd, LOWORD(wp));
        return 0;

    case WM_HUB_REFRESH:
        RefreshProjectStatuses();
        return 0;

    case WM_HUB_STATUS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_HUB_SELFUPD:
        ShellExecuteW(NULL, L"open", g_exePath, NULL, g_exeDir, SW_SHOWNORMAL);
        DestroyWindow(hwnd);
        return 0;

    case WM_TIMER:
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        RefreshProjectStatuses();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
        if (ev) { SetEvent(ev); CloseHandle(ev); }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitializeCriticalSection(&g_cs);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    g_qSem      = CreateSemaphoreW(NULL, 0, 64, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    InitAllPaths();
    wchar_t oldExe[MAX_PATH];
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = HubWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsProjectHubNativeWnd";
    RegisterClassExW(&wc);

    HANDLE hWorker = CreateThread(NULL, 0, HubWorkerThread, NULL, 0, NULL);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(780, dpi), S(600, dpi),
                             NULL, NULL, hInst, NULL);

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_running, 0);
    ReleaseSemaphore(g_qSem, 1, NULL);
    if (hWorker) { WaitForSingleObject(hWorker, 1500); CloseHandle(hWorker); }
    CloseHandle(g_qSem);
    if (g_showEvent) CloseHandle(g_showEvent);
    if (hMutex) CloseHandle(hMutex);
    DeleteCriticalSection(&g_cs);
    CoUninitialize();
    return 0;
}
