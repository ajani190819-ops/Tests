/* ---- named colour presets ---------------------------------------------
 * Eight named snapshots of the whole look (both zones + master). The store
 * is a plain Win32 INI file driven entirely through Get/WritePrivateProfile
 * - there is no parser of ours here to run off the end of anything. The
 * v2.7.0 JSON preset layer crashed for keeps; this cannot.
 * One click on Pattern swaps the entire look; Save snapshots it.
 */
#define PRESET_MAX 8
static wchar_t g_presetName[PRESET_MAX][32];
static BOOL    g_presetUsed[PRESET_MAX];
static int     g_presetIdx = -1;

static void PresetPath(wchar_t *out) {
    _snwprintf(out, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
}

static COLORREF ParseHexW(const wchar_t *p) {
    unsigned v = 0;
    int n = 0;
    while (n < 6) {
        wchar_t c = *p++;
        int d;
        if (c >= L'0' && c <= L'9') d = (int)(c - L'0');
        else if (c >= L'a' && c <= L'f') d = (int)(c - L'a') + 10;
        else if (c >= L'A' && c <= L'F') d = (int)(c - L'A') + 10;
        else break;
        v = (v << 4) | (unsigned)d;
        n++;
    }
    if (n != 6) return RGB(56, 189, 248);
    return RGB((v >> 16) & 255, (v >> 8) & 255, v & 255);
}

static void SaveGroupPreset(const wchar_t *sec, const GroupCfg *g, const wchar_t *path) {
    wchar_t v[24], sw[200];
    size_t o = 0;
    sw[0] = L'\0';
    for (int i = 0; i < g->swatchCount && i < 8; i++) {
        int n = _snwprintf(sw + o, 200 - o, L"%s%02X%02X%02X",
                           i ? L"|" : L"",
                           (unsigned)GetRValue(g->swatches[i]),
                           (unsigned)GetGValue(g->swatches[i]),
                           (unsigned)GetBValue(g->swatches[i]));
        if (n <= 0) break;
        o += (size_t)n;
        if (o >= 190) break;
    }
    WritePrivateProfileStringW(sec, L"On", g->on ? L"1" : L"0", path);
    _snwprintf(v, 24, L"%d", g->effectIdx);  WritePrivateProfileStringW(sec, L"Effect", v, path);
    _snwprintf(v, 24, L"%d", g->speed);      WritePrivateProfileStringW(sec, L"Speed", v, path);
    _snwprintf(v, 24, L"%d", g->brightness); WritePrivateProfileStringW(sec, L"Bright", v, path);
    _snwprintf(v, 24, L"%d", g->equalise ? 1 : 0); WritePrivateProfileStringW(sec, L"Eq", v, path);
    _snwprintf(v, 24, L"%d", g->loop ? 1 : 0);     WritePrivateProfileStringW(sec, L"Loop", v, path);
    _snwprintf(v, 24, L"%d", g->mirror ? 1 : 0);   WritePrivateProfileStringW(sec, L"Mirror", v, path);
    _snwprintf(v, 24, L"%d", g->reverse ? 1 : 0);  WritePrivateProfileStringW(sec, L"Rev", v, path);
    WritePrivateProfileStringW(sec, L"Swatches", sw, path);
}

static void LoadGroupPreset(const wchar_t *sec, GroupCfg *g, const wchar_t *path) {
    wchar_t v[24], sw[200];
    GetPrivateProfileStringW(sec, L"On", g->on ? L"1" : L"0", v, 24, path);
    g->on = (v[0] == L'1');
    GetPrivateProfileStringW(sec, L"Effect", L"", v, 24, path);
    if (v[0]) {
        int e = _wtoi(v);
        if (e < 0) e = 0;
        if (e >= EFFECT_COUNT) e = EFFECT_COUNT - 1;
        g->effectIdx = e;
    }
    GetPrivateProfileStringW(sec, L"Speed", L"", v, 24, path);
    if (v[0]) { g->speed = _wtoi(v); if (g->speed < 1) g->speed = 1; if (g->speed > 50) g->speed = 50; }
    GetPrivateProfileStringW(sec, L"Bright", L"", v, 24, path);
    if (v[0]) { g->brightness = _wtoi(v); if (g->brightness < 0) g->brightness = 0; if (g->brightness > 100) g->brightness = 100; }
    GetPrivateProfileStringW(sec, L"Eq", L"", v, 24, path);
    if (v[0]) g->equalise = (v[0] == L'1');
    GetPrivateProfileStringW(sec, L"Loop", L"", v, 24, path);
    if (v[0]) g->loop = (v[0] == L'1');
    GetPrivateProfileStringW(sec, L"Mirror", L"", v, 24, path);
    if (v[0]) g->mirror = (v[0] == L'1');
    GetPrivateProfileStringW(sec, L"Rev", L"", v, 24, path);
    if (v[0]) g->reverse = (v[0] == L'1');
    GetPrivateProfileStringW(sec, L"Swatches", L"", sw, 200, path);
    if (sw[0]) {
        int n = 0;
        wchar_t *q = sw;
        while (n < 8 && *q) {
            g->swatches[n++] = ParseHexW(q);
            while (*q && *q != L'|') q++;
            if (*q == L'|') q++;
        }
        if (n >= 1) g->swatchCount = n;
    }
}

static void LoadPresetIndex(void) {
    wchar_t path[MAX_PATH];
    PresetPath(path);
    for (int i = 0; i < PRESET_MAX; i++) {
        wchar_t key[8];
        _snwprintf(key, 8, L"Name%d", i + 1);
        GetPrivateProfileStringW(L"Names", key, L"", g_presetName[i], 32, path);
        g_presetUsed[i] = g_presetName[i][0] != L'\0';
    }
}

static void SyncLinkedZone(void);
static void FlushLiveApply(void);
static void SavePanelCfg(void);
static void ScheduleLiveApply(void);

static void SavePresetSlot(int idx) {
    wchar_t path[MAX_PATH], sec[16], key[8], v[24];
    if (idx < 0) idx = 0;
    if (idx >= PRESET_MAX) idx = PRESET_MAX - 1;
    PresetPath(path);
    if (!g_presetName[idx][0])
        _snwprintf(g_presetName[idx], 32, L"Custom %d", idx + 1);
    _snwprintf(key, 8, L"Name%d", idx + 1);
    WritePrivateProfileStringW(L"Names", key, g_presetName[idx], path);
    _snwprintf(sec, 16, L"Kbd%d", idx + 1); SaveGroupPreset(sec, &g_cfg.kbd, path);
    _snwprintf(sec, 16, L"Bar%d", idx + 1); SaveGroupPreset(sec, &g_cfg.bar, path);
    _snwprintf(sec, 16, L"P%d", idx + 1);
    _snwprintf(v, 24, L"%d", g_cfg.masterBrightness);
    WritePrivateProfileStringW(sec, L"Master", v, path);
    g_presetUsed[idx] = TRUE;
    g_presetIdx = idx;
}

static void ApplyPresetSlot(int idx) {
    wchar_t path[MAX_PATH], sec[16], v[24];
    if (idx < 0 || idx >= PRESET_MAX) return;
    PresetPath(path);
    _snwprintf(sec, 16, L"Kbd%d", idx + 1); LoadGroupPreset(sec, &g_cfg.kbd, path);
    _snwprintf(sec, 16, L"Bar%d", idx + 1); LoadGroupPreset(sec, &g_cfg.bar, path);
    _snwprintf(sec, 16, L"P%d", idx + 1);
    GetPrivateProfileStringW(sec, L"Master", L"100", v, 24, path);
    int m = _wtoi(v);
    if (m < 0) m = 0;
    if (m > 100) m = 100;
    g_cfg.masterBrightness = m;
    g_presetIdx = idx;
    SyncLinkedZone();
    FlushLiveApply();
    _snwprintf(g_toastText, 95, L"Preset \x00B7 %s", g_presetName[idx]);
    g_toastAt = GetTickCount64() + 1400ULL;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void DeletePresetSlot(int idx) {
    wchar_t path[MAX_PATH], sec[16], key[8];
    if (idx < 0 || idx >= PRESET_MAX) return;
    PresetPath(path);
    _snwprintf(key, 8, L"Name%d", idx + 1);
    WritePrivateProfileStringW(L"Names", key, NULL, path);
    _snwprintf(sec, 16, L"Kbd%d", idx + 1); WritePrivateProfileStringW(sec, NULL, NULL, path);
    _snwprintf(sec, 16, L"Bar%d", idx + 1); WritePrivateProfileStringW(sec, NULL, NULL, path);
    _snwprintf(sec, 16, L"P%d", idx + 1);   WritePrivateProfileStringW(sec, NULL, NULL, path);
    g_presetName[idx][0] = L'\0';
    g_presetUsed[idx] = FALSE;
    if (g_presetIdx == idx) g_presetIdx = -1;
}

/* Pattern picker: the same native popup menu the effect list uses. Every
 * saved pattern is one row, one click swaps the whole look. */
static void OpenPatternPopup(HWND hwnd, RECT anchor) {
    HMENU hMenu = CreatePopupMenu();
    int used = 0;
    for (int i = 0; i < PRESET_MAX; i++) {
        if (!g_presetUsed[i]) continue;
        AppendMenuW(hMenu, MF_STRING | (g_presetIdx == i ? MF_CHECKED : 0),
                    (UINT_PTR)(IDM_PAT_BASE + i), g_presetName[i]);
        used++;
    }
    if (used == 0)
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"No saved patterns yet");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_PAT_NEW, L"Save current as new pattern");
    if (g_presetIdx >= 0 && g_presetUsed[g_presetIdx]) {
        wchar_t lbl[48];
        _snwprintf(lbl, 48, L"Delete: %s", g_presetName[g_presetIdx]);
        AppendMenuW(hMenu, MF_STRING, IDM_PAT_DEL, lbl);
    }
    POINT pt = { anchor.left, anchor.bottom + 2 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

