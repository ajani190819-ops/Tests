/* ---------------- util: palette, dpi, files, text ---------------- */

static int AppsUseLightTheme(void) {
    DWORD val = 0, sz = sizeof(val);
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                              L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &val, &sz);
    if (st != ERROR_SUCCESS) return 0;
    return val != 0;
}

/* One palette, two schemes, chosen by the owner's Windows theme. The radar
   photo-real colours are fixed, but every surface the TEXT sits on comes
   from here, and COL_RAIN is the honest-probability bar colour. */
static void InitPalette(void) {
    if (AppsUseLightTheme()) {
        COL_BG      = RGB(247, 248, 250);
        COL_CARD    = RGB(255, 255, 255);
        COL_INPUT   = RGB(238, 241, 246);
        COL_TXT     = RGB(23, 27, 36);
        COL_MUTED   = RGB(88, 97, 114);
        COL_ACCENT  = RGB(2, 108, 163);
        COL_WARN    = RGB(146, 84, 6);
        COL_GOOD    = RGB(4, 108, 78);
        COL_DIV     = RGB(206, 212, 222);
        COL_HOT     = RGB(140, 150, 170);
        COL_CARD_BD = RGB(221, 226, 235);
        COL_RAIN    = RGB(2, 108, 163);
    } else {
        COL_BG      = RGB(24, 27, 36);
        COL_CARD    = RGB(34, 39, 52);
        COL_INPUT   = RGB(43, 49, 66);
        COL_TXT     = RGB(236, 239, 246);
        COL_MUTED   = RGB(154, 163, 184);
        COL_ACCENT  = RGB(56, 189, 248);
        COL_WARN    = RGB(251, 191, 36);
        COL_GOOD    = RGB(52, 211, 153);
        COL_DIV     = RGB(58, 66, 86);
        COL_HOT     = RGB(82, 94, 122);
        COL_CARD_BD = RGB(56, 64, 84);
        COL_RAIN    = RGB(56, 189, 248);
    }
}

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

/* Title bar and rounded corners follow the same theme as the client area, so
   a light-mode owner never gets a black bar bolted onto a white window. */
static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = AppsUseLightTheme() ? FALSE : TRUE;
        DWORD cornerPref = 2;   /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIV;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &cornerPref, sizeof(cornerPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static UINT GetWinDpi(HWND h) {
    UINT (WINAPI *pfn)(HWND) = (UINT (WINAPI *)(HWND))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return (pfn && h) ? pfn(h) : 96;
}

static void MakeFonts(void) {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    lf.lfHeight = -S(13);
    g_fontUI = CreateFontIndirectW(&lf);
    lf.lfHeight = -S(12);
    g_fontSmall = CreateFontIndirectW(&lf);
    lf.lfHeight = -S(18);
    lf.lfWeight = FW_SEMIBOLD;
    g_fontTitle = CreateFontIndirectW(&lf);
    lf.lfHeight = -S(34);
    lf.lfWeight = FW_BOLD;
    g_fontBig = CreateFontIndirectW(&lf);
    lf.lfHeight = -S(13);
    lf.lfWeight = FW_SEMIBOLD;
    g_fontBtn = CreateFontIndirectW(&lf);
}

/* ---------------- files ---------------- */

static int DirExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void EnsureDirW(const wchar_t *p) {
    if (DirExistsW(p)) return;
    wchar_t parent[MAX_PATH];
    wcsncpy(parent, p, MAX_PATH - 1);
    parent[MAX_PATH - 1] = L'\0';
    wchar_t *sl = wcsrchr(parent, L'\\');
    if (sl && sl != parent) { *sl = L'\0'; EnsureDirW(parent); }
    CreateDirectoryW(p, NULL);
}

static int ReadWholeFileW(const wchar_t *p, char **out, size_t *outLen) {
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD sz = GetFileSize(h, NULL);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { CloseHandle(h); return 0; }
    DWORD rd = 0;
    int ok = ReadFile(h, buf, sz, &rd, NULL) && rd == sz;
    CloseHandle(h);
    if (!ok) { free(buf); return 0; }
    buf[sz] = '\0';
    *out = buf;
    *outLen = sz;
    return 1;
}

static int WriteWholeFileW(const wchar_t *p, const void *data, size_t len) {
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0;
    int ok = WriteFile(h, data, (DWORD)len, &wr, NULL) && wr == (DWORD)len;
    CloseHandle(h);
    return ok;
}

/* Delete cache files older than `maxAgeSec`. Bounded cleanup at startup only;
   the cache is a courtesy to the tile hosts, not a database. */
static void PruneCacheDir(const wchar_t *dir, const wchar_t *prefix, long long maxAgeSec) {
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH];
    _snwprintf(pat, MAX_PATH - 1, L"%s\\*", dir);
    pat[MAX_PATH - 1] = L'\0';
    HANDLE f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) return;
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER nowU;
    nowU.LowPart = nowFt.dwLowDateTime;
    nowU.HighPart = nowFt.dwHighDateTime;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (prefix && wcsncmp(fd.cFileName, prefix, wcslen(prefix)) != 0) continue;
        ULARGE_INTEGER ftU;
        ftU.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        ftU.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        long long ageSec = (long long)((nowU.QuadPart - ftU.QuadPart) / 10000000ULL);
        if (ageSec > maxAgeSec) {
            wchar_t full[MAX_PATH];
            _snwprintf(full, MAX_PATH - 1, L"%s\\%s", dir, fd.cFileName);
            full[MAX_PATH - 1] = L'\0';
            DeleteFileW(full);
        }
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}

/* ---------------- text ---------------- */

static int Utf8ToWide(const char *utf8, wchar_t *out, int cap) {
    if (!utf8 || cap <= 0) return 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
    if (n <= 0) { out[0] = L'\0'; return 0; }
    return n - 1;
}

static void WideToUtf8(const wchar_t *w, char *out, int cap) {
    if (cap <= 0) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
    if (n <= 0) out[0] = '\0';
}

/* Percent-encode a UTF-8 string for a query value. Spaces go to %20 rather
   than '+', which geocoding-api.open-meteo.com accepts in either form but
   %20 is the form RFC 3986 actually defines. */
static void UrlEncodeUtf8(const char *in, char *out, int cap) {
    int o = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; in[i] && o < cap - 4; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15];
        }
    }
    out[o] = '\0';
}

static void NowLocalStamp(wchar_t *out, int cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(out, cap - 1, L"%02d:%02d", st.wHour, st.wMinute);
    out[cap - 1] = L'\0';
}

/* WMO weather codes, the words people actually use. */
static const wchar_t *WxCodeText(int code) {
    switch (code) {
    case 0:  return L"Clear sky";
    case 1:  return L"Mostly clear";
    case 2:  return L"Partly cloudy";
    case 3:  return L"Overcast";
    case 45: case 48: return L"Fog";
    case 51: case 53: case 55: return L"Drizzle";
    case 56: case 57: return L"Freezing drizzle";
    case 61: return L"Light rain";
    case 63: return L"Rain";
    case 65: return L"Heavy rain";
    case 66: case 67: return L"Freezing rain";
    case 71: return L"Light snow";
    case 73: return L"Snow";
    case 75: return L"Heavy snow";
    case 77: return L"Snow grains";
    case 80: return L"Light showers";
    case 81: return L"Showers";
    case 82: return L"Violent showers";
    case 85: case 86: return L"Snow showers";
    case 95: return L"Thunderstorm";
    case 96: case 99: return L"Thunderstorm with hail";
    default: return L"Unknown";
    }
}

/* Is this code any kind of wet weather? Used to tint the now-line. */
static int WxCodeIsWet(int code) {
    return (code >= 51 && code <= 67) || (code >= 71 && code <= 86) ||
           (code >= 95 && code <= 99);
}

static COLORREF MixCol(COLORREF a, COLORREF b, int pctB) {
    int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pctB / 100;
    int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pctB / 100;
    int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pctB / 100;
    return RGB(r, g, bl);
}

static void DrawRoundedBox(HDC dc, const RECT *r, int rad, COLORREF fill, COLORREF edge) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, r->left, r->top, r->right, r->bottom, rad, rad);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void DrawTextAt(HDC dc, const wchar_t *txt, const RECT *r, HFONT f,
                       COLORREF col, UINT align) {
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, f);
    SetTextColor(dc, col);
    DrawTextW(dc, txt, -1, (RECT *)r, align | DT_NOPREFIX);
}
