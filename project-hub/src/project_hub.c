#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define HUB_VERSION       "2.0.0"
#define HUB_VERSION_W     L"2.0.0"
#define APP_TITLE         L"Project Hub"
#define SINGLE_MUTEX_NAME L"Local\\Tests_ProjectHub_SingleInstance_v2"
#define SHOW_EVENT_NAME   L"Local\\Tests_ProjectHub_ShowWindow_v2"

#define REPO_OWNER_W      L"ajani190819-ops"
#define REPO_NAME_W       L"Tests"
#define FALLBACK_BRANCH_W L"arena/e895b94f-tests"

#define WM_HUB_REFRESH    (WM_APP + 10)
#define WM_HUB_STATUS     (WM_APP + 11)
#define WM_HUB_SELFUPD    (WM_APP + 12)

/* Control IDs */
#define ID_BTN_REFRESH_ALL   1001
#define ID_BTN_UPDATE_ALL    1002
#define ID_BTN_SHORTCUT      1003
#define ID_BTN_SELF_UPDATE   1004

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
    HWND grp;
    HWND lblStatus;
    HWND btns[4];
    int  btnCount;
} CardWidgets;

static HWND             g_hwnd        = NULL;
static HWND             g_lblHeader   = NULL;
static HWND             g_lblSub      = NULL;
static HWND             g_btnRefresh  = NULL;
static HWND             g_btnUpdAll   = NULL;
static HWND             g_btnShortcut = NULL;
static HWND             g_btnSelfUpd  = NULL;
static HWND             g_progress    = NULL;
static HWND             g_lblFooter   = NULL;
static CardWidgets      g_cards[5];

static HFONT            g_fontTitle   = NULL;
static HFONT            g_fontGroup   = NULL;
static HFONT            g_fontBody    = NULL;
static HICON            g_appIcon     = NULL;
static HANDLE           g_showEvent   = NULL;
static CRITICAL_SECTION g_cs;

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
static wchar_t          g_statusMsg[512]    = L"Ready — Checking local status and GitHub updates...";
static int              g_progressVal       = 0;

/* Worker queue */
static int              g_taskQueue[32];
static int              g_qHead             = 0;
static int              g_qTail             = 0;
static HANDLE           g_qSem              = NULL;
static volatile LONG    g_running           = 1;

typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

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

    HINTERNET hSession = WinHttpOpen(L"Tests-ProjectHub/2.0",
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
                                   L"User-Agent: Tests-ProjectHub/2.0\r\nAccept: */*\r\n",
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

    /* Probe whether PR has been merged to main yet (checks bluetooth-guard directory on main) */
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

/* Tiny string extractors for JSON / Python constants */
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

    /* Detect sibling folders if ProjectHub.exe lives inside a git clone, else managed folder */
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

    /* Load saved Spatial HUD mods target directory if present */
    wchar_t hudTargetTxt[MAX_PATH];
    _snwprintf(hudTargetTxt, MAX_PATH, L"%s\\SpatialHudUpdater\\target-directory.txt", g_localApp);
    char *savedMods = ReadTextFileUtf8(hudTargetTxt, NULL);
    if (savedMods) {
        /* Trim UTF-8 BOM + trailing newlines */
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

    wchar_t line[512], path[MAX_PATH];

    /* 1. Bluetooth Guard */
    _snwprintf(path, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    BOOL hasBtg = FileExistsW(path);
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    int blocked = 0, total = 0;
    int btgState = CheckPidLockStatus(btgLock, 12.0, &blocked, &total);
    if (btgState == 1) {
        _snwprintf(line, 511, L"Status: RUNNING — Guarding Bluetooth (%d of %d paired device(s) Blocked / Phone Safe)",
                   blocked, total);
    } else if (hasBtg) {
        _snwprintf(line, 511, L"Status: Installed (Stopped) — Click \"Open / Start\" to protect phone earbuds (%s)", g_btgDir);
    } else {
        wcsncpy(line, L"Status: Not installed yet — Click \"Install / Update\" to download & start BluetoothGuard.exe", 511);
    }
    SetWindowTextW(g_cards[0].lblStatus, line);

    /* 2. Keyboard Lighting */
    _snwprintf(path, MAX_PATH, L"%s\\Tray.ps1", g_kblDir);
    BOOL hasKbl = FileExistsW(path);
    wchar_t kblLock[MAX_PATH];
    _snwprintf(kblLock, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    int kblState = CheckPidLockStatus(kblLock, 12.0, NULL, NULL);
    if (kblState == 2) {
        wcsncpy(line, L"Status: FROZEN / STUCK — Click \"Restart / Unfreeze\" to recover immediately!", 511);
    } else if (kblState == 1) {
        wcsncpy(line, L"Status: RUNNING — Active in system tray", 511);
    } else if (hasKbl) {
        _snwprintf(line, 511, L"Status: Installed (Stopped) — Folder: %s", g_kblDir);
    } else {
        wcsncpy(line, L"Status: Not installed yet — Click \"Download / Update\" to download & configure Keyboard Lighting", 511);
    }
    SetWindowTextW(g_cards[1].lblStatus, line);

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
        _snwprintf(line, 511, L"Status: Not installed in mods folder (%s)", g_modsDir);
    } else if (remHudSz > 0 && foundSize != remHudSz) {
        _snwprintf(line, 511, L"Status: UPDATE AVAILABLE — Installed: %s (%lu KB) | Latest: %s (%lu KB)",
                   foundJar, (unsigned long)(foundSize / 1024), remHud, (unsigned long)(remHudSz / 1024));
    } else {
        _snwprintf(line, 511, L"Status: UP TO DATE — %s (%lu KB) in %s",
                   foundJar, (unsigned long)(foundSize / 1024), g_modsDir);
    }
    SetWindowTextW(g_cards[2].lblStatus, line);

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
        _snwprintf(line, 511, L"Status: Not installed — Latest on GitHub: Wave Overhangs %S | Unlayered Infill %S",
                   remWave[0] ? remWave : "...", remInfill[0] ? remInfill : "...");
    } else {
        BOOL upd = (remWave[0] && strcmp(locWave, remWave) != 0) ||
                   (remInfill[0] && strcmp(locInfill, remInfill) != 0);
        _snwprintf(line, 511, L"Status: %s — Installed: Wave Overhangs %S, Unlayered Infill %S",
                   upd ? L"UPDATE AVAILABLE" : L"UP TO DATE",
                   locWave[0] ? locWave : "none", locInfill[0] ? locInfill : "none");
    }
    SetWindowTextW(g_cards[3].lblStatus, line);

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
        _snwprintf(line, 511, L"Status: Source v%S (%s) — %d linked local Git folder(s)",
                   locArena, g_arenaDir, linkedRepos);
    } else {
        _snwprintf(line, 511, L"Status: %d linked local Git folder(s) — Click \"Sync Source\" to fetch latest Arena Link source",
                   linkedRepos);
    }
    SetWindowTextW(g_cards[4].lblStatus, line);
}

static void WorkerCheckRemote(void) {
    wchar_t branch[64];
    SetFooterStatus(L"Checking GitHub for project versions & updates...", 15);
    EnsureActiveBranch(branch, 64);

    /* 1. plugins.json */
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
    SetFooterStatus(L"Checking Arena Link & Spatial HUD releases...", 55);

    /* 2. arena-link-windows/package.json */
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

    /* 3. Spatial HUD GitHub release metadata */
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
    _snwprintf(doneMsg, 255, L"All projects checked against GitHub (%s).", branch);
    SetFooterStatus(doneMsg, 100);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static BOOL WorkerInstallBluetoothGuard(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_btgDir);

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);

    /* Cleanly stop running BluetoothGuard.exe first if updating */
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

    SetFooterStatus(L"Downloading native BluetoothGuard.exe from GitHub...", 55);
    if (!DownloadRawRepoFile(branch, L"bluetooth-guard/BluetoothGuard.exe", destExe)) {
        SetFooterStatus(L"Error: Could not download BluetoothGuard.exe from GitHub.", 0);
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
    SetFooterStatus(L"Bluetooth Guard installed and launched!", 100);
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
        SetFooterStatus(msg, 10 + (i * 75) / count);
        _snwprintf(rel, 127, L"keyboard-lighting/%s", files[i]);
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        DownloadRawRepoFile(branch, rel, dest);
    }

    wchar_t setupBat[MAX_PATH];
    _snwprintf(setupBat, MAX_PATH, L"%s\\Setup.bat", g_kblDir);
    if (FileExistsW(setupBat)) {
        ShellExecuteW(g_hwnd, L"open", setupBat, NULL, g_kblDir, SW_SHOWNORMAL);
        SetFooterStatus(L"Downloaded Keyboard Lighting files and started Setup (approve Admin prompt).", 100);
    } else {
        SetFooterStatus(L"Error: Could not download Keyboard Lighting files.", 0);
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

    SetFooterStatus(L"Downloading Spatial HUD Fabric JAR...", 55);
    DWORD sz = 0; int st = 0;
    BYTE *jarBytes = HttpGetBuffer(L"github.com", dlPath, &sz, &st);
    if (!jarBytes || st != 200 || sz < 4096 || jarBytes[0] != 'P' || jarBytes[1] != 'K') {
        free(jarBytes);
        SetFooterStatus(L"Error: Spatial HUD download failed or was not a valid JAR archive.", 0);
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
        _snwprintf(msg, 299, L"Installed %s into %s!", jarName, g_modsDir);
        SetFooterStatus(msg, 100);
    } else {
        SetFooterStatus(L"Error: Could not write JAR to mods folder (is Minecraft open?).", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallOrcaPlugins(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);

    SetFooterStatus(L"Downloading Wave Overhangs, Unlayered Infill & Orca-Plugins.bat...", 35);
    EnsureDirTree(g_orcaBatDir);

    wchar_t batPath[MAX_PATH], tmpWave[MAX_PATH], tmpInf[MAX_PATH];
    _snwprintf(batPath, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    _snwprintf(tmpWave, MAX_PATH, L"%s\\wave_overhangs_orca.py", g_hubCfgDir);
    _snwprintf(tmpInf,  MAX_PATH, L"%s\\unlayered_infill_orca.py", g_hubCfgDir);

    BOOL ok1 = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", batPath);
    BOOL ok2 = DownloadRawRepoFile(branch, L"plugins/wave-overhangs/wave_overhangs_orca.py", tmpWave);
    BOOL ok3 = DownloadRawRepoFile(branch, L"plugins/unlayered-infill/unlayered_infill_orca.py", tmpInf);
    if (!ok2 || !ok3) {
        SetFooterStatus(L"Error: Failed downloading OrcaSlicer plugin files from GitHub.", 0);
        (void)ok1;
        return;
    }

    /* Install into OrcaSlicer data directory (and any OrcaSlicer* sibling profiles in %APPDATA%) */
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

    SetFooterStatus(L"Updated Wave Overhangs + Unlayered Infill + Orca-Plugins.bat! Restart OrcaSlicer to load.", 100);
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
        SetFooterStatus(L"Syncing Arena Link files from GitHub...", 20 + (i * 75) / n);
        DownloadRawRepoFile(branch, rel, dest);
    }
    SetFooterStatus(L"Arena Link source files updated cleanly.", 100);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerSelfUpdateHub(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    SetFooterStatus(L"Downloading latest ProjectHub.exe from GitHub...", 40);

    wchar_t newExe[MAX_PATH], oldExe[MAX_PATH];
    _snwprintf(newExe, MAX_PATH, L"%s.new", g_exePath);
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    if (!DownloadRawRepoFile(branch, L"project-hub/ProjectHub.exe", newExe)) {
        SetFooterStatus(L"Could not download newer ProjectHub.exe from GitHub.", 0);
        return;
    }

    if (!MoveFileExW(g_exePath, oldExe, MOVEFILE_REPLACE_EXISTING) ||
        !MoveFileExW(newExe, g_exePath, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(newExe);
        SetFooterStatus(L"Error replacing ProjectHub.exe on disk.", 0);
        return;
    }

    SetFooterStatus(L"Updated ProjectHub.exe — restarting into new version...", 100);
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
            SetFooterStatus(L"Finished updating all projects!", 100);
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
    SetFooterStatus(L"Sent clean restart to Keyboard Lighting.", 100);
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

static void CreateProjectCard(HWND parent, int idx, const wchar_t *title,
                              int nBtns, const wchar_t **labels, const int *ids) {
    g_cards[idx].grp = CreateWindowExW(0, L"BUTTON", title,
                                       WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                       0, 0, 100, 100, parent, NULL, NULL, NULL);
    g_cards[idx].lblStatus = CreateWindowExW(0, L"STATIC", L"Checking...",
                                             WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS,
                                             0, 0, 100, 22, parent, NULL, NULL, NULL);
    g_cards[idx].btnCount = nBtns;
    for (int i = 0; i < nBtns; i++) {
        g_cards[idx].btns[i] = CreateWindowExW(0, L"BUTTON", labels[i],
                                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                               0, 0, 100, 30, parent, (HMENU)(INT_PTR)ids[i], NULL, NULL);
    }
}

static void LayoutHub(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int m = S(16, dpi);
    int cw = w - m * 2;

    MoveWindow(g_lblHeader, m, S(12, dpi), cw / 2, S(26, dpi), TRUE);
    MoveWindow(g_lblSub,    m, S(38, dpi), cw / 2, S(18, dpi), TRUE);

    int topBtnW = S(116, dpi);
    int bx = w - m - (topBtnW * 4 + S(8, dpi) * 3);
    MoveWindow(g_btnRefresh,  bx,                            S(16, dpi), topBtnW, S(32, dpi), TRUE);
    MoveWindow(g_btnUpdAll,   bx + (topBtnW + S(8, dpi)),    S(16, dpi), topBtnW, S(32, dpi), TRUE);
    MoveWindow(g_btnShortcut, bx + (topBtnW + S(8, dpi)) * 2, S(16, dpi), topBtnW, S(32, dpi), TRUE);
    MoveWindow(g_btnSelfUpd,  bx + (topBtnW + S(8, dpi)) * 3, S(16, dpi), topBtnW, S(32, dpi), TRUE);

    int y = S(66, dpi);
    int cardH = S(92, dpi);
    for (int i = 0; i < 5; i++) {
        MoveWindow(g_cards[i].grp, m, y, cw, cardH, TRUE);
        MoveWindow(g_cards[i].lblStatus, m + S(14, dpi), y + S(22, dpi), cw - S(28, dpi), S(20, dpi), TRUE);
        int nb = g_cards[i].btnCount;
        int bw = S(156, dpi);
        for (int b = 0; b < nb; b++) {
            MoveWindow(g_cards[i].btns[b],
                       m + S(14, dpi) + b * (bw + S(10, dpi)),
                       y + S(48, dpi), bw, S(32, dpi), TRUE);
        }
        y += cardH + S(8, dpi);
    }

    MoveWindow(g_progress,  m, y + S(4, dpi), cw, S(14, dpi), TRUE);
    MoveWindow(g_lblFooter, m, y + S(24, dpi), cw, S(22, dpi), TRUE);
}

static LRESULT CALLBACK HubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int dpi = GetWinDpi(hwnd);
        g_fontTitle = CreateFontW(-S(16, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontGroup = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBody  = CreateFontW(-S(9, dpi),  0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        g_lblHeader   = CreateWindowExW(0, L"STATIC", L"Project Hub", WS_CHILD | WS_VISIBLE,
                                        0, 0, 200, 26, hwnd, NULL, NULL, NULL);
        g_lblSub      = CreateWindowExW(0, L"STATIC", L"Check status, launch apps, and download updates in one place",
                                        WS_CHILD | WS_VISIBLE, 0, 0, 300, 18, hwnd, NULL, NULL, NULL);
        g_btnRefresh  = CreateWindowExW(0, L"BUTTON", L"Refresh Status", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        0, 0, 100, 32, hwnd, (HMENU)ID_BTN_REFRESH_ALL, NULL, NULL);
        g_btnUpdAll   = CreateWindowExW(0, L"BUTTON", L"Update All", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                        0, 0, 100, 32, hwnd, (HMENU)ID_BTN_UPDATE_ALL, NULL, NULL);
        g_btnShortcut = CreateWindowExW(0, L"BUTTON", L"Pin to Desktop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        0, 0, 100, 32, hwnd, (HMENU)ID_BTN_SHORTCUT, NULL, NULL);
        g_btnSelfUpd  = CreateWindowExW(0, L"BUTTON", L"Update Hub", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        0, 0, 100, 32, hwnd, (HMENU)ID_BTN_SELF_UPDATE, NULL, NULL);

        const wchar_t *b0[] = {L"Open / Start", L"Install / Update", L"Stop Guard", L"Open Folder"};
        const int      i0[] = {ID_BTG_OPEN, ID_BTG_UPDATE, ID_BTG_STOP, ID_BTG_FOLDER};
        CreateProjectCard(hwnd, 0, L"Bluetooth Guard  —  Stop Windows stealing phone earbuds", 4, b0, i0);

        const wchar_t *b1[] = {L"Open Panel", L"Restart / Unfreeze", L"Download / Update", L"Open Folder"};
        const int      i1[] = {ID_KBL_OPEN, ID_KBL_RESTART, ID_KBL_UPDATE, ID_KBL_FOLDER};
        CreateProjectCard(hwnd, 1, L"Keyboard Lighting  —  ROG Strix G16 RGB & System Tray", 4, b1, i1);

        const wchar_t *b2[] = {L"Install / Update Mod", L"Choose Mods Folder...", L"Open Mods Folder"};
        const int      i2[] = {ID_HUD_UPDATE, ID_HUD_PICK, ID_HUD_FOLDER};
        CreateProjectCard(hwnd, 2, L"Spatial HUD (Minecraft 26.3)  —  Modrinth Fabric Mod", 3, b2, i2);

        const wchar_t *b3[] = {L"Install / Update Plugins", L"Open Branch Picker (.bat)", L"Open Plugins Folder"};
        const int      i3[] = {ID_ORCA_UPDATE, ID_ORCA_MENU, ID_ORCA_FOLDER};
        CreateProjectCard(hwnd, 3, L"OrcaSlicer Plugins  —  Wave Overhangs + Unlayered Infill", 3, b3, i3);

        const wchar_t *b4[] = {L"Open App / Folder", L"Sync Latest Source", L"Open AppData"};
        const int      i4[] = {ID_ARENA_OPEN, ID_ARENA_SYNC, ID_ARENA_FOLDER};
        CreateProjectCard(hwnd, 4, L"Arena Link Windows  —  Local Git Repository Bridge", 3, b4, i4);

        g_progress  = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                      0, 0, 100, 14, hwnd, NULL, NULL, NULL);
        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        g_lblFooter = CreateWindowExW(0, L"STATIC", g_statusMsg, WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
                                      0, 0, 100, 22, hwnd, NULL, NULL, NULL);

        SendMessageW(g_lblHeader, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);
        SendMessageW(g_lblSub,    WM_SETFONT, (WPARAM)g_fontBody,  TRUE);
        SendMessageW(g_btnRefresh,  WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SendMessageW(g_btnUpdAll,   WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SendMessageW(g_btnShortcut, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SendMessageW(g_btnSelfUpd,  WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        SendMessageW(g_lblFooter,   WM_SETFONT, (WPARAM)g_fontBody, TRUE);

        for (int i = 0; i < 5; i++) {
            SendMessageW(g_cards[i].grp, WM_SETFONT, (WPARAM)g_fontGroup, TRUE);
            SendMessageW(g_cards[i].lblStatus, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
            for (int b = 0; b < g_cards[i].btnCount; b++)
                SendMessageW(g_cards[i].btns[b], WM_SETFONT, (WPARAM)g_fontBody, TRUE);
        }

        LayoutHub(hwnd);
        RefreshProjectStatuses();
        SetTimer(hwnd, 1, 2000, NULL);
        EnqueueTask(TASK_CHECK_REMOTE);
        return 0;
    }

    case WM_SIZE:
        LayoutHub(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_HUB_REFRESH:
        RefreshProjectStatuses();
        return 0;

    case WM_HUB_STATUS: {
        wchar_t buf[512];
        int pct = 0;
        EnterCriticalSection(&g_cs);
        wcsncpy(buf, g_statusMsg, 511); buf[511] = L'\0';
        pct = g_progressVal;
        LeaveCriticalSection(&g_cs);
        SetWindowTextW(g_lblFooter, buf);
        SendMessageW(g_progress, PBM_SETPOS, (WPARAM)pct, 0);
        return 0;
    }

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

    case WM_COMMAND: {
        int id = LOWORD(wp);
        wchar_t p[MAX_PATH];
        switch (id) {
        case ID_BTN_REFRESH_ALL:
            RefreshProjectStatuses();
            EnqueueTask(TASK_CHECK_REMOTE);
            break;
        case ID_BTN_UPDATE_ALL:
            EnqueueTask(TASK_UPDATE_ALL);
            break;
        case ID_BTN_SHORTCUT: {
            wchar_t desk[MAX_PATH], lnk[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
                _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
                if (CreateShortcutLink(lnk, g_exePath, g_exeDir, L"Project Hub - Status & Updates")) {
                    SetFooterStatus(L"Created 'Project Hub' shortcut on your Desktop!", 100);
                }
            }
            break;
        }
        case ID_BTN_SELF_UPDATE:
            EnqueueTask(TASK_SELF_UPDATE);
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
            SetFooterStatus(L"Stopped Bluetooth Guard.", 100);
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
        return 0;
    }

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

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    InitAllPaths();
    /* Clean leftover self-update backup if present */
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TestsProjectHubNativeWnd";
    RegisterClassExW(&wc);

    HANDLE hWorker = CreateThread(NULL, 0, HubWorkerThread, NULL, 0, NULL);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Project Hub",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(760, dpi), S(660, dpi),
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
