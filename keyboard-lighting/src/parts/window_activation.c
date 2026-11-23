/* A visible window can be wholly outside the current display topology. Only
   recover a normal offscreen window; leave connected/partial and maximized
   placement alone. This runs on the owning UI, never through a foreign move. */
static void RecoverLightingWindowPosition(HWND hwnd) {
    RECT rect;
    if (IsIconic(hwnd) || IsZoomed(hwnd) || !GetWindowRect(hwnd, &rect) ||
        rect.right <= rect.left || rect.bottom <= rect.top ||
        MonitorFromRect(&rect, MONITOR_DEFAULTTONULL)) return;
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return;
    LONG width = rect.right - rect.left, height = rect.bottom - rect.top;
    LONG spareX = info.rcWork.right - info.rcWork.left - width;
    LONG spareY = info.rcWork.bottom - info.rcWork.top - height;
    LONG x = info.rcWork.left + (spareX > 0 ? spareX / 2 : 0);
    LONG y = info.rcWork.top + (spareY > 0 ? spareY / 2 : 0);
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Open only reveals the existing panel on its own UI thread. It must not
   restart the engine, normalize a maximized window or hide an active dialog. */
static void RevealLightingPanel(HWND hwnd) {
    if (g_quitting || !IsWindow(hwnd)) return;
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    HWND target = GetLastActivePopup(hwnd);
    if (!target || !IsWindowVisible(target)) target = hwnd;
    RecoverLightingWindowPosition(target);
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
