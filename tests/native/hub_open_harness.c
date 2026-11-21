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
#define SEE_MASK_NOCLOSEPROCESS 0x40
#define SEE_MASK_FLAG_NO_UI 0x400
#define ZeroMemory(p, n) memset(p, 0, n)

typedef struct FakeWindow *HWND;
#define HWND_TOP ((HWND)0)
struct FakeWindow {
    BOOL exists, visible, iconic, maximized, tool, revealEnabled;
    DWORD pid;
    const wchar_t *klass, *title;
    HWND owner, popup;
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
static int lastShow, revealPosts, childShows, filterCalls;
static BOOL insideChild, g_quitting, postOk, registerOk = TRUE, inputOk;
static HWND queuedWindow;
static UINT queuedMessage;
static WPARAM queuedWp;
static LPARAM queuedLp;
static ULONGLONG tick;
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
    CHECK(insertAfter == HWND_TOP && !x && !y && !w && !h);
    CHECK(flags == (DWORD)(SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | (insideChild ? 0 : SWP_ASYNCWINDOWPOS)));
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
static UINT_PTR SetTimer(HWND window, UINT_PTR id, unsigned interval, void *callback) {
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
#include "../../project-hub/src/parts/app_activation.c"

static void DeliverReveal(void) {
    CHECK(queuedWindow);
    HWND window = queuedWindow; queuedWindow = NULL;
    insideChild = TRUE;
    CHECK(HandleHubPanelReveal(window, queuedMessage, queuedWp, queuedLp));
    insideChild = FALSE;
}

static const wchar_t *btg = L"C:\\Suite\\BluetoothGuard.exe";
static const wchar_t *notes = L"C:\\Suite\\AgentNotes.exe";
static const wchar_t *other = L"C:\\Apps\\Other.exe";
static HWND Window(int i, const wchar_t *klass, DWORD pid, BOOL visible, BOOL iconic) {
    memset(&windows[i], 0, sizeof(windows[i]));
    windows[i].exists = TRUE; windows[i].klass = klass; windows[i].pid = pid;
    windows[i].title = L"App"; windows[i].visible = visible; windows[i].iconic = iconic;
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
    revealPosts = childShows = filterCalls = 0;
    insideChild = g_quitting = FALSE; postOk = inputOk = TRUE;
    queuedWindow = NULL;
    lastFocused = lastPositioned = NULL; footer[0] = L'\0';
    shellPids[1] = 77; shellPids[2] = 88;
    inputTick = 10; tick = 1000; lastGranted = 0;
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
    EnableHubPanelReveal(guard);
    CHECK(filterCalls == 0 && !HandleHubPanelReveal(guard, 0, 0, 0));
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(foreground == guard && revealPosts == 0 && launchCalls == 0);
    registerOk = TRUE;

    Reset("cross-integrity reveal runs on the owning UI without a new launch");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, TRUE);
    guard->maximized = TRUE; EnableHubPanelReveal(guard);
    applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(revealPosts == 1 && childShows == 0 && !guard->visible && guard->iconic);
    CHECK(launchCalls == 0 && g_openRequest.active);
    DeliverReveal();
    CHECK(foreground == guard && guard->visible && !guard->iconic && guard->maximized);
    CHECK(childShows == 1 && lastShow == SW_RESTORE && filterCalls == 1);
    HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

    Reset("owning UI preserves a maximized window and focuses the active dialog");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    guard->maximized = TRUE; EnableHubPanelReveal(guard);
    HWND guardPopup = Window(2, L"Dialog", 11, TRUE, FALSE);
    guard->popup = guardPopup; guardPopup->owner = guard;
    applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL)); DeliverReveal();
    CHECK(guard->maximized && lastShow == SW_SHOW && foreground == guardPopup);
    CHECK(lastPositioned == guardPopup);
    HubPollOpen(); CHECK(!g_openRequest.active && launchCalls == 0);

    Reset("queued reveal loses permission after newer user input");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    foreground = Window(2, L"Other", 99, TRUE, FALSE); inputTick++;
    DeliverReveal(); HubPollOpen();
    CHECK(childShows == 0 && !guard->visible && !g_openRequest.active && revealPosts == 1);

    Reset("queued reveal expires while the app UI is busy");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL)); tick += SUITE_WINDOW_REVEAL_MAX_AGE_MS + 1;
    DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
    HubPollOpen(); DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

    Reset("ordinary pointer activity can retry while the Hub still owns foreground");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL)); inputTick++;
    DeliverReveal(); CHECK(childShows == 0 && g_openRequest.active);
    HubPollOpen(); DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

    Reset("reveal freshness is DWORD-wrap-safe");
    tick = UINT32_MAX - 50ULL;
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL)); tick += 100;
    DeliverReveal(); CHECK(childShows == 1 && foreground == guard);

    Reset("missing input snapshot and quitting never reveal");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); applyShow = canFocus = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL)); inputOk = FALSE;
    DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
    inputOk = TRUE; HubPollOpen(); g_quitting = TRUE;
    DeliverReveal(); CHECK(childShows == 0 && !guard->visible);
    CHECK(!HandleHubPanelReveal(guard, 0x111, 0, 0));

    Reset("missing send-time input snapshot uses legacy reveal without a private post");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); inputOk = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(revealPosts == 0 && foreground == guard && launchCalls == 0);

    Reset("blocked private post retains the normal legacy reveal");
    guard = Window(1, L"TestsBluetoothGuardModernWnd", 11, FALSE, FALSE);
    EnableHubPanelReveal(guard); postOk = FALSE;
    CHECK(HubOpenApp(btg, NULL, NULL));
    CHECK(foreground == guard && revealPosts == 1 && childShows == 0 && !queuedWindow);

    /* Every native app uses the same real reveal route, including a stray
       running copy whose expected installed file has disappeared. */
    for (size_t i = 0; i < sizeof(g_openIdentities) / sizeof(g_openIdentities[0]); i++) {
        Reset("each native hidden window"); fileExists = FALSE;
        HWND app = Window(1, g_openIdentities[i].windowClass, 11, FALSE, FALSE);
        CHECK(HubOpenApp(g_openIdentities[i].exeName, NULL, NULL));
        CHECK(launchCalls == 0 && foreground == app && lastShow == SW_SHOW && lastGranted == 11);
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
