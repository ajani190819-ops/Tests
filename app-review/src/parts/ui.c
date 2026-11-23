/* On-demand selection UI. All window mutation here targets this utility's own
   controls/window; capture.c never activates or manipulates a target. */
#define ID_REFRESH 101
#define ID_OTHER 102
#define ID_CAPTURE 103
#define ID_OPEN 104
#define ID_FOLDER 105
static HWND g_folder, g_notice;

static int ReviewScale(int value) {
    typedef UINT (WINAPI *SystemDpiFn)(void);
    SystemDpiFn system_dpi = (SystemDpiFn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
    UINT dpi = g_main ? ReviewDpi(g_main) : (system_dpi ? system_dpi() : 96);
    return MulDiv(value, dpi ? (int)dpi : 96, 96);
}

static void ReviewFonts(void) {
    HFONT previous = g_font, old_heading = g_heading;
    g_font = CreateFontW(-ReviewScale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH, L"Segoe UI");
    g_heading = CreateFontW(-ReviewScale(25), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Segoe UI");
    HWND controls[] = {g_list, g_refresh, g_other, g_capture, g_open, g_folder, g_status, g_notice};
    for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
        SendMessageW(controls[i], WM_SETFONT, (WPARAM)g_font, TRUE);
    if (previous) DeleteObject(previous);
    if (old_heading) DeleteObject(old_heading);
}

static void ReviewLayout(void) {
    RECT rect;
    GetClientRect(g_main, &rect);
    int width = rect.right, height = rect.bottom, pad = ReviewScale(18);
    int button_y = height - ReviewScale(48), list_y = ReviewScale(184);
    int list_height = height - list_y - ReviewScale(112);
    if (list_height < ReviewScale(50)) list_height = ReviewScale(50);
    MoveWindow(g_notice, pad, ReviewScale(76), width - 2 * pad, ReviewScale(76), TRUE);
    MoveWindow(g_other, pad, ReviewScale(150), width - 2 * pad, ReviewScale(26), TRUE);
    MoveWindow(g_list, pad, list_y, width - 2 * pad, list_height, TRUE);
    MoveWindow(g_status, pad, height - ReviewScale(104), width - 2 * pad, ReviewScale(52), TRUE);
    MoveWindow(g_refresh, pad, button_y, ReviewScale(100), ReviewScale(32), TRUE);
    MoveWindow(g_capture, pad + ReviewScale(110), button_y, ReviewScale(240), ReviewScale(32), TRUE);
    MoveWindow(g_open, width - pad - ReviewScale(244), button_y, ReviewScale(118), ReviewScale(32), TRUE);
    MoveWindow(g_folder, width - pad - ReviewScale(118), button_y, ReviewScale(118), ReviewScale(32), TRUE);
    ListView_SetColumnWidth(g_list, 0, ReviewScale(185));
    ListView_SetColumnWidth(g_list, 1, ReviewScale(60));
    ListView_SetColumnWidth(g_list, 2, ReviewScale(90));
    int title_width = width - 2 * pad - ReviewScale(360);
    ListView_SetColumnWidth(g_list, 3, title_width > ReviewScale(120) ? title_width : ReviewScale(120));
    InvalidateRect(g_main, NULL, FALSE);
}

static BOOL CALLBACK ReviewEnum(HWND window, LPARAM unused) {
    (void)unused;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == GetCurrentProcessId() || !IsWindowVisible(window) ||
        (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) return TRUE;
    ReviewWindow info;
    if (!ReviewDescribe(window, &info)) return TRUE;
    const wchar_t *known = ReviewKnownApp(info.image);
    if (!known && Button_GetCheck(g_other) != BST_CHECKED) return TRUE;
    if (!known && !info.title[0]) return TRUE;
    if (g_window_count >= REVIEW_MAX_LIST) { g_list_limited = 1; return FALSE; }
    int row = g_window_count++;
    g_windows[row] = info;
    LVITEMW item = {0};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.pszText = (wchar_t *)(known ? known : info.image);
    item.lParam = row;
    ListView_InsertItem(g_list, &item);
    wchar_t number[24];
    StringCchPrintfW(number, 24, L"%lu", (unsigned long)info.pid);
    ListView_SetItemText(g_list, row, 1, number);
    ListView_SetItemText(g_list, row, 2, IsIconic(window) ? L"Minimized" : L"Visible");
    ListView_SetItemText(g_list, row, 3, info.title);
    ListView_SetCheckState(g_list, row, FALSE); /* Explicit selection, never opt-out capture. */
    return TRUE;
}

static void ReviewRefresh(void) {
    if (g_busy) return;
    g_window_count = 0;
    g_list_limited = 0;
    ListView_DeleteAllItems(g_list);
    EnumWindows(ReviewEnum, 0);
    SetWindowTextW(g_status, g_list_limited ?
        L"List limit reached. No windows are selected. Check only the windows you want to share." :
        L"Nothing is selected. Check up to 16 already-running windows. Refresh clears selections; it does not open apps.");
}

static void ReviewEnable(int busy) {
    g_busy = busy;
    EnableWindow(g_list, !busy);
    EnableWindow(g_refresh, !busy);
    EnableWindow(g_other, !busy);
    EnableWindow(g_open, !busy && g_run.report_ok);
    EnableWindow(g_folder, !busy && g_run.directory[0]);
    SetWindowTextW(g_capture, busy ? L"Cancel review" : L"Capture + read-only checks");
}

static void ReviewStart(void) {
    if (g_busy) {
        InterlockedExchange(&g_cancel, 1);
        SetWindowTextW(g_status, L"Cancelling this utility's capture worker only. No target app will be closed or changed.");
        return;
    }
    int count = 0;
    for (int row = 0; row < g_window_count; ++row) if (ListView_GetCheckState(g_list, row)) ++count;
    if (!count || count > REVIEW_MAX_WINDOWS) {
        MessageBoxW(g_main, L"Select between 1 and 16 running windows. No app will be opened or restored automatically.",
                    L"Choose windows first", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t directory[REVIEW_PATH];
    if (!ReviewNewSession(directory)) {
        MessageBoxW(g_main, L"Could not create a new, local, non-redirected report directory. No captures were started. "
                    L"The utility will not reuse a folder or change permissions. Check ordinary Windows storage access.",
                    L"Report storage unavailable", MB_OK | MB_ICONERROR);
        return;
    }
    ZeroMemory(&g_run, sizeof(g_run));
    StringCchCopyW(g_run.directory, REVIEW_PATH, directory);
    for (int row = 0; row < g_window_count; ++row)
        if (ListView_GetCheckState(g_list, row)) g_run.selected[g_run.count++] = g_windows[row];
    InterlockedExchange(&g_cancel, 0);
    ReviewEnable(1);
    SetWindowTextW(g_status, L"Capturing selected windows only. Each worker has an 8-second limit. No clicking, navigation or settings access.");
    g_thread = CreateThread(NULL, 0, ReviewBatchThread, NULL, 0, NULL);
    if (!g_thread) {
        ReviewEnable(0);
        SetWindowTextW(g_status, L"Could not start the review worker. No settings change was requested; no complete report was produced.");
    }
}

static void ReviewHeader(HDC dc) {
    int size = ReviewScale(48);
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    uint32_t *pixels = (uint32_t *)malloc((size_t)size * size * 4);
    if (pixels) {
        for (int i = 0; i < size * size; ++i) pixels[i] = SuiteEmblemDibPixel(RGB(24, 27, 36));
        COLORREF near_color, far_color;
        SuiteEmblemResolvePalette(RGB(24, 27, 36), RGB(151, 190, 239), RGB(236, 237, 243), &near_color, &far_color);
        SuiteEmblemDraw(&g_emblem, pixels, size, size, 0, 0, size,
                         SuiteEmblemAngleNow(0, 30000), near_color, far_color);
        SetDIBitsToDevice(dc, ReviewScale(18), ReviewScale(16), (DWORD)size, (DWORD)size,
                          0, 0, 0, (UINT)size, pixels, &info, DIB_RGB_COLORS);
        free(pixels);
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(236, 237, 243));
    SelectObject(dc, g_heading);
    TextOutW(dc, ReviewScale(80), ReviewScale(13), L"Review apps", 11);
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(188, 199, 218));
    const wchar_t *subtitle = L"Local window images + read-only checks. Your settings stay yours.";
    TextOutW(dc, ReviewScale(81), ReviewScale(46), subtitle, (int)wcslen(subtitle));
}

static void ReviewDrawButton(const DRAWITEMSTRUCT *item) {
    int disabled = (item->itemState & ODS_DISABLED) != 0;
    COLORREF fill = disabled ? RGB(35, 39, 50) :
        ((item->itemState & ODS_SELECTED) ? RGB(64, 79, 102) : RGB(45, 55, 73));
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(76, 89, 110));
    HGDIOBJ old_brush = SelectObject(item->hDC, brush), old_pen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right,
               item->rcItem.bottom, ReviewScale(12), ReviewScale(12));
    SelectObject(item->hDC, old_brush); SelectObject(item->hDC, old_pen);
    DeleteObject(brush); DeleteObject(pen);
    wchar_t text[80];
    GetWindowTextW(item->hwndItem, text, 80);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, disabled ? RGB(139, 149, 166) : RGB(238, 243, 251));
    SelectObject(item->hDC, g_font);
    RECT rect = item->rcItem;
    DrawTextW(item->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS) { InflateRect(&rect, -4, -4); DrawFocusRect(item->hDC, &rect); }
}

static LRESULT CALLBACK ReviewWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g_main = window;
        g_surface = CreateSolidBrush(RGB(24, 27, 36));
        g_notice = CreateWindowExW(0, L"STATIC",
            L"Choose running windows, then capture once. No app input, preference edits, launch/update, restore, resize or automatic sharing. "
            L"Selected images and titles may contain private data. Windows may show its capture border; unsupported content is an evidence gap. "
            L"Do not open blocked Guard for this review.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, NULL, g_instance, NULL);
        g_other = CreateWindowExW(0, L"BUTTON", L"Show other running windows (e.g. Orca / Minecraft) - still unchecked",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window, (HMENU)ID_OTHER, g_instance, NULL);
        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"Running windows - choose explicitly",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 0, 0, 0, window, NULL, g_instance, NULL);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(g_list, RGB(30, 35, 46));
        ListView_SetTextBkColor(g_list, RGB(30, 35, 46));
        ListView_SetTextColor(g_list, RGB(235, 239, 247));
        const wchar_t *names[] = {L"App / executable", L"PID", L"State", L"Window title (check for private contents)"};
        for (int i = 0; i < 4; ++i) {
            LVCOLUMNW column = {0};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = (wchar_t *)names[i]; column.cx = 150;
            ListView_InsertColumn(g_list, i, &column);
        }
        g_refresh = CreateWindowExW(0, L"BUTTON", L"Refresh list", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, (HMENU)ID_REFRESH, g_instance, NULL);
        g_capture = CreateWindowExW(0, L"BUTTON", L"Capture + read-only checks", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, (HMENU)ID_CAPTURE, g_instance, NULL);
        g_open = CreateWindowExW(0, L"BUTTON", L"Open report", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | WS_DISABLED,
            0, 0, 0, 0, window, (HMENU)ID_OPEN, g_instance, NULL);
        g_folder = CreateWindowExW(0, L"BUTTON", L"Open folder", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | WS_DISABLED,
            0, 0, 0, 0, window, (HMENU)ID_FOLDER, g_instance, NULL);
        g_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, NULL, g_instance, NULL);
        ReviewFonts(); ReviewRefresh(); ReviewLayout();
        return 0;
    }
    case WM_SIZE: ReviewLayout(); return 0;
    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lparam;
        SetWindowPos(window, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                      suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        ReviewFonts(); ReviewLayout(); return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        MONITORINFO monitor = {0}; monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            int width = monitor.rcWork.right - monitor.rcWork.left, height = monitor.rcWork.bottom - monitor.rcWork.top;
            limits->ptMinTrackSize.x = min(ReviewScale(680), width);
            limits->ptMinTrackSize.y = min(ReviewScale(500), height);
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_REFRESH: case ID_OTHER: ReviewRefresh(); return 0;
        case ID_CAPTURE: ReviewStart(); return 0;
        case ID_OPEN: {
            wchar_t path[REVIEW_PATH];
            if (!g_busy && g_run.report_ok && ReviewJoin(path, g_run.directory, L"report.html"))
                ShellExecuteW(window, L"open", path, NULL, NULL, SW_SHOWNORMAL); /* Explicit owner click only. */
            return 0;
        }
        case ID_FOLDER:
            if (!g_busy && g_run.directory[0]) ShellExecuteW(window, L"open", g_run.directory, NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        break;
    case REVIEW_PROGRESS: {
        wchar_t status[160];
        StringCchPrintfW(status, 160, L"Processed %u of %d selected windows. No target input or settings-change requests.", (unsigned)wparam, g_run.count);
        SetWindowTextW(g_status, status); return 0;
    }
    case REVIEW_DONE: {
        if (g_thread) { WaitForSingleObject(g_thread, INFINITE); CloseHandle(g_thread); g_thread = NULL; }
        ReviewEnable(0);
        int images = 0;
        for (int i = 0; i < g_run.count; ++i) if (g_run.results[i].status == REVIEW_IMAGE) ++images;
        wchar_t status[600];
        if (g_run.report_ok) StringCchPrintfW(status, 600,
            L"Saved %d unverified image(s); %d skipped/failed. Review report.html before sharing. No acceptance marked complete.\r\n%ls",
            images, g_run.count - images, g_run.directory);
        else StringCchCopyW(status, 600, L"Output failed or is incomplete - not a successful review. Open folder for any partial evidence. No target settings change was requested.");
        SetWindowTextW(g_status, status);
        if (g_close_after) DestroyWindow(window);
        return 0;
    }
    case WM_DRAWITEM: ReviewDrawButton((const DRAWITEMSTRUCT *)lparam); return TRUE;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        SetTextColor((HDC)wparam, RGB(212, 221, 237));
        SetBkColor((HDC)wparam, RGB(24, 27, 36));
        return (LRESULT)g_surface;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT rect; GetClientRect(window, &rect);
        FillRect(dc, &rect, g_surface); ReviewHeader(dc);
        EndPaint(window, &paint); return 0;
    }
    case WM_CLOSE:
        if (g_busy) { g_close_after = 1; InterlockedExchange(&g_cancel, 1); return 0; }
        DestroyWindow(window); return 0;
    case WM_DESTROY:
        SuiteEmblemRelease(&g_emblem);
        if (g_font) DeleteObject(g_font);
        if (g_heading) DeleteObject(g_heading);
        if (g_surface) DeleteObject(g_surface);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static int ReviewMain(int show) {
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    if (!InitCommonControlsEx(&controls)) return 2;
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls); cls.lpfnWndProc = ReviewWindowProc; cls.hInstance = g_instance;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW); cls.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(1));
    cls.hIconSm = cls.hIcon; cls.lpszClassName = L"Tests_AppReview_LocalOnly_v1";
    if (!RegisterClassExW(&cls)) return 2;
    RECT work;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) return 2;
    int width = min(ReviewScale(920), work.right - work.left), height = min(ReviewScale(640), work.bottom - work.top);
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"App Review - local only - " REVIEW_VERSION_W,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, work.left + (work.right - work.left - width) / 2,
        work.top + (work.bottom - work.top - height) / 2, width, height, NULL, NULL, g_instance, NULL);
    if (!window) return 2;
    ShowWindow(window, show); UpdateWindow(window);
    MSG message;
    int got;
    while ((got = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return got < 0 ? 2 : (int)message.wParam;
}

/* A second explicit launch reveals only this utility, never a selected app. */
static int ReviewLaunchUi(int show) {
    HANDLE mutex = CreateMutexW(NULL, FALSE, L"Local\\Tests_AppReview_UI_v1");
    if (!mutex) return 2;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"Tests_AppReview_LocalOnly_v1", NULL);
        if (existing) {
            if (IsIconic(existing)) ShowWindowAsync(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(mutex);
        return 0;
    }
    int result = ReviewMain(show);
    CloseHandle(mutex);
    return result;
}
