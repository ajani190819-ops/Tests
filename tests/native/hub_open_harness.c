/* Execute the production Hub activation code with deterministic Win32 I/O.
   This tests the real decision/call ordering, not Windows' focus-lock policy. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef int BOOL;
typedef uint32_t DWORD;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef unsigned UINT;
typedef uintptr_t WPARAM;
typedef uint64_t ULONGLONG;
typedef intptr_t LPARAM;
typedef intptr_t LONG_PTR;
typedef uintptr_t UINT_PTR;
typedef void *HANDLE;
#define TRUE 1
#define FALSE 0
#define CALLBACK
#define MAX_PATH 260
#define UPDATE_SCAN_MAX_PIDS 64
#define SYNCHRONIZE 0x100000
#define ERROR_SUCCESS 0
#define ERROR_ACCESS_DENIED 5
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
#define MB_OK 0
#define MB_ICONERROR 0x10
#define GW_OWNER 4
#define GWL_EXSTYLE (-20)
#define WS_EX_TOOLWINDOW 0x80
#define MSGFLT_ALLOW 1
#define SW_RESTORE 9
#define SW_SHOW 5
#define SW_SHOWNORMAL 1
#define SWP_NOMOVE 2
#define SWP_NOSIZE 1
#define SWP_SHOWWINDOW 0x40
#define SWP_ASYNCWINDOWPOS 0x4000
#define SWP_NOZORDER 4
#define SWP_NOACTIVATE 0x10
#define MONITOR_DEFAULTTONULL 0
#define MONITOR_DEFAULTTONEAREST 2
#define SEE_MASK_NOCLOSEPROCESS 0x40
#define SEE_MASK_FLAG_NO_UI 0x400
#define ZeroMemory(p, n) memset(p, 0, n)

typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; } MONITORINFO;
typedef struct FakeMonitor { RECT monitor, work; } *HMONITOR;
static struct FakeMonitor monitors[2];
typedef struct FakeWindow *HWND;
#define HWND_TOP ((HWND)0)
struct FakeWindow {
    BOOL exists, visible, iconic, maximized, tool, revealEnabled;
    DWORD pid;
    const wchar_t *klass, *title;
    HWND owner, popup;
    RECT bounds;
};
typedef struct { DWORD pids[UPDATE_SCAN_MAX_PIDS]; int count; } UpdateImageProcesses;
typedef struct { DWORD cbSize, dwTime; } LASTINPUTINFO;
typedef struct {
    DWORD cbSize, fMask;
    HWND hwnd;
    const wchar_t *lpVerb, *lpFile, *lpParameters, *lpDirectory;
    int nShow;
    HANDLE hProcess;
} SHELLEXECUTEINFOW;

static struct FakeWindow windows[16];
static HWND g_hwnd = &windows[0], foreground, lastFocused, lastPositioned;
static int launchCalls, showCalls, focusCalls, positionCalls, grantCalls;
static int processCloses, mutexCloses, timerStarts, timerKills, scanCalls;
static BOOL fileExists, canFocus, applyShow, denyMutex, shellOk, returnProcess, delayShell;
static DWORD lastError, inputTick, lastGranted, shellPids[16], shellError;
static int errorDialogs, failLaunchCall;
static BOOL failErrorFormat, longErrorFormat, shellSetsError;
static wchar_t errorDetails[1600];
static int lastShow, revealPosts, childShows, filterCalls, childTimers;
static BOOL insideChild, g_quitting, postOk, registerOk = TRUE, inputOk;
static HWND queuedWindow;
static UINT queuedMessage;
static WPARAM queuedWp;
static LPARAM queuedLp;
static ULONGLONG tick;
static int recoveryMoves;
static BOOL rectOk, monitorInfoOk;
static HMONITOR recoveryMonitor;
static const wchar_t *availableMutex, *reenterImage;
static const char *scenario;
static wchar_t footer[256], launchedImage[MAX_PATH], launchedArgs[128], launchedDir[MAX_PATH];
static struct { const wchar_t *path; DWORD pid; BOOL uncertain; } scans[8];

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s, C line %d: %s\n", scenario, __LINE__, #c); exit(1); } } while (0)
static int WcsIcmp(const wchar_t *a, const wchar_t *b) {
    while (*a && towlower(*a) == towlower(*b)) { a++; b++; }
    return (int)towlower(*a) - (int)towlower(*b);
}
#define _wcsicmp WcsIcmp
static const wchar_t *ImageFileNameW(const wchar_t *path) {
    const wchar_t *slash = path ? wcsrchr(path, L'\\') : NULL;
    return slash ? slash + 1 : path;
}
static DWORD GetCurrentProcessId(void) { return 1; }
static DWORD GetWindowThreadProcessId(HWND window, DWORD *pid) {
    *pid = window && window->exists ? window->pid : 0; return *pid ? 10 : 0;
}
static HWND GetForegroundWindow(void) { return foreground; }
static ULONGLONG GetTickCount64(void) { return tick; }
static BOOL GetLastInputInfo(LASTINPUTINFO *input) { input->dwTime = inputTick; return inputOk; }
static BOOL IsWindow(HWND window) { return window && window->exists; }
static BOOL IsWindowVisible(HWND window) { return IsWindow(window) && window->visible; }
static BOOL IsIconic(HWND window) { return window && window->iconic; }
static inline BOOL IsZoomed(HWND window) { return window && window->maximized; }
static inline BOOL GetWindowRect(HWND window, RECT *rect) {
    CHECK(IsWindow(window)); *rect = window->bounds; return rectOk;
}
static inline HMONITOR MonitorFromRect(const RECT *rect, DWORD flags) {
    CHECK(flags == MONITOR_DEFAULTTONULL || flags == MONITOR_DEFAULTTONEAREST);
    for (int i = 0; i < 2; i++) {
        RECT m = monitors[i].monitor;
        if (rect->left < m.right && rect->right > m.left &&
            rect->top < m.bottom && rect->bottom > m.top) return &monitors[i];
    }
    /* The OS chooses the nearest monitor; tests select that I/O result rather
       than duplicating the Windows distance/virtual-screen implementation. */
    return flags == MONITOR_DEFAULTTONEAREST ? recoveryMonitor : NULL;
}
static inline HMONITOR MonitorFromWindow(HWND window, DWORD flags) {
    CHECK(IsWindow(window)); return MonitorFromRect(&window->bounds, flags);
}
static inline BOOL GetMonitorInfoW(HMONITOR monitor, MONITORINFO *info) {
    CHECK((monitor == &monitors[0] || monitor == &monitors[1]) && info->cbSize == sizeof(*info));
    info->rcMonitor = monitor->monitor; info->rcWork = monitor->work;
    return monitorInfoOk;
}
static HWND GetLastActivePopup(HWND window) { return window->popup ? window->popup : window; }
static HWND GetWindow(HWND window, int what) { CHECK(what == GW_OWNER); return window->owner; }
static LONG_PTR GetWindowLongPtrW(HWND window, int what) {
    CHECK(what == GWL_EXSTYLE); return window->tool ? WS_EX_TOOLWINDOW : 0;
}
static int GetWindowTextLengthW(HWND window) { return window->title ? (int)wcslen(window->title) : 0; }
static HWND FindWindowW(const wchar_t *klass, const wchar_t *title) {
    CHECK(title == NULL);
    for (int i = 0; i < 16; i++)
        if (windows[i].exists && windows[i].klass && wcscmp(windows[i].klass, klass) == 0) return &windows[i];
    return NULL;
}
static BOOL EnumWindows(BOOL (*callback)(HWND, LPARAM), LPARAM lp) {
    for (int i = 0; i < 16; i++)
        if (windows[i].exists && !callback(&windows[i], lp)) return FALSE;
    return TRUE;
}
static int FindProcessesUsingImage(const wchar_t *path, UpdateImageProcesses *out) {
    scanCalls++; memset(out, 0, sizeof(*out));
    for (int i = 0; i < 8; i++) {
        if (!scans[i].path || WcsIcmp(path, scans[i].path) != 0) continue;
        if (scans[i].pid) out->pids[out->count++] = scans[i].pid;
        if (scans[i].uncertain) return -1;
    }
    return out->count;
}
static BOOL AllowSetForegroundWindow(DWORD pid) { grantCalls++; lastGranted = pid; return TRUE; }
static BOOL ShowWindowAsync(HWND window, int command) {
    CHECK(IsWindow(window)); showCalls++; lastShow = command;
    if (applyShow) {
        window->visible = TRUE; window->iconic = FALSE;
        if (command == SW_SHOWNORMAL) window->maximized = FALSE;
    }
    return TRUE;
}
static UINT RegisterWindowMessageW(const wchar_t *name) {
    CHECK(wcscmp(name, L"Tests.Suite.RevealWindow.v1") == 0);
    return registerOk ? 0xC123 : 0;
}
static BOOL ChangeWindowMessageFilterEx(HWND window, UINT message, DWORD action, void *change) {
    CHECK(IsWindow(window) && message == 0xC123 && action == MSGFLT_ALLOW && !change);
    filterCalls++; window->revealEnabled = TRUE; return TRUE;
}
static BOOL PostMessageW(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    CHECK(IsWindow(window) && message == 0xC123);
    CHECK(lastGranted == window->pid); /* grant must precede the app-owned show */
    revealPosts++;
    if (!postOk || !window->revealEnabled) return FALSE;
    queuedWindow = window; queuedMessage = message; queuedWp = wp; queuedLp = lp;
    return TRUE;
}
static BOOL ShowWindow(HWND window, int command) {
    CHECK(insideChild && IsWindow(window)); childShows++; lastShow = command;
    window->visible = TRUE; window->iconic = FALSE;
    if (command == SW_SHOWNORMAL) window->maximized = FALSE;
    return TRUE;
}
static BOOL SetWindowPos(HWND window, HWND insertAfter, int x, int y, int w, int h, DWORD flags) {
    CHECK(insertAfter == HWND_TOP && !w && !h);
    if (!(flags & SWP_NOMOVE)) {
        CHECK(insideChild && flags == (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
        LONG width = window->bounds.right - window->bounds.left;
        LONG height = window->bounds.bottom - window->bounds.top;
        window->bounds = (RECT){ x, y, x + width, y + height };
        recoveryMoves++;
    } else {
        CHECK(!x && !y);
        CHECK(flags == (DWORD)(SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | (insideChild ? 0 : SWP_ASYNCWINDOWPOS)));
    }
    positionCalls++; lastPositioned = window; return TRUE;
}
static BOOL SetForegroundWindow(HWND window) {
    CHECK((insideChild ? childShows : showCalls) > 0 && grantCalls > 0 && positionCalls > 0);
    focusCalls++; lastFocused = window;
    if (canFocus || insideChild) foreground = window;
    return canFocus || insideChild;
}
static HANDLE OpenMutexW(DWORD access, BOOL inherit, const wchar_t *name) {
    CHECK(access == SYNCHRONIZE && !inherit);
    lastError = denyMutex ? ERROR_ACCESS_DENIED : 2;
    return availableMutex && wcscmp(name, availableMutex) == 0 ? (HANDLE)(uintptr_t)1001 : NULL;
}
static DWORD GetLastError(void) { return lastError; }
static void SetLastError(DWORD error) { lastError = error; }
static DWORD FormatMessageW(DWORD flags, const void *source, DWORD code, DWORD language,
                     wchar_t *buffer, DWORD count, void *args) {
    CHECK(flags == (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS));
    CHECK(!source && code == shellError && !language && count >= 128 && !args);
    lastError = 9999; /* Formatting cannot replace the captured launch error. */
    if (failErrorFormat) return 0;
    if (longErrorFormat) {
        wmemset(buffer, L'q', count - 1); buffer[count - 1] = L'\0'; return count - 1;
    }
    const wchar_t *reason = L"Windows reason: %1!s! and %n.\r\n";
    CHECK(wcslen(reason) < count); wcscpy(buffer, reason);
    return (DWORD)wcslen(buffer);
}
static int MessageBoxW(HWND owner, const wchar_t *text, const wchar_t *title, unsigned flags) {
    CHECK(owner == g_hwnd && title && wcsstr(title, L"launch") != NULL);
    CHECK(flags == (MB_OK | MB_ICONERROR) && wcslen(text) < 1600);
    wcscpy(errorDetails, text); errorDialogs++;
    return 1;
}
static BOOL CloseHandle(HANDLE handle) {
    lastError = 8888; /* Cleanup APIs may destroy the shell failure code. */
    if ((uintptr_t)handle == 1001) mutexCloses++;
    else { CHECK((uintptr_t)handle > 900 && (uintptr_t)handle < 916); processCloses++; }
    return TRUE;
}
static DWORD GetProcessId(HANDLE handle) {
    CHECK((uintptr_t)handle > 900 && (uintptr_t)handle < 916);
    lastError = 7777;
    return shellPids[(uintptr_t)handle - 900];
}
static void ApplyModernWindowFrame(HWND window) { CHECK(insideChild && IsWindow(window)); }
static UINT_PTR SetTimer(HWND window, UINT_PTR id, unsigned interval, void *callback) {
    if (insideChild) {
        CHECK(IsWindow(window) && !callback);
        BOOL lighting = wcscmp(window->klass, L"TestsKeyboardLightingSuiteWnd") == 0;
        CHECK(lighting ? ((id == 1 && interval == 45) || (id == 2 && interval == 600) || (id == 3 && interval == 15))
                       : ((id == 1 && interval == 700) || (id == 3 && interval == 50)));
        childTimers++; return id;
    }
    CHECK(window == g_hwnd && id == 3 && interval == 100 && !callback);
    timerStarts++; return id;
}
static BOOL KillTimer(HWND window, UINT_PTR id) {
    CHECK(window == g_hwnd && id == 3); timerKills++; lastError = 6666; return TRUE;
}
static BOOL FileExistsW(const wchar_t *path) { CHECK(path && path[0]); return fileExists; }
static void SetFooterStatus(const wchar_t *text, int progress) {
    (void)progress; wcsncpy(footer, text, 255); footer[255] = L'\0';
}
static BOOL HubOpenApp(const wchar_t *imagePath, const wchar_t *args, const wchar_t *dir);
static void HubPollOpen(void);
static BOOL ShellExecuteExW(SHELLEXECUTEINFOW *launch) {
    int call = ++launchCalls;
    CHECK(call < 16);
    CHECK(launch->hwnd == g_hwnd && wcscmp(launch->lpVerb, L"open") == 0);
    CHECK(launch->fMask == (SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI));
    CHECK(launch->nShow == SW_SHOWNORMAL);
    wcscpy(launchedImage, launch->lpFile);
    wcscpy(launchedArgs, launch->lpParameters ? launch->lpParameters : L"");
    wcscpy(launchedDir, launch->lpDirectory ? launch->lpDirectory : L"");
    if (delayShell) { delayShell = FALSE; tick += 20000; HubPollOpen(); }
    if (reenterImage) {
        const wchar_t *newImage = reenterImage; reenterImage = NULL;
        CHECK(HubOpenApp(newImage, NULL, NULL));
    }
    launch->hProcess = returnProcess ? (HANDLE)(uintptr_t)(900 + call) : NULL;
    BOOL ok = shellOk && call != failLaunchCall;
    if (shellSetsError) lastError = ok ? ERROR_SUCCESS : shellError;
    return ok;
}

#include "../../shared/win32/window_reveal.h"
#include "../../bluetooth-guard/src/parts/window_activation.c"
#include "../../keyboard-lighting/src/parts/window_activation.c"
/* Exact WM_CREATE bodies extracted from the two production window procedures. */
#include "hub_open_create_production.h"
#include "../../project-hub/src/parts/app_activation.c"

static BOOL IsLighting(HWND window) {
    return window->klass && wcscmp(window->klass, L"TestsKeyboardLightingSuiteWnd") == 0;
}
static void EnableChildReveal(HWND window) {
    int before = childTimers;
    insideChild = TRUE;
    CHECK((IsLighting(window) ? LightingWindowCreate(window) : GuardWindowCreate(window)) == 0);
    insideChild = FALSE;
    CHECK(childTimers - before == (IsLighting(window) ? 3 : 2));
}
static BOOL HandleChildReveal(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    return IsLighting(window) ? HandleHubLightingReveal(window, msg, wp, lp)
                              : HandleHubPanelReveal(window, msg, wp, lp);
}

static void DeliverReveal(void) {
    CHECK(queuedWindow);
    HWND window = queuedWindow; queuedWindow = NULL;
    insideChild = TRUE;
    CHECK(HandleChildReveal(window, queuedMessage, queuedWp, queuedLp));
    insideChild = FALSE;
}

static const wchar_t *btg = L"C:\\Suite\\BluetoothGuard.exe";
static const wchar_t *notes = L"C:\\Suite\\AgentNotes.exe";
static const wchar_t *other = L"C:\\Apps\\Other.exe";
static HWND Window(int i, const wchar_t *klass, DWORD pid, BOOL visible, BOOL iconic) {
    memset(&windows[i], 0, sizeof(windows[i]));
    windows[i].exists = TRUE; windows[i].klass = klass; windows[i].pid = pid;
    windows[i].title = L"App"; windows[i].visible = visible; windows[i].iconic = iconic;
    windows[i].bounds = (RECT){ 100, 100, 900, 700 };
    return &windows[i];
}
static void Reset(const char *name) {
    scenario = name;
    memset(windows, 0, sizeof(windows)); memset(scans, 0, sizeof(scans));
    memset(&g_openRequest, 0, sizeof(g_openRequest));
    memset(shellPids, 0, sizeof(shellPids));
    launchCalls = showCalls = focusCalls = positionCalls = grantCalls = 0;
    processCloses = mutexCloses = timerStarts = timerKills = scanCalls = 0;
    fileExists = canFocus = applyShow = shellOk = returnProcess = TRUE;
    denyMutex = delayShell = FALSE; availableMutex = reenterImage = NULL;
    lastShow = 0; lastError = 0; shellError = ERROR_ACCESS_DENIED;
    errorDialogs = failLaunchCall = 0; failErrorFormat = longErrorFormat = FALSE;
    shellSetsError = TRUE;
    errorDetails[0] = L'\0';
    revealPosts = childShows = filterCalls = childTimers = 0;
    insideChild = g_quitting = FALSE; postOk = inputOk = TRUE;
    queuedWindow = NULL;
    lastFocused = lastPositioned = NULL; footer[0] = L'\0';
    shellPids[1] = 77; shellPids[2] = 88;
    inputTick = 10; tick = 1000; lastGranted = 0;
    recoveryMoves = 0; rectOk = monitorInfoOk = TRUE;
    monitors[0] = (struct FakeMonitor){ {0, 0, 1920, 1080}, {0, 0, 1920, 1040} };
    monitors[1] = (struct FakeMonitor){ {-1600, -200, 0, 900}, {-1600, -200, 0, 860} };
    recoveryMonitor = &monitors[0];
    foreground = Window(0, L"Hub", 1, TRUE, FALSE);
}
static void LaunchFailureScenarios(void) {
    const DWORD codes[] = { 2, 3, 5, 193, 225, 267, 740, 1157, 1223, 1260, 67890, 0 };
    const wchar_t *path = L"C:\\Apps\\Bluetooth Guard \x03B1\\BluetoothGuard.exe";
    const wchar_t *dir = L"C:\\Apps\\Bluetooth Guard \x03B1";
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        Reset("failed launch retains app, raw Windows error and both paths");
        shellOk = FALSE; shellError = codes[i]; failErrorFormat = shellError == 67890;
        CHECK(HubOpenApp(path, NULL, dir));
        CHECK(launchCalls == 1 && !g_openRequest.active && errorDialogs == 1);
        CHECK(wcsstr(footer, L"BluetoothGuard.exe") != NULL);
        wchar_t workingFolder[512]; swprintf(workingFolder, 512, L"Working folder:\n%ls", dir);
        CHECK(wcsstr(errorDetails, path) != NULL && wcsstr(errorDetails, workingFolder) != NULL);
        if (shellError) {
            wchar_t code[40]; swprintf(code, 40, L"Windows %lu", (unsigned long)shellError);
            CHECK(wcsstr(footer, code) != NULL);
            if (failErrorFormat) CHECK(wcsstr(errorDetails, L"description") != NULL);
            else CHECK(wcsstr(errorDetails, L"Windows reason: %1!s! and %n.") != NULL);
        } else {
            CHECK(wcsstr(footer, L"no Windows error code") != NULL);
            CHECK(wcsstr(errorDetails, L"no error code") != NULL);
        }
        CHECK(wcsstr(errorDetails, L"No automatic retry") != NULL);
        HubPollOpen(); CHECK(launchCalls == 1 && errorDialogs == 1);
    }

    Reset("a shell failure without a code cannot inherit an old mutex error");
    shellOk = shellSetsError = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(errorDialogs == 1 && wcsstr(footer, L"no Windows error code") != NULL);

    Reset("no working folder is an explicit Windows default, not a null string");
    shellOk = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(wcsstr(errorDetails, L"Windows default") != NULL);

    Reset("long catalogue filename cannot hide the raw code or overrun a dialog");
    wchar_t longPath[MAX_PATH];
    wmemset(longPath, L'x', MAX_PATH - 1); longPath[MAX_PATH - 1] = L'\0';
    shellOk = FALSE; longErrorFormat = TRUE;
    CHECK(HubOpenApp(longPath, NULL, longPath));
    CHECK(errorDialogs == 1 && wcsstr(errorDetails, longPath) != NULL);
    CHECK(wcsstr(footer, L"Windows 5") != NULL);
    wchar_t longFolder[512]; swprintf(longFolder, 512, L"Working folder:\n%ls", longPath);
    CHECK(wcsstr(errorDetails, longFolder) != NULL);

    Reset("a failed older shell cannot show an error for a newer successful Open");
    reenterImage = notes; failLaunchCall = 1;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(errorDialogs == 0 && launchCalls == 2 && g_openRequest.active);
    CHECK(g_openRequest.identity == HubIdentityForImage(notes));
    CHECK(wcsstr(footer, L"Opening app") != NULL);
    puts("Hub launch error detail scenarios passed");
}
int main(void) {
    /* Failed registration must never allow message zero (WM_NULL) through UIPI. */
    Reset("registered-message failure keeps the legacy path"); registerOk = FALSE;
    HWND guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableChildReveal(guard);
    CHECK(filterCalls == 0 && !HandleHubPanelReveal(guard, 0, 0, 0));
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(foreground == guard && revealPosts == 0 && launchCalls == 0);
    Reset("Lighting registered-message failure keeps the legacy path");
    HWND unregisteredLighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
    EnableChildReveal(unregisteredLighting);
    CHECK(filterCalls == 0 && !HandleHubLightingReveal(unregisteredLighting, 0, 0, 0));
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    CHECK(foreground == unregisteredLighting && revealPosts == 0 && launchCalls == 0);
    registerOk = TRUE;

    /* The actual Guard and Lighting receivers must pass the same safety,
       placement, modal, freshness and fallback cases. */
    for (int child = 0; child < 2; child++) {
        const wchar_t *testClass = child ? L"TestsKeyboardLightingSuiteWnd" : L"TestsBluetoothGuardModernWnd";
        const wchar_t *testImage = child ? L"KeyboardLighting.exe" : btg;
        Reset("cross-integrity reveal runs on the owning UI without a new launch");
        HWND guard = Window(1, testClass, 11, FALSE, TRUE);
        guard->maximized = TRUE; EnableChildReveal(guard);
        applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL));
        CHECK(revealPosts == 1 && childShows == 0 && !guard->visible && guard->iconic);
        CHECK(launchCalls == 0 && g_openRequest.active);
        DeliverReveal();
        CHECK(foreground == guard && guard->visible && !guard->iconic && guard->maximized);
        CHECK(childShows == 1 && lastShow == SW_RESTORE && filterCalls == 1);
        HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

        Reset("owning UI preserves a maximized window and focuses the active dialog");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        guard->maximized = TRUE; EnableChildReveal(guard);
        HWND guardPopup = Window(2, L"Dialog", 11, TRUE, FALSE);
        guard->popup = guardPopup; guardPopup->owner = guard;
        applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL)); DeliverReveal();
        CHECK(guard->maximized && lastShow == SW_SHOW && foreground == guardPopup);
        CHECK(lastPositioned == guardPopup);
        HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

        Reset("queued reveal loses permission after newer user input");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL));
        foreground = Window(2, L"Other", 99, TRUE, FALSE); inputTick++;
        DeliverReveal(); HubPollOpen();
        CHECK(childShows == 0 && !guard->visible && !g_openRequest.active && revealPosts == 1);

        Reset("queued reveal expires while the app UI is busy");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL)); tick += SUITE_WINDOW_REVEAL_MAX_AGE_MS + 1;
        DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
        HubPollOpen(); DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

        Reset("ordinary pointer activity can retry while the Hub still owns foreground");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL)); inputTick++;
        DeliverReveal(); CHECK(childShows == 0 && g_openRequest.active);
        HubPollOpen(); DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

        Reset("reveal freshness is DWORD-wrap-safe");
        tick = UINT32_MAX - 50ULL;
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL)); tick += 100;
        DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

        Reset("missing input snapshot and quitting never reveal");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL)); inputOk = FALSE;
        DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
        inputOk = TRUE; HubPollOpen(); g_quitting = TRUE;
        DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
        CHECK(!HandleChildReveal(guard, 0x111, 0, 0));

        Reset("missing send-time input snapshot uses legacy reveal without a private post");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); inputOk = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL));
        CHECK(revealPosts == 0 && foreground == guard && launchCalls == 0);

        Reset("blocked private post retains the normal legacy reveal");
        guard = Window(1, testClass, 11, FALSE, FALSE);
        EnableChildReveal(guard); postOk = FALSE;
        CHECK(HubOpenApp(testImage, NULL, NULL));
        CHECK(foreground == guard && revealPosts == 1 && childShows == 0 && !queuedWindow);

    }

    Reset("cross-integrity Lighting Open must not rely on foreign show or restart");
    HWND lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, TRUE);
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"C:\\Suite\\KeyboardLighting.exe", NULL, NULL));
    CHECK(revealPosts == 1 && launchCalls == 0 && !lighting->visible);
    DeliverReveal();
    CHECK(childShows == 1 && foreground == lighting && !lighting->iconic);
    HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

    /* WS_VISIBLE and foreground ownership do not mean a window is on a
       connected screen. Execute the real Hub/Lighting recovery together. */
    Reset("offscreen visible Lighting is not falsely accepted as successful Open");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, TRUE, FALSE);
    lighting->bounds = (RECT){ 5000, 200, 5800, 800 }; foreground = lighting;
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    CHECK(g_openRequest.active && revealPosts == 1 && launchCalls == 0);
    DeliverReveal();
    CHECK(recoveryMoves == 1 && lighting->bounds.left == 560 && lighting->bounds.top == 220);
    CHECK(lighting->bounds.right - lighting->bounds.left == 800 && lastShow == SW_SHOW);
    HubPollOpen(); CHECK(!g_openRequest.active && foreground == lighting);

    Reset("offscreen foreground modal is not hidden by its connected visible owner");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, TRUE, FALSE);
    HWND strandedPopup = Window(2, L"Dialog", 12, TRUE, FALSE);
    strandedPopup->owner = lighting; lighting->popup = strandedPopup;
    strandedPopup->bounds = (RECT){5000, 200, 5400, 500}; foreground = strandedPopup;
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    CHECK(g_openRequest.active && revealPosts == 1);
    DeliverReveal(); HubPollOpen(); CHECK(recoveryMoves == 1 && !g_openRequest.active);

    Reset("connected active modal is sufficient even when its owner is offscreen");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, TRUE, FALSE);
    lighting->bounds = (RECT){5000, 200, 5800, 800};
    HWND connectedPopup = Window(2, L"Dialog", 12, TRUE, FALSE);
    connectedPopup->owner = lighting; lighting->popup = connectedPopup; foreground = connectedPopup;
    EnableChildReveal(lighting);
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    CHECK(!g_openRequest.active && !recoveryMoves && !revealPosts && launchCalls == 0);

    Reset("hidden former popup must not prevent completion for a connected panel");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, TRUE, FALSE);
    HWND hiddenPopup = Window(2, L"Dialog", 12, FALSE, FALSE);
    hiddenPopup->owner = lighting; lighting->popup = hiddenPopup;
    hiddenPopup->bounds = (RECT){5000, 200, 5400, 500}; foreground = lighting;
    EnableChildReveal(lighting);
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    CHECK(!g_openRequest.active && !revealPosts && launchCalls == 0);

    Reset("minimized offscreen Lighting restores its existing window without a restart");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, TRUE);
    lighting->bounds = (RECT){ 5000, 200, 5800, 800 };
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
    CHECK(recoveryMoves == 1 && !lighting->iconic && lastShow == SW_RESTORE && launchCalls == 0);
    HubPollOpen(); CHECK(!g_openRequest.active);

    Reset("offscreen active modal recovers without moving or normalizing its maximized owner");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
    lighting->maximized = TRUE;
    HWND lightingPopup = Window(2, L"Dialog", 12, TRUE, FALSE);
    lightingPopup->owner = lighting; lighting->popup = lightingPopup;
    lightingPopup->bounds = (RECT){ 5000, 300, 5400, 600 };
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
    CHECK(recoveryMoves == 1 && foreground == lightingPopup && lightingPopup->bounds.left == 760);
    CHECK(lighting->maximized && lighting->bounds.left == 100 && lastShow == SW_SHOW);
    HubPollOpen(); CHECK(!g_openRequest.active);

    for (int placement = 0; placement < 2; placement++) {
        Reset("partly visible or negative-coordinate connected placement must not move");
        lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
        RECT original = placement ? (RECT){1850, 200, 2650, 800} : (RECT){-1500, -100, -700, 500};
        lighting->bounds = original;
        EnableChildReveal(lighting); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
        CHECK(!recoveryMoves && memcmp(&original, &lighting->bounds, sizeof(original)) == 0);
        CHECK(foreground == lighting && launchCalls == 0);
    }

    Reset("recovery uses the nearest monitor work area including negative coordinates");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
    lighting->bounds = (RECT){-5000, 200, -4200, 800}; recoveryMonitor = &monitors[1];
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
    CHECK(recoveryMoves == 1 && lighting->bounds.left == -1200 && lighting->bounds.top == 30);

    Reset("oversized offscreen panel retains its dimensions and gets an accessible origin");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
    lighting->bounds = (RECT){5000, 200, 7500, 1600};
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
    CHECK(recoveryMoves == 1 && lighting->bounds.left == 0 && lighting->bounds.top == 0);
    CHECK(lighting->bounds.right == 2500 && lighting->bounds.bottom == 1400);

    for (int failure = 0; failure < 4; failure++) {
        Reset("unknown geometry and maximized state never permit a speculative relocation");
        lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
        RECT original = {5000, 200, 5800, 800}; lighting->bounds = original;
        rectOk = failure != 0; monitorInfoOk = failure != 1;
        if (failure == 2) recoveryMonitor = NULL;
        if (failure == 3) lighting->maximized = TRUE;
        EnableChildReveal(lighting); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
        CHECK(!recoveryMoves && memcmp(&original, &lighting->bounds, sizeof(original)) == 0);
        HubPollOpen(); CHECK(g_openRequest.active && launchCalls == 0);
        tick += HUB_OPEN_TIMEOUT_MS; HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);
    }

    for (int invalid = 0; invalid < 2; invalid++) {
        Reset("invalid offscreen dimensions never cause a speculative move");
        lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
        lighting->bounds = invalid ? (RECT){5000, 200, 5800, 200} : (RECT){5000, 200, 5000, 800};
        EnableChildReveal(lighting); applyShow = canFocus = FALSE;
        CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL)); DeliverReveal();
        CHECK(!recoveryMoves && launchCalls == 0);
    }

    Reset("expired offscreen reveal cannot move or focus the panel after newer input");
    lighting = Window(1, L"TestsKeyboardLightingSuiteWnd", 12, FALSE, FALSE);
    lighting->bounds = (RECT){5000, 200, 5800, 800};
    EnableChildReveal(lighting); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(L"KeyboardLighting.exe", NULL, NULL));
    foreground = Window(2, L"Other", 99, TRUE, FALSE); inputTick++;
    DeliverReveal(); HubPollOpen();
    CHECK(!recoveryMoves && !childShows && !g_openRequest.active && foreground != lighting);

    /* Every native app uses the same real reveal route, including a stray
       running copy whose expected installed file has disappeared. */
    for (size_t i = 0; i < sizeof(g_openIdentities) / sizeof(g_openIdentities[0]); i++) {
        Reset("each native hidden window"); fileExists = FALSE;
        HWND app = Window(1, g_openIdentities[i].windowClass, 11, FALSE, FALSE);
        CHECK(HubOpenApp(g_openIdentities[i].exeName, NULL, NULL));
        CHECK(launchCalls == 0 && foreground == app && lastShow == SW_SHOW && lastGranted == 11);
        CHECK(revealPosts == ((i == 0 || i == 1) ? 1 : 0));
        HubPollOpen(); CHECK(!g_openRequest.active);
    }
    Reset("minimized window restores without normalizing its placement");
    HWND app = Window(1, L"TestsBluetoothGuardModernWnd", 11, TRUE, TRUE);
    app->maximized = TRUE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(!app->iconic && app->maximized && lastShow == SW_RESTORE && launchCalls == 0);

    Reset("maximized window stays maximized");
    app = Window(1, L"AgentNotesWnd", 11, TRUE, FALSE); app->maximized = TRUE;
    CHECK(HubOpenApp(notes, NULL, NULL));
    CHECK(app->maximized && lastShow == SW_SHOW && launchCalls == 0);

    Reset("active modal popup receives focus, not its parent");
    app = Window(1, L"AgentNotesWnd", 11, FALSE, FALSE);
    HWND popup = Window(2, L"Dialog", 11, TRUE, FALSE); popup->owner = app; app->popup = popup;
    CHECK(HubOpenApp(notes, NULL, NULL));
    CHECK(foreground == popup && lastFocused == popup && lastPositioned == popup);

    Reset("running pre-window mutex is not a new launch");
    availableMutex = L"Local\\Tests_AgentNotes_SingleInstance_v1";
    CHECK(HubOpenApp(notes, NULL, NULL));
    CHECK(launchCalls == 0 && mutexCloses == 1 && g_openRequest.active);
    CHECK(HubOpenApp(notes, NULL, NULL)); CHECK(launchCalls == 0);
    app = Window(1, L"AgentNotesWnd", 11, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app && launchCalls == 0);

    Reset("catalogue process lookup reveals hidden main window only");
    fileExists = FALSE; scans[0].path = other; scans[0].pid = 31;
    Window(1, L"Worker", 31, FALSE, FALSE)->title = L"";
    Window(2, L"Tool", 31, TRUE, FALSE)->tool = TRUE;
    Window(3, L"Unrelated", 32, TRUE, FALSE);
    app = Window(4, L"OtherMain", 31, FALSE, FALSE);
    CHECK(HubOpenApp(other, L"--custom", L"C:\\Apps"));
    CHECK(launchCalls == 0 && foreground == app);

    Reset("catalogue process is starting with no HWND yet");
    scans[0].path = other; scans[0].pid = 31;
    CHECK(HubOpenApp(other, NULL, NULL)); CHECK(launchCalls == 0 && g_openRequest.active);
    app = Window(1, L"OtherMain", 31, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app && launchCalls == 0);

    Reset("new launch is tracked and double clicks do not duplicate it");
    CHECK(HubOpenApp(other, L"--custom", L"C:\\Apps"));
    CHECK(launchCalls == 1 && processCloses == 1 && lastGranted == 77);
    CHECK(wcscmp(launchedImage, other) == 0 && wcscmp(launchedArgs, L"--custom") == 0);
    CHECK(wcscmp(launchedDir, L"C:\\Apps") == 0 && g_openRequest.active);
    CHECK(HubOpenApp(other, L"--custom", L"C:\\Apps")); CHECK(launchCalls == 1);
    app = Window(1, L"OtherMain", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app && launchCalls == 1);
    HubPollOpen(); CHECK(!g_openRequest.active);

    Reset("existing child consent wait does not consume the startup timeout");
    delayShell = TRUE;
    CHECK(HubOpenApp(notes, NULL, NULL));
    CHECK(g_openRequest.active && !g_openRequest.launching && launchCalls == 1);
    CHECK(g_openRequest.deadline == tick + HUB_OPEN_TIMEOUT_MS && lastGranted == 77);
    app = Window(1, L"AgentNotesWnd", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app);

    Reset("shell has no process handle; delayed PID discovery still works");
    returnProcess = FALSE;
    CHECK(HubOpenApp(other, NULL, NULL)); CHECK(launchCalls == 1 && processCloses == 0);
    scans[0].path = other; scans[0].pid = 77;
    app = Window(1, L"OtherMain", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app && launchCalls == 1);

    Reset("blocked async show and foreground retries never relaunch");
    app = Window(1, L"AgentNotesWnd", 11, FALSE, TRUE);
    applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(notes, NULL, NULL)); HubPollOpen();
    CHECK(launchCalls == 0 && g_openRequest.active && showCalls == 2);
    applyShow = canFocus = TRUE;
    HubPollOpen(); CHECK(foreground == app && !app->iconic);
    HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

    Reset("new user activity in another app cancels deferred focus");
    CHECK(HubOpenApp(notes, NULL, NULL));
    foreground = Window(2, L"Unrelated", 99, TRUE, FALSE); inputTick++;
    app = Window(1, L"AgentNotesWnd", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(!g_openRequest.active && focusCalls == 0 && launchCalls == 1);
    HubPollOpen(); CHECK(foreground != app);

    Reset("new input in the same foreign origin also cancels");
    foreground = Window(2, L"Unrelated", 99, TRUE, FALSE);
    CHECK(HubOpenApp(notes, NULL, NULL)); inputTick++;
    app = Window(1, L"AgentNotesWnd", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(!g_openRequest.active && focusCalls == 0);

    Reset("startup retry is bounded");
    CHECK(HubOpenApp(notes, NULL, NULL)); tick += HUB_OPEN_TIMEOUT_MS;
    HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 1 && focusCalls == 0);
    CHECK(wcsstr(footer, L"did not expose a window") != NULL);

    Reset("only newest Open may take focus");
    CHECK(HubOpenApp(btg, NULL, NULL)); CHECK(HubOpenApp(notes, NULL, NULL));
    Window(1, L"TestsBluetoothGuardModernWnd", 77, FALSE, FALSE);
    HubPollOpen(); CHECK(focusCalls == 0 && launchCalls == 2);
    app = Window(2, L"AgentNotesWnd", 88, FALSE, FALSE);
    HubPollOpen(); CHECK(foreground == app);

    Reset("shell message reentry cannot overwrite a newer request");
    reenterImage = notes;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(launchCalls == 2 && processCloses == 2);
    CHECK(g_openRequest.identity == HubIdentityForImage(notes));
    CHECK(g_openRequest.processes.count == 1 && g_openRequest.processes.pids[0] == 88);
    CHECK(lastGranted == 88);

    Reset("launch failure is handled, not mistaken for missing installation");
    shellOk = FALSE;
    CHECK(HubOpenApp(notes, NULL, NULL));
    CHECK(!g_openRequest.active && launchCalls == 1 && processCloses == 1);
    CHECK(wcsstr(footer, L"Could not open") != NULL);

    Reset("inaccessible mutex does not launch a second copy");
    denyMutex = TRUE;
    CHECK(HubOpenApp(notes, NULL, NULL)); CHECK(launchCalls == 0 && !g_openRequest.active);

    Reset("uncertain process inventory does not launch a second copy");
    scans[0].path = other; scans[0].uncertain = TRUE;
    CHECK(HubOpenApp(other, NULL, NULL)); CHECK(launchCalls == 0 && !g_openRequest.active);

    Reset("missing app can still be installed by caller");
    fileExists = FALSE;
    CHECK(!HubOpenApp(notes, NULL, NULL)); CHECK(launchCalls == 0 && !g_openRequest.active);
    LaunchFailureScenarios();
    puts("Hub Open activation scenarios passed");
    return 0;
}
