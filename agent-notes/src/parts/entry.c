/* ---------------- entry ---------------- */

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;
    g_hInst = hInst;

    CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Normal case: nudge the running window forward and exit. But if
           nothing answers - a hung half-dead process still holding the
           mutex - start anyway, or the app looks like it 'won't open'. */
        g_showEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT);
        if (g_showEvent) SetEvent(g_showEvent);
        HWND other = NULL;
        for (int i = 0; i < 40; i++) {          /* up to ~2 s */
            other = FindWindowW(L"AgentNotesWnd", NULL);
            if (other) break;
            Sleep(50);
        }
        if (other) {
            DWORD_PTR reply = 0;
            if (!SendMessageTimeoutW(other, WM_NULL, 0, 0,
                                     SMTO_ABORTIFHUNG | SMTO_BLOCK, 400, &reply))
                other = NULL;                   /* window exists but is hung */
        }
        if (other) return 0;                    /* healthy instance is up */
        /* zombie or windowless leftover - fall through and open for real */
    }
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT);

    InitPaths();
    LoadConfig();
    HistLoad();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"AgentNotesWnd";
    RegisterClassExW(&wc);

    /* Create first at a nominal size, then size to the true DPI: S() still
       uses the default 96 dpi at this point, so sizing the window before
       GetWinDpi would cut the layout off on any scaled display. */
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE_W, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             S(620), S(600),
                             NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    g_dpi = GetWinDpi(g_hwnd);
    {
        /* Size to the true DPI, then shrink uniformly if that would not fit
           the work area - the layout can never be cut off on any display. */
        RECT wrc = { 0, 0, S(620), S(600) };
        AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        int workW = (wa.right - wa.left) - S(32);
        int workH = (wa.bottom - wa.top) - S(32);
        int winW = wrc.right - wrc.left, winH = wrc.bottom - wrc.top;
        if (winW > workW || winH > workH) {
            int fx = MulDiv(100, workW, winW);
            int fy = MulDiv(100, workH, winH);
            g_fit = (fx < fy) ? fx : fy;
            if (g_fit < 50) g_fit = 50;
            if (g_fit > 100) g_fit = 100;
            RECT w2 = { 0, 0, S(620), S(600) };
            AdjustWindowRectEx(&w2, WS_OVERLAPPEDWINDOW, FALSE, 0);
            wrc = w2;
        }
        SetWindowPos(g_hwnd, NULL, 0, 0,
                     wrc.right - wrc.left, wrc.bottom - wrc.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ApplyModernWindowFrame(g_hwnd);
    g_bgBrush = CreateSolidBrush(COL_BG);
    g_fontUI = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(22), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontBtn = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

    /* rich-edits give us dark, standard multiline/single-line inputs */
    LoadLibraryW(L"msftedit.dll");
    g_hLink = CreateWindowExW(0, L"RichEdit50W", g_link,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              S(130), S(98), S(330), S(32),
                              g_hwnd, (HMENU)(INT_PTR)ID_EDIT_LINK, hInst, NULL);
    g_hNote = CreateWindowExW(0, L"RichEdit50W", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                              ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                              S(20), S(178), S(580), S(190),
                              g_hwnd, (HMENU)(INT_PTR)ID_EDIT_NOTE, hInst, NULL);
    if (g_hLink) {
        SendMessageW(g_hLink, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_INPUT);
        SendMessageW(g_hLink, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hLink, EM_SETLIMITTEXT, 0, 255);
    }
    if (g_hNote) {
        SendMessageW(g_hNote, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_INPUT);
        SendMessageW(g_hNote, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hNote, EM_SETLIMITTEXT, 0, 4000);
        NoteSetHint(1);
    }

    if (OutboxCount() > 0)
        _snwprintf(g_status, 255, L"%d queued note(s) found - delivering when reachable.",
                   OutboxCount());
    else if (g_link[0])
        wcsncpy(g_status, L"Ready - type a note and press Send to Agent.", 255);

    SetTimer(g_hwnd, TIMER_FLUSH, 60000, NULL);
    SetTimer(g_hwnd, TIMER_SPIN, 50, NULL);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);       /* an explicit Open must take focus */

    MSG msg;
    for (;;) {
        DWORD w = MsgWaitForMultipleObjects(1, &g_showEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (w == WAIT_OBJECT_0) {
            /* second-launch nudge: bring the window forward */
            if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
            SetForegroundWindow(g_hwnd);
            continue;
        }
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            if (!IsDialogMessageW(g_hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
}
