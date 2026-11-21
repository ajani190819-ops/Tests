/* ---------------- unified suite notification-area tray ----------------

   Project Hub is the opt-in tray host only while it is already running. It
   creates no Run key, task, service, or sign-in path. Current suite apps
   discover the host through HUB_TRAY_HOST_EVENT and remove their duplicate
   icons; if the host goes away, each app restores its own icon on its regular
   timer. The event is a capability marker, not a command channel. */

#define HUB_TRAY_MENU_ITEM_CAP 64

enum {
    HUB_TRAY_MENU_TEXT = 0,
    HUB_TRAY_MENU_SECTION,
    HUB_TRAY_MENU_SEPARATOR
};

typedef struct {
    wchar_t text[96];
    int kind;
    BOOL checked;
    BOOL disabled;
    BOOL isApp;
    BOOL running;
    BOOL isHub;
    BOOL hasSubmenu;
} HubTrayMenuItem;

static HubTrayMenuItem g_hubTrayMenuItems[HUB_TRAY_MENU_ITEM_CAP];
static int g_hubTrayMenuItemCount = 0;
static HBRUSH g_hubTrayMenuBrush = NULL;
static BOOL g_hubTrayMenuTracking = FALSE;

static HubTrayMenuItem *HubTrayMenuAdd(HMENU menu, UINT_PTR commandOrSubmenu,
                                       const wchar_t *text, int kind,
                                       BOOL checked, BOOL disabled, BOOL isApp,
                                       BOOL running, BOOL isHub, BOOL hasSubmenu) {
    if (!menu || g_hubTrayMenuItemCount >= HUB_TRAY_MENU_ITEM_CAP) return NULL;
    HubTrayMenuItem *item = &g_hubTrayMenuItems[g_hubTrayMenuItemCount++];
    ZeroMemory(item, sizeof(*item));
    if (text) {
        wcsncpy(item->text, text, 95);
        item->text[95] = L'\0';
    }
    item->kind = kind;
    item->checked = checked;
    item->disabled = disabled;
    item->isApp = isApp;
    item->running = running;
    item->isHub = isHub;
    item->hasSubmenu = hasSubmenu;

    UINT flags = MF_OWNERDRAW;
    if (checked) flags |= MF_CHECKED;
    if (disabled) flags |= MF_DISABLED | MF_GRAYED;
    if (hasSubmenu) flags |= MF_POPUP;
    if (!AppendMenuW(menu, flags, commandOrSubmenu, (LPCWSTR)(ULONG_PTR)item)) {
        g_hubTrayMenuItemCount--;
        return NULL;
    }
    return item;
}

static HMENU HubTrayCreateMenu(void) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return NULL;
    if (g_hubTrayMenuBrush) {
        MENUINFO info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        info.fMask = MIM_BACKGROUND;
        info.hbrBack = g_hubTrayMenuBrush;
        SetMenuInfo(menu, &info);
    }
    return menu;
}

static BOOL HubTrayMeasureMenuItem(MEASUREITEMSTRUCT *measure) {
    if (!measure || measure->CtlType != ODT_MENU || !measure->itemData) return FALSE;
    const HubTrayMenuItem *item = (const HubTrayMenuItem *)measure->itemData;
    int dpi = GetWinDpi(g_hwnd);
    measure->itemWidth = (UINT)S(360, dpi);
    if (item->kind == HUB_TRAY_MENU_SEPARATOR)
        measure->itemHeight = (UINT)S(9, dpi);
    else if (item->kind == HUB_TRAY_MENU_SECTION)
        measure->itemHeight = (UINT)S(27, dpi);
    else
        measure->itemHeight = (UINT)S(36, dpi);
    return TRUE;
}

static void HubTrayDrawCheckmark(HDC dc, int x, int y, int dpi) {
    HPEN pen = CreatePen(PS_SOLID, S(2, dpi), COL_SKY);
    if (!pen) return;
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x, y + S(7, dpi), NULL);
    LineTo(dc, x + S(4, dpi), y + S(11, dpi));
    LineTo(dc, x + S(12, dpi), y + S(2, dpi));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

static void HubTrayDrawMenuText(HDC dc, const wchar_t *text, RECT *rc,
                                COLORREF color, HFONT font) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : NULL;
    DrawTextW(dc, text ? text : L"", -1, rc,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

static void HubTrayDrawArrow(HDC dc, RECT rc, int dpi, COLORREF color) {
    int cx = rc.right - S(15, dpi);
    int cy = (rc.top + rc.bottom) / 2;
    POINT points[3] = {
        { cx - S(2, dpi), cy - S(5, dpi) },
        { cx + S(3, dpi), cy },
        { cx - S(2, dpi), cy + S(5, dpi) }
    };
    HPEN pen = CreatePen(PS_SOLID, S(1, dpi), color);
    HBRUSH brush = CreateSolidBrush(color);
    if (!pen || !brush) {
        if (pen) DeleteObject(pen);
        if (brush) DeleteObject(brush);
        return;
    }
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    Polygon(dc, points, 3);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void HubTrayDrawMenuItemContent(const DRAWITEMSTRUCT *draw) {
    const HubTrayMenuItem *item = (const HubTrayMenuItem *)draw->itemData;
    HDC dc = draw->hDC;
    RECT rc = draw->rcItem;
    int dpi = GetWinDpi(g_hwnd);
    HBRUSH bg = g_hubTrayMenuBrush ? g_hubTrayMenuBrush : GetSysColorBrush(COLOR_MENU);
    FillRect(dc, &rc, bg);

    if (item->kind == HUB_TRAY_MENU_SEPARATOR) {
        HPEN pen = CreatePen(PS_SOLID, 1, COL_DIVIDER);
        if (pen) {
            HGDIOBJ oldPen = SelectObject(dc, pen);
            int y = (rc.top + rc.bottom) / 2;
            MoveToEx(dc, rc.left + S(14, dpi), y, NULL);
            LineTo(dc, rc.right - S(14, dpi), y);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
        return;
    }

    BOOL disabled = item->disabled || (draw->itemState & ODS_DISABLED) != 0;
    BOOL selected = !disabled && item->kind == HUB_TRAY_MENU_TEXT &&
                    (draw->itemState & ODS_SELECTED) != 0;
    if (selected) {
        RECT hover = rc;
        InflateRect(&hover, -S(4, dpi), -S(2, dpi));
        HPEN pen = CreatePen(PS_SOLID, S(1, dpi), COL_HOVER_BORDER);
        HBRUSH brush = CreateSolidBrush(COL_BTN_HOVER);
        if (pen && brush) {
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            RoundRect(dc, hover.left, hover.top, hover.right, hover.bottom,
                      S(8, dpi), S(8, dpi));
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
        }
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
    }

    RECT textRc = rc;
    if (item->kind == HUB_TRAY_MENU_SECTION) {
        textRc.left += S(16, dpi);
        textRc.right -= S(12, dpi);
        HubTrayDrawMenuText(dc, item->text, &textRc, COL_SKY, g_fontBody);
        return;
    }

    if (item->isApp) {
        int size = S(8, dpi);
        int left = rc.left + S(14, dpi);
        int top = (rc.top + rc.bottom - size) / 2;
        COLORREF dotColor = item->running ? COL_MINT : COL_TEXT_MUTED;
        HBRUSH dot = CreateSolidBrush(dotColor);
        if (dot) {
            HGDIOBJ oldBrush = SelectObject(dc, dot);
            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, left, top, left + size, top + size);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(dot);
        }
        textRc.left += S(29, dpi);
        textRc.right -= S(108, dpi);
        RECT statusRc = rc;
        statusRc.left = rc.right - S(102, dpi);
        statusRc.right = rc.right - S(28, dpi);
        HubTrayDrawMenuText(dc, item->running ? L"Running" : L"Not running",
                            &statusRc,
                            item->running ? COL_MINT : COL_TEXT_SECONDARY, g_fontBody);
    } else if (item->isHub && g_appIcon) {
        int size = S(18, dpi);
        DrawIconEx(dc, rc.left + S(12, dpi),
                   (rc.top + rc.bottom - size) / 2,
                   g_appIcon, size, size, 0, NULL, DI_NORMAL);
        textRc.left += S(38, dpi);
        textRc.right -= S(14, dpi);
    } else if (item->checked || (draw->itemState & ODS_CHECKED) != 0) {
        HubTrayDrawCheckmark(dc, rc.left + S(14, dpi),
                             (rc.top + rc.bottom - S(14, dpi)) / 2, dpi);
        textRc.left += S(36, dpi);
        textRc.right -= S(14, dpi);
    } else {
        textRc.left += S(16, dpi);
        textRc.right -= item->hasSubmenu ? S(30, dpi) : S(14, dpi);
    }

    HubTrayDrawMenuText(dc, item->text, &textRc,
                        disabled ? COL_TEXT_MUTED : COL_TEXT_PRIMARY, g_fontBody);
    if (item->hasSubmenu)
        HubTrayDrawArrow(dc, rc, dpi, disabled ? COL_TEXT_MUTED : COL_TEXT_SECONDARY);
}

static BOOL HubTrayDrawMenuItem(const DRAWITEMSTRUCT *draw) {
    if (!draw || draw->CtlType != ODT_MENU || !draw->itemData || !draw->hDC)
        return FALSE;
    /* Treat each row as a complete paint transaction. Text drawing changes DC
       colors/mode; restore that state before the menu manager redraws another
       row during hover transitions. */
    int savedDc = SaveDC(draw->hDC);
    HubTrayDrawMenuItemContent(draw);
    if (savedDc) RestoreDC(draw->hDC, savedDc);
    return TRUE;
}

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

static BOOL HubTrayShowAgentWaitingAlert(void) {
    if (!g_hubTrayVisible) return FALSE;
    g_hubNid.uFlags = NIF_INFO;
    g_hubNid.dwInfoFlags = NIIF_INFO | (HubTrayAgentWaitSound() ? 0 : NIIF_NOSOUND);
    wcsncpy(g_hubNid.szInfoTitle, L"Agent is waiting for input", 63);
    wcsncpy(g_hubNid.szInfo,
             L"A question needs your reply in Arena. Open the Arena chat when you are ready.",
             255);
    g_hubNid.szInfoTitle[63] = L'\0';
    g_hubNid.szInfo[255] = L'\0';
    BOOL notified = Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);
    g_hubNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    return notified;
}

static void HubTrayPollAgentWait(void) {
    if (!g_hubTrayVisible) return;
    wchar_t notesCfg[MAX_PATH], waitId[80];
    HubTrayAgentNotesCfg(notesCfg, MAX_PATH);
    GetPrivateProfileStringW(L"HubRelay", L"WaitId", L"", waitId, 79, notesCfg);
    waitId[79] = L'\0';
    if (!waitId[0] || wcscmp(waitId, g_hubSeenAgentWait) == 0) return;

    /* Do not consume the token unless Explorer accepted the notification.
       The two-second poll will retry after transient shell/tray failures. */
    if (!HubTrayShowAgentWaitingAlert()) return;
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
    BOOL notified = Shell_NotifyIconW(NIM_MODIFY, &g_hubNid);
    g_hubNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    if (!notified) return;  /* leave the token pending for the next Hub poll */
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

static void HubTrayReleaseOwnership(void) {
    g_hubTrayVisible = FALSE;
    if (g_trayHostEvent) {
        CloseHandle(g_trayHostEvent);
        g_trayHostEvent = NULL;
    }
}

static void HubTrayRestore(void) {
    if (!g_hubNid.cbSize || g_hubQuitting) return;
    /* A delayed/duplicate TaskbarCreated or a transient modify failure can
       leave the same HWND/uID registered. Modify that icon rather than treating
       a duplicate NIM_ADD failure as proof that the tray is unavailable. */
    if (!Shell_NotifyIconW(NIM_ADD, &g_hubNid) &&
        !Shell_NotifyIconW(NIM_MODIFY, &g_hubNid)) {
        /* Do not claim the shared tray while Windows has no Hub icon. The
           independent apps will re-add their owner-accessible fallbacks. */
        HubTrayReleaseOwnership();
        return;
    }
    g_hubTrayVisible = TRUE;
    g_hubNid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_hubNid);
    /* Publish only after the shell accepted this icon, never on a timer alone. */
    HubTrayPublishHostMarker();
}

/* The two-second housekeeping timer must verify the shell, not just a stale
 * in-process BOOL. Explorer can lose an icon without a usable TaskbarCreated
 * broadcast (including resume/startup races). A lightweight callback-only
 * modify proves the existing identity is live without replaying a balloon.
 * Failed add/modify releases suite ownership; later polls retry indefinitely
 * while the Hub is running, but never after an explicit Quit/removal. */
static void HubTrayEnsureLive(void) {
    if (!g_hubNid.cbSize || g_hubQuitting) return;
    if (g_hubTrayVisible) {
        NOTIFYICONDATAW probe = g_hubNid;
        probe.uFlags = NIF_MESSAGE;
        if (!Shell_NotifyIconW(NIM_MODIFY, &probe)) HubTrayReleaseOwnership();
    }
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
       immediately if the Hub is explicitly exited or replaced by an update.
       Also try deleting an initialized icon whose last probe was inconclusive. */
    HubTrayReleaseOwnership();
    if (g_hubNid.cbSize) Shell_NotifyIconW(NIM_DELETE, &g_hubNid);
    ZeroMemory(&g_hubNid, sizeof(g_hubNid));
}

static HWND HubTrayAppWindow(const wchar_t *className) {
    return className ? FindWindowW(className, NULL) : NULL;
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
    for (int i = BUILTIN_CARD_COUNT; i < g_cardCount; i++)
        if (g_cards[i].exeName && _wcsicmp(g_cards[i].exeName, exeName) == 0)
            return i;
    return -1;
}

static void HubTrayOpenWeather(void) {
    int idx = HubTrayFindCardByExe(L"WeatherRadar.exe");
    if (idx >= 0) {
        HandleVerb(idx, L"open");
        return;
    }
    wchar_t weatherDir[MAX_PATH], weatherExe[MAX_PATH];
    SuiteAppPath(weatherDir, MAX_PATH, L"WeatherRadar");
    _snwprintf(weatherExe, MAX_PATH - 1, L"%s\\WeatherRadar.exe", weatherDir);
    weatherExe[MAX_PATH - 1] = L'\0';
    if (!HubOpenApp(weatherExe, NULL, weatherDir))
        SetFooterStatus(L"Weather Radar will appear after the next Hub refresh confirms its install.", 0);
}

static void HubTrayOpenApp(int id) {
    switch (id) {
    case ID_TRAY_NOTES_OPEN:
        HandleActionCommand(g_hwnd, ID_NOTES_OPEN);
        break;
    case ID_TRAY_BTG_OPEN:
        HandleActionCommand(g_hwnd, ID_BTG_OPEN);
        break;
    case ID_TRAY_KBL_OPEN:
        HandleActionCommand(g_hwnd, ID_KBL_OPEN);
        break;
    case ID_TRAY_DNS_OPEN:
        HandleActionCommand(g_hwnd, ID_DNS_OPEN);
        break;
    case ID_TRAY_AUDIO_OPEN:
        HandleActionCommand(g_hwnd, ID_AUDIOSWITCHER_OPEN);
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
    HMENU sub = HubTrayCreateMenu();
    if (!sub) return;
    if (!HubTrayMenuAdd(root, (UINT_PTR)sub, title, HUB_TRAY_MENU_TEXT,
                        FALSE, FALSE, TRUE, app != NULL, FALSE, TRUE)) {
        DestroyMenu(sub);
        return;
    }
    HubTrayMenuAdd(sub, openId, app ? L"Show window" : L"Open",
                   HUB_TRAY_MENU_TEXT, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    if (app) {
        HubTrayMenuAdd(sub, hideId, L"Hide window", HUB_TRAY_MENU_TEXT,
                       FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
        if (toggleId && toggleText)
            HubTrayMenuAdd(sub, toggleId, toggleText, HUB_TRAY_MENU_TEXT,
                           FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
        HubTrayMenuAdd(sub, 0, NULL, HUB_TRAY_MENU_SEPARATOR,
                       FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
        HubTrayMenuAdd(sub, quitId, L"Quit", HUB_TRAY_MENU_TEXT,
                       FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    }
}

static void HubTrayShowMenu(HWND hwnd) {
    /* TrackPopupMenu pumps the owner window's message queue. A second tray
       callback can therefore re-enter here while the first menu still uses
       the shared item array and background brush. Do not reset that live state. */
    if (g_hubTrayMenuTracking) return;
    g_hubTrayMenuTracking = TRUE;
    g_hubTrayMenuItemCount = 0;
    g_hubTrayMenuBrush = CreateSolidBrush(COL_CARD_BG);
    HMENU menu = HubTrayCreateMenu();
    if (!menu) {
        if (g_hubTrayMenuBrush) DeleteObject(g_hubTrayMenuBrush);
        g_hubTrayMenuBrush = NULL;
        g_hubTrayMenuItemCount = 0;
        g_hubTrayMenuTracking = FALSE;
        return;
    }
    HubTrayMenuAdd(menu, ID_TRAY_HUB_OPEN, L"Open Project Hub",
                   HUB_TRAY_MENU_TEXT, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE);
    HubTrayMenuAdd(menu, 0, NULL, HUB_TRAY_MENU_SEPARATOR,
                   FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    HubTrayMenuAdd(menu, 0, L"Suite apps", HUB_TRAY_MENU_SECTION,
                   FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
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
    HubTrayMenuAdd(menu, 0, NULL, HUB_TRAY_MENU_SEPARATOR,
                   FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    HubTrayMenuAdd(menu, ID_TRAY_NOTES_SOUND,
                   L"Play a sound for Agent Notes waiting alerts",
                   HUB_TRAY_MENU_TEXT, HubTrayAgentWaitSound(), FALSE,
                   FALSE, FALSE, FALSE, FALSE);
    HubTrayMenuAdd(menu, ID_TRAY_HUB_FOLDER, L"Open Managed Suite Folder",
                   HUB_TRAY_MENU_TEXT, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    HubTrayMenuAdd(menu, 0, NULL, HUB_TRAY_MENU_SEPARATOR,
                   FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    HubTrayMenuAdd(menu, ID_TRAY_HUB_EXIT,
                   L"Exit Project Hub (restore app tray icons)",
                   HUB_TRAY_MENU_TEXT, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    if (g_hubTrayMenuBrush) DeleteObject(g_hubTrayMenuBrush);
    g_hubTrayMenuBrush = NULL;
    g_hubTrayMenuItemCount = 0;
    g_hubTrayMenuTracking = FALSE;
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
