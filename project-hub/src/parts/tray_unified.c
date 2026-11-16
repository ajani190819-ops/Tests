/* ---------------- unified suite notification-area tray ----------------

   Project Hub is the opt-in tray host only while it is already running. It
   creates no Run key, task, service, or sign-in path. Current suite apps
   discover the host through HUB_TRAY_HOST_EVENT and remove their duplicate
   icons; if the host goes away, each app restores its own icon on its regular
   timer. The event is a capability marker, not a command channel. */

static void HubTrayAgentNotesCfg(wchar_t *out, size_t cap) {
    _snwprintf(out, cap - 1, L"%s\\AgentNotes\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static void HubTrayConfig(wchar_t *out, size_t cap) {
    BranchCfgPath(out, cap);
}

static void HubTrayWeatherCfg(wchar_t *out, size_t cap) {
    _snwprintf(out, cap - 1, L"%s\\WeatherRadar\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static int HubTrayAgentWaitSound(void) {
    wchar_t cfg[MAX_PATH];
    HubTrayAgentNotesCfg(cfg, MAX_PATH);
    return GetPrivateProfileIntW(L"AgentNotes", L"WaitSound", 0, cfg) ? 1 : 0;
}

static void HubTrayToggleAgentWaitSound(void) {
    wchar_t cfg[MAX_PATH];
    HubTrayAgentNotesCfg(cfg, MAX_PATH);
    int on = !HubTrayAgentWaitSound();
    WritePrivateProfileStringW(L"AgentNotes", L"WaitSound", on ? L"1" : L"0", cfg);
    SetFooterStatus(on
        ? L"Agent Notes waiting-alert sound is on. Windows controls its standard notification sound."
        : L"Agent Notes waiting-alert sound is off. Visual alerts remain on.", 0);
}

static void HubTrayShowAgentWaitingAlert(void) {
    if (!g_hubTrayVisible) return;
    g_hubNid.uFlags = NIF_INFO;
    g_hubNid.dwInfoFlags = NIIF_INFO | (HubTrayAgentWaitSound() ? 0 : NIIF_NOSOUND);
    wcsncpy(g_hubNid.szInfoTitle, L"Agent is waiting for input", 63);
    wcsncpy(g_hubNid.szInfo,
             L"A question needs your reply in Arena. Open the Arena chat when you are ready.",
             255);
    g_hubNid.szInfoTitle[63] = L'\0';
    g_hubNid.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);
    g_hubNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void HubTrayPollAgentWait(void) {
    if (!g_hubTrayVisible) return;
    wchar_t notesCfg[MAX_PATH], waitId[80];
    HubTrayAgentNotesCfg(notesCfg, MAX_PATH);
    GetPrivateProfileStringW(L"HubRelay", L"WaitId", L"", waitId, 79, notesCfg);
    waitId[79] = L'\0';
    if (!waitId[0] || wcscmp(waitId, g_hubSeenAgentWait) == 0) return;

    HubTrayShowAgentWaitingAlert();
    wcsncpy(g_hubSeenAgentWait, waitId, 79);
    g_hubSeenAgentWait[79] = L'\0';
    wchar_t hubCfg[MAX_PATH];
    HubTrayConfig(hubCfg, MAX_PATH);
    WritePrivateProfileStringW(L"Tray", L"SeenAgentWait", g_hubSeenAgentWait, hubCfg);
}

static void HubTrayPollWeatherAlert(void) {
    if (!g_hubTrayVisible) return;
    wchar_t weatherCfg[MAX_PATH], token[80], title[64], body[256];
    HubTrayWeatherCfg(weatherCfg, MAX_PATH);
    GetPrivateProfileStringW(L"HubRelay", L"RainToken", L"", token, 79, weatherCfg);
    token[79] = L'\0';
    if (!token[0] || wcscmp(token, g_hubSeenRainAlert) == 0) return;
    GetPrivateProfileStringW(L"HubRelay", L"RainTitle", L"Rain likely near your selected place",
                             title, 63, weatherCfg);
    GetPrivateProfileStringW(L"HubRelay", L"RainBody", L"Open Weather Radar for details.",
                             body, 255, weatherCfg);
    title[63] = body[255] = L'\0';
    g_hubNid.uFlags = NIF_INFO;
    g_hubNid.dwInfoFlags = NIIF_WARNING;
    wcsncpy(g_hubNid.szInfoTitle, title, 63);
    wcsncpy(g_hubNid.szInfo, body, 255);
    g_hubNid.szInfoTitle[63] = L'\0';
    g_hubNid.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);
    g_hubNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    wcsncpy(g_hubSeenRainAlert, token, 79);
    g_hubSeenRainAlert[79] = L'\0';
    wchar_t hubCfg[MAX_PATH];
    HubTrayConfig(hubCfg, MAX_PATH);
    WritePrivateProfileStringW(L"Tray", L"SeenWeatherAlert", g_hubSeenRainAlert, hubCfg);
}

static void HubTrayPublishHostMarker(void) {
    if (!g_trayHostEvent)
        g_trayHostEvent = CreateEventW(NULL, TRUE, TRUE, HUB_TRAY_HOST_EVENT);
}

static void HubTrayRestore(void) {
    if (!g_hubNid.cbSize) return;
    if (!Shell_NotifyIconW(NIM_ADD, &g_hubNid)) {
        /* Do not claim the shared tray while Windows has no Hub icon. The
           independent apps will re-add their owner-accessible fallbacks. */
        g_hubTrayVisible = FALSE;
        if (g_trayHostEvent) {
            CloseHandle(g_trayHostEvent);
            g_trayHostEvent = NULL;
        }
        return;
    }
    g_hubTrayVisible = TRUE;
    g_hubNid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_hubNid);
    /* Do this only after NIM_ADD succeeds. Apps must retain their individual
       fallback icons if Windows declines the Hub notification-area icon. */
    HubTrayPublishHostMarker();
}

/* NIM_ADD can lose a startup race with Explorer without producing a later
 * TaskbarCreated broadcast for this window.  Do not advertise ownership until
 * the icon exists, but retry a failed registration on the Hub's lightweight
 * timer so the suite does not remain in fallback mode for an entire session.
 * The same retry also recovers a rare CreateEvent failure after a successful
 * icon registration. */
static void HubTrayEnsureLive(void) {
    if (!g_hubNid.cbSize) return;
    if (!g_hubTrayVisible) {
        HubTrayRestore();
        return;
    }
    HubTrayPublishHostMarker();
}

static void HubTrayInit(void) {
    ZeroMemory(&g_hubNid, sizeof(g_hubNid));
    g_hubNid.cbSize = sizeof(g_hubNid);
    g_hubNid.hWnd = g_hwnd;
    g_hubNid.uID = 1;
    g_hubNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_hubNid.uCallbackMessage = WM_HUB_TRAYICON;
    g_hubNid.hIcon = g_appIcon;
    wcsncpy(g_hubNid.szTip, L"Project Hub - suite controls", 127);
    g_hubNid.szTip[127] = L'\0';
    HubTrayRestore();

    wchar_t hubCfg[MAX_PATH];
    HubTrayConfig(hubCfg, MAX_PATH);
    GetPrivateProfileStringW(L"Tray", L"SeenAgentWait", L"",
                             g_hubSeenAgentWait, 79, hubCfg);
    g_hubSeenAgentWait[79] = L'\0';
    GetPrivateProfileStringW(L"Tray", L"SeenWeatherAlert", L"",
                             g_hubSeenRainAlert, 79, hubCfg);
    g_hubSeenRainAlert[79] = L'\0';
}

static void HubTrayRemove(void) {
    /* Close the capability marker first: apps can return their own tray icons
       immediately if the Hub is explicitly exited or replaced by an update. */
    if (g_trayHostEvent) {
        CloseHandle(g_trayHostEvent);
        g_trayHostEvent = NULL;
    }
    if (g_hubTrayVisible) Shell_NotifyIconW(NIM_DELETE, &g_hubNid);
    g_hubTrayVisible = FALSE;
    ZeroMemory(&g_hubNid, sizeof(g_hubNid));
}

static HWND HubTrayAppWindow(const wchar_t *className) {
    return className ? FindWindowW(className, NULL) : NULL;
}

static int HubTrayShowApp(const wchar_t *className) {
    HWND app = HubTrayAppWindow(className);
    if (!app) return 0;
    if (IsIconic(app)) ShowWindow(app, SW_RESTORE);
    else ShowWindow(app, SW_SHOW);
    SetWindowPos(app, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(app);
    return 1;
}

static void HubTrayHideApp(const wchar_t *className) {
    HWND app = HubTrayAppWindow(className);
    if (app) ShowWindow(app, SW_HIDE);
}

static void HubTrayStopExe(const wchar_t *dir, const wchar_t *exe, const wchar_t *name) {
    wchar_t path[MAX_PATH];
    if (!dir || !exe) return;
    _snwprintf(path, MAX_PATH - 1, L"%s\\%s", dir, exe);
    path[MAX_PATH - 1] = L'\0';
    if (!FileExistsW(path)) return;
    HINSTANCE result = ShellExecuteW(g_hwnd, L"open", path, L"--stop", dir, SW_HIDE);
    if ((INT_PTR)result <= 32) {
        wchar_t msg[192];
        _snwprintf(msg, 191, L"Could not ask %s to quit. Open it to check its status.", name);
        msg[191] = L'\0';
        SetFooterStatus(msg, 0);
    } else {
        wchar_t msg[192];
        _snwprintf(msg, 191, L"Asked %s to quit.", name);
        msg[191] = L'\0';
        SetFooterStatus(msg, 0);
    }
}

static int HubTrayFindCardByExe(const wchar_t *exeName) {
    for (int i = 7; i < g_cardCount; i++)
        if (g_cards[i].exeName && _wcsicmp(g_cards[i].exeName, exeName) == 0)
            return i;
    return -1;
}

static void HubTrayOpenWeather(void) {
    if (HubTrayShowApp(L"WeatherRadarMainV1")) return;
    int idx = HubTrayFindCardByExe(L"WeatherRadar.exe");
    if (idx >= 0) {
        HandleVerb(idx, L"open");
        return;
    }
    wchar_t weatherDir[MAX_PATH], weatherExe[MAX_PATH];
    SuiteAppPath(weatherDir, MAX_PATH, L"WeatherRadar");
    _snwprintf(weatherExe, MAX_PATH - 1, L"%s\\WeatherRadar.exe", weatherDir);
    weatherExe[MAX_PATH - 1] = L'\0';
    if (FileExistsW(weatherExe))
        ShellExecuteW(g_hwnd, L"open", weatherExe, NULL, weatherDir, SW_SHOWNORMAL);
    else
        SetFooterStatus(L"Weather Radar will appear after the next Hub refresh confirms its install.", 0);
}

static void HubTrayOpenApp(int id) {
    switch (id) {
    case ID_TRAY_NOTES_OPEN:
        if (!HubTrayShowApp(L"AgentNotesWnd")) HandleActionCommand(g_hwnd, ID_NOTES_OPEN);
        break;
    case ID_TRAY_BTG_OPEN:
        if (!HubTrayShowApp(L"TestsBluetoothGuardModernWnd")) HandleActionCommand(g_hwnd, ID_BTG_OPEN);
        break;
    case ID_TRAY_KBL_OPEN:
        if (!HubTrayShowApp(L"TestsKeyboardLightingSuiteWnd")) HandleActionCommand(g_hwnd, ID_KBL_OPEN);
        break;
    case ID_TRAY_DNS_OPEN:
        if (!HubTrayShowApp(L"KBLSuiteDnsPilotWindow")) HandleActionCommand(g_hwnd, ID_DNS_OPEN);
        break;
    case ID_TRAY_AUDIO_OPEN:
        if (!HubTrayShowApp(L"TestsAudioSwitcherWnd")) HandleActionCommand(g_hwnd, ID_AUDIOSWITCHER_OPEN);
        break;
    case ID_TRAY_WEATHER_OPEN:
        HubTrayOpenWeather();
        break;
    }
}

static void HubTrayRunControl(const wchar_t *dir, const wchar_t *exe, const wchar_t *args,
                              const wchar_t *what) {
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH - 1, L"%s\\%s", dir, exe);
    path[MAX_PATH - 1] = L'\0';
    if (!FileExistsW(path)) return;
    HINSTANCE result = ShellExecuteW(g_hwnd, L"open", path, args, dir, SW_HIDE);
    if ((INT_PTR)result <= 32) {
        wchar_t msg[192];
        _snwprintf(msg, 191, L"Could not request %s from the Hub tray.", what);
        msg[191] = L'\0';
        SetFooterStatus(msg, 0);
    }
}

static void HubTrayToggleBluetooth(void) {
    /* The app may be elevated. Its short-lived companion process writes a
       one-shot for the live instance instead of relying on cross-integrity
       window-command delivery. */
    HubTrayRunControl(g_btgDir, L"BluetoothGuard.exe", L"--hub-toggle-guard",
                       L"Bluetooth Guard");
}

static void HubTrayAppendApp(HMENU root, const wchar_t *title, const wchar_t *className,
                             UINT openId, UINT hideId, UINT quitId, UINT toggleId,
                             const wchar_t *toggleText) {
    HWND app = HubTrayAppWindow(className);
    wchar_t label[128];
    _snwprintf(label, 127, L"%s%s", title, app ? L"  -  active" : L"  -  not running");
    label[127] = L'\0';
    HMENU sub = CreatePopupMenu();
    if (!sub) return;
    AppendMenuW(sub, MF_STRING, openId, app ? L"Show window" : L"Open");
    if (app) {
        AppendMenuW(sub, MF_STRING, hideId, L"Hide window");
        if (toggleId && toggleText) AppendMenuW(sub, MF_STRING, toggleId, toggleText);
        AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
        AppendMenuW(sub, MF_STRING, quitId, L"Quit");
    }
    AppendMenuW(root, MF_POPUP, (UINT_PTR)sub, label);
}

static void HubTrayShowMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_HUB_OPEN, L"Open Project Hub");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Suite apps");
    HubTrayAppendApp(menu, L"Agent Notes", L"AgentNotesWnd",
                      ID_TRAY_NOTES_OPEN, ID_TRAY_NOTES_HIDE, ID_TRAY_NOTES_QUIT, 0, NULL);
    HubTrayAppendApp(menu, L"Bluetooth Guard", L"TestsBluetoothGuardModernWnd",
                      ID_TRAY_BTG_OPEN, ID_TRAY_BTG_HIDE, ID_TRAY_BTG_QUIT,
                      ID_TRAY_BTG_TOGGLE, L"Toggle Guard Enabled");
    HubTrayAppendApp(menu, L"Keyboard Lighting", L"TestsKeyboardLightingSuiteWnd",
                      ID_TRAY_KBL_OPEN, ID_TRAY_KBL_HIDE, ID_TRAY_KBL_QUIT,
                      ID_TRAY_KBL_TOGGLE, L"Toggle Lighting On or Off");
    HubTrayAppendApp(menu, L"DNS Pilot", L"KBLSuiteDnsPilotWindow",
                      ID_TRAY_DNS_OPEN, ID_TRAY_DNS_HIDE, ID_TRAY_DNS_QUIT,
                      ID_TRAY_DNS_TOGGLE, L"Toggle DNS Auto-Pilot");
    HubTrayAppendApp(menu, L"Audio Switcher", L"TestsAudioSwitcherWnd",
                      ID_TRAY_AUDIO_OPEN, ID_TRAY_AUDIO_HIDE, ID_TRAY_AUDIO_QUIT, 0, NULL);
    HubTrayAppendApp(menu, L"Weather Radar", L"WeatherRadarMainV1",
                      ID_TRAY_WEATHER_OPEN, ID_TRAY_WEATHER_HIDE, ID_TRAY_WEATHER_QUIT, 0, NULL);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (HubTrayAgentWaitSound() ? MF_CHECKED : MF_UNCHECKED),
                ID_TRAY_NOTES_SOUND, L"Play a sound for Agent Notes waiting alerts");
    AppendMenuW(menu, MF_STRING, ID_TRAY_HUB_FOLDER, L"Open Managed Suite Folder");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_HUB_EXIT, L"Exit Project Hub (restore app tray icons)");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

static int HubTrayHandleCommand(HWND hwnd, int id) {
    switch (id) {
    case ID_TRAY_HUB_OPEN:
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        return 1;
    case ID_TRAY_HUB_FOLDER:
        EnsureDirTree(g_suiteDir);
        ShellExecuteW(hwnd, L"open", g_suiteDir, NULL, NULL, SW_SHOWNORMAL);
        return 1;
    case ID_TRAY_HUB_EXIT:
        g_hubQuitting = TRUE;
        DestroyWindow(hwnd);
        return 1;
    case ID_TRAY_NOTES_SOUND:
        HubTrayToggleAgentWaitSound();
        return 1;

    case ID_TRAY_NOTES_OPEN: case ID_TRAY_BTG_OPEN: case ID_TRAY_KBL_OPEN:
    case ID_TRAY_DNS_OPEN: case ID_TRAY_AUDIO_OPEN: case ID_TRAY_WEATHER_OPEN:
        HubTrayOpenApp(id);
        return 1;

    case ID_TRAY_NOTES_HIDE: HubTrayHideApp(L"AgentNotesWnd"); return 1;
    case ID_TRAY_BTG_HIDE: HubTrayHideApp(L"TestsBluetoothGuardModernWnd"); return 1;
    case ID_TRAY_KBL_HIDE: HubTrayHideApp(L"TestsKeyboardLightingSuiteWnd"); return 1;
    case ID_TRAY_DNS_HIDE: HubTrayHideApp(L"KBLSuiteDnsPilotWindow"); return 1;
    case ID_TRAY_AUDIO_HIDE: HubTrayHideApp(L"TestsAudioSwitcherWnd"); return 1;
    case ID_TRAY_WEATHER_HIDE: HubTrayHideApp(L"WeatherRadarMainV1"); return 1;

    case ID_TRAY_NOTES_QUIT: HubTrayStopExe(g_notesDir, L"AgentNotes.exe", L"Agent Notes"); return 1;
    case ID_TRAY_BTG_QUIT:
        HubTrayRunControl(g_btgDir, L"BluetoothGuard.exe", L"--hub-exit", L"Bluetooth Guard");
        return 1;
    case ID_TRAY_KBL_QUIT:
        HubTrayRunControl(g_kblDir, L"KeyboardLighting.exe", L"--hub-exit", L"Keyboard Lighting");
        return 1;
    case ID_TRAY_DNS_QUIT:
        HubTrayRunControl(g_dnsDir, L"DnsPilot.exe", L"--hub-exit", L"DNS Pilot");
        return 1;
    case ID_TRAY_AUDIO_QUIT: HubTrayStopExe(g_audioSwitcherDir, L"AudioSwitcher.exe", L"Audio Switcher"); return 1;
    case ID_TRAY_WEATHER_QUIT: {
        int idx = HubTrayFindCardByExe(L"WeatherRadar.exe");
        if (idx >= 0) HubTrayStopExe(g_cards[idx].appDir, L"WeatherRadar.exe", L"Weather Radar");
        else {
            wchar_t dir[MAX_PATH];
            SuiteAppPath(dir, MAX_PATH, L"WeatherRadar");
            HubTrayStopExe(dir, L"WeatherRadar.exe", L"Weather Radar");
        }
        return 1;
    }

    case ID_TRAY_BTG_TOGGLE: HubTrayToggleBluetooth(); return 1;
    case ID_TRAY_KBL_TOGGLE:
        HubTrayRunControl(g_kblDir, L"KeyboardLighting.exe", L"--hub-toggle", L"Keyboard Lighting");
        return 1;
    case ID_TRAY_DNS_TOGGLE:
        HubTrayRunControl(g_dnsDir, L"DnsPilot.exe", L"--hub-toggle-auto", L"DNS Auto-Pilot");
        return 1;
    }
    return 0;
}
