/* ---------------- alerts: exact-place rain watcher ----------------

   Native Shell notification-area delivery keeps the watcher running after the
   window is closed. It never uses the regional coverage-blended PoP: only the
   same ensemble window shown in the rain card can trigger an alert. */

static int RainAlertPlaceMatches(const Place *p) {
    return p && !isnan(g_rainAlertLat) && !isnan(g_rainAlertLon) &&
           fabs(g_rainAlertLat - p->lat) < 0.00001 &&
           fabs(g_rainAlertLon - p->lon) < 0.00001;
}

static ULONGLONG UnixNowSeconds(void) {
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (u.QuadPart - 116444736000000000ULL) / 10000000ULL;
}

static void LoadRainAlertState(void) {
    wchar_t num[64];
    g_rainAlertEpisode = GetPrivateProfileIntW(L"RainAlert", L"Episode", 0, g_cfgPath) ? 1 : 0;
    GetPrivateProfileStringW(L"RainAlert", L"LastEpoch", L"0", num, 63, g_cfgPath);
    g_rainAlertLastEpoch = wcstoull(num, NULL, 10);
    GetPrivateProfileStringW(L"RainAlert", L"Lat", L"nan", num, 63, g_cfgPath);
    g_rainAlertLat = wcstod(num, NULL);
    GetPrivateProfileStringW(L"RainAlert", L"Lon", L"nan", num, 63, g_cfgPath);
    g_rainAlertLon = wcstod(num, NULL);
}

static void SaveRainAlertState(void) {
    wchar_t num[64];
    WritePrivateProfileStringW(L"RainAlert", L"Episode",
                               g_rainAlertEpisode ? L"1" : L"0", g_cfgPath);
    _snwprintf(num, 63, L"%llu", (unsigned long long)g_rainAlertLastEpoch);
    num[63] = L'\0';
    WritePrivateProfileStringW(L"RainAlert", L"LastEpoch", num, g_cfgPath);
    _snwprintf(num, 63, L"%.5f", g_rainAlertLat);
    num[63] = L'\0';
    WritePrivateProfileStringW(L"RainAlert", L"Lat", num, g_cfgPath);
    _snwprintf(num, 63, L"%.5f", g_rainAlertLon);
    num[63] = L'\0';
    WritePrivateProfileStringW(L"RainAlert", L"Lon", num, g_cfgPath);
}

static int ForecastIsRainingNow(const Forecast *f) {
    int code = f ? f->wcode : 0;
    if (f && f->precipIn >= RAIN_ALERT_TRACE_IN) return 1;
    return code == 51 || code == 53 || code == 55 || code == 56 || code == 57 ||
           code == 61 || code == 63 || code == 65 || code == 66 || code == 67 ||
           code == 80 || code == 81 || code == 82;
}

static void WeatherTraySync(void) {
    int wantOwnIcon = !SuiteHubOwnsTray();
    if (wantOwnIcon && !g_trayVisible) {
        if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
            g_trayVisible = 1;
            g_nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
        }
    } else if (!wantOwnIcon && g_trayVisible) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_trayVisible = 0;
    }
}

static void WeatherTrayInit(void) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
    wcsncpy(g_nid.szTip, L"Weather Report - exact-place rain watch", 127);
    g_nid.szTip[127] = L'\0';
    WeatherTraySync();
}

static void WeatherTrayRemove(void) {
    if (g_trayVisible) Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_trayVisible = 0;
    ZeroMemory(&g_nid, sizeof(g_nid));
}

static void ShowWeatherTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open Weather Report");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

static void WeatherTrayHintOnce(void) {
    if (g_trayHinted || !g_nid.cbSize || !g_trayVisible) return;
    g_trayHinted = 1;
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_INFO;
    wcsncpy(g_nid.szInfoTitle, L"Weather Report keeps watching", 63);
    wcsncpy(g_nid.szInfo, L"It will notify you when the selected place is likely to get rain. Right-click its notification-area icon to exit.", 255);
    g_nid.szInfoTitle[63] = L'\0';
    g_nid.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void PublishHubRainAlert(const Place *p, double odds) {
    wchar_t token[64], title[64], body[256];
    _snwprintf(token, 63, L"%llu", (unsigned long long)UnixNowSeconds());
    _snwprintf(title, 63, L"Rain likely near %s", p->name);
    _snwprintf(body, 255,
               L"Exact-location ensemble chance: %d%% within the next %d hours. Open Weather Report for details.",
               (int)(odds * 100.0 + 0.5), RAIN_ALERT_HOURS);
    token[63] = title[63] = body[255] = L'\0';
    /* Commit the token last: Hub ignores an alert until the title and body are
       both already durable, so it cannot relay a half-written notification. */
    WritePrivateProfileStringW(L"HubRelay", L"RainTitle", title, g_cfgPath);
    WritePrivateProfileStringW(L"HubRelay", L"RainBody", body, g_cfgPath);
    WritePrivateProfileStringW(L"HubRelay", L"RainToken", token, g_cfgPath);
}

static void ShowRainAlert(const Place *p, double odds) {
    if (!g_trayVisible && SuiteHubOwnsTray()) {
        PublishHubRainAlert(p, odds);
        return;
    }
    if (!g_nid.cbSize || !g_trayVisible) return;
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_WARNING;
    _snwprintf(g_nid.szInfoTitle, 63, L"Rain likely near %s", p->name);
    _snwprintf(g_nid.szInfo, 255,
               L"Exact-location ensemble chance: %d%% within the next %d hours. Weather Report will not repeat this while this rain episode continues.",
               (int)(odds * 100.0 + 0.5), RAIN_ALERT_HOURS);
    g_nid.szInfoTitle[63] = L'\0';
    g_nid.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void RainAlertRemember(const Place *p, int episode, int notified) {
    g_rainAlertEpisode = episode;
    g_rainAlertLat = p->lat;
    g_rainAlertLon = p->lon;
    if (notified) g_rainAlertLastEpoch = UnixNowSeconds();
    SaveRainAlertState();
}

static void RainAlertConsider(const Place *p) {
    if (!p || g_fc.have < 2 || !g_ens.have) return;
    int samePlace = RainAlertPlaceMatches(p);
    int raining = ForecastIsRainingNow(&g_fc);
    double odds = EnsWindowProb(&g_ens, 0, RAIN_ALERT_HOURS);
    int likely = !isnan(odds) && odds >= RAIN_ALERT_PROB;

    /* Current rain starts or continues the episode silently. This means a
       restart during a shower cannot turn into a fresh "about to rain" alert. */
    if (raining) {
        if (!samePlace || !g_rainAlertEpisode) RainAlertRemember(p, 1, 0);
        return;
    }
    if (!likely) {
        if (samePlace && g_rainAlertEpisode) RainAlertRemember(p, 0, 0);
        return;
    }
    if (samePlace && g_rainAlertEpisode) return;

    ULONGLONG now = UnixNowSeconds();
    if (samePlace && g_rainAlertLastEpoch &&
        now >= g_rainAlertLastEpoch &&
        now - g_rainAlertLastEpoch < RAIN_ALERT_COOLDOWN_SEC) {
        RainAlertRemember(p, 1, 0);
        return;
    }
    ShowRainAlert(p, odds);
    RainAlertRemember(p, 1, 1);
}
