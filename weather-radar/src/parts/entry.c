/* ---------------- entry: wWinMain ---------------- */

static const wchar_t *g_className = L"WeatherRadarMainV1";

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev;
    (void)cmdLine;
    g_hInst = hInst;

    HANDLE mtx = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Second launch: show the window that is already running instead of
           opening a twin that would fight it for the same config file. */
        HWND other = FindWindowW(g_className, NULL);
        if (other) {
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }

    InitializeCriticalSection(&g_lock);
    InitPalette();
    InitPaths();
    LoadPlaces();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCE(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, 0);
    wc.lpszClassName = g_className;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, g_className, APP_TITLE_W,
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
                                NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;
    g_hwnd = hwnd;
    g_dpi = GetWinDpi(hwnd);

    /* Fit the fixed logical layout onto whatever screen is there: shrink
       uniformly rather than letting a small laptop clip the radar pane. */
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int availW = (wa.right - wa.left) - S(32);
    int availH = (wa.bottom - wa.top) - S(32);
    int wantW = MulDiv(WIN_LW, (int)g_dpi, 96);
    int wantH = MulDiv(WIN_LH, (int)g_dpi, 96);
    int fit = 100;
    if (wantW > availW) fit = availW * 100 / wantW;
    if (wantH * fit / 100 > availH) fit = availH * 100 / wantH;
    if (fit < 55) fit = 55;
    if (fit > 100) fit = 100;
    g_fit = fit;

    MakeFonts();
    g_bgBrush = CreateSolidBrush(COL_BG);
    g_inputBrush = CreateSolidBrush(COL_INPUT);
    LayoutSearchEdit();
    ApplyModernWindowFrame(hwnd);

    RECT desired = { 0, 0, S(WIN_LW), S(WIN_LH) };
    AdjustWindowRect(&desired, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int cx = wa.left + ((wa.right - wa.left) - (desired.right - desired.left)) / 2;
    int cy = wa.top + ((wa.bottom - wa.top) - (desired.bottom - desired.top)) / 2;
    SetWindowPos(hwnd, NULL, cx, cy, desired.right - desired.left,
                 desired.bottom - desired.top, SWP_NOZORDER | SWP_NOACTIVATE);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    WorkerStart();
    SetTimer(hwnd, TIMER_REFRESH, 10 * 60 * 1000, NULL);
    if (g_placeSel >= 0 && g_placeSel < g_placeN) {
        wchar_t m[160];
        _snwprintf(m, 159, L"%s - fetching forecast, ensemble and radar...",
                   g_places[g_placeSel].name);
        StatusSet(m, 0);
        RequestPlaceData(&g_places[g_placeSel]);
    } else {
        StatusSet(L"Search for a city on the left - everything else follows from the place.", 0);
    }

    MSG m2;
    while (GetMessageW(&m2, NULL, 0, 0) > 0) {
        TranslateMessage(&m2);
        DispatchMessageW(&m2);
    }
    (void)mtx;
    return (int)m2.wParam;
}
