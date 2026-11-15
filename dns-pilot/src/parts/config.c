/* ---- config ---- */
static void LoadConfig(void) {
    wchar_t dir[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    wcsncat(dir, APP_DIR_NAME, MAX_PATH - wcslen(dir) - 1);
    CreateDirectoryW(dir, NULL);
    _snwprintf(g_cfgPath, MAX_PATH, L"%s\\config.ini", dir);
    g_auto = GetPrivateProfileIntW(L"Pilot", L"Auto", 0, g_cfgPath);
    g_intervalMin = GetPrivateProfileIntW(L"Pilot", L"IntervalMin", 30, g_cfgPath);
    if (g_intervalMin != 0 && g_intervalMin < 5) g_intervalMin = 5;
    {
        char tmp[64];
        wchar_t w[64];
        GetPrivateProfileStringW(L"Pilot", L"LastV4", L"", w, 64, g_cfgPath);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, tmp, 64, NULL, NULL);
        strncpy(g_lastV4, tmp, 63);
        GetPrivateProfileStringW(L"Pilot", L"LastV6", L"", w, 64, g_cfgPath);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, tmp, 64, NULL, NULL);
        strncpy(g_lastV6, tmp, 63);
    }
    strncpy(g_curV4, g_lastV4, 63);
    strncpy(g_curV6, g_lastV6, 63);
}

static void SaveConfig(void) {
    wchar_t w[64];
    wchar_t num[16];
    _snwprintf(num, 15, L"%d", g_auto);
    WritePrivateProfileStringW(L"Pilot", L"Auto", num, g_cfgPath);
    _snwprintf(num, 15, L"%d", g_intervalMin);
    WritePrivateProfileStringW(L"Pilot", L"IntervalMin", num, g_cfgPath);
    Widen(g_lastV4, w, 64);
    WritePrivateProfileStringW(L"Pilot", L"LastV4", w, g_cfgPath);
    Widen(g_lastV6, w, 64);
    WritePrivateProfileStringW(L"Pilot", L"LastV6", w, g_cfgPath);
}

