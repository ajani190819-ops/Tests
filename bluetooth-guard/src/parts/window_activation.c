/* Run every reveal on the window's own UI thread, including Hub requests.
   Keep maximized placement and the active modal popup; never toggle topmost. */
static void RevealPanel(HWND hwnd) {
    if (g_quitting || !IsWindow(hwnd)) return;
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    HWND target = GetLastActivePopup(hwnd);
    if (!target || !IsWindowVisible(target)) target = hwnd;
    SetWindowPos(target, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(target);
}

static void EnableHubPanelReveal(HWND hwnd) {
    UINT message = SuiteWindowRevealMessage();
    /* Per-window, show-only exception. Do not allow WM_COMMAND, WM_CLOSE,
       arbitrary WM_APP messages or a process-wide UIPI filter change. */
    if (message) ChangeWindowMessageFilterEx(hwnd, message, MSGFLT_ALLOW, NULL);
}

static BOOL HandleHubPanelReveal(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UINT message = SuiteWindowRevealMessage();
    if (!message || msg != message) return FALSE;
    if (SuiteWindowRevealIsCurrent(wp, lp)) RevealPanel(hwnd);
    return TRUE;
}
