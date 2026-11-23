/* Open only reveals the existing panel on its own UI thread. It must not
   restart the engine, normalize a maximized window or hide an active dialog. */
static void RevealLightingPanel(HWND hwnd) {
    if (g_quitting || !IsWindow(hwnd)) return;
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    HWND target = GetLastActivePopup(hwnd);
    if (!target || !IsWindowVisible(target)) target = hwnd;
    SetWindowPos(target, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(target);
}

static void EnableHubLightingReveal(HWND hwnd) {
    UINT message = SuiteWindowRevealMessage();
    /* One per-window, show-only exception; no commands, pointers, engine
       actions or process-wide message-filter changes cross UIPI. */
    if (message) ChangeWindowMessageFilterEx(hwnd, message, MSGFLT_ALLOW, NULL);
}

static BOOL HandleHubLightingReveal(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UINT message = SuiteWindowRevealMessage();
    if (!message || msg != message) return FALSE;
    if (SuiteWindowRevealIsCurrent(wp, lp)) RevealLightingPanel(hwnd);
    return TRUE;
}
