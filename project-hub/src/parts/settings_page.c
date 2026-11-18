/* ---- Suite settings page -------------------------------------------
   One polished place for the settings that used to need each app's tray:
   Keyboard Lighting hold behaviour, DNS Pilot scheduling, and the Hub's
   branch-following preference. Each control writes the file or API that
   the owning app already uses; there is no shadow settings schema.

   Keyboard Lighting and branch-following changes apply immediately. DNS
   Pilot reads its INI at startup, so the page says that restart is needed. */

#define IDS_SET_TITLE      2001
#define IDS_SET_SUBTITLE   2002
#define IDS_KBL_GROUP      2003
#define IDS_HOLD_TAP       2004
#define IDS_HOLD_LATCH     2005
#define IDS_HOLD_BURST     2006
#define IDS_HOLD_STATE     2007
#define IDS_DNS_GROUP      2008
#define IDS_DNS_AUTO       2009
#define IDS_DNS_INT_LBL    2010
#define IDS_DNS_INT        2011
#define IDS_DNS_NOTE       2012
#define IDS_BRANCH_GROUP   2013
#define IDS_FOLLOW_LATEST  2014
#define IDS_BRANCH_NOTE    2015
#define IDS_STATUS         2016
#define IDS_SMOOTH         2017
#define IDS_HOTKEYS        2018
#define IDS_BALANCE        2019

#define SET_STATUS_INFO   0
#define SET_STATUS_OK     1
#define SET_STATUS_ERROR  2

static HWND g_setHwnd       = NULL;
static HWND g_setHold[3];
static HWND g_setHoldState  = NULL;
static HWND g_setSmooth    = NULL;
static HWND g_setBalance   = NULL;
static HWND g_setHotkeys   = NULL;
static HWND g_setDnsAuto    = NULL;
static HWND g_setDnsInt     = NULL;
static HWND g_setAutoBranch = NULL;
static HWND g_setStatus     = NULL;
static HBRUSH g_setBgBrush  = NULL;
static HBRUSH g_setCardBrush = NULL;
static HBRUSH g_setAccentBrush = NULL;
static int g_setStatusTone  = SET_STATUS_INFO;

/* The three values the lighting engine accepts, verbatim. */
static const char *kSetModes[3] = { "tap", "latch", "burst" };
/* Keep these in lockstep with DNS Pilot's interval-cycle steps. */
static const int kSetIntervals[6] = { 0, 15, 30, 60, 120, 300 };

static int SetScale(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

static void SetKblPath(wchar_t *out, int cap, const wchar_t *leaf) {
    _snwprintf(out, cap - 1, L"%s\\KeyboardLighting\\%s", g_localApp, leaf);
    out[cap - 1] = L'\0';
}

static void SetDnsCfgPath(wchar_t *out, int cap) {
    _snwprintf(out, cap - 1, L"%s\\DnsPilot\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static void SetStatusWithTone(const wchar_t *text, int tone) {
    g_setStatusTone = tone;
    if (g_setStatus) {
        SetWindowTextW(g_setStatus, text);
        InvalidateRect(g_setStatus, NULL, TRUE);
    }
}

static void SetStatusText(const wchar_t *text) {
    SetStatusWithTone(text, SET_STATUS_INFO);
}

static void SetStatusOk(const wchar_t *text) {
    SetStatusWithTone(text, SET_STATUS_OK);
}

static void SetStatusError(const wchar_t *text) {
    SetStatusWithTone(text, SET_STATUS_ERROR);
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
    if (mode < 0 || mode >= 3) return FALSE;
    SetKblPath(p, MAX_PATH, L"hold_mode.txt");
    return WriteBinaryFileAtomic(p, (const BYTE *)kSetModes[mode],
                                 (DWORD)strlen(kSetModes[mode]));
}

/* Keyboard Lighting treats panel.json as the saved preference and
   theme.json as the live engine inbox. Update both so a Hub change applies now
   and survives the tray's next restart. The app writes flat JSON objects for
   the root and the Kbd group; these small helpers preserve every other byte. */
static BOOL JsonSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *SkipJsonString(char *p, char *end) {
    if (p >= end || *p != '"') return NULL;
    BOOL escaped = FALSE;
    for (p++; p < end; p++) {
        if (escaped) escaped = FALSE;
        else if (*p == '\\') escaped = TRUE;
        else if (*p == '"') return p + 1;
    }
    return NULL;
}

static char *SkipJsonValue(char *p, char *end) {
    if (p >= end) return NULL;
    if (*p == '"') return SkipJsonString(p, end);
    if (*p == '{' || *p == '[') {
        int objects = 0, arrays = 0;
        BOOL inString = FALSE, escaped = FALSE;
        for (; p < end; p++) {
            char c = *p;
            if (inString) {
                if (escaped) escaped = FALSE;
                else if (c == '\\') escaped = TRUE;
                else if (c == '"') inString = FALSE;
                continue;
            }
            if (c == '"') inString = TRUE;
            else if (c == '{') objects++;
            else if (c == '[') arrays++;
            else if (c == '}' && --objects == 0 && arrays == 0) return p + 1;
            else if (c == ']' && --arrays == 0 && objects == 0) return p + 1;
        }
        return NULL;
    }
    while (p < end && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* Find one direct property in a JSON object whose braces are supplied by the
   caller. Values are skipped with string/array/object awareness, so a nested
   Bar.Equalise can never be mistaken for the keyboard's Kbd.Equalise. */
static char *FindJsonValue(char *objectOpen, char *objectClose, const char *name) {
    if (!objectOpen || !objectClose || objectClose <= objectOpen || *objectOpen != '{')
        return NULL;
    size_t nameLen = strlen(name);
    char *p = objectOpen + 1;
    while (p < objectClose) {
        while (p < objectClose && (JsonSpace(*p) || *p == ',')) p++;
        if (p >= objectClose) return NULL;
        char *keyStart = p;
        char *keyAfter = SkipJsonString(p, objectClose);
        if (!keyAfter) return NULL;
        char *keyEnd = keyAfter - 1;
        BOOL match = (size_t)(keyEnd - (keyStart + 1)) == nameLen &&
                     memcmp(keyStart + 1, name, nameLen) == 0;
        p = keyAfter;
        while (p < objectClose && JsonSpace(*p)) p++;
        if (p >= objectClose || *p != ':') return NULL;
        p++;
        while (p < objectClose && JsonSpace(*p)) p++;
        if (p >= objectClose) return NULL;
        if (match) return p;
        p = SkipJsonValue(p, objectClose);
        if (!p) return NULL;
    }
    return NULL;
}

static BOOL JsonRootSpan(char *json, char *end, char **open, char **close) {
    char *p = json;
    if (end - p >= 3 && (unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3; /* Windows PowerShell 5.1 may write a UTF-8 BOM. */
    while (p < end && JsonSpace(*p)) p++;
    if (p >= end || *p != '{') return FALSE;

    int depth = 0;
    BOOL inString = FALSE, escaped = FALSE;
    *open = p;
    for (; p < end; p++) {
        char c = *p;
        if (inString) {
            if (escaped) escaped = FALSE;
            else if (c == '\\') escaped = TRUE;
            else if (c == '"') inString = FALSE;
            continue;
        }
        if (c == '"') inString = TRUE;
        else if (c == '{') depth++;
        else if (c == '}' && --depth == 0) {
            *close = p;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL JsonObjectSpan(char *json, char *end, const char *name,
                           char **open, char **close) {
    char *rootOpen, *rootClose;
    if (!JsonRootSpan(json, end, &rootOpen, &rootClose)) return FALSE;
    char *value = FindJsonValue(rootOpen, rootClose, name);
    if (!value || *value != '{') return FALSE;
    int depth = 0;
    BOOL inString = FALSE, escaped = FALSE;
    *open = value;
    for (char *p = value; p < end; p++) {
        char c = *p;
        if (inString) {
            if (escaped) escaped = FALSE;
            else if (c == '\\') escaped = TRUE;
            else if (c == '"') inString = FALSE;
            continue;
        }
        if (c == '"') inString = TRUE;
        else if (c == '{') depth++;
        else if (c == '}' && --depth == 0) {
            *close = p;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL JsonReadBool(char *objectOpen, char *objectClose, const char *name, int *out) {
    char *value = FindJsonValue(objectOpen, objectClose, name);
    if (!value) return FALSE;
    size_t remain = (size_t)(objectClose - value);
    size_t tokenLen;
    int parsed;
    if (remain >= 4 && memcmp(value, "true", 4) == 0) {
        parsed = 1; tokenLen = 4;
    } else if (remain >= 5 && memcmp(value, "false", 5) == 0) {
        parsed = 0; tokenLen = 5;
    } else return FALSE;
    char *after = value + tokenLen;
    while (after < objectClose && JsonSpace(*after)) after++;
    if (after < objectClose && *after != ',' && *after != '}' && *after != ']') return FALSE;
    if (out) *out = parsed;
    return TRUE;
}

static BOOL JsonReadSetting(char *json, DWORD len, const char *objectName,
                            const char *memberName, int *out) {
    char *end = json + len, *rootOpen, *rootClose, *open, *close;
    if (!JsonRootSpan(json, end, &rootOpen, &rootClose)) return FALSE;
    if (objectName && JsonObjectSpan(json, end, objectName, &open, &close) &&
        JsonReadBool(open, close, memberName, out))
        return TRUE;
    /* Older single-zone Keyboard Lighting files stored Equalise at the root. */
    return JsonReadBool(rootOpen, rootClose, memberName, out);
}

static int KeyboardSettingCurrent(const char *objectName, const char *memberName,
                                  int fallback) {
    const wchar_t *files[2] = { L"panel.json", L"theme.json" };
    for (int i = 0; i < 2; i++) {
        wchar_t path[MAX_PATH];
        DWORD len = 0;
        int value;
        char *txt;
        SetKblPath(path, MAX_PATH, files[i]);
        txt = ReadTextFileUtf8(path, &len);
        if (!txt) continue;
        BOOL found = JsonReadSetting(txt, len, objectName, memberName, &value);
        free(txt);
        if (found) return value;
    }
    return fallback;
}

static BOOL AddJsonEntry(char *json, DWORD len, char *objectOpen, char *objectClose,
                         const char *entry, char **out, DWORD *outLen) {
    char *insert = objectClose;
    while (insert > objectOpen + 1 && JsonSpace(insert[-1])) insert--;
    size_t head = (size_t)(insert - json);
    size_t entryLen = strlen(entry);
    BOOL hasEntries = (insert > objectOpen + 1);
    size_t newLen = (size_t)len + (hasEntries ? 1u : 0u) + entryLen;
    if (newLen > 0xFFFFFFFFUL) return FALSE;
    char *updated = (char *)malloc(newLen + 1);
    if (!updated) return FALSE;
    size_t at = head;
    memcpy(updated, json, head);
    if (hasEntries) updated[at++] = ',';
    memcpy(updated + at, entry, entryLen);
    at += entryLen;
    memcpy(updated + at, json + head, (size_t)len - head);
    at += (size_t)len - head;
    updated[at] = '\0';
    *out = updated;
    *outLen = (DWORD)at;
    return TRUE;
}

static BOOL BuildJsonBoolUpdate(const char *source, DWORD sourceLen,
                                const char *objectName, const char *memberName,
                                int on, char **out, DWORD *outLen) {
    *out = NULL;
    *outLen = 0;
    DWORD len = source ? sourceLen : 2;
    char *json = (char *)malloc((size_t)len + 1);
    if (!json) return FALSE;
    if (source) memcpy(json, source, len);
    else { json[0] = '{'; json[1] = '}'; }
    json[len] = '\0';

    char *end = json + len, *rootOpen, *rootClose;
    if (!JsonRootSpan(json, end, &rootOpen, &rootClose)) { free(json); return FALSE; }
    char *scopeOpen = rootOpen, *scopeClose = rootClose;
    if (objectName && objectName[0]) {
        char *objectOpen, *objectClose;
        if (JsonObjectSpan(json, end, objectName, &objectOpen, &objectClose)) {
            scopeOpen = objectOpen;
            scopeClose = objectClose;
        } else if (!FindJsonValue(rootOpen, rootClose, memberName) &&
                   !FindJsonValue(rootOpen, rootClose, "Effect")) {
            /* No group means a newer/empty config; create just that group
               rather than writing a Kbd-only setting into the shared root. */
            char entry[128];
            _snprintf(entry, sizeof(entry) - 1, "\"%s\": {\"%s\": %s}",
                      objectName, memberName, on ? "true" : "false");
            entry[sizeof(entry) - 1] = '\0';
            BOOL ok = AddJsonEntry(json, len, rootOpen, rootClose,
                                   entry, out, outLen);
            free(json);
            return ok;
        }
        /* Otherwise this is a legacy root-level Equalise value. */
    }

    char *value = FindJsonValue(scopeOpen, scopeClose, memberName);
    if (value) {
        size_t remain = (size_t)(scopeClose - value);
        size_t oldLen;
        if (remain >= 4 && memcmp(value, "true", 4) == 0) oldLen = 4;
        else if (remain >= 5 && memcmp(value, "false", 5) == 0) oldLen = 5;
        else { free(json); return FALSE; }
        const char *replacement = on ? "true" : "false";
        size_t replacementLen = strlen(replacement);
        size_t prefix = (size_t)(value - json);
        size_t newLen = (size_t)len - oldLen + replacementLen;
        if (newLen > 0xFFFFFFFFUL) { free(json); return FALSE; }
        char *updated = (char *)malloc(newLen + 1);
        if (!updated) { free(json); return FALSE; }
        memcpy(updated, json, prefix);
        memcpy(updated + prefix, replacement, replacementLen);
        memcpy(updated + prefix + replacementLen, value + oldLen,
               (size_t)len - prefix - oldLen);
        updated[newLen] = '\0';
        *out = updated;
        *outLen = (DWORD)newLen;
        free(json);
        return TRUE;
    }

    char entry[96];
    _snprintf(entry, sizeof(entry) - 1, "\"%s\": %s",
              memberName, on ? "true" : "false");
    entry[sizeof(entry) - 1] = '\0';
    BOOL ok = AddJsonEntry(json, len, scopeOpen, scopeClose, entry, out, outLen);
    free(json);
    return ok;
}

static int SmoothCurrent(void) {
    return KeyboardSettingCurrent(NULL, "Smooth", 1);
}

static int BalanceCurrent(void) {
    return KeyboardSettingCurrent("Kbd", "Equalise", 1);
}

static BOOL ApplyKeyboardSetting(const char *objectName, const char *memberName,
                                 int on) {
    wchar_t panelPath[MAX_PATH], themePath[MAX_PATH];
    DWORD panelLen = 0, themeLen = 0, panelOutLen = 0, themeOutLen = 0;
    char *panelTxt, *themeTxt, *panelOut = NULL, *themeOut = NULL;
    BOOL panelExists, themeExists, ok = FALSE;
    SetKblPath(panelPath, MAX_PATH, L"panel.json");
    SetKblPath(themePath, MAX_PATH, L"theme.json");
    panelExists = FileExistsW(panelPath);
    themeExists = FileExistsW(themePath);
    panelTxt = ReadTextFileUtf8(panelPath, &panelLen);
    themeTxt = ReadTextFileUtf8(themePath, &themeLen);
    if ((panelExists && !panelTxt) || (themeExists && !themeTxt)) goto done;
    if (!BuildJsonBoolUpdate(panelTxt, panelLen, objectName, memberName, on,
                             &panelOut, &panelOutLen) ||
        !BuildJsonBoolUpdate(themeTxt, themeLen, objectName, memberName, on,
                             &themeOut, &themeOutLen))
        goto done;

    /* Persist the panel preference first. If the live theme cannot be replaced,
       restore the saved file so the two settings sources do not disagree. */
    if (!WriteBinaryFileAtomic(panelPath, (const BYTE *)panelOut, panelOutLen))
        goto done;
    if (!WriteBinaryFileAtomic(themePath, (const BYTE *)themeOut, themeOutLen)) {
        if (panelTxt) WriteBinaryFileAtomic(panelPath, (const BYTE *)panelTxt, panelLen);
        else DeleteFileW(panelPath);
        goto done;
    }
    ok = TRUE;

done:
    free(panelTxt);
    free(themeTxt);
    free(panelOut);
    free(themeOut);
    return ok;
}

static BOOL ApplySmooth(int on) {
    return ApplyKeyboardSetting(NULL, "Smooth", on);
}

static BOOL ApplyBalance(int on) {
    return ApplyKeyboardSetting("Kbd", "Equalise", on);
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
    else if (vk == 0x26)               wcsncpy(out, L"Up Arrow", (size_t)cap - 1);
    else if (vk == 0x28)               wcsncpy(out, L"Down Arrow", (size_t)cap - 1);
    else if (vk == 0x25)               wcsncpy(out, L"Left Arrow", (size_t)cap - 1);
    else if (vk == 0x27)               wcsncpy(out, L"Right Arrow", (size_t)cap - 1);
    else                               _snwprintf(out, cap - 1, L"key 0x%02X", vk);
    out[cap - 1] = L'\0';
}

static void UpdateHotkeysSummary(void) {
    wchar_t p[MAX_PATH], line[192];
    int um, uk, dm, dk;
    wchar_t mu[40], ku[24], md[40], kd[24];
    if (!g_setHotkeys) return;
    SetKblPath(p, MAX_PATH, L"presets.ini");
    um = GetPrivateProfileIntW(L"Keys", L"UpMod", 3, p);
    uk = GetPrivateProfileIntW(L"Keys", L"UpKey", 0x26, p);
    dm = GetPrivateProfileIntW(L"Keys", L"DownMod", 3, p);
    dk = GetPrivateProfileIntW(L"Keys", L"DownKey", 0x28, p);
    ModNamesW(um, mu, 40); KeyNameW(uk, ku, 24);
    ModNamesW(dm, md, 40); KeyNameW(dk, kd, 24);
    _snwprintf(line, 191,
               L"Brightness hotkeys: %s%s / %s%s (presets.ini; applies on restart)",
               mu, ku, md, kd);
    line[191] = L'\0';
    SetWindowTextW(g_setHotkeys, line);
}

/* Hold-to-sweep needs a usable key-down/up source for both keys. Keep the
   Settings status in sync with the same persisted bindings the tray uses. */
static BOOL HoldKeyReady(const wchar_t *scanKey, const wchar_t *hidKey,
                         const wchar_t *releaseKey, const wchar_t *path) {
    if (GetPrivateProfileIntW(L"Keys", scanKey, 0, path) != 0) return TRUE;
    return GetPrivateProfileIntW(L"Keys", hidKey, 0, path) != 0 &&
           GetPrivateProfileIntW(L"Keys", releaseKey, 0, path) != 0;
}

static BOOL HoldSweepReady(void) {
    wchar_t p[MAX_PATH];
    SetKblPath(p, MAX_PATH, L"presets.ini");
    return HoldKeyReady(L"M4Scan", L"M4Hid", L"M4HidRelease", p) &&
           HoldKeyReady(L"M5Scan", L"M5Hid", L"M5HidRelease", p);
}

static void UpdateHoldStateLabel(void) {
    wchar_t p[MAX_PATH];
    BOOL m4, m5;
    const wchar_t *text;
    if (!g_setHoldState) return;
    SetKblPath(p, MAX_PATH, L"presets.ini");
    m4 = HoldKeyReady(L"M4Scan", L"M4Hid", L"M4HidRelease", p);
    m5 = HoldKeyReady(L"M5Scan", L"M5Hid", L"M5HidRelease", p);
    if (m4 && m5)
        text = L"Hold-to-sweep is ready: M4 brightness-down and M5 brightness-up are recognized.";
    else if (m4)
        text = L"M4 down is ready; M5 up is still learning. Press-to-glide works without hold detection.";
    else if (m5)
        text = L"M5 up is ready; M4 down is still learning. Press-to-glide works without hold detection.";
    else
        text = L"M4/M5 hold signals are still learning. Press-to-glide works without key recognition.";
    SetWindowTextW(g_setHoldState, text);
    InvalidateRect(g_setHoldState, NULL, TRUE);
}

static BOOL ApplyDns(void) {
    wchar_t cfg[MAX_PATH], num[16];
    LRESULT sel;
    int mins = 30;
    BOOL a, b, supported = FALSE;
    SetDnsCfgPath(cfg, MAX_PATH);
    sel = SendMessageW(g_setDnsInt, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) {
        LRESULT value = SendMessageW(g_setDnsInt, CB_GETITEMDATA, (WPARAM)sel, 0);
        if (value != CB_ERR) mins = (int)value;
    }
    for (int i = 0; i < 6; i++) {
        if (mins == kSetIntervals[i]) { supported = TRUE; break; }
    }
    if (!supported) mins = 30;
    _snwprintf(num, 15, L"%d", mins);
    num[15] = L'\0';
    a = WritePrivateProfileStringW(L"Pilot", L"Auto",
            (SendMessageW(g_setDnsAuto, BM_GETCHECK, 0, 0) & BST_CHECKED) ? L"1" : L"0", cfg);
    b = WritePrivateProfileStringW(L"Pilot", L"IntervalMin", num, cfg);
    return a && b;
}

typedef HRESULT (WINAPI *PFN_SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
static HMODULE g_settingsThemeModule = NULL;
static PFN_SetWindowTheme g_settingsSetWindowTheme = NULL;
static BOOL g_settingsThemeChecked = FALSE;

static void SetSettingsNativeControlTheme(HWND ctl) {
    if (!ctl) return;
    if (!g_settingsThemeChecked) {
        g_settingsThemeChecked = TRUE;
        g_settingsThemeModule = LoadLibraryW(L"uxtheme.dll");
        if (g_settingsThemeModule) {
            g_settingsSetWindowTheme =
                (PFN_SetWindowTheme)(void *)GetProcAddress(g_settingsThemeModule,
                                                           "SetWindowTheme");
        }
    }
    /* The themed BUTTON/COMBOBOX renderer can ignore WM_CTLCOLOR text colors
       and choose black system text against this dark card. Disabling only the
       native theme for these child controls makes their existing Soft Slate
       WM_CTLCOLOR brushes and readable text colors authoritative. */
    if (g_settingsSetWindowTheme)
        g_settingsSetWindowTheme(ctl, L"", L"");
}

static HWND SetCtl(DWORD style, const wchar_t *cls, const wchar_t *text,
                   int x, int y, int w, int h, int id, HWND parent, int dpi) {
    HWND ctl = CreateWindowExW(0, cls, text, style | WS_CHILD | WS_VISIBLE,
                               SetScale(x, dpi), SetScale(y, dpi),
                               SetScale(w, dpi), SetScale(h, dpi),
                               parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (ctl && g_fontBody) SendMessageW(ctl, WM_SETFONT, (WPARAM)g_fontBody, TRUE);
    if (ctl && (lstrcmpiW(cls, L"BUTTON") == 0 || lstrcmpiW(cls, L"COMBOBOX") == 0))
        SetSettingsNativeControlTheme(ctl);
    return ctl;
}

static void SetSettingsFont(HWND ctl, HFONT font) {
    if (ctl && font) SendMessageW(ctl, WM_SETFONT, (WPARAM)font, TRUE);
}

static void EnsureSettingsBrushes(void) {
    if (!g_setBgBrush) g_setBgBrush = CreateSolidBrush(COL_BG);
    if (!g_setCardBrush) g_setCardBrush = CreateSolidBrush(COL_CARD_BG);
    if (!g_setAccentBrush) g_setAccentBrush = CreateSolidBrush(COL_SKY);
}

static void ReleaseSettingsBrushes(void) {
    if (g_setBgBrush) DeleteObject(g_setBgBrush);
    if (g_setCardBrush) DeleteObject(g_setCardBrush);
    if (g_setAccentBrush) DeleteObject(g_setAccentBrush);
    g_setBgBrush = NULL;
    g_setCardBrush = NULL;
    g_setAccentBrush = NULL;
}

static void DrawSettingsCard(HDC dc, int dpi, int clientRight,
                             int top, int bottom, BOOL statusCard) {
    RECT rc;
    HPEN pen = CreatePen(PS_SOLID, SetScale(1, dpi),
                         statusCard ? COL_DIVIDER : COL_CARD_BORDER);
    HGDIOBJ oldPen, oldBrush;
    SetRect(&rc, SetScale(16, dpi), SetScale(top, dpi),
            clientRight - SetScale(16, dpi), SetScale(bottom, dpi));
    oldPen = SelectObject(dc, pen);
    oldBrush = SelectObject(dc, g_setCardBrush);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom,
              SetScale(14, dpi), SetScale(14, dpi));
    if (!statusCard) {
        RECT accent = { rc.left + SetScale(1, dpi), rc.top + SetScale(18, dpi),
                        rc.left + SetScale(4, dpi), rc.bottom - SetScale(18, dpi) };
        FillRect(dc, &accent, g_setAccentBrush);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

static void PaintSettings(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    int dpi = GetWinDpi(hwnd);
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, g_setBgBrush ? g_setBgBrush : GetSysColorBrush(COLOR_BTNFACE));
    SetBkMode(dc, TRANSPARENT);
    DrawSettingsCard(dc, dpi, client.right, 84, 352, FALSE);
    DrawSettingsCard(dc, dpi, client.right, 364, 526, FALSE);
    DrawSettingsCard(dc, dpi, client.right, 538, 660, FALSE);
    DrawSettingsCard(dc, dpi, client.right, 672, 756, TRUE);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND) {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if ((id == IDS_HOLD_TAP || id == IDS_HOLD_LATCH || id == IDS_HOLD_BURST) &&
            code == BN_CLICKED) {
            int mode = (id == IDS_HOLD_TAP) ? 0 : (id == IDS_HOLD_LATCH) ? 1 : 2;
            if (ApplyHoldMode(mode)) {
                UpdateHoldStateLabel();
                if (mode == 0 && !HoldSweepReady())
                    SetStatusText(L"Sweep selected; key signals are still learning. Press-to-glide is the fallback.");
                else if (mode == 0)
                    SetStatusOk(L"Sweep while held saved. M4/M5 signals are ready.");
                else if (mode == 1)
                    SetStatusOk(L"Press-to-glide saved. Press the same key again to stop.");
                else
                    SetStatusOk(L"Short glide saved. Each press moves brightness by about 20%.");
            } else {
                int savedMode = HoldModeCurrent();
                for (int i = 0; i < 3; i++)
                    SendMessageW(g_setHold[i], BM_SETCHECK,
                                 i == savedMode ? BST_CHECKED : BST_UNCHECKED, 0);
                SetStatusError(L"Could not save the Keyboard Lighting hold choice. Try again.");
            }
            return 0;
        }
        if (id == IDS_BALANCE && code == BN_CLICKED) {
            int on = (SendMessageW(g_setBalance, BM_GETCHECK, 0, 0) & BST_CHECKED) ? 1 : 0;
            if (ApplyBalance(on))
                SetStatusOk(on ? L"Balance Colours is on for the keyboard. The light bar keeps its own setting."
                               : L"Balance Colours is off for the keyboard. The light bar keeps its own setting.");
            else {
                SendMessageW(g_setBalance, BM_SETCHECK,
                             BalanceCurrent() ? BST_CHECKED : BST_UNCHECKED, 0);
                SetStatusError(L"Could not save Keyboard Lighting's balance setting. Try again.");
            }
            return 0;
        }
        if (id == IDS_SMOOTH && code == BN_CLICKED) {
            int on = (SendMessageW(g_setSmooth, BM_GETCHECK, 0, 0) & BST_CHECKED) ? 1 : 0;
            if (ApplySmooth(on))
                SetStatusOk(on ? L"Steady Colours is on. Fine dithering is off."
                               : L"Steady Colours is off. Fine dithering is on.");
            else {
                SendMessageW(g_setSmooth, BM_SETCHECK,
                             SmoothCurrent() ? BST_CHECKED : BST_UNCHECKED, 0);
                SetStatusError(L"Could not save Keyboard Lighting's colour setting. Open the app once, then try again.");
            }
            return 0;
        }
        if ((id == IDS_DNS_AUTO && code == BN_CLICKED) ||
            (id == IDS_DNS_INT && code == CBN_SELCHANGE)) {
            if (ApplyDns())
                SetStatusOk(L"DNS Pilot settings saved. Restart DNS Pilot to apply them.");
            else
                SetStatusError(L"Could not save DNS Pilot settings. Try again.");
            return 0;
        }
        if (id == IDS_FOLLOW_LATEST && code == BN_CLICKED) {
            int on = (SendMessageW(g_setAutoBranch, BM_GETCHECK, 0, 0) & BST_CHECKED) ? 1 : 0;
            SetFollowLatestTestBranch(on);
            if (on) {
                SetStatusText(L"Automatic branch following is on. Checking the newest test branch now.");
                EnqueueTask(TASK_CHECK_REMOTE);
            } else {
                SetStatusText(L"Automatic branch following is paused. Choose a branch from the header.");
            }
            return 0;
        }
        return 0;
    }
    if (msg == WM_TIMER && wp == 1) {
        UpdateHoldStateLabel();
        UpdateHotkeysSummary();
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_setBgBrush ? g_setBgBrush : GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    if (msg == WM_PAINT) {
        PaintSettings(hwnd);
        return 0;
    }
    if (msg == WM_CTLCOLORSTATIC) {
        HDC dc = (HDC)wp;
        int id = GetDlgCtrlID((HWND)lp);
        SetBkMode(dc, TRANSPARENT);
        if (id == IDS_SET_TITLE || id == IDS_KBL_GROUP ||
            id == IDS_DNS_GROUP || id == IDS_BRANCH_GROUP)
            SetTextColor(dc, COL_TEXT_PRIMARY);
        else if (id == IDS_STATUS) {
            COLORREF c = COL_TEXT_SECONDARY;
            if (g_setStatusTone == SET_STATUS_OK) c = COL_MINT;
            else if (g_setStatusTone == SET_STATUS_ERROR) c = COL_CORAL;
            SetTextColor(dc, c);
        } else if (id == IDS_HOLD_STATE) {
            SetTextColor(dc, COL_TEXT_SECONDARY);
        } else {
            SetTextColor(dc, COL_TEXT_SECONDARY);
        }
        return (LRESULT)(g_setCardBrush ? g_setCardBrush : GetSysColorBrush(COLOR_BTNFACE));
    }
    if (msg == WM_CTLCOLORBTN || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COL_TEXT_PRIMARY);
        SetBkColor(dc, COL_CARD_BG);
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)(g_setCardBrush ? g_setCardBrush : GetSysColorBrush(COLOR_BTNFACE));
    }
    if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, 1);
        g_setHwnd = NULL;
        g_setHoldState = NULL;
        g_setSmooth = NULL;
        g_setBalance = NULL;
        g_setHotkeys = NULL;
        g_setStatus = NULL;
        ReleaseSettingsBrushes();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OpenSettingsWindow(HWND owner) {
    if (g_setHwnd) { SetForegroundWindow(g_setHwnd); return; }
    WNDCLASSEXW wc;
    EnsureSettingsBrushes();
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hInstance     = g_hInst;
    /* WM_ERASEBKGND and WM_PAINT own every client pixel; leaving the class
       brush null prevents a system-white flash before the Soft Slate paint. */
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"HubSuiteSettingsWnd";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        ReleaseSettingsBrushes();
        return;
    }

    int dpi = GetWinDpi(owner);
    int W = SetScale(640, dpi), H = SetScale(820, dpi);
    RECT rr;
    GetWindowRect(owner, &rr);
    int x = rr.left + ((rr.right - rr.left) - W) / 2;
    int y = rr.top + ((rr.bottom - rr.top) - H) / 2;
    g_setHwnd = CreateWindowExW(0, wc.lpszClassName, L"Project Hub Settings",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                                x, y, W, H, owner, NULL, g_hInst, NULL);
    if (!g_setHwnd) { ReleaseSettingsBrushes(); return; }
    ApplyModernWindowFrame(g_setHwnd);

    int CW = 600, xL = 20;
    HWND title = SetCtl(0, L"STATIC", L"Settings", xL, 17, CW, 30,
                        IDS_SET_TITLE, g_setHwnd, dpi);
    SetSettingsFont(title, g_fontTitle);
    SetCtl(0, L"STATIC", L"Keyboard Lighting, DNS Pilot, and update preferences in one place.",
           xL, 49, CW, 22, IDS_SET_SUBTITLE, g_setHwnd, dpi);

    HWND kblHead = SetCtl(0, L"STATIC", L"Keyboard Lighting", xL + 12, 98, CW - 24, 22,
                          IDS_KBL_GROUP, g_setHwnd, dpi);
    SetSettingsFont(kblHead, g_fontTitle);
    g_setHold[0] = SetCtl(BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, L"BUTTON",
                          L"Sweep while held (uses M4/M5 key signals)",
                          xL + 16, 127, CW - 32, 22, IDS_HOLD_TAP, g_setHwnd, dpi);
    g_setHold[1] = SetCtl(BS_AUTORADIOBUTTON | WS_TABSTOP, L"BUTTON",
                          L"Press to start a glide; press the same key to stop",
                          xL + 16, 155, CW - 32, 22, IDS_HOLD_LATCH, g_setHwnd, dpi);
    g_setHold[2] = SetCtl(BS_AUTORADIOBUTTON | WS_TABSTOP, L"BUTTON",
                          L"Short glide: about 20% brightness per press",
                          xL + 16, 183, CW - 32, 22, IDS_HOLD_BURST, g_setHwnd, dpi);
    g_setBalance = SetCtl(BS_AUTOCHECKBOX | WS_TABSTOP, L"BUTTON",
                          L"Balance Colours for the keyboard",
                          xL + 16, 211, CW - 32, 22, IDS_BALANCE, g_setHwnd, dpi);
    g_setSmooth = SetCtl(BS_AUTOCHECKBOX | WS_TABSTOP, L"BUTTON",
                          L"Steady Colours (off uses fine dithering)",
                          xL + 16, 239, CW - 32, 22, IDS_SMOOTH, g_setHwnd, dpi);
    g_setHotkeys = SetCtl(0, L"STATIC", L"", xL + 16, 268, CW - 32, 32,
                          IDS_HOTKEYS, g_setHwnd, dpi);
    g_setHoldState = SetCtl(0, L"STATIC", L"",
                            xL + 16, 302, CW - 32, 42, IDS_HOLD_STATE, g_setHwnd, dpi);

    HWND dnsHead = SetCtl(0, L"STATIC", L"DNS Pilot", xL + 12, 378, CW - 24, 22,
                          IDS_DNS_GROUP, g_setHwnd, dpi);
    SetSettingsFont(dnsHead, g_fontTitle);
    g_setDnsAuto = SetCtl(BS_AUTOCHECKBOX | WS_TABSTOP, L"BUTTON",
                          L"Automatically choose the fastest DNS",
                          xL + 16, 407, CW - 32, 22, IDS_DNS_AUTO, g_setHwnd, dpi);
    SetCtl(0, L"STATIC", L"Check DNS every", xL + 16, 439, 108, 24,
           IDS_DNS_INT_LBL, g_setHwnd, dpi);
    g_setDnsInt = SetCtl(CBS_DROPDOWNLIST | WS_TABSTOP, L"COMBOBOX", L"",
                         xL + 128, 435, 190, 130, IDS_DNS_INT, g_setHwnd, dpi);
    SetCtl(0, L"STATIC", L"DNS Pilot applies saved changes after it restarts.",
           xL + 16, 476, CW - 32, 24, IDS_DNS_NOTE, g_setHwnd, dpi);

    HWND branchHead = SetCtl(0, L"STATIC", L"Project Hub updates", xL + 12, 551,
                             CW - 24, 22, IDS_BRANCH_GROUP, g_setHwnd, dpi);
    SetSettingsFont(branchHead, g_fontTitle);
    g_setAutoBranch = SetCtl(BS_AUTOCHECKBOX | WS_TABSTOP, L"BUTTON",
                             L"Automatically follow the newest active test branch",
                             xL + 16, 580, CW - 32, 22, IDS_FOLLOW_LATEST, g_setHwnd, dpi);
    SetCtl(0, L"STATIC",
           L"Choosing a branch manually pauses this. Turn it back on here to resume.",
           xL + 16, 609, CW - 32, 38, IDS_BRANCH_NOTE, g_setHwnd, dpi);

    g_setStatus = SetCtl(0, L"STATIC",
                         L"Lighting and Hub choices apply now. DNS Pilot needs a restart.",
                         xL + 12, 689, CW - 24, 48, IDS_STATUS, g_setHwnd, dpi);

    int mode = HoldModeCurrent();
    for (int i = 0; i < 3; i++)
        SendMessageW(g_setHold[i], BM_SETCHECK,
                     i == mode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_setBalance, BM_SETCHECK,
                 BalanceCurrent() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_setSmooth, BM_SETCHECK,
                 SmoothCurrent() ? BST_CHECKED : BST_UNCHECKED, 0);

    wchar_t cfg[MAX_PATH];
    SetDnsCfgPath(cfg, MAX_PATH);
    int autoOn = GetPrivateProfileIntW(L"Pilot", L"Auto", 0, cfg);
    int imin = GetPrivateProfileIntW(L"Pilot", L"IntervalMin", 30, cfg);
    int selIdx = -1;
    BOOL badInterval = FALSE;
    BOOL intervalFixOk = TRUE;
    SendMessageW(g_setDnsAuto, BM_SETCHECK, autoOn ? BST_CHECKED : BST_UNCHECKED, 0);
    for (int i = 0; i < 6; i++) {
        wchar_t item[32];
        if (kSetIntervals[i] == 0)
            _snwprintf(item, 31, L"Auto (live watch)");
        else
            _snwprintf(item, 31, L"%d minutes", kSetIntervals[i]);
        item[31] = L'\0';
        LRESULT idx = SendMessageW(g_setDnsInt, CB_ADDSTRING, 0, (LPARAM)item);
        if (idx != CB_ERR && idx != CB_ERRSPACE) {
            SendMessageW(g_setDnsInt, CB_SETITEMDATA, (WPARAM)idx,
                         (LPARAM)kSetIntervals[i]);
            if (kSetIntervals[i] == imin) selIdx = (int)idx;
        }
    }
    if (selIdx < 0) {
        wchar_t valid[16];
        imin = 30;
        badInterval = TRUE;
        _snwprintf(valid, 15, L"%d", imin);
        valid[15] = L'\0';
        intervalFixOk = WritePrivateProfileStringW(L"Pilot", L"IntervalMin", valid, cfg);
        for (int i = 0; i < 6; i++) {
            if (kSetIntervals[i] == imin) {
                selIdx = i;
                break;
            }
        }
    }
    SendMessageW(g_setDnsInt, CB_SETCURSEL, (WPARAM)selIdx, 0);
    SendMessageW(g_setAutoBranch, BM_SETCHECK,
                 FollowLatestTestBranch() ? BST_CHECKED : BST_UNCHECKED, 0);

    UpdateHoldStateLabel();
    UpdateHotkeysSummary();
    if (badInterval && intervalFixOk)
        SetStatusText(L"An unsupported DNS interval was reset to 30 minutes. DNS Pilot needs a restart.");
    else if (badInterval)
        SetStatusError(L"The DNS interval is unsupported and could not be reset. Choose an interval to save a valid value.");
    else
        SetStatusText(L"Lighting and Hub choices apply now. DNS Pilot needs a restart.");

    SetTimer(g_setHwnd, 1, 1000, NULL);
    ShowWindow(g_setHwnd, SW_SHOWNORMAL);
    UpdateWindow(g_setHwnd);
}
