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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define HUB_VERSION       "2.7.0"
#define HUB_VERSION_W     L"2.7"
#define APP_TITLE         L"Project Hub"
#define SINGLE_MUTEX_NAME L"Local\\Tests_ProjectHub_SingleInstance_v2"
#define SHOW_EVENT_NAME   L"Local\\Tests_ProjectHub_ShowWindow_v2"

#define REPO_OWNER_W      L"ajani190819-ops"
#define REPO_NAME_W       L"Tests"
#define FALLBACK_BRANCH_W L"arena/e895b94f-tests"

#define WM_HUB_REFRESH    (WM_APP + 10)
#define WM_HUB_STATUS     (WM_APP + 11)
#define WM_HUB_SELFUPD    (WM_APP + 12)

/* Embedded multi-resolution 32-bit RGBA icons in hub.rc */
#define RES_ICON_APP         1
#define RES_ICON_EMBLEM      2
#define RES_ICON_BTG         3
#define RES_ICON_KBL         4
#define RES_ICON_HUD         5
#define RES_ICON_ORCA        6
#define RES_ICON_ARENA       7

/* Action IDs */
#define ID_BTN_REFRESH_ALL   1001
#define ID_BTN_SELF_UPDATE   1002
#define ID_BTN_UPDATE_ALL    1003
#define ID_EMBLEM_MENU       1004
#define ID_MENU_SHORTCUT     1011
#define ID_MENU_SUITE_DIR    1012

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

/* Soft Slate Suite Palette (matches Keyboard Lighting, Bluetooth Guard & docs/UI_STYLE_GUIDE.md) */
#define COL_BG               RGB(24, 27, 36)    /* #181B24 */
#define COL_FOOTER_BG        RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER          RGB(48, 55, 72)    /* #303748 */
#define COL_CARD_BG          RGB(34, 39, 52)    /* #222734 */
#define COL_CARD_BORDER      RGB(56, 64, 84)    /* #384054 */
#define COL_BTN_BG           RGB(43, 49, 66)    /* #2B3142 */
#define COL_BTN_HOVER        RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER       RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_BORDER     RGB(96, 112, 144)  /* #607090 */

#define COL_PRIMARY_BG       RGB(37, 99, 235)   /* #2563EB */
#define COL_PRIMARY_HOVER    RGB(59, 130, 246)  /* #3B82F6 */
#define COL_AMBER_BTN        RGB(68, 52, 26)
#define COL_AMBER_BTN_HOVER  RGB(88, 66, 30)

#define COL_TEXT_PRIMARY     RGB(236, 239, 246) /* #ECEFF6 */
#define COL_TEXT_SECONDARY   RGB(154, 163, 184) /* #9AA3B8 */
#define COL_TEXT_MUTED       RGB(116, 126, 148) /* #747E94 */

#define COL_SKY              RGB(56, 189, 248)  /* #38BDF8 */
#define COL_SKY_BG           RGB(24, 48, 68)
#define COL_MINT             RGB(52, 211, 153)  /* #34D399 */
#define COL_MINT_BG          RGB(24, 52, 44)
#define COL_AMBER            RGB(251, 191, 36)  /* #FBBF24 */
#define COL_AMBER_BG         RGB(56, 44, 24)
#define COL_CORAL            RGB(248, 113, 113) /* #F87171 */
#define COL_CORAL_BG         RGB(58, 30, 36)
#define COL_IDLE_SLATE       RGB(108, 120, 144)
#define COL_IDLE_BG          RGB(42, 48, 64)

enum {
    BADGE_IDLE    = 0,
    BADGE_OK      = 1,
    BADGE_WARN    = 2,
    BADGE_SKY     = 3,
    BADGE_DANGER  = 4
};

enum {
    BTN_SECONDARY = 0,
    BTN_AMBER     = 1,
    BTN_PRIMARY   = 2
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
    int            resIconId;
    const wchar_t *title;
    wchar_t        badge[48];
    int            badgeKind;
    wchar_t        detail[256];
    ActionBtn      btns[4];
    int            btnCount;
    RECT           rcCard;
} ProjectCard;

static HINSTANCE        g_hInst       = NULL;
static HWND             g_hwnd        = NULL;
static HFONT            g_fontHero    = NULL;
static HFONT            g_fontSub     = NULL;
static HFONT            g_fontTitle   = NULL;
static HFONT            g_fontBody    = NULL;
static HFONT            g_fontBtn     = NULL;
static HFONT            g_fontBadge   = NULL;
static HICON            g_appIcon     = NULL;
static HICON            g_hdrEmblem   = NULL;
static HICON            g_cardIcons[5] = {NULL, NULL, NULL, NULL, NULL};
static HANDLE           g_showEvent   = NULL;
static CRITICAL_SECTION g_cs;

static ActionBtn        g_topBtns[3] = {
    { ID_BTN_REFRESH_ALL, L"Refresh",    BTN_SECONDARY, 92,  {0,0,0,0} },
    { ID_BTN_SELF_UPDATE, L"Update Hub", BTN_AMBER,     112, {0,0,0,0} },
    { ID_BTN_UPDATE_ALL,  L"Update All", BTN_PRIMARY,   108, {0,0,0,0} }
};
static RECT             g_rcEmblem    = {0, 0, 0, 0};

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
static wchar_t          g_btgExtraExe[MAX_PATH];
static wchar_t          g_kblDir[MAX_PATH];
static wchar_t          g_modsDir[MAX_PATH];
static wchar_t          g_orcaBatDir[MAX_PATH];
static wchar_t          g_arenaDir[MAX_PATH];

/* Shared remote state protected by g_cs */
static wchar_t          g_activeBranch[64]  = L"";
static char             g_remHubVer[32]     = "";
static char             g_remWaveVer[32]    = "";
static char             g_remInfillVer[32]  = "";
static char             g_remArenaVer[32]   = "";
static wchar_t          g_remHudName[128]   = L"";
static wchar_t          g_remHudPath[512]   = L"";
static DWORD            g_remHudSize        = 0;
static wchar_t          g_statusMsg[512]    = L"Checking installed apps and GitHub updates...";
static int              g_progressVal       = 0;
static BOOL             g_hubUpdAvail       = FALSE;

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
        DWORD cornerPref = 2; /* DWMWCP_ROUND — full smooth Windows 11 rounded corners */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIVIDER;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &cornerPref, sizeof(cornerPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static void RebuildFontsAndIcons(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    if (g_fontHero)  DeleteObject(g_fontHero);
    if (g_fontSub)   DeleteObject(g_fontSub);
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontBody)  DeleteObject(g_fontBody);
    if (g_fontBtn)   DeleteObject(g_fontBtn);
    if (g_fontBadge) DeleteObject(g_fontBadge);

    g_fontHero  = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSub   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBody  = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    if (g_hdrEmblem) { DestroyIcon(g_hdrEmblem); g_hdrEmblem = NULL; }
    int embPx = S(36, dpi);
    g_hdrEmblem = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(RES_ICON_EMBLEM),
                                    IMAGE_ICON, embPx, embPx, LR_DEFAULTCOLOR);

    int cardIcoPx = S(44, dpi);
    const int ids[5] = { RES_ICON_BTG, RES_ICON_KBL, RES_ICON_HUD, RES_ICON_ORCA, RES_ICON_ARENA };
    for (int i = 0; i < 5; i++) {
        if (g_cardIcons[i]) { DestroyIcon(g_cardIcons[i]); g_cardIcons[i] = NULL; }
        g_cardIcons[i] = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(ids[i]),
                                           IMAGE_ICON, cardIcoPx, cardIcoPx, LR_DEFAULTCOLOR);
    }
}

/* Exposed symbol reference kept in sync with suite style tests */
static void DrawWayfinderEmblem(HDC hdc, int x, int y, int sz) {
    if (g_hdrEmblem) DrawIconEx(hdc, x, y, g_hdrEmblem, sz, sz, 0, NULL, DI_NORMAL);
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

static BYTE *HttpGetBuffer(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    if (outSize) *outSize = 0;
    if (outStatus) *outStatus = 0;

    HINTERNET hSession = WinHttpOpen(L"Tests-ProjectHub/2.3",
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
                                   L"User-Agent: Tests-ProjectHub/2.3\r\nAccept: */*\r\n",
                                   (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
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

/* Resolve directory from an existing .lnk shortcut if the user previously installed elsewhere */
static BOOL ResolveShortcutTargetDir(const wchar_t *lnkPath, wchar_t *outDir) {
    if (!FileExistsW(lnkPath)) return FALSE;
    IShellLinkW *psl = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&psl)) || !psl)
        return FALSE;

    BOOL found = FALSE;
    IPersistFile *ppf = NULL;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        if (SUCCEEDED(IPersistFile_Load(ppf, lnkPath, STGM_READ))) {
            wchar_t target[MAX_PATH] = L"";
            if (SUCCEEDED(IShellLinkW_GetPath(psl, target, MAX_PATH, NULL, 0)) && target[0]) {
                wchar_t *sl = wcsrchr(target, L'\\');
                if (sl) {
                    *sl = L'\0';
                    if (DirExistsW(target)) {
                        wcsncpy(outDir, target, MAX_PATH - 1);
                        outDir[MAX_PATH - 1] = L'\0';
                        found = TRUE;
                    }
                }
            }
        }
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return found;
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

    wchar_t desk[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk);

    wchar_t parent[MAX_PATH];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    wchar_t *psl = wcsrchr(parent, L'\\');
    if (psl) *psl = L'\0';

    wchar_t cand[MAX_PATH], lnk[MAX_PATH];

    /* 1. Locate existing BluetoothGuard installation (running process, same folder, sibling, shortcut, or suite dir) */
    g_btgDir[0] = L'\0';
    g_btgExtraExe[0] = L'\0';
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    char *lockTxt = ReadTextFileUtf8(btgLock, NULL);
    if (lockTxt) {
        unsigned long pid = strtoul(lockTxt, NULL, 10);
        free(lockTxt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            if (hp) {
                wchar_t procPath[MAX_PATH] = L"";
                DWORD plen = MAX_PATH;
                if (QueryFullProcessImageNameW(hp, 0, procPath, &plen) && FileExistsW(procPath)) {
                    wcsncpy(g_btgExtraExe, procPath, MAX_PATH - 1);
                }
                CloseHandle(hp);
            }
        }
    }

    _snwprintf(cand, MAX_PATH, L"%s\\BluetoothGuard.exe", g_exeDir);
    if (FileExistsW(cand)) {
        wcsncpy(g_btgDir, g_exeDir, MAX_PATH - 1);
    } else {
        _snwprintf(cand, MAX_PATH, L"%s\\bluetooth-guard\\BluetoothGuard.exe", parent);
        if (FileExistsW(cand)) {
            _snwprintf(g_btgDir, MAX_PATH, L"%s\\bluetooth-guard", parent);
        } else if (desk[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
            ResolveShortcutTargetDir(lnk, g_btgDir);
        }
    }
    if (!g_btgDir[0]) _snwprintf(g_btgDir, MAX_PATH, L"%s\\BluetoothGuard", g_suiteDir);

    /* 2. Locate existing Keyboard Lighting installation (sibling, Desktop shortcut, Start Menu shortcut, or suite dir) */
    g_kblDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH, L"%s\\keyboard-lighting\\Tray.ps1", parent);
    if (FileExistsW(cand)) {
        _snwprintf(g_kblDir, MAX_PATH, L"%s\\keyboard-lighting", parent);
    } else {
        if (desk[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
            ResolveShortcutTargetDir(lnk, g_kblDir);
        }
        if (!g_kblDir[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Microsoft\\Windows\\Start Menu\\Programs\\Keyboard Lighting.lnk", g_roamApp);
            ResolveShortcutTargetDir(lnk, g_kblDir);
        }
    }
    if (!g_kblDir[0]) _snwprintf(g_kblDir, MAX_PATH, L"%s\\KeyboardLighting", g_suiteDir);

    /* 3. OrcaSlicer & Arena Link */
    _snwprintf(cand, MAX_PATH, L"%s\\Orca-Plugins.bat", parent);
    if (FileExistsW(cand)) wcsncpy(g_orcaBatDir, parent, MAX_PATH - 1);
    else _snwprintf(g_orcaBatDir, MAX_PATH, L"%s\\OrcaPlugins", g_suiteDir);

    _snwprintf(cand, MAX_PATH, L"%s\\arena-link-windows\\package.json", parent);
    if (FileExistsW(cand)) _snwprintf(g_arenaDir, MAX_PATH, L"%s\\arena-link-windows", parent);
    else _snwprintf(g_arenaDir, MAX_PATH, L"%s\\ArenaLink", g_suiteDir);

    /* 4. Spatial HUD mods folder */
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
    IShellLinkW_SetIconLocation(psl, targetExe, 0);
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

    g_cards[0].resIconId = RES_ICON_BTG;
    g_cards[0].title     = L"Bluetooth Guard";
    g_cards[0].btnCount  = 4;
    g_cards[0].btns[0]   = (ActionBtn){ ID_BTG_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[0].btns[1]   = (ActionBtn){ ID_BTG_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0} };
    g_cards[0].btns[2]   = (ActionBtn){ ID_BTG_STOP,   L"Stop",   BTN_SECONDARY, 62, {0,0,0,0} };
    g_cards[0].btns[3]   = (ActionBtn){ ID_BTG_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[1].resIconId = RES_ICON_KBL;
    g_cards[1].title     = L"Keyboard Lighting";
    g_cards[1].btnCount  = 4;
    g_cards[1].btns[0]   = (ActionBtn){ ID_KBL_OPEN,    L"Open",     BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[1].btns[1]   = (ActionBtn){ ID_KBL_RESTART, L"Unfreeze", BTN_SECONDARY, 84, {0,0,0,0} };
    g_cards[1].btns[2]   = (ActionBtn){ ID_KBL_UPDATE,  L"Update",   BTN_SECONDARY, 74, {0,0,0,0} };
    g_cards[1].btns[3]   = (ActionBtn){ ID_KBL_FOLDER,  L"Folder",   BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[2].resIconId = RES_ICON_HUD;
    g_cards[2].title     = L"Spatial HUD";
    g_cards[2].btnCount  = 3;
    g_cards[2].btns[0]   = (ActionBtn){ ID_HUD_UPDATE, L"Update Mod", BTN_SECONDARY, 98, {0,0,0,0} };
    g_cards[2].btns[1]   = (ActionBtn){ ID_HUD_PICK,   L"Mods Dir",   BTN_SECONDARY, 84, {0,0,0,0} };
    g_cards[2].btns[2]   = (ActionBtn){ ID_HUD_FOLDER, L"Folder",     BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[3].resIconId = RES_ICON_ORCA;
    g_cards[3].title     = L"OrcaSlicer Plugins";
    g_cards[3].btnCount  = 3;
    g_cards[3].btns[0]   = (ActionBtn){ ID_ORCA_UPDATE, L"Update", BTN_SECONDARY, 78, {0,0,0,0} };
    g_cards[3].btns[1]   = (ActionBtn){ ID_ORCA_MENU,   L"Branch", BTN_SECONDARY, 78, {0,0,0,0} };
    g_cards[3].btns[2]   = (ActionBtn){ ID_ORCA_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0} };

    g_cards[4].resIconId = RES_ICON_ARENA;
    g_cards[4].title     = L"Arena Link";
    g_cards[4].btnCount  = 3;
    g_cards[4].btns[0]   = (ActionBtn){ ID_ARENA_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[4].btns[1]   = (ActionBtn){ ID_ARENA_SYNC,   L"Sync",   BTN_SECONDARY, 68, {0,0,0,0} };
    g_cards[4].btns[2]   = (ActionBtn){ ID_ARENA_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0} };
}

static void RefreshProjectStatuses(void) {
    char remHub[32], remWave[32], remInfill[32], remArena[32];
    wchar_t remHud[128];
    DWORD remHudSz = 0;

    EnterCriticalSection(&g_cs);
    strncpy(remHub, g_remHubVer, 31); remHub[31] = '\0';
    strncpy(remWave, g_remWaveVer, 31); remWave[31] = '\0';
    strncpy(remInfill, g_remInfillVer, 31); remInfill[31] = '\0';
    strncpy(remArena, g_remArenaVer, 31); remArena[31] = '\0';
    wcsncpy(remHud, g_remHudName, 127); remHud[127] = L'\0';
    remHudSz = g_remHudSize;
    LeaveCriticalSection(&g_cs);

    g_hubUpdAvail = (remHub[0] != '\0' && strcmp(remHub, HUB_VERSION) != 0);
    g_topBtns[1].label = g_hubUpdAvail ? L"Update Hub \x25B2" : L"Update Hub";

    wchar_t path[MAX_PATH];

    /* 1. Bluetooth Guard */
    _snwprintf(path, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    BOOL hasBtg = FileExistsW(path) || (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe));
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    int blocked = 0, total = 0;
    int btgState = CheckPidLockStatus(btgLock, 12.0, &blocked, &total);
    if (btgState == 1) {
        wcsncpy(g_cards[0].badge, L"Active", 47);
        g_cards[0].badgeKind = BADGE_OK;
        g_cards[0].btns[0].label = L"Open";
        g_cards[0].btns[0].style = BTN_SECONDARY;
        _snwprintf(g_cards[0].detail, 255,
                   L"Phone audio protection  \x00B7  %d guarded (%d paired)", blocked, total);
    } else if (hasBtg) {
        wcsncpy(g_cards[0].badge, L"Standby", 47);
        g_cards[0].badgeKind = BADGE_IDLE;
        g_cards[0].btns[0].label = L"Start";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Start to launch", 255);
    } else {
        wcsncpy(g_cards[0].badge, L"Setup Ready", 47);
        g_cards[0].badgeKind = BADGE_WARN;
        g_cards[0].btns[0].label = L"Install";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Install to set up", 255);
    }

    /* 2. Keyboard Lighting */
    wchar_t kblExe[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(path, MAX_PATH, L"%s\\Tray.ps1", g_kblDir);
    BOOL hasKbl = FileExistsW(kblExe) || FileExistsW(path);
    wchar_t kblLock[MAX_PATH];
    _snwprintf(kblLock, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    int kblState = CheckPidLockStatus(kblLock, 12.0, NULL, NULL);
    if (kblState == 2) {
        wcsncpy(g_cards[1].badge, L"Frozen", 47);
        g_cards[1].badgeKind = BADGE_DANGER;
        g_cards[1].btns[1].style = BTN_AMBER;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Unfreeze to restart", 255);
    } else if (kblState == 1) {
        wcsncpy(g_cards[1].badge, L"Active", 47);
        g_cards[1].badgeKind = BADGE_OK;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting", 255);
    } else if (hasKbl) {
        wcsncpy(g_cards[1].badge, L"Standby", 47);
        g_cards[1].badgeKind = BADGE_IDLE;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Installed locally", 255);
    } else {
        wcsncpy(g_cards[1].badge, L"Setup Ready", 47);
        g_cards[1].badgeKind = BADGE_WARN;
        g_cards[1].btns[2].style = BTN_PRIMARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Update to install", 255);
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
        wcsncpy(g_cards[2].badge, L"Update Ready", 47);
        g_cards[2].badgeKind = BADGE_WARN;
        g_cards[2].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[2].detail, L"Minecraft 26.3 Fabric mod  \x00B7  Click Update Mod to install", 255);
    } else if (remHudSz > 0 && foundSize != remHudSz) {
        wcsncpy(g_cards[2].badge, L"Update Ready", 47);
        g_cards[2].badgeKind = BADGE_WARN;
        g_cards[2].btns[0].style = BTN_PRIMARY;
        _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  Newer build (%s)", remHud);
    } else {
        wcsncpy(g_cards[2].badge, L"Up to Date", 47);
        g_cards[2].badgeKind = BADGE_OK;
        g_cards[2].btns[0].style = BTN_SECONDARY;
        _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  %s", foundJar);
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
        wcsncpy(g_cards[3].badge, L"Update Ready", 47);
        g_cards[3].badgeKind = BADGE_WARN;
        _snwprintf(g_cards[3].detail, 255, L"Wave Overhangs %S & Unlayered Infill %S available",
                   remWave[0] ? remWave : "latest", remInfill[0] ? remInfill : "latest");
    } else {
        BOOL upd = (remWave[0] && strcmp(locWave, remWave) != 0) ||
                   (remInfill[0] && strcmp(locInfill, remInfill) != 0);
        wcsncpy(g_cards[3].badge, upd ? L"Update Ready" : L"Up to Date", 47);
        g_cards[3].badgeKind = upd ? BADGE_WARN : BADGE_OK;
        g_cards[3].btns[0].style = upd ? BTN_PRIMARY : BTN_SECONDARY;
        _snwprintf(g_cards[3].detail, 255, L"Wave Overhangs %S & Unlayered Infill %S",
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
        g_cards[4].badgeKind = upd ? BADGE_WARN : BADGE_SKY;
        _snwprintf(g_cards[4].detail, 255, L"Local Git workspace bridge  \x00B7  v%S (%d linked)",
                   locArena, linkedRepos);
    } else {
        wcsncpy(g_cards[4].badge, L"Ready", 47);
        g_cards[4].badgeKind = BADGE_SKY;
        _snwprintf(g_cards[4].detail, 255, L"Local Git workspace bridge  \x00B7  %d linked workspace%s",
                   linkedRepos, linkedRepos == 1 ? L"" : L"s");
    }

    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static void WorkerCheckRemote(void) {
    wchar_t branch[64];
    SetFooterStatus(L"Checking GitHub for Hub & app updates...", 15);
    EnsureActiveBranch(branch, 64);

    wchar_t p[256];
    DWORD sz = 0; int st = 0;

    _snwprintf(p, 255, L"/%s/%s/%s/project-hub/src/project_hub.c", REPO_OWNER_W, REPO_NAME_W, branch);
    char *hubSrc = (char *)HttpGetBuffer(L"raw.githubusercontent.com", p, &sz, &st);
    if (hubSrc && st == 200) {
        char hv[32] = "";
        if (ExtractQuotedAfter(hubSrc, "#define HUB_VERSION", hv, sizeof(hv))) {
            EnterCriticalSection(&g_cs);
            strncpy(g_remHubVer, hv, 31);
            LeaveCriticalSection(&g_cs);
        }
    }
    free(hubSrc);

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
    SetFooterStatus(L"Checking Spatial HUD & Arena Link versions...", 60);

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

    char remHubLocal[32] = "";
    EnterCriticalSection(&g_cs);
    strncpy(remHubLocal, g_remHubVer, 31);
    LeaveCriticalSection(&g_cs);

    if (remHubLocal[0] && strcmp(remHubLocal, HUB_VERSION) != 0) {
        wchar_t updMsg[256];
        _snwprintf(updMsg, 255,
                   L"Project Hub v%S is available on GitHub  ·  Click 'Update Hub' above",
                   remHubLocal);
        SetFooterStatus(updMsg, 0);
    } else {
        SetFooterStatus(L"All suite apps verified  ·  GitHub connected", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void StopProcessFromLock(const wchar_t *lockPath) {
    char *txt = ReadTextFileUtf8(lockPath, NULL);
    if (txt) {
        unsigned long pid = strtoul(txt, NULL, 10);
        free(txt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
        }
    }
    DeleteFileW(lockPath);
}

static BOOL WorkerInstallBluetoothGuard(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_btgDir);

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);

    const wchar_t *stopTarget = FileExistsW(destExe) ? destExe
                              : (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe) ? g_btgExtraExe : NULL);
    if (stopTarget) {
        SetFooterStatus(L"Stopping running Bluetooth Guard before update...", 20);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = stopTarget;
        sei.lpParameters = L"--stop";
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2500);
            CloseHandle(sei.hProcess);
        }
        Sleep(300);
    }

    SetFooterStatus(L"Downloading latest BluetoothGuard.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"bluetooth-guard/BluetoothGuard.exe", destExe)) {
        SetFooterStatus(L"Could not download BluetoothGuard.exe from GitHub.", 0);
        return FALSE;
    }

    /* Also keep any previously launched standalone copy (e.g. in Downloads) in sync! */
    if (g_btgExtraExe[0] && _wcsicmp(g_btgExtraExe, destExe) != 0) {
        CopyFileW(destExe, g_btgExtraExe, FALSE);
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
        CreateShortcutLink(lnk, destExe, g_btgDir, L"Bluetooth Guard - Phone audio protection");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_btgDir, SW_SHOWNORMAL);
    } else {
        /* Start high-priority tray guard immediately in background & register FastBoot logon task */
        ShellExecuteW(NULL, L"open", destExe, L"--silent", g_btgDir, SW_HIDE);
    }
    SetFooterStatus(L"Bluetooth Guard updated with new UI & icon!", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
    return TRUE;
}

static void WorkerInstallKeyboardLighting(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_kblDir);

    wchar_t kblExe[MAX_PATH], lp[MAX_PATH], uiDll[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    SetFooterStatus(L"Stopping running Keyboard Lighting for clean update...", 10);
    if (FileExistsW(kblExe)) {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = kblExe;
        sei.lpParameters = L"--stop";
        sei.nShow  = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2000);
            CloseHandle(sei.hProcess);
        }
    }
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);
    Sleep(300);
    _snwprintf(uiDll, MAX_PATH, L"%s\\Ui.dll", g_kblDir);
    DeleteFileW(uiDll);

    const wchar_t *files[] = {
        L"KeyboardLighting.exe", L"Aura-Background.ps1", L"app.ico",
        L"Tray.ps1", L"ui_controls.cs.txt", L"Setup.ps1", L"Setup.bat",
        L"Restart.bat", L"Zones.ps1", L"Zones.bat", L"MyEffect.ps1", L"README.md"
    };
    int count = (int)(sizeof(files) / sizeof(files[0]));
    for (int i = 0; i < count; i++) {
        wchar_t msg[256], rel[128], dest[MAX_PATH];
        _snwprintf(msg, 255, L"Downloading Keyboard Lighting (%d/%d): %s...", i + 1, count, files[i]);
        SetFooterStatus(msg, 15 + (i * 80) / count);
        _snwprintf(rel, 127, L"keyboard-lighting/%s", files[i]);
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        DownloadRawRepoFile(branch, rel, dest);
    }

    if (FileExistsW(kblExe)) {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
            CreateShortcutLink(lnk, kblExe, g_kblDir, L"Keyboard Lighting - ROG Strix G16 RGB");
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        }
        ShellExecuteW(NULL, L"open", kblExe, NULL, g_kblDir, SW_SHOWNORMAL);
        SetFooterStatus(L"Keyboard Lighting native EXE updated and launched!", 0);
    } else {
        SetFooterStatus(L"Could not download KeyboardLighting.exe from GitHub.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallSpatialHud(void) {
    SetFooterStatus(L"Checking latest Spatial HUD release...", 20);
    if (!g_remHudPath[0]) WorkerCheckRemote();

    wchar_t dlPath[512], jarName[128];
    EnterCriticalSection(&g_cs);
    wcsncpy(dlPath, g_remHudPath, 511); dlPath[511] = L'\0';
    wcsncpy(jarName, g_remHudName, 127); jarName[127] = L'\0';
    LeaveCriticalSection(&g_cs);

    if (!dlPath[0] || !jarName[0]) {
        SetFooterStatus(L"No published Spatial HUD JAR found under 'spatial-hud-latest'.", 0);
        return;
    }

    SetFooterStatus(L"Downloading Spatial HUD Fabric JAR...", 60);
    DWORD sz = 0; int st = 0;
    BYTE *jarBytes = HttpGetBuffer(L"github.com", dlPath, &sz, &st);
    if (!jarBytes || st != 200 || sz < 4096 || jarBytes[0] != 'P' || jarBytes[1] != 'K') {
        free(jarBytes);
        SetFooterStatus(L"Spatial HUD download failed or invalid archive.", 0);
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
        _snwprintf(msg, 299, L"Spatial HUD updated (%s).", jarName);
        SetFooterStatus(msg, 0);
    } else {
        SetFooterStatus(L"Could not write JAR to mods folder (close Minecraft first).", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallOrcaPlugins(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);

    SetFooterStatus(L"Downloading Wave Overhangs & Unlayered Infill...", 40);
    EnsureDirTree(g_orcaBatDir);

    wchar_t batPath[MAX_PATH], tmpWave[MAX_PATH], tmpInf[MAX_PATH];
    _snwprintf(batPath, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    _snwprintf(tmpWave, MAX_PATH, L"%s\\wave_overhangs_orca.py", g_hubCfgDir);
    _snwprintf(tmpInf,  MAX_PATH, L"%s\\unlayered_infill_orca.py", g_hubCfgDir);

    BOOL ok1 = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", batPath);
    BOOL ok2 = DownloadRawRepoFile(branch, L"plugins/wave-overhangs/wave_overhangs_orca.py", tmpWave);
    BOOL ok3 = DownloadRawRepoFile(branch, L"plugins/unlayered-infill/unlayered_infill_orca.py", tmpInf);
    if (!ok2 || !ok3) {
        SetFooterStatus(L"Failed downloading OrcaSlicer plugins from GitHub.", 0);
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
                EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);

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

    SetFooterStatus(L"OrcaSlicer plugins updated — restart OrcaSlicer to load.", 0);
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
        SetFooterStatus(L"Syncing Arena Link files from GitHub...", 15 + (i * 80) / n);
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

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    SetFooterStatus(L"Project Hub updated — relaunching...", 100);
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
            WorkerInstallKeyboardLighting();
            WorkerInstallOrcaPlugins();
            WorkerInstallSpatialHud();
            WorkerSyncArenaLink();
            WorkerCheckRemote();
            SetFooterStatus(L"All suite apps updated!", 0);
            break;
        case TASK_SELF_UPDATE:         WorkerSelfUpdateHub(); break;
        default: break;
        }
    }
    return 0;
}

static void RestartKeyboardLightingCleanly(void) {
    wchar_t lp[MAX_PATH];
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);

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
 * Sub-Pixel Anti-Aliased Smooth Rounded UI Renderer (32-bit DIBSection)
 * ============================================================================ */

static inline uint32_t PackRgb(COLORREF c) {
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static inline uint32_t BlendRgb(uint32_t dst, uint32_t src, int a256) {
    if (a256 <= 0) return dst;
    if (a256 >= 256) return src;
    int inv = 256 - a256;
    uint32_t rb = (((dst & 0x00FF00FFu) * (uint32_t)inv) + ((src & 0x00FF00FFu) * (uint32_t)a256)) >> 8;
    uint32_t g  = (((dst & 0x0000FF00u) * (uint32_t)inv) + ((src & 0x0000FF00u) * (uint32_t)a256)) >> 8;
    return (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void FillCanvasRect(uint32_t *px, int W, int H, int x0, int y0, int x1, int y1, COLORREF col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t p = PackRgb(col);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; x++) row[x] = p;
    }
}

static void SmoothRoundRect(uint32_t *px, int W, int H, const RECT *rc, float radius,
                            COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    float x0 = (float)rc->left, y0 = (float)rc->top;
    float x1 = (float)rc->right, y1 = (float)rc->bottom;
    float rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0.5f || rh <= 0.5f) return;
    if (radius * 2.0f > rw) radius = rw * 0.5f;
    if (radius * 2.0f > rh) radius = rh * 0.5f;

    float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    float hx = 0.5f * rw - radius, hy = 0.5f * rh - radius;

    int iy0 = (int)floorf(y0 - 1.0f);
    int iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f);
    int ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP   = PackRgb(fillCol);
    uint32_t borderP = PackRgb(borderCol);
    BOOL sameBorder  = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd;
            if (qx > 0.0f && qy > 0.0f) {
                sd = sqrtf(qx * qx + qy * qy) - radius;
            } else {
                sd = (qx > qy ? qx : qy) - radius;
            }
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) {
                row[x] = c;
            } else {
                int cov = (int)((0.5f - sd) * 256.0f);
                row[x] = BlendRgb(row[x], c, cov);
            }
        }
    }
}

static void DrawActionBtn(HDC hdc, uint32_t *px, int W, int H, const ActionBtn *btn,
                          int dpi, int clipTop, int clipBottom) {
    BOOL hot = (g_hoverId == btn->id);
    COLORREF bg, border, fg = COL_TEXT_PRIMARY;
    if (btn->style == BTN_PRIMARY) {
        bg     = hot ? COL_PRIMARY_HOVER : COL_PRIMARY_BG;
        border = bg;
    } else if (btn->style == BTN_AMBER) {
        bg     = hot ? COL_AMBER_BTN_HOVER : COL_AMBER_BTN;
        border = COL_AMBER;
        fg     = COL_AMBER;
    } else {
        bg     = hot ? COL_BTN_HOVER : COL_BTN_BG;
        border = hot ? COL_HOVER_BORDER : COL_BTN_BORDER;
    }
    SmoothRoundRect(px, W, H, &btn->rc, (float)S(9, dpi), bg, border, clipTop, clipBottom);
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
    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int cardH = S(80, dpi);
    int gap = S(11, dpi);

    int embSz = S(36, dpi);
    int embY = (headerH - embSz) / 2;
    SetRect(&g_rcEmblem, pad, embY, pad + embSz, embY + embSz);

    int bx = w - pad;
    int topBtnH = S(35, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    for (int i = 2; i >= 0; i--) {
        int bw = S(g_topBtns[i].widthPx, dpi);
        bx -= bw;
        SetRect(&g_topBtns[i].rc, bx, topBtnY, bx + bw, topBtnY + topBtnH);
        bx -= S(9, dpi);
    }

    int contentH = 5 * cardH + 4 * gap + S(24, dpi);
    int viewH = h - headerH - footerH;
    g_maxScrollY = (contentH > viewH) ? (contentH - viewH) : 0;
    if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
    if (g_scrollY < 0) g_scrollY = 0;

    int startY = headerH + ((viewH > contentH) ? (viewH - contentH) / 2 + S(12, dpi) : S(12, dpi)) - g_scrollY;
    int btnH = S(30, dpi);
    for (int c = 0; c < 5; c++) {
        SetRect(&g_cards[c].rcCard, pad, startY, w - pad, startY + cardH);
        int rx = w - pad - S(16, dpi);
        int by = startY + cardH - S(12, dpi) - btnH;
        for (int b = g_cards[c].btnCount - 1; b >= 0; b--) {
            int bw = S(g_cards[c].btns[b].widthPx, dpi);
            rx -= bw;
            SetRect(&g_cards[c].btns[b].rc, rx, by, rx + bw, by + btnH);
            rx -= S(7, dpi);
        }
        startY += cardH + gap;
    }
}

static int HitTestButtons(POINT pt, int headerBottom, int footerTop) {
    if (PtInRect(&g_rcEmblem, pt)) return ID_EMBLEM_MENU;
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

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; /* top-down 32bpp */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC hdc = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(hdc); return; }
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);
    SetBkMode(hdc, TRANSPARENT);

    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int footerTop = h - footerH;

    /* Soft low-glare slate canvas */
    FillCanvasRect(px, w, h, 0, 0, w, footerTop, COL_BG);
    FillCanvasRect(px, w, h, 0, footerTop, w, h, COL_FOOTER_BG);

    /* Clipped project cards */
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, 0, headerH + 1, w, footerTop - 1);

    int cardIcoSz = S(44, dpi);
    for (int c = 0; c < 5; c++) {
        const ProjectCard *card = &g_cards[c];
        if (card->rcCard.bottom < headerH || card->rcCard.top > footerTop) continue;

        SmoothRoundRect(px, w, h, &card->rcCard, (float)S(14, dpi),
                        COL_CARD_BG, COL_CARD_BORDER, headerH + 1, footerTop - 1);

        COLORREF tagFg = COL_TEXT_SECONDARY, tagBg = COL_IDLE_BG, barCol = COL_IDLE_SLATE;
        if (card->badgeKind == BADGE_OK) {
            tagFg = COL_MINT; tagBg = COL_MINT_BG; barCol = COL_MINT;
        } else if (card->badgeKind == BADGE_WARN) {
            tagFg = COL_AMBER; tagBg = COL_AMBER_BG; barCol = COL_AMBER;
        } else if (card->badgeKind == BADGE_SKY) {
            tagFg = COL_SKY; tagBg = COL_SKY_BG; barCol = COL_SKY;
        } else if (card->badgeKind == BADGE_DANGER) {
            tagFg = COL_CORAL; tagBg = COL_CORAL_BG; barCol = COL_CORAL;
        }

        /* Soft inset rounded accent capsule on left of card */
        RECT barRc = {
            card->rcCard.left + S(1, dpi),
            card->rcCard.top + S(16, dpi),
            card->rcCard.left + S(5, dpi),
            card->rcCard.bottom - S(16, dpi)
        };
        SmoothRoundRect(px, w, h, &barRc, (float)S(2, dpi), barCol, barCol, headerH + 1, footerTop - 1);

        /* Embedded 32-bit RGBA Suite App Icon */
        if (g_cardIcons[c]) {
            int ix = card->rcCard.left + S(18, dpi);
            int iy = (card->rcCard.top + card->rcCard.bottom - cardIcoSz) / 2;
            DrawIconEx(hdc, ix, iy, g_cardIcons[c], cardIcoSz, cardIcoSz, 0, NULL, DI_NORMAL);
        }

        /* Smooth rounded status pill badge on top right */
        SelectObject(hdc, g_fontBadge);
        const wchar_t *badgeTxt = card->badge[0] ? card->badge : L"Checking";
        SIZE bsz = {0, 0};
        GetTextExtentPoint32W(hdc, badgeTxt, (int)wcslen(badgeTxt), &bsz);
        int pillH = S(23, dpi);
        int pillW = bsz.cx + S(28, dpi);
        RECT pillRc = {
            card->rcCard.right - S(16, dpi) - pillW,
            card->rcCard.top + S(11, dpi),
            card->rcCard.right - S(16, dpi),
            card->rcCard.top + S(11, dpi) + pillH
        };
        SmoothRoundRect(px, w, h, &pillRc, (float)pillH * 0.5f, tagBg, tagBg, headerH + 1, footerTop - 1);

        int dotD = S(6, dpi);
        RECT dotRc = {
            pillRc.left + S(9, dpi),
            pillRc.top + (pillH - dotD) / 2,
            pillRc.left + S(9, dpi) + dotD,
            pillRc.top + (pillH - dotD) / 2 + dotD
        };
        SmoothRoundRect(px, w, h, &dotRc, (float)dotD * 0.5f, tagFg, tagFg, headerH + 1, footerTop - 1);

        RECT pillTxtRc = { pillRc.left + S(19, dpi), pillRc.top, pillRc.right - S(8, dpi), pillRc.bottom };
        SetTextColor(hdc, tagFg);
        DrawTextW(hdc, badgeTxt, -1, &pillTxtRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        /* Clean Title Case card title + calm summary */
        int textLeft = card->rcCard.left + S(74, dpi);
        SelectObject(hdc, g_fontTitle);
        SetTextColor(hdc, COL_TEXT_PRIMARY);
        RECT titleRc = { textLeft, card->rcCard.top + S(13, dpi), pillRc.left - S(12, dpi), card->rcCard.top + S(37, dpi) };
        DrawTextW(hdc, card->title, -1, &titleRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        int firstBtnLeft = (card->btnCount > 0) ? card->btns[0].rc.left : (card->rcCard.right - S(16, dpi));
        SelectObject(hdc, g_fontBody);
        SetTextColor(hdc, COL_TEXT_SECONDARY);
        RECT detRc = { textLeft, card->rcCard.top + S(39, dpi), firstBtnLeft - S(14, dpi), card->rcCard.bottom - S(12, dpi) };
        DrawTextW(hdc, card->detail, -1, &detRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        for (int b = 0; b < card->btnCount; b++) {
            DrawActionBtn(hdc, px, w, h, &card->btns[b], dpi, headerH + 1, footerTop - 1);
        }
    }

    RestoreDC(hdc, saved);

    /* Header: Suite Tri-Spoke Core Emblem + natural title + calm subtitle + rounded buttons */
    int embSz = S(36, dpi);
    DrawWayfinderEmblem(hdc, g_rcEmblem.left, g_rcEmblem.top, embSz);

    int hdrTextLeft = g_rcEmblem.right + S(14, dpi);
    int hdrRightLimit = g_topBtns[0].rc.left - S(14, dpi);
    SelectObject(hdc, g_fontHero);
    SetTextColor(hdc, COL_TEXT_PRIMARY);
    RECT hTitle = { hdrTextLeft, S(13, dpi), hdrRightLimit, S(40, dpi) };
    DrawTextW(hdc, L"Project Hub", -1, &hTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, COL_TEXT_MUTED);
    RECT hSub = { hdrTextLeft, S(40, dpi), hdrRightLimit, S(60, dpi) };
    DrawTextW(hdc, L"Suite Control  \x00B7  v" HUB_VERSION_W, -1, &hSub,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    for (int i = 0; i < 3; i++) {
        DrawActionBtn(hdc, px, w, h, &g_topBtns[i], dpi, 0, headerH);
    }

    FillCanvasRect(px, w, h, pad, headerH, w - pad, headerH + 1, COL_DIVIDER);
    FillCanvasRect(px, w, h, 0, footerTop, w, footerTop + 1, COL_DIVIDER);

    wchar_t footMsg[512];
    int pct = 0;
    EnterCriticalSection(&g_cs);
    wcsncpy(footMsg, g_statusMsg, 511); footMsg[511] = L'\0';
    pct = g_progressVal;
    LeaveCriticalSection(&g_cs);

    if (pct > 0 && pct < 100) {
        int fillW = (w * pct) / 100;
        FillCanvasRect(px, w, h, 0, footerTop - 1, fillW, footerTop + S(2, dpi), COL_SKY);
    }

    COLORREF dotCol = g_hubUpdAvail ? COL_AMBER : ((pct > 0 && pct < 100) ? COL_SKY : COL_MINT);
    int fdSz = S(8, dpi);
    RECT fDot = { pad, footerTop + (footerH - fdSz) / 2, pad + fdSz, footerTop + (footerH - fdSz) / 2 + fdSz };
    SmoothRoundRect(px, w, h, &fDot, (float)fdSz * 0.5f, dotCol, dotCol, footerTop, h);

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, g_hubUpdAvail ? COL_AMBER : COL_TEXT_SECONDARY);
    RECT fTxt = { pad + S(16, dpi), footerTop, w - pad, h };
    DrawTextW(hdc, footMsg, -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    BitBlt(targetDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

static void ShowUtilityMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SHORTCUT,  L"Pin Project Hub Shortcut to Desktop");
    AppendMenuW(hMenu, MF_STRING, ID_BTN_SELF_UPDATE, L"Self-Update Project Hub Now");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SUITE_DIR, L"Open Managed Suite Folder");

    POINT pt = { g_rcEmblem.left, g_rcEmblem.bottom + 4 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static void HandleActionCommand(HWND hwnd, int id) {
    wchar_t p[MAX_PATH];
    switch (id) {
    case ID_BTN_REFRESH_ALL:
        RefreshProjectStatuses();
        EnqueueTask(TASK_CHECK_REMOTE);
        break;
    case ID_BTN_SELF_UPDATE:
        EnqueueTask(TASK_SELF_UPDATE);
        break;
    case ID_BTN_UPDATE_ALL:
        EnqueueTask(TASK_UPDATE_ALL);
        break;
    case ID_EMBLEM_MENU:
        ShowUtilityMenu(hwnd);
        break;
    case ID_MENU_SHORTCUT: {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
            if (CreateShortcutLink(lnk, g_exePath, g_exeDir, L"Project Hub - Status & Updates")) {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
                SetFooterStatus(L"Pinned 'Project Hub' shortcut to Desktop.", 0);
            }
        }
        break;
    }
    case ID_MENU_SUITE_DIR:
        EnsureDirTree(g_suiteDir);
        ShellExecuteW(hwnd, L"open", g_suiteDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_BTG_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_btgDir, SW_SHOWNORMAL);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, NULL, NULL, SW_SHOWNORMAL);
        else EnqueueTask(TASK_OPEN_OR_INSTALL_BTG);
        break;
    case ID_BTG_UPDATE:
        EnqueueTask(TASK_INSTALL_BTG);
        break;
    case ID_BTG_STOP:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, L"--stop", g_btgDir, SW_HIDE);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, L"--stop", NULL, SW_HIDE);
        SetFooterStatus(L"Stopped Bluetooth Guard.", 0);
        break;
    case ID_BTG_FOLDER:
        EnsureDirTree(g_btgDir);
        ShellExecuteW(hwnd, L"open", g_btgDir, NULL, NULL, SW_SHOWNORMAL);
        break;

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
        RebuildFontsAndIcons(hwnd);
        InitCardDefinitions();
        RefreshProjectStatuses();
        SetTimer(hwnd, 1, 2000, NULL);
        EnqueueTask(TASK_CHECK_REMOTE);
        return 0;

    case WM_DPICHANGED: {
        RebuildFontsAndIcons(hwnd);
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
        mmi->ptMinTrackSize.x = S(720, dpi);
        mmi->ptMinTrackSize.y = S(460, dpi);
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
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
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
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
        if (hit != 0) HandleActionCommand(hwnd, hit);
        return 0;
    }

    case WM_RBUTTONUP:
        ShowUtilityMenu(hwnd);
        return 0;

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
    g_hInst = hInst;

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

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
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
                             CW_USEDEFAULT, CW_USEDEFAULT, S(860, dpi), S(620, dpi),
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
