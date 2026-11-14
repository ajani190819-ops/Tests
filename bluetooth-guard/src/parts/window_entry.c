static void RenderWindow(HWND hwnd, HDC targetDc) {
    RECT client;
    GetClientRect(hwnd, &client);
    int W = client.right - client.left;
    int H = client.bottom - client.top;
    if (W <= 0 || H <= 0) return;

    int dpi = GetWinDpi(hwnd);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H; /* top-down 32bpp */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC mem = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(mem); return; }
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    HFONT fTitle = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fSub   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fCard  = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBody  = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    int M = S(28, dpi);
    int CW = W - M * 2;
    int headerH = S(76, dpi);
    int footerH = S(44, dpi);
    int footerTop = H - footerH;

    /* Calm low-glare slate canvas */
    FillCanvasRect(px, W, H, 0, 0, W, footerTop, COL_BG);
    FillCanvasRect(px, W, H, 0, footerTop, W, H, COL_FOOTER_BG);

    g_hitCount = 0;

    /* Copy device snapshot thread-safely */
    DeviceEntry snap[MAX_DEVICES];
    int count = 0, blockedCount = 0;
    BOOL radioOn = TRUE;
    EnterCriticalSection(&g_lock);
    count = g_devCount;
    radioOn = g_radioOn;
    if (count > 0) memcpy(snap, g_devices, sizeof(DeviceEntry) * (size_t)count);
    LeaveCriticalSection(&g_lock);
    int visCount = 0;
    for (int i = 0; i < count; i++) {
        if (!snap[i].visible) continue;
        visCount++;
        if (!snap[i].allowAuto) blockedCount++;
    }

    /* Header Suite Tri-Spoke Core Emblem + natural title + calm subtitle */
    int embSz = S(36, dpi);
    DrawWayfinderEmblem(px, W, H, M, (headerH - embSz) / 2, embSz);

    int topBtnH = S(34, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    RECT btnRefR   = { W - M - S(86, dpi),  topBtnY, W - M,                    topBtnY + topBtnH };
    RECT togStartR = { btnRefR.left - S(132, dpi), topBtnY, btnRefR.left - S(8, dpi), topBtnY + topBtnH };
    RECT togGuardR = { togStartR.left - S(122, dpi), topBtnY, togStartR.left - S(8, dpi), topBtnY + topBtnH };

    int idxGuard = g_hitCount;
    AddHit(togGuardR, HIT_TOGGLE_GUARD, 0);
    DrawPillToggle(mem, px, W, H, fBtn, togGuardR, L"Guard", g_guardOn, g_hoverIdx == idxGuard, dpi, 0, headerH);

    int idxStart = g_hitCount;
    AddHit(togStartR, HIT_TOGGLE_STARTUP, 0);
    DrawPillToggle(mem, px, W, H, fBtn, togStartR, L"Startup", g_autostart, g_hoverIdx == idxStart, dpi, 0, headerH);

    int idxRef = g_hitCount;
    AddHit(btnRefR, HIT_BTN_REFRESH, 0);
    DrawActionBtn(mem, px, W, H, fBtn, btnRefR, L"Refresh", 0, g_hoverIdx == idxRef, dpi, 0, headerH);

    int hdrLeft = M + embSz + S(14, dpi);
    SelectObject(mem, fTitle);
    SetTextColor(mem, COL_TEXT);
    RECT titleR = { hdrLeft, S(13, dpi), togGuardR.left - S(12, dpi), S(40, dpi) };
    DrawTextW(mem, L"Bluetooth Guard", -1, &titleR,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    wchar_t subHdr[96];
    if (!g_guardOn)
        wcsncpy(subHdr, L"Audio shield paused", 95);
    else if (!radioOn)
        wcsncpy(subHdr, L"Bluetooth radio is off", 95);
    else
        _snwprintf(subHdr, 95, L"Audio protection  \x00B7  %d protected (%d paired)", blockedCount, visCount);

    SelectObject(mem, fSub);
    SetTextColor(mem, COL_TELEMETRY);
    RECT subR = { hdrLeft, S(40, dpi), togGuardR.left - S(12, dpi), S(60, dpi) };
    DrawTextW(mem, subHdr, -1, &subR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillCanvasRect(px, W, H, M, headerH, W - M, headerH + 1, COL_DIVIDER);

    /* Scrollable device cards region */
    int listTop = headerH + S(14, dpi);
    int listBottom = footerTop;
    int cardH = S(82, dpi);
    int gap = S(11, dpi);
    g_contentH = visCount * (cardH + gap);
    int maxScroll = g_contentH - (listBottom - listTop);
    if (maxScroll < 0) maxScroll = 0;
    if (g_scrollY > maxScroll) g_scrollY = maxScroll;
    if (g_scrollY < 0) g_scrollY = 0;

    HRGN clip = CreateRectRgn(0, listTop, W, listBottom);
    SelectClipRgn(mem, clip);

    int cardIcoSz = S(42, dpi);
    HICON hCardIco = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP),
                                       IMAGE_ICON, cardIcoSz, cardIcoSz, LR_DEFAULTCOLOR);

    if (visCount == 0) {
        RECT emptyR = { M, listTop, M + CW, listTop + S(82, dpi) };
        SmoothRoundRect(px, W, H, &emptyR, (float)S(10, dpi), COL_CARD, COL_BORDER, listTop, listBottom);
        SelectObject(mem, fBody);
        SetTextColor(mem, COL_MUTED);
        RECT er = { emptyR.left + S(24, dpi), emptyR.top, emptyR.right - S(18, dpi), emptyR.bottom };
        DrawTextW(mem, L"No paired Bluetooth devices found yet. Pair headphones in Windows Settings, then click Refresh.",
                  -1, &er, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        int y = listTop - g_scrollY;
        for (int i = 0; i < count; i++) {
            DeviceEntry *d = &snap[i];
            /* hidden entries take no row space - the hit index below stays
               the raw snapshot index so clicks still land correctly */
            if (!d->visible) continue;
            RECT cr = { M, y, M + CW, y + cardH };
            if (cr.bottom >= listTop && cr.top <= listBottom) {
                SmoothRoundRect(px, W, H, &cr, (float)S(10, dpi), COL_CARD, COL_BORDER, listTop, listBottom);

                COLORREF barCol = COL_IDLE_SLATE, tagFg = COL_MUTED, tagBg = COL_PILL_MUTED;
                const wchar_t *tagTxt = L"Auto On";
                if (d->connected) {
                    barCol = COL_GOOD; tagFg = COL_GOOD; tagBg = COL_GOOD_BG;
                    tagTxt = !d->allowAuto ? L"Manual Session" : L"Connected";
                } else if (!d->allowAuto) {
                    barCol = COL_ACCENT; tagFg = COL_ACCENT; tagBg = COL_ACCENT_BG;
                    tagTxt = L"Phone Safe";
                }

                /* Soft rounded left accent capsule + Suite Bluetooth icon */
                RECT barRc = { cr.left + S(1, dpi), cr.top + S(16, dpi), cr.left + S(5, dpi), cr.bottom - S(16, dpi) };
                SmoothRoundRect(px, W, H, &barRc, (float)S(2, dpi), barCol, barCol, listTop, listBottom);
                if (hCardIco) {
                    DrawIconEx(mem, cr.left + S(18, dpi), (cr.top + cr.bottom - cardIcoSz) / 2,
                               hCardIco, cardIcoSz, cardIcoSz, 0, NULL, DI_NORMAL);
                }

                /* Top-right smooth status pill */
                DrawStatusTag(mem, px, W, H, fBadge, cr.right - S(16, dpi), cr.top + S(11, dpi),
                              tagTxt, tagFg, tagBg, dpi, listTop, listBottom);

                /* Clean natural-case device name */
                SelectObject(mem, fCard);
                SetTextColor(mem, COL_TEXT);
                RECT nr = { cr.left + S(72, dpi), cr.top + S(13, dpi), cr.right - S(176, dpi), cr.top + S(37, dpi) };
                DrawTextW(mem, d->name, -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                /* Right bottom controls: smooth pill switch [Auto-Connect] + rounded [Connect / Disconnect] */
                int btnBottomY = cr.bottom - S(12, dpi);
                int btnTopY    = btnBottomY - S(30, dpi);
                RECT connBtnR  = { cr.right - S(118, dpi), btnTopY, cr.right - S(16, dpi), btnBottomY };
                RECT autoTogR  = { connBtnR.left - S(156, dpi), btnTopY, connBtnR.left - S(8, dpi), btnBottomY };
                RECT forgetR   = { autoTogR.left - S(96, dpi), btnTopY, autoTogR.left - S(8, dpi), btnBottomY };

                const wchar_t *subTxt = NULL;
                if (d->connected && !d->allowAuto)
                    subTxt = L"Connected manually  \x00B7  Re-locks after disconnect";
                else if (d->connected)
                    subTxt = L"Connected audio session";
                else if (!d->allowAuto)
                    subTxt = L"Auto-connect off  \x00B7  Click Connect anytime";
                else
                    subTxt = L"Auto-connect on  \x00B7  Connects automatically in range";

                SelectObject(mem, fBody);
                SetTextColor(mem, COL_MUTED);
                RECT sr = { cr.left + S(72, dpi), cr.top + S(39, dpi), forgetR.left - S(14, dpi), cr.bottom - S(12, dpi) };
                DrawTextW(mem, subTxt, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                int hitAuto = g_hitCount;
                AddHit(autoTogR, HIT_DEV_AUTO_TOGGLE, i);
                DrawPillToggle(mem, px, W, H, fBtn, autoTogR, L"Auto-Connect",
                               d->allowAuto, g_hoverIdx == hitAuto, dpi, listTop, listBottom);

                int hitConn = g_hitCount;
                AddHit(connBtnR, HIT_DEV_CONNECT_BTN, i);
                DrawActionBtn(mem, px, W, H, fBtn, connBtnR,
                              d->connected ? L"Disconnect" : L"Connect",
                              d->connected ? 2 : 3,
                              g_hoverIdx == hitConn, dpi, listTop, listBottom);

                int hitForget = g_hitCount;
                AddHit(forgetR, HIT_DEV_FORGET_BTN, i);
                DrawActionBtn(mem, px, W, H, fBtn, forgetR, L"Forget", 2,
                              g_hoverIdx == hitForget, dpi, listTop, listBottom);
            }
            y += cardH + gap;
        }
    }

    if (hCardIco) DestroyIcon(hCardIco);
    SelectClipRgn(mem, NULL);
    DeleteObject(clip);

    /* Calm rounded footer bar */
    FillCanvasRect(px, W, H, 0, footerTop, W, footerTop + 1, COL_DIVIDER);

    COLORREF footDot = g_guardOn ? COL_GOOD : COL_WARN;
    int fdSz = S(8, dpi);
    RECT fdRc = { M, footerTop + (footerH - fdSz) / 2, M + fdSz, footerTop + (footerH - fdSz) / 2 + fdSz };
    SmoothRoundRect(px, W, H, &fdRc, (float)fdSz * 0.5f, footDot, footDot, footerTop, H);

    SelectObject(mem, fSub);
    SetTextColor(mem, COL_MUTED);
    RECT footTxtR = { M + S(16, dpi), footerTop, W - M - S(112, dpi), H };
    DrawTextW(mem, L"Running in system tray  \x00B7  Manual connect works here & in Windows Quick Settings",
              -1, &footTxtR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    RECT quitBtnR = { W - M - S(96, dpi), footerTop + S(7, dpi), W - M, H - S(8, dpi) };
    int idxQuit = g_hitCount;
    AddHit(quitBtnR, HIT_BTN_QUIT, 0);
    DrawActionBtn(mem, px, W, H, fBtn, quitBtnR, L"Quit", 0, g_hoverIdx == idxQuit, dpi, footerTop, H);

    BitBlt(targetDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

    DeleteObject(fTitle);
    DeleteObject(fSub);
    DeleteObject(fCard);
    DeleteObject(fBtn);
    DeleteObject(fBadge);
    DeleteObject(fBody);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void HandleHitClick(int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return;
    int kind = g_hits[hitIdx].kind;
    int di   = g_hits[hitIdx].index;

    if (kind == HIT_TOGGLE_GUARD) {
        g_guardOn = !g_guardOn;
        SaveRules();
        ReleaseSemaphore(g_qSem, 1, NULL);
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_TOGGLE_STARTUP) {
        SetAutostart(!g_autostart);
        SaveRules();
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_BTN_REFRESH) {
        ReleaseSemaphore(g_qSem, 1, NULL);
    } else if (kind == HIT_BTN_QUIT) {
        g_quitting = TRUE;
        DestroyWindow(g_hwnd);
    } else if (kind == HIT_DEV_AUTO_TOGGLE) {
        ULONGLONG addr = 0;
        wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
        BOOL nowAllow = TRUE, wasConn = FALSE;
        EnterCriticalSection(&g_lock);
        if (di >= 0 && di < g_devCount) {
            DeviceEntry *d = &g_devices[di];
            d->allowAuto = !d->allowAuto;
            d->sessionAllowed = FALSE;
            d->seenConnected = FALSE;
            nowAllow = d->allowAuto;
            wasConn = d->connected;
            addr = d->address;
            wcsncpy(dkind, d->kind, 15);
            wcsncpy(dname, d->name, 127);
            wcsncpy(dmac, d->mac, 23);
        }
        LeaveCriticalSection(&g_lock);
        SaveRules();
        if (!nowAllow && wasConn && g_guardOn) {
            EnqueueWork(1, addr, dmac, dkind, dname);
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_DEV_CONNECT_BTN) {
        ULONGLONG addr = 0;
        wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
        BOOL wasConn = FALSE;
        EnterCriticalSection(&g_lock);
        if (di >= 0 && di < g_devCount) {
            DeviceEntry *d = &g_devices[di];
            wasConn = d->connected;
            addr = d->address;
            wcsncpy(dkind, d->kind, 15);
            wcsncpy(dname, d->name, 127);
            wcsncpy(dmac, d->mac, 23);
            if (wasConn) {
                d->sessionAllowed = FALSE;
                d->seenConnected = FALSE;
                d->connected = FALSE;
            } else {
                d->sessionAllowed = TRUE;
                d->seenConnected = FALSE;
                d->sessionStartTick = GetTickCount64();
            }
        }
        LeaveCriticalSection(&g_lock);
        EnqueueWork(wasConn ? 1 : 2, addr, dmac, dkind, dname);
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_DEV_FORGET_BTN) {
        ULONGLONG addr = 0;
        wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
        EnterCriticalSection(&g_lock);
        if (di >= 0 && di < g_devCount) {
            DeviceEntry *d = &g_devices[di];
            addr = d->address;
            wcsncpy(dkind, d->kind, 15);
            wcsncpy(dname, d->name, 127);
            wcsncpy(dmac, d->mac, 23);
            d->visible = FALSE;   /* drop it from the list at once */
            d->connected = FALSE;
            d->allowAuto = FALSE;
        }
        LeaveCriticalSection(&g_lock);
        SaveRules();
        EnqueueWork(3, addr, dmac, dkind, dname);   /* true unpair on the worker */
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void ShowTrayMenu(void) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Bluetooth Guard");
    AppendMenuW(hMenu, MF_STRING | (g_guardOn ? MF_CHECKED : 0), IDM_TRAY_PAUSE, L"Guard Enabled");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount && i < 20; i++) {
        wchar_t label[300];
        _snwprintf(label, 299, L"%s  %s",
                   g_devices[i].connected ? L"Disconnect" : L"Connect",
                   g_devices[i].name);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_DEV0 + i, label);
    }
    LeaveCriticalSection(&g_lock);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Bluetooth Guard");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        SetTimer(hwnd, 1, 700, NULL);
        SetTimer(hwnd, 3, 50, NULL);    /* emblem spin repaint */
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RenderWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        g_scrollY -= (delta / WHEEL_DELTA) * S(44, GetWinDpi(hwnd));
        if (g_scrollY < 0) g_scrollY = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int found = -1;
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) { found = i; break; }
        }
        SetCursor(LoadCursorW(NULL, found >= 0 ? IDC_HAND : IDC_ARROW));
        if (found != g_hoverIdx) {
            g_hoverIdx = found;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) {
                HandleHitClick(i);
                break;
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (id == IDM_TRAY_PAUSE) {
            g_guardOn = !g_guardOn;
            SaveRules();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
        } else if (id >= IDM_TRAY_DEV0 && id < IDM_TRAY_DEV0 + MAX_DEVICES) {
            int di = id - IDM_TRAY_DEV0;
            ULONGLONG addr = 0;
            wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
            BOOL wasConn = FALSE;
            EnterCriticalSection(&g_lock);
            if (di < g_devCount) {
                DeviceEntry *d = &g_devices[di];
                wasConn = d->connected;
                addr = d->address;
                wcsncpy(dkind, d->kind, 15);
                wcsncpy(dname, d->name, 127);
                wcsncpy(dmac, d->mac, 23);
                if (wasConn) {
                    d->sessionAllowed = FALSE;
                    d->seenConnected = FALSE;
                } else {
                    d->sessionAllowed = TRUE;
                    d->seenConnected = FALSE;
                    d->sessionStartTick = GetTickCount64();
                }
            }
            LeaveCriticalSection(&g_lock);
            EnqueueWork(wasConn ? 1 : 2, addr, dmac, dkind, dname);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_WORKER_DONE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        if (wp == 3) { InvalidateRect(hwnd, NULL, FALSE); return 0; }
        CheckForegroundManualIntent();
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        WriteHeartbeat();
        return 0;

    case WM_DEVICECHANGE:
        /* Kernel radio / HCI connection state changed — wake high-priority worker in <1ms! */
        ArmFastBurstWindow(5000ULL);
        ReleaseSemaphore(g_qSem, 1, NULL);
        return TRUE;

    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
        /* Clear active sessions & disconnect guarded earbuds before Restart / Fast Startup hibernate */
        g_manualGraceUntil = 0;
        EnterCriticalSection(&g_lock);
        for (int i = 0; i < g_devCount; i++) {
            g_devices[i].sessionAllowed = FALSE;
            g_devices[i].seenConnected = FALSE;
        }
        LeaveCriticalSection(&g_lock);
        DisconnectGuardedDevicesNow();
        return TRUE;

    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) {
            g_manualGraceUntil = 0;
            EnterCriticalSection(&g_lock);
            for (int i = 0; i < g_devCount; i++) {
                g_devices[i].sessionAllowed = FALSE;
                g_devices[i].seenConnected = FALSE;
            }
            LeaveCriticalSection(&g_lock);
            DisconnectGuardedDevicesNow();
        } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            g_manualGraceUntil = 0;
            ArmFastBurstWindow(30000ULL);
            ReleaseSemaphore(g_qSem, 1, NULL);
        }
        return TRUE;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;

    case WM_CLOSE:
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            if (!g_trayHinted) {
                g_trayHinted = TRUE;
                g_nid.uFlags = NIF_INFO;
                wcsncpy(g_nid.szInfoTitle, L"Bluetooth Guard is active in tray", 63);
                wcsncpy(g_nid.szInfo, L"Right-click the tray icon any time to quick-connect or disconnect.", 255);
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        UnregisterBluetoothNotifications();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        DeleteFileW(g_lockPath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;
    BoostProcessAndShutdownPriority();
    InitPaths();

    BOOL silent = (cmdLine && (wcsstr(cmdLine, L"--silent") != NULL ||
                               wcsstr(cmdLine, L"--startup") != NULL));

    if (cmdLine && wcsstr(cmdLine, L"--stop")) {
        FILE *fp = _wfopen(g_lockPath, L"r");
        if (fp) {
            unsigned long pid = 0;
            if (fscanf(fp, "%lu", &pid) == 1 && pid > 0 && pid != GetCurrentProcessId()) {
                HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
                if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
            }
            fclose(fp);
        }
        DeleteFileW(g_lockPath);
        return 0;
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!silent) {
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    InitializeCriticalSection(&g_lock);
    g_qSem = CreateSemaphoreW(NULL, 1, 64, NULL);

    InitBluetoothApis();
    BOOL regAuto = IsAutostartEnabled();
    LoadRules();
    if (!g_autostartExplicit) g_autostart = regAuto ? TRUE : TRUE;

    /* Immediate millisecond-0 Bluetooth sweep BEFORE creating window/fonts/icons! */
    ArmFastBurstWindow(60000ULL);
    PollPairedDevices();
    WriteHeartbeat();

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsBluetoothGuardModernWnd";
    RegisterClassExW(&wc);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(740, dpi), S(560, dpi),
                             NULL, NULL, hInst, NULL);
    RegisterBluetoothNotifications(g_hwnd);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_appIcon;
    wcsncpy(g_nid.szTip, L"Bluetooth Guard (High-Priority Active)", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

    if (!silent) {
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_hwnd);
        SetForegroundWindow(g_hwnd);   /* an explicit Open must take focus */
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_workerRun, 0);
    ReleaseSemaphore(g_qSem, 1, NULL);
    if (hThread) { WaitForSingleObject(hThread, 1500); CloseHandle(hThread); }
    CloseHandle(g_qSem);
    if (g_showEvent) CloseHandle(g_showEvent);
    if (hMutex) CloseHandle(hMutex);
    DeleteCriticalSection(&g_lock);
    CoUninitialize();
    return 0;
}
