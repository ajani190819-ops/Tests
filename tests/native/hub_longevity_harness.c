/* Run the production icon/tray lifecycle with deterministic Win32 I/O.
   This measures ownership and recovery, not native Explorer or a Windows soak. */
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef int BOOL;
typedef unsigned UINT;
typedef void *HWND;
typedef void *HANDLE;
typedef int CRITICAL_SECTION;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define IMAGE_ICON 1
#define LR_LOADFROMFILE 0x10
#define NIM_ADD 0
#define NIM_MODIFY 1
#define NIM_DELETE 2
#define NIM_SETVERSION 4
#define NOTIFYICON_VERSION_4 4
#define NIF_MESSAGE 1
#define NIF_ICON 2
#define NIF_TIP 4
#define WM_HUB_TRAYICON 0x800d
#define HUB_TRAY_HOST_EVENT L"Local\\Tests_ProjectHub_UnifiedTray_v1"
#define ZeroMemory(p, n) memset(p, 0, n)

typedef struct FakeIcon { int live; } *HICON;
typedef struct { HICON hIcon; wchar_t id[24], iconFile[MAX_PATH]; } ProjectCard;
typedef struct {
    UINT cbSize;
    HWND hWnd;
    UINT uID, uFlags, uCallbackMessage, uVersion;
    HICON hIcon;
    wchar_t szTip[128];
} NOTIFYICONDATAW;

static const char *scenario;
static int iteration;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s iteration %d, C line %d: %s\n", \
    scenario, iteration, __LINE__, #c); exit(1); } } while (0)

static HWND g_hwnd = (HWND)(uintptr_t)7;
static wchar_t g_localApp[MAX_PATH] = L"C:\\User\\AppData\\Local";
static struct FakeIcon iconSlots[64];
static HICON g_appIcon = (HICON)(uintptr_t)1;
static int liveIcons, peakIcons, loadCalls, destroyCalls, lockDepth;
static CRITICAL_SECTION g_cs;
static BOOL failLoad, haveFile = TRUE, downloadOk;
static int downloadCalls;
static NOTIFYICONDATAW g_hubNid;
static BOOL g_hubTrayVisible, g_hubQuitting, shellOnline = TRUE, shellIcon;
static HANDLE g_trayHostEvent;
static wchar_t g_hubSeenAgentWait[80], g_hubSeenRainAlert[80];
static int marker, markerLive, failMarker, createCalls, closeCalls;
static int addCalls, modifyCalls, deleteCalls, versionCalls, failAdd, failModify;

static void EnterCriticalSection(CRITICAL_SECTION *cs) { CHECK(cs == &g_cs); lockDepth++; }
static void LeaveCriticalSection(CRITICAL_SECTION *cs) { CHECK(cs == &g_cs && lockDepth > 0); lockDepth--; }
static int GetWinDpi(HWND window) { CHECK(window == g_hwnd); return 96; }
static int S(int px, int dpi) { return px * dpi / 96; }
static HICON LoadImageW(void *instance, const wchar_t *path, int type, int w, int h, int flags) {
    CHECK(!instance && path[0] && type == IMAGE_ICON && w == 44 && h == 44);
    CHECK(flags == LR_LOADFROMFILE);
    loadCalls++;
    if (failLoad) { failLoad = FALSE; return NULL; }
    for (int i = 0; i < 64; i++) {
        if (iconSlots[i].live) continue;
        iconSlots[i].live = TRUE;
        if (++liveIcons > peakIcons) peakIcons = liveIcons;
        return &iconSlots[i];
    }
    return NULL; /* model Windows' finite USER-handle budget */
}
static BOOL DestroyIcon(HICON icon) {
    CHECK(lockDepth > 0);
    CHECK(icon >= iconSlots && icon < iconSlots + 64 && icon->live);
    icon->live = FALSE;
    liveIcons--;
    destroyCalls++;
    return TRUE;
}
/* Windows wide printf uses %s for wchar_t*. Translate just that spelling for
   this portable harness, preserving the production path/hash formatting. */
static int _snwprintf(wchar_t *out, size_t cap, const wchar_t *format, ...) {
    wchar_t portable[512];
    size_t n = 0;
    for (size_t i = 0; format[i] && n + 3 < 512; i++) {
        portable[n++] = format[i];
        if (format[i] == L'%' && format[i + 1] == L's') portable[n++] = L'l';
    }
    portable[n] = 0;
    va_list args;
    va_start(args, format);
    int result = vswprintf(out, cap, portable, args);
    va_end(args);
    return result;
}
static BOOL CreateDirectoryW(const wchar_t *path, void *security) { CHECK(path[0] && !security); return TRUE; }
static BOOL FileExistsW(const wchar_t *path) { CHECK(path[0]); return haveFile; }
static void EnsureProbeRef(wchar_t *out, size_t cap) { wcsncpy(out, L"released-ref", cap); }
static const wchar_t *PoolAdd(const char *utf8) { CHECK(utf8 && utf8[0]); return L"asset.ico"; }
static BOOL DownloadRawRepoFile(const wchar_t *ref, const wchar_t *path, const wchar_t *dest) {
    CHECK(ref[0] && path[0] && dest[0]); downloadCalls++; return downloadOk;
}
static HANDLE CreateEventW(void *security, BOOL manual, BOOL signaled, const wchar_t *name) {
    CHECK(!security && manual && signaled && wcscmp(name, HUB_TRAY_HOST_EVENT) == 0);
    createCalls++;
    if (failMarker) { failMarker--; return NULL; }
    CHECK(!markerLive);
    markerLive = TRUE;
    return &marker;
}
static BOOL CloseHandle(HANDLE handle) {
    CHECK(handle == &marker && markerLive);
    markerLive = FALSE; closeCalls++; return TRUE;
}
static BOOL Shell_NotifyIconW(UINT command, NOTIFYICONDATAW *icon) {
    CHECK(icon->cbSize == sizeof(*icon) && icon->hWnd == g_hwnd && icon->uID == 1);
    CHECK(icon->uCallbackMessage == WM_HUB_TRAYICON && icon->hIcon == g_appIcon);
    if (command == NIM_ADD) {
        addCalls++;
        if (failAdd) { failAdd--; return FALSE; }
        if (!shellOnline || shellIcon) return FALSE;
        shellIcon = TRUE; return TRUE;
    }
    if (command == NIM_MODIFY) {
        modifyCalls++;
        if (failModify) { failModify--; return FALSE; }
        return shellOnline && shellIcon;
    }
    if (command == NIM_SETVERSION) {
        CHECK(shellOnline && shellIcon && icon->uVersion == NOTIFYICON_VERSION_4);
        versionCalls++; return TRUE;
    }
    CHECK(command == NIM_DELETE);
    deleteCalls++; shellIcon = FALSE; return TRUE;
}
static void HubTrayConfig(wchar_t *out, size_t cap) { wcsncpy(out, L"hub.ini", cap); }
static UINT GetPrivateProfileStringW(const wchar_t *section, const wchar_t *key,
                                     const wchar_t *fallback, wchar_t *out,
                                     UINT cap, const wchar_t *file) {
    CHECK(section[0] && key[0] && file[0]); wcsncpy(out, fallback, cap); return 0;
}

/* Generated from the current production files by the Python regression. */
#include "hub_longevity_production.h"

static void TestIconLifetime(void) {
    ProjectCard cards[7] = {0};
    scenario = "catalog icon lifetime";
    for (int i = 0; i < 7; i++) swprintf(cards[i].id, 24, L"app%d", i);
    for (iteration = 0; iteration < 10000; iteration++) {
        for (int i = 0; i < 7; i++) {
            LoadCardIcon(&cards[i], "app/icon.ico");
            CHECK(cards[i].hIcon && cards[i].hIcon->live);
        }
        CHECK(liveIcons == 7); /* not seven MORE handles every minute */
        CHECK(lockDepth == 0);
    }
    CHECK(loadCalls == 70000 && destroyCalls == 69993 && peakIcons <= 8);
    HICON old = cards[0].hIcon;
    failLoad = TRUE;
    LoadCardIcon(&cards[0], "app/new-icon.ico");
    CHECK(cards[0].hIcon == old && old->live && liveIcons == 7);
    haveFile = FALSE;
    LoadCardIcon(&cards[0], "app/unavailable.ico");
    CHECK(downloadCalls == 1 && cards[0].hIcon == old && old->live);
    haveFile = TRUE;
    for (int i = 0; i < 7; i++) {
        EnterCriticalSection(&g_cs);
        DestroyIcon(cards[i].hIcon);
        LeaveCriticalSection(&g_cs);
    }
    CHECK(liveIcons == 0 && lockDepth == 0);
    puts("Hub catalog icon lifetime scenarios passed");
}

static void TestTrayLifetime(void) {
    scenario = "tray lifetime";
    failAdd = 1;
    HubTrayInit();
    CHECK(!g_hubTrayVisible && !markerLive && !shellIcon);
    HubTrayEnsureLive();
    CHECK(g_hubTrayVisible && markerLive && shellIcon && versionCalls == 1);
    for (iteration = 0; iteration < 10000; iteration++) {
        if (iteration % 17 == 0) shellIcon = FALSE; /* no TaskbarCreated message */
        HubTrayEnsureLive();
        CHECK(g_hubTrayVisible && markerLive && shellIcon);
        CHECK(g_hubNid.uFlags == (NIF_ICON | NIF_MESSAGE | NIF_TIP));
    }
    CHECK(createCalls == closeCalls + 1 && modifyCalls >= 10000);
    /* Duplicate/delayed Explorer broadcasts must not strand a live icon. */
    HubTrayRestore();
    CHECK(g_hubTrayVisible && markerLive && shellIcon);
    /* Shell unavailable: stop claiming ownership so running apps can fall back. */
    shellOnline = FALSE; shellIcon = FALSE;
    HubTrayEnsureLive();
    CHECK(!g_hubTrayVisible && !g_trayHostEvent && !markerLive);
    for (iteration = 0; iteration < 20; iteration++) HubTrayEnsureLive();
    CHECK(!markerLive);
    shellOnline = TRUE;
    failMarker = 1;
    HubTrayEnsureLive();
    CHECK(g_hubTrayVisible && shellIcon && !markerLive);
    HubTrayEnsureLive();
    CHECK(markerLive);
    /* Transient failed modify followed by duplicate add recovers via modify. */
    failModify = 1;
    HubTrayEnsureLive();
    CHECK(g_hubTrayVisible && shellIcon && markerLive);
    g_hubQuitting = TRUE;
    int calls = addCalls + modifyCalls + createCalls;
    HubTrayEnsureLive();
    HubTrayRestore();
    CHECK(calls == addCalls + modifyCalls + createCalls);
    g_hubTrayVisible = FALSE; /* last registration is uncertain, still delete */
    HubTrayRemove();
    CHECK(!markerLive && !g_trayHostEvent && !g_hubTrayVisible && !g_hubNid.cbSize);
    CHECK(deleteCalls == 1 && !shellIcon);
    calls = addCalls + modifyCalls + createCalls;
    g_hubQuitting = FALSE; /* zeroed lifetime must also be inert after removal */
    HubTrayEnsureLive();
    HubTrayRestore();
    CHECK(calls == addCalls + modifyCalls + createCalls);
    puts("Hub tray lifetime scenarios passed");
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (strcmp(argv[1], "icons") == 0) TestIconLifetime();
    else { CHECK(strcmp(argv[1], "tray") == 0); TestTrayLifetime(); }
    return 0;
}
