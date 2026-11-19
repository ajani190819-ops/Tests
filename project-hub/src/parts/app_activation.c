/* User-requested Open belongs to the Hub UI thread. Do not launch a companion
 * just to reveal a tray app: its foreground permission does not automatically
 * transfer to the older instance. Cards and the unified tray share this route.
 * All waits are bounded timer retries, never sleeps or input-queue attachment.
 */
#define HUB_OPEN_TIMER 3
#define HUB_OPEN_TIMEOUT_MS 10000ULL

typedef struct {
    const wchar_t *exeName;
    const wchar_t *windowClass;
    const wchar_t *mutexName;
} HubAppIdentity;

static const HubAppIdentity g_openIdentities[] = {
    { L"BluetoothGuard.exe", L"TestsBluetoothGuardModernWnd", L"Local\\Tests_BluetoothGuard_SingleInstance" },
    { L"KeyboardLighting.exe", L"TestsKeyboardLightingSuiteWnd", L"Local\\Tests_KeyboardLighting_SingleInstance_v2" },
    { L"DnsPilot.exe", L"KBLSuiteDnsPilotWindow", L"Local\\Tests_DnsPilot_SingleInstance" },
    { L"AgentNotes.exe", L"AgentNotesWnd", L"Local\\Tests_AgentNotes_SingleInstance_v1" },
    { L"AudioSwitcher.exe", L"TestsAudioSwitcherWnd", L"Local\\Tests_AudioSwitcher_SingleInstance_v1" },
    { L"WeatherRadar.exe", L"WeatherRadarMainV1", L"Local\\Tests_WeatherRadar_SingleInstance_v1" }
};

typedef struct {
    BOOL active;
    BOOL launching;
    ULONG serial;
    wchar_t imagePath[MAX_PATH];
    const HubAppIdentity *identity;
    HWND window;
    HWND origin;
    DWORD inputTick;
    BOOL haveInputTick;
    ULONGLONG deadline;
    UpdateImageProcesses processes;
} HubOpenRequest;

static HubOpenRequest g_openRequest;
static ULONG g_openSerial;

static const HubAppIdentity *HubIdentityForImage(const wchar_t *imagePath) {
    const wchar_t *name = ImageFileNameW(imagePath);
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(g_openIdentities) / sizeof(g_openIdentities[0]); i++)
        if (_wcsicmp(name, g_openIdentities[i].exeName) == 0) return &g_openIdentities[i];
    return NULL;
}

static BOOL HubOpenContainsPid(DWORD pid) {
    if (!pid) return FALSE;
    for (int i = 0; i < g_openRequest.processes.count; i++)
        if (g_openRequest.processes.pids[i] == pid) return TRUE;
    return FALSE;
}

static BOOL CALLBACK HubFindOpenWindow(HWND hwnd, LPARAM lp) {
    HWND *result = (HWND *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!HubOpenContainsPid(pid) || GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;
    /* A catalogue app may not have a suite-specific class. Ignore its tool/
       worker windows, but do not filter out a hidden main window. */
    if ((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0 ||
        GetWindowTextLengthW(hwnd) == 0) return TRUE;
    *result = hwnd;
    return FALSE;
}

static void HubCancelOpen(void) {
    g_openRequest.active = FALSE;
    KillTimer(g_hwnd, HUB_OPEN_TIMER);
}

static void HubBeginOpen(const wchar_t *imagePath, const HubAppIdentity *identity,
                          HWND window, const UpdateImageProcesses *processes) {
    ZeroMemory(&g_openRequest, sizeof(g_openRequest));
    g_openRequest.active = TRUE;
    g_openRequest.serial = ++g_openSerial;
    wcsncpy(g_openRequest.imagePath, imagePath, MAX_PATH - 1);
    g_openRequest.identity = identity;
    g_openRequest.window = window;
    g_openRequest.origin = GetForegroundWindow();
    g_openRequest.deadline = GetTickCount64() + HUB_OPEN_TIMEOUT_MS;
    LASTINPUTINFO input = { sizeof(input), 0 };
    g_openRequest.haveInputTick = GetLastInputInfo(&input);
    g_openRequest.inputTick = input.dwTime;
    if (processes) g_openRequest.processes = *processes;
    SetTimer(g_hwnd, HUB_OPEN_TIMER, 100, NULL);
}

/* Use async show/position calls so a hung foreign UI cannot hang the Hub.
 * SW_SHOW preserves a maximized window; SW_RESTORE is only for minimized ones.
 * Never use HWND_TOPMOST, synthetic input, or a global foreground-lock change.
 */
static void HubRevealWindow(HWND app) {
    DWORD pid = 0;
    GetWindowThreadProcessId(app, &pid);
    if (pid) AllowSetForegroundWindow(pid);
    ShowWindowAsync(app, IsIconic(app) ? SW_RESTORE : SW_SHOW);
    HWND target = GetLastActivePopup(app);
    if (!target || !IsWindowVisible(target)) target = app;
    SetWindowPos(target, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);
    SetForegroundWindow(target);
}

static BOOL HubOpenMayTakeForeground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD pid = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &pid);
    if (pid == GetCurrentProcessId() || HubOpenContainsPid(pid)) return TRUE;
    if (foreground != g_openRequest.origin) return FALSE;
    LASTINPUTINFO input = { sizeof(input), 0 };
    return !g_openRequest.haveInputTick || !GetLastInputInfo(&input) ||
           input.dwTime == g_openRequest.inputTick;
}

static void HubPollOpen(void) {
    if (!g_openRequest.active || g_openRequest.launching) return;
    if (GetTickCount64() >= g_openRequest.deadline) {
        HubCancelOpen();
        SetFooterStatus(L"The app did not expose a window in time. No second instance was started. Retry Open when it is ready.", 0);
        return;
    }
    /* A slow startup must not pull the owner away from a newer app/window.
       Only the latest explicit Open request is ever allowed to take focus. */
    if (!HubOpenMayTakeForeground()) { HubCancelOpen(); return; }

    HWND app = g_openRequest.window;
    if (!app || !IsWindow(app)) {
        app = g_openRequest.identity
            ? FindWindowW(g_openRequest.identity->windowClass, NULL) : NULL;
        if (!app) {
            if (!g_openRequest.processes.count)
                FindProcessesUsingImage(g_openRequest.imagePath, &g_openRequest.processes);
            EnumWindows(HubFindOpenWindow, (LPARAM)&app);
        }
        g_openRequest.window = app;
    }
    if (!app) return;
    DWORD pid = 0, foregroundPid = 0;
    GetWindowThreadProcessId(app, &pid);
    if (pid && !HubOpenContainsPid(pid) && g_openRequest.processes.count < UPDATE_SCAN_MAX_PIDS)
        g_openRequest.processes.pids[g_openRequest.processes.count++] = pid;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    if (pid && foregroundPid == pid && IsWindowVisible(app) && !IsIconic(app)) {
        HubCancelOpen();
        return;
    }
    HubRevealWindow(app);
}

/* TRUE means the click was handled, including a launch error or a running app
 * still starting. FALSE means absent: only then may the caller offer install.
 * A mutex/process with no HWND yet is NOT permission to launch another copy.
 */
static BOOL HubOpenApp(const wchar_t *imagePath, const wchar_t *args, const wchar_t *dir) {
    if (!imagePath || !imagePath[0] || wcslen(imagePath) >= MAX_PATH) {
        HubCancelOpen();
        SetFooterStatus(L"The app has no valid executable path. No launch was attempted.", 0);
        return TRUE;
    }
    const HubAppIdentity *identity = HubIdentityForImage(imagePath);
    if (g_openRequest.active &&
        (_wcsicmp(g_openRequest.imagePath, imagePath) == 0 ||
         (identity && identity == g_openRequest.identity))) {
        /* Repeated clicks refresh user intent, not the process count. */
        HubOpenRequest prior = g_openRequest;
        HubBeginOpen(imagePath, identity, prior.window, &prior.processes);
        HubPollOpen();
        return TRUE;
    }
    HubCancelOpen();
    HWND app = identity ? FindWindowW(identity->windowClass, NULL) : NULL;
    if (app) {
        HubBeginOpen(imagePath, identity, app, NULL);
        HubPollOpen();
        return TRUE;
    }
    if (identity) {
        HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, identity->mutexName);
        if (mutex) {
            CloseHandle(mutex);
            HubBeginOpen(imagePath, identity, NULL, NULL);
            HubPollOpen();
            return TRUE;
        }
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            SetFooterStatus(L"The running app could not be accessed. No second instance was started.", 0);
            return TRUE;
        }
    }
    UpdateImageProcesses processes;
    int count = FindProcessesUsingImage(imagePath, &processes);
    if (processes.count > 0) {
        HubBeginOpen(imagePath, identity, NULL, &processes);
        HubPollOpen();
        return TRUE;
    }
    if (count < 0) {
        SetFooterStatus(L"Could not verify whether the app is already running. No second instance was started.", 0);
        return TRUE;
    }
    if (!FileExistsW(imagePath)) return FALSE;

    HubBeginOpen(imagePath, identity, NULL, NULL);
    ULONG serial = g_openRequest.serial;
    SHELLEXECUTEINFOW launch;
    ZeroMemory(&launch, sizeof(launch));
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    launch.hwnd = g_hwnd;
    launch.lpVerb = L"open";
    launch.lpFile = imagePath;
    launch.lpParameters = args;
    launch.lpDirectory = dir;
    launch.nShow = SW_SHOWNORMAL;
    g_openRequest.launching = TRUE;
    BOOL started = ShellExecuteExW(&launch);
    DWORD pid = launch.hProcess ? GetProcessId(launch.hProcess) : 0;
    if (launch.hProcess) CloseHandle(launch.hProcess);
    /* ShellExecuteEx can pump messages (e.g. a child's existing UAC contract).
       A newer Open may have replaced this request while the shell was busy. */
    if (g_openRequest.active && g_openRequest.serial == serial) {
        g_openRequest.launching = FALSE;
        /* A child's existing consent prompt is not a startup timeout. */
        g_openRequest.deadline = GetTickCount64() + HUB_OPEN_TIMEOUT_MS;
        if (!started) {
            HubCancelOpen();
            SetFooterStatus(L"Could not open the app. No update or elevation change was attempted.", 0);
        } else {
            if (pid) {
                g_openRequest.processes.pids[0] = pid;
                g_openRequest.processes.count = 1;
                AllowSetForegroundWindow(pid);
            }
            SetFooterStatus(L"Opening app...", 0);
            HubPollOpen();
        }
    }
    return TRUE;
}
