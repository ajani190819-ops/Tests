/* ---- Suite settings page -------------------------------------------
   One small window holding the settings that used to need each app's
   tray: Keyboard Lighting's M4/M5 hold behaviour and Smooth colour, and
   DNS Pilot's auto-pilot and check interval. The owner asked for a
   single place in the hub instead of hunting through tray menus, and
   chose this over per-app dialogs because the whole list is six
   controls.

   Every control writes the SAME file the app itself uses, in the same
   format, so there is one source of truth per setting and no second
   schema to drift. The lighting engine re-reads hold_mode.txt on every
   tick and reloads theme.json when it changes, so those two apply live;
   DNS Pilot reads its config when it starts, and the page says so
   instead of pretending otherwise. */

#define IDS_KBL_GROUP    2001
#define IDS_HOLD_TAP     2002
#define IDS_HOLD_LATCH   2003
#define IDS_HOLD_BURST   2004
#define IDS_SMOOTH       2005
#define IDS_HOTKEYS      2006
#define IDS_DNS_GROUP    2007
#define IDS_DNS_AUTO     2008
#define IDS_DNS_INT_LBL  2009
#define IDS_DNS_INT      2010
#define IDS_DNS_NOTE     2011
#define IDS_STATUS       2012

static HWND g_setHwnd    = NULL;
static HWND g_setHold[3];
static HWND g_setSmooth  = NULL;
static HWND g_setDnsAuto = NULL;
static HWND g_setDnsInt  = NULL;
static HWND g_setStatus  = NULL;

/* The three values the lighting engine accepts, verbatim. The engine
   validates hold_mode.txt against exactly this set, so the page cannot
   offer a mode that would silently do nothing; the tray submenu and the
   -HoldMode switch write the same three strings. */
static const char *kSetModes[3] = { "tap", "latch", "burst" };
static const int kSetIntervals[4] = { 15, 30, 60, 120 };

static void SetKblPath(wchar_t *out, int cap, const wchar_t *leaf) {
    _snwprintf(out, cap - 1, L"%s\\KeyboardLighting\\%s", g_localApp, leaf);
    out[cap - 1] = L'\0';
}

static void SetDnsCfgPath(wchar_t *out, int cap) {
    _snwprintf(out, cap - 1, L"%s\\DnsPilot\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static void SetStatusText(const wchar_t *t) {
    if (g_setStatus) SetWindowTextW(g_setStatus, t);
}

/* 0 = tap, 1 = latch, 2 = burst; missing or unreadable means the default. */
static int HoldModeCurrent(void) {
    wchar_t p[MAX_PATH];
    DWORD len = 0;
    SetKblPath(p, MAX_PATH, L"hold_mode.txt");
    char *txt = ReadTextFileUtf8(p, &len);
    int mode = 0;
    if (txt) {
        if (strstr(txt, "latch")) mode = 1;
        else if (strstr(txt, "burst")) mode = 2;
        free(txt);
    }
    return mode;
}

static BOOL ApplyHoldMode(int mode) {
    wchar_t p[MAX_PATH];
    SetKblPath(p, MAX_PATH, L"hold_mode.txt");
    return WriteBinaryFileAtomic(p, (const BYTE *)kSetModes[mode],
                                 (DWORD)strlen(kSetModes[mode]));
}

/* The panel always writes "Smooth": true|false into theme.json; read the
   value back rather than keeping a shadow copy that could disagree. */
static int SmoothCurrent(int *found) {
    wchar_t p[MAX_PATH];
    DWORD len = 0;
    SetKblPath(p, MAX_PATH, L"theme.json");
    char *txt = ReadTextFileUtf8(p, &len);
    int val = 1;
    *found = 0;
    if (txt) {
        char *k = strstr(txt, "\"Smooth\"");
        if (k) {
            char *c = strchr(k, ':');
            if (c) {
                while (*c == ':' || *c == ' ' || *c == '\t') c++;
                if (*c == 't' || *c == 'f') { *found = 1; val = (*c == 't'); }
            }
        }
        free(txt);
    }
    return val;
}

static BOOL ApplySmooth(int on) {
    wchar_t p[MAX_PATH];
    DWORD len = 0;
    SetKblPath(p, MAX_PATH, L"theme.json");
    char *txt = ReadTextFileUtf8(p, &len);
    if (!txt) return FALSE;
    char *k = strstr(txt, "\"Smooth\"");
    char *c = k ? strchr(k, ':') : NULL;
    if (c) {
        while (*c == ':' || *c == ' ' || *c == '\t') c++;
        if (*c != 't' && *c != 'f') c = NULL;
    }
    if (!c) { free(txt); return FALSE; }
    size_t head = (size_t)(c - txt);
    size_t oldLen = (*c == 't') ? 4 : 5;
    const char *rep = on ? "true" : "false";
    size_t repLen = strlen(rep);
    size_t newLen = len - oldLen + repLen;
    char *out = (char *)malloc(newLen + 1);
    if (!out) { free(txt); return FALSE; }
    memcpy(out, txt, head);
    memcpy(out + head, rep, repLen);
    memcpy(out + head + repLen, c + oldLen, len - head - oldLen);
    out[newLen] = '\0';
    BOOL ok = WriteBinaryFileAtomic(p, (const BYTE *)out, (DWORD)newLen);
    free(out);
    free(txt);
    return ok;
}

static void ModNamesW(int bits, wchar_t *out, int cap) {
    out[0] = L'\0';
    if (bits & 2) wcsncat(out, L"Ctrl+", (size_t)(cap - 1) - wcslen(out));
    if (bits & 1) wcsncat(out, L"Alt+",  (size_t)(cap - 1) - wcslen(out));
    if (bits & 4) wcsncat(out, L"Shift+", (size_t)(cap - 1) - wcslen(out));
    if (bits & 8) wcsncat(out, L"Win+",  (size_t)(cap - 1) - wcslen(out));
}

static void KeyNameW(int vk, wchar_t *out, int cap) {
    if (vk >= 0x70 && vk <= 0x7B)      _snwprintf(out, cap - 1, L"F%d", vk - 0x6F);
    else if (vk == 0x26)               _snwprintf(out, cap - 1, L"%s", L"Up Arrow");
    else if (vk == 0x28)               _snwprintf(out, cap - 1, L"%s", L"Down Arrow");
    else if (vk == 0x25)               _snwprintf(out, cap - 1, L"%s", L"Left Arrow");
    else if (vk == 0x27)               _snwprintf(out, cap - 1, L"%s", L"Right Arrow");
    else                               _snwprintf(out, cap - 1, L"key 0x%02X", vk);
    out[cap - 1] = L'\0';
}

static void ApplyDns(void) {
    wchar_t cfg[MAX_PATH], num[16];
    SetDnsCfgPath(cfg, MAX_PATH);
    int on = (SendMessageW(g_setDnsAuto, BM_GETCHECK, 0, 0) & BST_CHECKED) ? 1 : 0;
    LRESULT sel = SendMessageW(g_setDnsInt, CB_GETCURSEL, 0, 0);
    int mins = 30;
    if (sel != CB_ERR) mins = (int)SendMessageW(g_setDnsInt, CB_GETITEMDATA, (WPARAM)sel, 0);
    _snwprintf(num, 15, L"%d", mins);
    num[15] = L'\0';
    WritePrivateProfileStringW(L"Pilot", L"Auto", on ? L"1" : L"0", cfg);
    WritePrivateProfileStringW(L"Pilot", L"IntervalMin", num, cfg);
}

static HWND SetCtl(DWORD style, const wchar_t *cls, const wchar_t *text,
                   int x, int y, int w, int h, int id, HWND parent, int dpi) {
    return CreateWindowExW(0, cls, text, style | WS_CHILD | WS_VISIBLE,
                           MulDiv(x, dpi, 96), MulDiv(y, dpi, 96),
                           MulDiv(w, dpi, 96), MulDiv(h, dpi, 96),
                           parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (msg == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDS_HOLD_TAP || id == IDS_HOLD_LATCH || id == IDS_HOLD_BURST) {
            int mode = (id == IDS_HOLD_TAP) ? 0 : (id == IDS_HOLD_LATCH) ? 1 : 2;
            if (ApplyHoldMode(mode))
                SetStatusText(L"M4/M5 keys set - the engine picks it up immediately.");
            else
                SetStatusText(L"Could not save the M4/M5 setting - try again.");
            return 0;
        }
        if (id == IDS_SMOOTH) {
            int on = (SendMessageW(g_setSmooth, BM_GETCHECK, 0, 0) & BST_CHECKED) ? 1 : 0;
            if (ApplySmooth(on))
                SetStatusText(on ? L"Smooth colour on - applies immediately."
                                 : L"Smooth colour off - applies immediately.");
            else
                SetStatusText(L"theme.json has no Smooth entry yet - toggle it once in the panel first.");
            return 0;
        }
        if (id == IDS_DNS_AUTO || id == IDS_DNS_INT) {
            ApplyDns();
            SetStatusText(L"DNS Pilot saved - applies next time DNS Pilot starts.");
            return 0;
        }
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (msg == WM_DESTROY) { g_setHwnd = NULL; return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OpenSettingsWindow(HWND owner) {
    if (g_setHwnd) { SetForegroundWindow(g_setHwnd); return; }
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"HubSuiteSettingsWnd";
    RegisterClassExW(&wc);   /* second and later calls fail harmlessly */
    int dpi = GetWinDpi(NULL);
    int W = MulDiv(480, dpi, 96), H = MulDiv(408, dpi, 96);
    RECT rr;
    GetWindowRect(owner, &rr);
    int x = rr.left + ((rr.right - rr.left) - W) / 2;
    int y = rr.top + ((rr.bottom - rr.top) - H) / 2;
    g_setHwnd = CreateWindowExW(0, wc.lpszClassName, L"Suite Settings",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                x, y, W, H, owner, NULL, g_hInst, NULL);
    if (!g_setHwnd) return;

    int CW = 448, xL = 16;
    SetCtl(0, L"STATIC", L"Keyboard Lighting", xL, 14, CW, 18, IDS_KBL_GROUP, g_setHwnd, dpi);
    g_setHold[0] = SetCtl(BS_AUTORADIOBUTTON | WS_GROUP, L"BUTTON",
                          L"Sweep while held (needs the M4/M5 keys recognised)",
                          xL + 8, 38, CW - 8, 20, IDS_HOLD_TAP, g_setHwnd, dpi);
    g_setHold[1] = SetCtl(BS_AUTORADIOBUTTON, L"BUTTON",
                          L"Press to glide, press to stop",
                          xL + 8, 62, CW - 8, 20, IDS_HOLD_LATCH, g_setHwnd, dpi);
    g_setHold[2] = SetCtl(BS_AUTORADIOBUTTON, L"BUTTON",
                          L"Press for a 20% glide",
                          xL + 8, 86, CW - 8, 20, IDS_HOLD_BURST, g_setHwnd, dpi);
    g_setSmooth = SetCtl(BS_AUTOCHECKBOX, L"BUTTON",
                         L"Smooth colour (steady steps instead of fine dithering)",
                         xL, 114, CW, 20, IDS_SMOOTH, g_setHwnd, dpi);

    wchar_t ini[MAX_PATH], line[192];
    SetKblPath(ini, MAX_PATH, L"presets.ini");
    int um = GetPrivateProfileIntW(L"Keys", L"UpMod", 3, ini);
    int uk = GetPrivateProfileIntW(L"Keys", L"UpKey", 0x26, ini);
    int dm = GetPrivateProfileIntW(L"Keys", L"DownMod", 3, ini);
    int dk = GetPrivateProfileIntW(L"Keys", L"DownKey", 0x28, ini);
    wchar_t mu[40], ku[24], md[40], kd[24];
    ModNamesW(um, mu, 40); KeyNameW(uk, ku, 24);
    ModNamesW(dm, md, 40); KeyNameW(dk, kd, 24);
    _snwprintf(line, 191, L"Brightness hotkeys: %s%s / %s%s  (presets.ini, applies on restart)",
               mu, ku, md, kd);
    line[191] = L'\0';
    SetCtl(0, L"STATIC", line, xL, 142, CW, 32, IDS_HOTKEYS, g_setHwnd, dpi);

    SetCtl(0, L"STATIC", L"DNS Pilot", xL, 184, CW, 18, IDS_DNS_GROUP, g_setHwnd, dpi);
    g_setDnsAuto = SetCtl(BS_AUTOCHECKBOX, L"BUTTON",
                          L"Auto-pilot: switch to the fastest DNS on its own",
                          xL, 208, CW, 20, IDS_DNS_AUTO, g_setHwnd, dpi);
    SetCtl(0, L"STATIC", L"Check every", xL, 236, 80, 20, IDS_DNS_INT_LBL, g_setHwnd, dpi);
    g_setDnsInt = SetCtl(CBS_DROPDOWNLIST | WS_TABSTOP, L"COMBOBOX", L"",
                         xL + 84, 232, 130, 120, IDS_DNS_INT, g_setHwnd, dpi);
    SetCtl(0, L"STATIC", L"DNS Pilot reads these when it starts, so restart it to apply.",
           xL, 262, CW, 18, IDS_DNS_NOTE, g_setHwnd, dpi);

    g_setStatus = SetCtl(0, L"STATIC",
                         L"Changes save as soon as you make them.",
                         xL, 330, CW, 34, IDS_STATUS, g_setHwnd, dpi);

    int mode = HoldModeCurrent();
    for (int i = 0; i < 3; i++)
        SendMessageW(g_setHold[i], BM_SETCHECK, i == mode ? BST_CHECKED : BST_UNCHECKED, 0);
    int found = 0;
    int smooth = SmoothCurrent(&found);
    SendMessageW(g_setSmooth, BM_SETCHECK, smooth ? BST_CHECKED : BST_UNCHECKED, 0);

    wchar_t cfg[MAX_PATH];
    SetDnsCfgPath(cfg, MAX_PATH);
    int autoOn = GetPrivateProfileIntW(L"Pilot", L"Auto", 0, cfg);
    int imin = GetPrivateProfileIntW(L"Pilot", L"IntervalMin", 30, cfg);
    SendMessageW(g_setDnsAuto, BM_SETCHECK, autoOn ? BST_CHECKED : BST_UNCHECKED, 0);
    int selIdx = -1;
    for (int i = 0; i < 4; i++) {
        wchar_t t[32];
        _snwprintf(t, 31, L"%d minutes", kSetIntervals[i]);
        t[31] = L'\0';
        LRESULT idx = SendMessageW(g_setDnsInt, CB_ADDSTRING, 0, (LPARAM)t);
        SendMessageW(g_setDnsInt, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)kSetIntervals[i]);
        if (kSetIntervals[i] == imin) selIdx = (int)idx;
    }
    if (selIdx < 0) {
        wchar_t t[32];
        _snwprintf(t, 31, L"%d minutes", imin);
        t[31] = L'\0';
        LRESULT idx = SendMessageW(g_setDnsInt, CB_ADDSTRING, 0, (LPARAM)t);
        SendMessageW(g_setDnsInt, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)imin);
        selIdx = (int)idx;
    }
    SendMessageW(g_setDnsInt, CB_SETCURSEL, (WPARAM)selIdx, 0);

    ShowWindow(g_setHwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_setHwnd);
}
