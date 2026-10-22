#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ============================================================================
 *  Keyboard Lighting - Native Win32 Suite App (v2.6.1)
 *
 *  Matches the unified Soft Slate Suite UI & visual layout shared with
 *  ProjectHub.exe and BluetoothGuard.exe while preserving 100% config &
 *  protocol compatibility with %LOCALAPPDATA%\KeyboardLighting\panel.json,
 *  theme.json, live.txt, frame.txt, layout.txt, and Aura-Background.ps1.
 * ============================================================================ */

#define APP_TITLE           L"Keyboard Lighting"
#define SINGLE_MUTEX_NAME   L"Local\\Tests_KeyboardLighting_SingleInstance_v2"
#define SHOW_EVENT_NAME     L"Local\\Tests_KeyboardLighting_ShowWindow_v2"
#define REPO_OWNER_W        L"ajani190819-ops"
#define REPO_NAME_W         L"Tests"
#define FALLBACK_BRANCH_W   L"arena/e895b94f-tests"

#define RES_ICON_APP        1
#define RES_ICON_EMBLEM     2

#define WM_TRAYICON         (WM_APP + 1)
#define WM_ENGINE_BOOTED    (WM_APP + 2)

#define IDM_TRAY_OPEN       200
#define IDM_TRAY_TOGGLE     201
#define IDM_TRAY_KBD        202
#define IDM_TRAY_BAR        203
#define IDM_TRAY_RESTART    204
#define IDM_TRAY_AUTOSTART  205
#define IDM_TRAY_ZONES      206
#define IDM_TRAY_FOLDER     207
#define IDM_TRAY_QUIT       208
#define IDM_EFF_BASE        400
#define IDM_PAT_BASE        420
#define IDM_PAT_NEW         428
#define IDM_PAT_DEL         429

/* Soft Slate Suite Palette (matches Project Hub, Bluetooth Guard & docs/UI_STYLE_GUIDE.md) */
#define COL_BG              RGB(24, 27, 36)    /* #181B24 */
#define COL_FOOTER_BG       RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER         RGB(48, 55, 72)    /* #303748 */
#define COL_CARD            RGB(34, 39, 52)    /* #222734 */
#define COL_BORDER          RGB(56, 64, 84)    /* #384054 */
#define COL_SEC_BTN         RGB(43, 49, 66)    /* #2B3142 */
#define COL_SEC_HOV         RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER      RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_RIM       RGB(96, 112, 144)  /* #607090 */
#define COL_TEXT            RGB(236, 239, 246) /* #ECEFF6 */
#define COL_MUTED           RGB(154, 163, 184) /* #9AA3B8 */
#define COL_TELEMETRY       RGB(116, 126, 148) /* #747E94 */
#define COL_GOOD            RGB(52, 211, 153)  /* #34D399 */
#define COL_GOOD_BG         RGB(24, 52, 44)
#define COL_WARN            RGB(251, 191, 36)  /* #FBBF24 */
#define COL_WARN_BG         RGB(56, 44, 24)
#define COL_ACCENT          RGB(56, 189, 248)  /* #38BDF8 */
#define COL_ACCENT_STRONG   RGB(37, 99, 235)   /* #2563EB */
#define COL_ACCENT_HOV      RGB(59, 130, 246)  /* #3B82F6 */
#define COL_INDIGO          RGB(129, 140, 248) /* #818CF8 */
#define COL_DANGER          RGB(248, 113, 113) /* #F87171 */
#define COL_DANGER_BG       RGB(58, 30, 36)
#define COL_IDLE_SLATE      RGB(108, 120, 144)
#define COL_PILL_MUTED      RGB(42, 48, 64)

typedef struct {
    const char *label;
    const char *token;
    const char *hint;
    int         usesPalette;
} EffectDef;

static const EffectDef kEffects[] = {
    { "Scrolling gradient",  "gradient",  "Smooth flowing multi-colour band across lamps",        1 },
    { "Rainbow",             "rainbow",   "Full spectrum wave (picks its own colours)",           0 },
    { "Wave",                "wave",      "A rhythmic travelling wave using your palette",         1 },
    { "Comet",               "comet",     "Bright head with glowing trailing tail",               1 },
    { "Scanner",             "scanner",   "Sweeps back and forth end-to-end",                     1 },
    { "Breathing",           "breathe",   "Gentle fade in and out across your palette",           1 },
    { "Pulse",               "pulse",     "Crisp rhythmic beat across your palette",              1 },
    { "Fire",                "fire",      "Warm flickering ember glow (picks its own colours)",   0 },
    { "Solid colour",        "static",    "Steady non-moving colour across zones",                1 },
    { "Colour cycle",        "cycle",     "Slowly shifts every lamp through the spectrum",        0 },
    { "Strobe",              "strobe",    "High-energy flash using your palette",                 1 },
    { "Starry night",        "stars",     "Twinkling sparks fading across zones",                 1 },
    { "Ripple",              "ripple",    "Expanding rings from the centre outward",              1 },
    { "Aurora",              "aurora",    "Organic northern-lights shimmer",                      1 },
    { "Music - spectrum",    "spectrum",  "Live audio frequency visualizer from speakers",        1 },
    { "Music - level meter", "vumeter",   "Live stereo volume meter from system audio",           1 },
    { "Music - beat flash",  "beat",      "Flashes on musical beats in real time",                1 },
    { "Music - bass pulse",  "pulsebass", "Pulses with low-frequency bass hits",                  1 },
    { "Screen mirror",       "ambient",   "Samples live colours from your display",               0 },
    { "Battery meter",       "battery",   "Shows charge level from green (full) to red",          0 },
    { "CPU meter",           "cpu",       "Reacts to live processor load",                        0 },
    { "Clock",               "clock",     "Shifts palette gently with the time of day",            0 }
};
#define EFFECT_COUNT ((int)(sizeof(kEffects) / sizeof(kEffects[0])))

typedef struct {
    BOOL     on;
    int      effectIdx;
    int      speed;        /* 1..50 -> 0.1x..5.0x */
    int      brightness;   /* 0..100 (0 = full off) */
    BOOL     mirror;
    BOOL     reverse;
    BOOL     equalise;
    BOOL     loop;
    int      swatchCount;  /* 2..8 */
    COLORREF swatches[8];
    double   widths[8];    /* 0.25..4.0 */
} GroupCfg;

typedef struct {
    int      masterBrightness; /* 0..100 (0 = full off, driven by Fn F2/F3 too) */
    BOOL     overlay;
    BOOL     link;
    BOOL     smooth;
    char     onExit[16];
    COLORREF customColors[16];
    GroupCfg kbd;
    GroupCfg bar;
} PanelCfg;

enum HitKind {
    HIT_NONE = 0,
    HIT_HDR_LIGHTS,
    HIT_HDR_STARTUP,
    HIT_HDR_RESTART,
    HIT_SLIDER_MASTER,
    HIT_TAB_KBD,
    HIT_TAB_BAR,
    HIT_TOG_ZONE_ON,
    HIT_TOG_LINK,
    HIT_PICK_EFFECT,
    HIT_SWATCH_0,          /* +0..7 */
    HIT_WIDTH_0 = 20,      /* +0..7 */
    HIT_SWATCH_ADD = 30,
    HIT_SWATCH_REM,
    HIT_SLIDER_SPEED,
    HIT_SLIDER_BRIGHT,
    HIT_TOG_LOOP,
    HIT_TOG_EQ,
    HIT_TOG_MIRROR,
    HIT_TOG_REVERSE,
    HIT_TOG_SMOOTH,
    HIT_TOG_OVERLAY,
    HIT_FOOT_SAVEP,
    HIT_FOOT_PATTERN,
    HIT_FOOT_ZONES,
    HIT_FOOT_ELEVATE,
    HIT_FOOT_QUIT
};

typedef struct {
    RECT rect;
    int  kind;
    int  index;
} HitBox;

static HWND             g_hwnd         = NULL;
static HICON            g_appIcon      = NULL;
static NOTIFYICONDATAW  g_nid;
static HANDLE           g_showEvent    = NULL;
static HANDLE           g_engineProc   = NULL;
static DWORD            g_enginePid    = 0;
static BOOL             g_isAdmin      = FALSE;
static BOOL             g_autostart    = FALSE;
static BOOL             g_wantOff      = FALSE;
static BOOL             g_quitting     = FALSE;
static BOOL             g_trayHinted   = FALSE;
static volatile LONG    g_downloading  = 0;

static PanelCfg         g_cfg;
static int              g_tab          = 0; /* 0 = Keyboard, 1 = Light Bar */
static HitBox           g_hits[96];
static int              g_hitCount     = 0;
static int              g_hoverIdx     = -1;
static int              g_dragKind     = HIT_NONE;
static int              g_dragIdx      = 0;
static RECT             g_dragRect     = {0, 0, 0, 0};
static int              g_scrollY      = 0;
static int              g_contentH     = 0;
static BOOL             g_applyPending = FALSE;
static ULONGLONG        g_applyAtTick  = 0;

/* Live preview state */
static COLORREF         g_liveCols[64];
static int              g_liveKbdN     = 4;
static int              g_liveBarN     = 12;
static BOOL             g_hasLiveFrame = FALSE;
static ULONGLONG        g_lastFrameTick = 0;

/* Fn F2/F3 sync + on-screen brightness toast */
static wchar_t          g_toastText[96] = L"";
static ULONGLONG        g_toastAt       = 0;
static ULONGLONG        g_lastFnPoll    = 0;

/* Paths */
static wchar_t          g_exePath[MAX_PATH];
static wchar_t          g_exeDir[MAX_PATH];
static wchar_t          g_cfgDir[MAX_PATH];
static wchar_t          g_panelPath[MAX_PATH];
static wchar_t          g_themePath[MAX_PATH];
static wchar_t          g_livePath[MAX_PATH];
static wchar_t          g_framePath[MAX_PATH];
static wchar_t          g_layoutPath[MAX_PATH];
static wchar_t          g_lockPath[MAX_PATH];
static wchar_t          g_showPath[MAX_PATH];
static wchar_t          g_engPidPath[MAX_PATH];
static wchar_t          g_stopPath[MAX_PATH];
static wchar_t          g_brightUpPath[MAX_PATH];
static wchar_t          g_brightDownPath[MAX_PATH];
static wchar_t          g_holdModePath[MAX_PATH];
static wchar_t          g_engineScript[MAX_PATH];
static wchar_t          g_zonesBat[MAX_PATH];
static wchar_t          g_statusText[256] = L"Starting ROG Strix G16 lighting engine...";

typedef UINT    (WINAPI *PFN_GetDpiForWindow)(HWND);
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static int GetWinDpi(HWND hwnd) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        PFN_GetDpiForWindow fn = (PFN_GetDpiForWindow)(void *)GetProcAddress(u32, "GetDpiForWindow");
        if (fn && hwnd) {
            UINT d = fn(hwnd);
            if (d >= 96) return (int)d;
        }
    }
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi >= 96 ? dpi : 96;
}

static int S(int px, int dpi) { return MulDiv(px, dpi, 96); }

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD roundPref = 2; /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIVIDER;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 33, &roundPref, sizeof(roundPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static BOOL FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static void EnsureDirTree(const wchar_t *dir) {
    wchar_t tmp[MAX_PATH];
    wcsncpy(tmp, dir, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = L'\0';
    for (wchar_t *p = tmp + 3; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t ch = *p;
            *p = L'\0';
            CreateDirectoryW(tmp, NULL);
            *p = ch;
        }
    }
    CreateDirectoryW(tmp, NULL);
}

static BOOL CheckIsAdmin(void) {
    BOOL admin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID admGroup = NULL;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admGroup)) {
        CheckTokenMembership(NULL, admGroup, &admin);
        FreeSid(admGroup);
    }
    return admin;
}

static char *ReadFileUtf8(const wchar_t *path, DWORD *outLen) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(hf, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 4 * 1024 * 1024) { CloseHandle(hf); return NULL; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { CloseHandle(hf); return NULL; }
    DWORD rd = 0;
    if (!ReadFile(hf, buf, sz, &rd, NULL)) { free(buf); CloseHandle(hf); return NULL; }
    buf[rd] = '\0';
    CloseHandle(hf);
    if (outLen) *outLen = rd;
    return buf;
}

static BOOL WriteAtomicUtf8(const wchar_t *dest, const char *data) {
    wchar_t tmp[MAX_PATH];
    _snwprintf(tmp, MAX_PATH, L"%s.tmp", dest);
    HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;
    DWORD len = (DWORD)strlen(data), wr = 0;
    BOOL ok = WriteFile(hf, data, len, &wr, NULL) && (wr == len);
    CloseHandle(hf);
    if (!ok) { DeleteFileW(tmp); return FALSE; }
    if (!MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return FALSE;
    }
    return TRUE;
}

static COLORREF ParseHexColor(const char *s, COLORREF fallback) {
    while (*s && (*s == ' ' || *s == '"' || *s == '\'')) s++;
    if (*s == '#') s++;
    if (strlen(s) < 6) return fallback;
    char buf[7] = { s[0], s[1], s[2], s[3], s[4], s[5], 0 };
    unsigned long v = strtoul(buf, NULL, 16);
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

static void FormatHexColor(COLORREF c, char *out) {
    snprintf(out, 8, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
}

static int FindEffectIndex(const char *nameOrTok) {
    for (int i = 0; i < EFFECT_COUNT; i++) {
        if (_stricmp(kEffects[i].label, nameOrTok) == 0 ||
            _stricmp(kEffects[i].token, nameOrTok) == 0) {
            return i;
        }
    }
    return 0;
}

static GroupCfg *CurGroup(void) {
    return (g_tab == 1) ? &g_cfg.bar : &g_cfg.kbd;
}

static void InitDefaultCfg(void) {
    ZeroMemory(&g_cfg, sizeof(g_cfg));
    g_cfg.masterBrightness = 100;
    g_cfg.overlay          = TRUE;
    g_cfg.link             = FALSE;
    g_cfg.smooth           = TRUE;
    strncpy(g_cfg.onExit, "off", sizeof(g_cfg.onExit) - 1);
    for (int i = 0; i < 16; i++) g_cfg.customColors[i] = RGB(255, 255, 255);

    g_cfg.kbd.on          = TRUE;
    g_cfg.kbd.effectIdx   = 0; /* Scrolling gradient */
    g_cfg.kbd.speed       = 10;
    g_cfg.kbd.brightness  = 100;
    g_cfg.kbd.equalise    = TRUE;
    g_cfg.kbd.loop        = FALSE;
    g_cfg.kbd.swatchCount = 6;
    g_cfg.kbd.swatches[0] = RGB(255, 0, 0);
    g_cfg.kbd.swatches[1] = RGB(255, 127, 0);
    g_cfg.kbd.swatches[2] = RGB(255, 255, 0);
    g_cfg.kbd.swatches[3] = RGB(0, 255, 0);
    g_cfg.kbd.swatches[4] = RGB(0, 0, 255);
    g_cfg.kbd.swatches[5] = RGB(139, 0, 255);
    for (int i = 0; i < 8; i++) g_cfg.kbd.widths[i] = 1.0;

    g_cfg.bar.on          = TRUE;
    g_cfg.bar.effectIdx   = 1; /* Rainbow */
    g_cfg.bar.speed       = 10;
    g_cfg.bar.brightness  = 100;
    g_cfg.bar.equalise    = TRUE;
    g_cfg.bar.loop        = TRUE;
    g_cfg.bar.swatchCount = 2;
    g_cfg.bar.swatches[0] = RGB(0, 180, 255);
    g_cfg.bar.swatches[1] = RGB(255, 0, 102);
    for (int i = 0; i < 8; i++) g_cfg.bar.widths[i] = 1.0;
}

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

/* ---- global brightness hotkeys --------------------------------------
 * Two system-wide hotkeys (default Ctrl+Alt+Up / Ctrl+Alt+Down) so
 * brightness can live on keys G-Helper does not fight over. Bind M4/M5 to
 * these combos in G-Helper's key remap and the spare keys become smooth
 * brightness control with no Fn traffic at all. Override in presets.ini:
 * [Keys] UpMod/UpKey/DownMod/DownKey use the Win32 RegisterHotKey codes.
 */
static void StepMasterBrightness(int dir) {
    int pct = g_cfg.masterBrightness + dir * 10;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (pct == g_cfg.masterBrightness) return;
    g_cfg.masterBrightness = pct;
    if (pct <= 0)
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  Off");
    else
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  %d%%", pct);
    g_toastAt = GetTickCount64() + 1400ULL;
    SavePanelCfg();
    ScheduleLiveApply();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void RegisterBrightnessHotkeys(HWND hwnd) {
    wchar_t path[MAX_PATH];
    PresetPath(path);
    UINT upMod = GetPrivateProfileIntW(L"Keys", L"UpMod", MOD_CONTROL | MOD_ALT, path);
    UINT upKey = GetPrivateProfileIntW(L"Keys", L"UpKey", 0x26, path);
    UINT dnMod = GetPrivateProfileIntW(L"Keys", L"DownMod", MOD_CONTROL | MOD_ALT, path);
    UINT dnKey = GetPrivateProfileIntW(L"Keys", L"DownKey", 0x28, path);
    if (!RegisterHotKey(hwnd, 1, upMod, upKey))
        RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 0x74);   /* fallback Ctrl+Alt+F5 */
    if (!RegisterHotKey(hwnd, 2, dnMod, dnKey))
        RegisterHotKey(hwnd, 2, MOD_CONTROL | MOD_ALT, 0x75);   /* fallback Ctrl+Alt+F6 */
}

/* Lightweight parser preserving existing user panel.json settings */
static const char *JsonVal(const char *src, const char *end, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = src;
    while (p && p < end) {
        const char *hit = strstr(p, pat);
        if (!hit || hit >= end) return NULL;
        const char *q = hit + strlen(pat);
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q < end && *q == ':') {
            q++;
            while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
            return q;
        }
        p = hit + 1;
    }
    return NULL;
}

static BOOL JsonBool(const char *src, const char *end, const char *key, BOOL defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v) return defVal;
    if (strncmp(v, "true", 4) == 0) return TRUE;
    if (strncmp(v, "false", 5) == 0) return FALSE;
    return defVal;
}

static int JsonInt(const char *src, const char *end, const char *key, int defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v) return defVal;
    return atoi(v);
}

static void JsonStr(const char *src, const char *end, const char *key, char *out, size_t cap) {
    const char *v = JsonVal(src, end, key);
    if (!v || *v != '"') return;
    v++;
    size_t i = 0;
    while (v < end && *v && *v != '"' && i + 1 < cap) out[i++] = *v++;
    out[i] = '\0';
}

static void ParseGroupJson(const char *src, const char *end, GroupCfg *g) {
    g->on         = JsonBool(src, end, "On", g->on);
    g->speed      = JsonInt(src, end, "Speed", g->speed);
    g->brightness = JsonInt(src, end, "Brightness", g->brightness);
    g->mirror     = JsonBool(src, end, "Mirror", g->mirror);
    g->reverse    = JsonBool(src, end, "Reverse", g->reverse);
    g->equalise   = JsonBool(src, end, "Equalise", g->equalise);
    g->loop       = JsonBool(src, end, "Loop", g->loop);
    if (g->speed < 1) g->speed = 1;
    if (g->speed > 50) g->speed = 50;
    if (g->brightness < 0) g->brightness = 0;
    if (g->brightness > 100) g->brightness = 100;

    char eff[64] = "";
    JsonStr(src, end, "Effect", eff, sizeof(eff));
    if (eff[0]) g->effectIdx = FindEffectIndex(eff);

    const char *sw = JsonVal(src, end, "Swatches");
    if (sw && *sw == '[') {
        const char *brEnd = strchr(sw, ']');
        if (brEnd && brEnd < end) {
            int n = 0;
            const char *p = sw + 1;
            while (p < brEnd && n < 8) {
                const char *q1 = strchr(p, '"');
                if (!q1 || q1 >= brEnd) break;
                const char *q2 = strchr(q1 + 1, '"');
                if (!q2 || q2 >= brEnd) break;
                g->swatches[n++] = ParseHexColor(q1 + 1, RGB(56, 189, 248));
                p = q2 + 1;
            }
            if (n >= 1) g->swatchCount = n;
        }
    }

    const char *wd = JsonVal(src, end, "Widths");
    if (wd && *wd == '[') {
        const char *brEnd = strchr(wd, ']');
        if (brEnd && brEnd < end) {
            int n = 0;
            const char *p = wd + 1;
            while (p < brEnd && n < 8) {
                while (p < brEnd && (*p == ' ' || *p == ',' || *p == '\r' || *p == '\n')) p++;
                if (p >= brEnd || *p == ']') break;
                char *next = NULL;
                double val = strtod(p, &next);
                if (next == p) break;
                if (val < 0.25) val = 0.25;
                if (val > 4.0) val = 4.0;
                g->widths[n++] = val;
                p = next;
            }
        }
    }
}

static void LoadPanelCfg(void) {
    InitDefaultCfg();
    DWORD len = 0;
    char *json = ReadFileUtf8(g_panelPath, &len);
    if (!json) return;
    const char *end = json + len;

    const char *kbdPos = JsonVal(json, end, "Kbd");
    const char *topEnd = kbdPos ? kbdPos : end;
    g_cfg.masterBrightness = JsonInt(json, topEnd, "Brightness", 100);
    if (g_cfg.masterBrightness < 0) g_cfg.masterBrightness = 0;
    if (g_cfg.masterBrightness > 100) g_cfg.masterBrightness = 100;
    g_cfg.overlay = JsonBool(json, topEnd, "Overlay", g_cfg.overlay);
    g_cfg.link    = JsonBool(json, topEnd, "Link", g_cfg.link);
    g_cfg.smooth  = JsonBool(json, topEnd, "Smooth", g_cfg.smooth);
    JsonStr(json, topEnd, "OnExit", g_cfg.onExit, sizeof(g_cfg.onExit));

    const char *barPos = JsonVal(json, end, "Bar");
    if (kbdPos) {
        const char *kEnd = (barPos && barPos > kbdPos) ? barPos : end;
        ParseGroupJson(kbdPos, kEnd, &g_cfg.kbd);
    }
    if (barPos) {
        ParseGroupJson(barPos, end, &g_cfg.bar);
    }
    free(json);
}

static void FormatGroupPanelJson(const GroupCfg *g, char *out, size_t cap) {
    char swBuf[256] = "", wdBuf[256] = "";
    size_t so = 0, wo = 0;
    for (int i = 0; i < g->swatchCount; i++) {
        char hx[8];
        FormatHexColor(g->swatches[i], hx);
        so += (size_t)snprintf(swBuf + so, sizeof(swBuf) - so, "%s\"%s\"", i ? ", " : "", hx);
        wo += (size_t)snprintf(wdBuf + wo, sizeof(wdBuf) - wo, "%s%.3f", i ? ", " : "", g->widths[i]);
    }
    snprintf(out, cap,
        "  \"%s\": {\r\n"
        "    \"On\": %s,\r\n"
        "    \"Effect\": \"%s\",\r\n"
        "    \"Speed\": %d,\r\n"
        "    \"Brightness\": %d,\r\n"
        "    \"Mirror\": %s,\r\n"
        "    \"Reverse\": %s,\r\n"
        "    \"Equalise\": %s,\r\n"
        "    \"Loop\": %s,\r\n"
        "    \"Swatches\": [ %s ],\r\n"
        "    \"Widths\": [ %s ]\r\n"
        "  }",
        (g == &g_cfg.kbd) ? "Kbd" : "Bar",
        g->on ? "true" : "false",
        kEffects[g->effectIdx].label,
        g->speed,
        g->brightness,
        g->mirror ? "true" : "false",
        g->reverse ? "true" : "false",
        g->equalise ? "true" : "false",
        g->loop ? "true" : "false",
        swBuf, wdBuf);
}

static void SavePanelCfg(void) {
    char kJson[1024], bJson[1024], custBuf[512] = "", full[3200];
    FormatGroupPanelJson(&g_cfg.kbd, kJson, sizeof(kJson));
    FormatGroupPanelJson(&g_cfg.bar, bJson, sizeof(bJson));
    size_t co = 0;
    for (int i = 0; i < 16; i++) {
        if (g_cfg.customColors[i] != RGB(255, 255, 255)) {
            co += (size_t)snprintf(custBuf + co, sizeof(custBuf) - co, "%s%lu",
                                   co ? ", " : "", (unsigned long)g_cfg.customColors[i]);
        }
    }
    snprintf(full, sizeof(full),
        "{\r\n"
        "  \"Brightness\": %d,\r\n"
        "  \"Overlay\": %s,\r\n"
        "  \"Link\": %s,\r\n"
        "  \"OnExit\": \"%s\",\r\n"
        "  \"Smooth\": %s,\r\n"
        "  \"Custom\": [ %s ],\r\n"
        "%s,\r\n"
        "%s\r\n"
        "}\r\n",
        g_cfg.masterBrightness,
        g_cfg.overlay ? "true" : "false",
        g_cfg.link ? "true" : "false",
        g_cfg.onExit[0] ? g_cfg.onExit : "off",
        g_cfg.smooth ? "true" : "false",
        custBuf, kJson, bJson);
    WriteAtomicUtf8(g_panelPath, full);
}

static void WriteLiveBrightness(void) {
    char buf[32];
    int lvl = g_cfg.masterBrightness * 10;
    if (lvl < 0) lvl = 0;
    if (lvl > 1000) lvl = 1000;
    snprintf(buf, sizeof(buf), "%d\r\n", lvl);
    WriteAtomicUtf8(g_livePath, buf);
}

/* Mirror external master-brightness changes (Fn F2 / F3 pressed while this
 * window is open or in tray) back into the panel. The engine owns the keys
 * and writes live.txt; anything we did not just write ourselves is adopted
 * here, so the slider, panel.json and the on-screen toast all follow along
 * exactly like screen-brightness keys drive the system OSD. */
static void ScheduleLiveApply(void);

static void OsdShow(int pct);

static void PollFnBrightness(void) {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastFnPoll < 150ULL) return;
    g_lastFnPoll = now;
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_livePath, &len);
    if (!txt) return;
    int val = atoi(txt);
    free(txt);
    if (val < 0) val = 0;
    if (val > 1000) val = 1000;
    int pct = (val + 5) / 10;
    if (pct == g_cfg.masterBrightness) return;
    /* Value moved without the slider: a Fn key (or another panel) changed it. */
    g_cfg.masterBrightness = pct;
    if (pct <= 0)
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  Off");
    else
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  %d%%", pct);
    g_toastAt = now + 1400ULL;
    OsdShow(pct);
    SavePanelCfg();
    ScheduleLiveApply();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* One-line plain-English explanation for every control, shown in the footer
 * while the mouse is over it. This is the built-in manual: no settings are
 * mysterious, even the subtle ones. */
static const wchar_t *HelpForHit(int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return NULL;
    int kind = g_hits[hitIdx].kind;
    switch (kind) {
    case HIT_HDR_LIGHTS:    return L"Master on/off for every light. The Fn F2 / F3 brightness keys still work while off.";
    case HIT_HDR_STARTUP:   return L"Launch Keyboard Lighting automatically when you sign in to Windows.";
    case HIT_HDR_RESTART:   return L"Restart the lighting engine - use if the lights ever freeze or after an update.";
    case HIT_SLIDER_MASTER: return L"Overall brightness of ALL lights, 0-100%. Same scale the Fn F2 / F3 keys move. 0% = full off.";
    case HIT_TAB_KBD:
    case HIT_TAB_BAR:       return L"Choose which zone the settings below edit. 'Link Zones' keeps both zones identical.";
    case HIT_TOG_ZONE_ON:   return L"Turn just this zone (Keyboard / Light Bar) on or off without losing its pattern.";
    case HIT_TOG_LINK:      return L"Link Zones: every change you make here is copied to the other zone automatically.";
    case HIT_PICK_EFFECT:   return L"Pick the animation pattern. Each entry lists what it does in this bar.";
    case HIT_SWATCH_ADD:    return L"Add another colour band to the palette (up to 8).";
    case HIT_SWATCH_REM:    return L"Remove the last colour band from the palette.";
    case HIT_SLIDER_SPEED:  return L"How fast the pattern moves: 0.1x (slow drift) to 5.0x (fast).";
    case HIT_SLIDER_BRIGHT: return L"Brightness of just this zone, 0-100%. 0% = this zone dark. Multiplies with Master.";
    case HIT_TOG_LOOP:      return L"Loop Travel: pattern circles the Light Bar ring. Off: it bounces back and forth.";
    case HIT_TOG_EQ:        return L"Balance Colours: evens out palette brightness so no single colour dominates.";
    case HIT_TOG_MIRROR:    return L"Mirror: the pattern runs out from the centre to both ends.";
    case HIT_TOG_REVERSE:   return L"Reverse: the pattern travels in the opposite direction.";
    case HIT_TOG_SMOOTH:    return L"Steady Colours: on = no shimmer at any level. Off = fine dithering for ultra-smooth dimming.";
    case HIT_TOG_OVERLAY:   return L"Battery Alerts: briefly show battery status on the lights (restarts engine when toggled).";
    case HIT_FOOT_SAVEP:    return L"Save the current colours and effect as a named pattern (presets.ini).";
    case HIT_FOOT_PATTERN:  return L"Open the pattern list - click any saved pattern to swap the whole look. Ctrl+Alt+Up/Down also step brightness on keys G-Helper does not touch.";
    case HIT_FOOT_ZONES:    return L"Open Zones Setup if patterns jump about - lets you fix which lamp is which.";
    case HIT_FOOT_ELEVATE:  return L"Restart as Administrator to unlock direct USB RGB control of the ROG Strix G16.";
    case HIT_FOOT_QUIT:     return L"Quit Keyboard Lighting and turn the lights off.";
    default:
        if (kind >= HIT_SWATCH_0 && kind < HIT_SWATCH_0 + 8)
            return L"Click to pick this colour band with the Windows colour picker.";
        if (kind >= HIT_WIDTH_0 && kind < HIT_WIDTH_0 + 8)
            return L"Drag to widen or narrow how much of the strip this colour takes.";
        return NULL;
    }
}

static void FormatThemeBlock(const GroupCfg *g, const char *name, char *out, size_t cap) {
    char cols[128] = "", wds[128] = "";
    size_t co = 0, wo = 0;
    for (int i = 0; i < g->swatchCount; i++) {
        char hx[8];
        FormatHexColor(g->swatches[i], hx);
        co += (size_t)snprintf(cols + co, sizeof(cols) - co, "%s%s", i ? "," : "", hx);
        wo += (size_t)snprintf(wds + wo, sizeof(wds) - wo, "%s%.3f", i ? "," : "", g->widths[i]);
    }
    snprintf(out, cap,
        "  \"%s\": {\r\n"
        "    \"On\": %s,\r\n"
        "    \"Effect\": \"%s\",\r\n"
        "    \"Colors\": \"%s\",\r\n"
        "    \"Widths\": \"%s\",\r\n"
        "    \"Speed\": %.2f,\r\n"
        "    \"Brightness\": %.3f,\r\n"
        "    \"Mirror\": %s,\r\n"
        "    \"Reverse\": %s,\r\n"
        "    \"Equalise\": %s,\r\n"
        "    \"Loop\": %s\r\n"
        "  }",
        name,
        g->on ? "true" : "false",
        kEffects[g->effectIdx].token,
        cols, wds,
        (double)g->speed / 10.0,
        (double)g->brightness / 100.0,
        g->mirror ? "true" : "false",
        g->reverse ? "true" : "false",
        g->equalise ? "true" : "false",
        g->loop ? "true" : "false");
}

static void WriteThemeJson(void) {
    char kb[768], bb[768], full[2048];
    FormatThemeBlock(&g_cfg.kbd, "Kbd", kb, sizeof(kb));
    FormatThemeBlock(&g_cfg.bar, "Bar", bb, sizeof(bb));
    snprintf(full, sizeof(full),
        "{\r\n"
        "  \"Brightness\": %.3f,\r\n"
        "  \"Smooth\": %s,\r\n"
        "%s,\r\n"
        "%s\r\n"
        "}\r\n",
        (double)g_cfg.masterBrightness / 100.0,
        g_cfg.smooth ? "true" : "false",
        kb, bb);
    WriteAtomicUtf8(g_themePath, full);
}

static void SyncLinkedZone(void) {
    if (!g_cfg.link) return;
    GroupCfg *src = CurGroup();
    GroupCfg *dst = (g_tab == 1) ? &g_cfg.kbd : &g_cfg.bar;
    *dst = *src;
}

static void WriteHeartbeat(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu\r\n", (unsigned long)GetCurrentProcessId());
    HANDLE hf = CreateFileW(g_lockPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(hf, buf, (DWORD)strlen(buf), &wr, NULL);
        CloseHandle(hf);
    }
}

/* ============================================================================
 *  Engine Lifecycle & Automatic Standalone GitHub Bootstrap
 * ============================================================================ */

static BOOL IsEngineAlive(void) {
    if (!g_engineProc) return FALSE;
    DWORD code = 0;
    if (GetExitCodeProcess(g_engineProc, &code) && code == STILL_ACTIVE) return TRUE;
    CloseHandle(g_engineProc);
    g_engineProc = NULL;
    g_enginePid = 0;
    return FALSE;
}

static void KillPidFromFile(const wchar_t *path) {
    char *txt = ReadFileUtf8(path, NULL);
    if (txt) {
        unsigned long pid = strtoul(txt, NULL, 10);
        free(txt);
        if (pid > 0 && pid != GetCurrentProcessId()) {
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
        }
    }
    DeleteFileW(path);
}

static void StopEngine(BOOL graceful) {
    if (graceful && (IsEngineAlive() || FileExistsW(g_engPidPath))) {
        WriteAtomicUtf8(g_stopPath, "1");
        if (g_engineProc) WaitForSingleObject(g_engineProc, 450);
        else Sleep(200);
    }
    if (g_engineProc) {
        TerminateProcess(g_engineProc, 0);
        CloseHandle(g_engineProc);
        g_engineProc = NULL;
        g_enginePid = 0;
    }
    KillPidFromFile(g_engPidPath);
    DeleteFileW(g_stopPath);
}

static BOOL DownloadRepoFileWinHttp(const wchar_t *branch, const wchar_t *relName, const wchar_t *dest) {
    wchar_t urlPath[512];
    _snwprintf(urlPath, 511, L"/%s/%s/%s/keyboard-lighting/%s",
               REPO_OWNER_W, REPO_NAME_W, branch, relName);

    HINTERNET hSess = WinHttpOpen(L"Tests-KeyboardLighting/2.5",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return FALSE;
    WinHttpSetTimeouts(hSess, 6000, 8000, 12000, 20000);
    HINTERNET hConn = WinHttpConnect(hSess, L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return FALSE; }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return FALSE; }

    BOOL ok = FALSE;
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, NULL)) {
        DWORD status = 0, ssz = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &ssz, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            DWORD cap = 65536, len = 0;
            BYTE *buf = (BYTE *)malloc(cap);
            for (; buf;) {
                DWORD avail = 0, rd = 0;
                if (!WinHttpQueryDataAvailable(hReq, &avail) || avail == 0) break;
                if (len + avail + 1 > cap) {
                    while (len + avail + 1 > cap) cap *= 2;
                    BYTE *nb = (BYTE *)realloc(buf, cap);
                    if (!nb) { free(buf); buf = NULL; break; }
                    buf = nb;
                }
                if (!WinHttpReadData(hReq, buf + len, avail, &rd) || rd == 0) break;
                len += rd;
            }
            if (buf && len > 64) {
                wchar_t tmp[MAX_PATH];
                _snwprintf(tmp, MAX_PATH, L"%s.tmp", dest);
                HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hf != INVALID_HANDLE_VALUE) {
                    DWORD wr = 0;
                    if (WriteFile(hf, buf, len, &wr, NULL) && wr == len) {
                        CloseHandle(hf);
                        ok = MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING);
                    } else {
                        CloseHandle(hf);
                        DeleteFileW(tmp);
                    }
                }
            }
            free(buf);
        }
    }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return ok;
}

static DWORD WINAPI BootstrapEngineWorker(LPVOID unused) {
    (void)unused;
    const wchar_t *branches[2] = { L"main", FALLBACK_BRANCH_W };
    wchar_t dest[MAX_PATH];
    _snwprintf(dest, MAX_PATH, L"%s\\Aura-Background.ps1", g_cfgDir);
    BOOL got = FALSE;
    for (int b = 0; b < 2 && !got; b++) {
        got = DownloadRepoFileWinHttp(branches[b], L"Aura-Background.ps1", dest);
        if (got) {
            wchar_t z1[MAX_PATH], z2[MAX_PATH];
            _snwprintf(z1, MAX_PATH, L"%s\\Zones.ps1", g_cfgDir);
            _snwprintf(z2, MAX_PATH, L"%s\\Zones.bat", g_cfgDir);
            DownloadRepoFileWinHttp(branches[b], L"Zones.ps1", z1);
            DownloadRepoFileWinHttp(branches[b], L"Zones.bat", z2);
        }
    }
    InterlockedExchange(&g_downloading, 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_ENGINE_BOOTED, got ? 1 : 0, 0);
    return 0;
}

static void StartEngine(void) {
    StopEngine(FALSE);
    g_wantOff = FALSE;

    const wchar_t *script = NULL;
    if (FileExistsW(g_engineScript)) {
        script = g_engineScript;
    } else {
        static wchar_t cachedEngine[MAX_PATH];
        _snwprintf(cachedEngine, MAX_PATH, L"%s\\Aura-Background.ps1", g_cfgDir);
        if (FileExistsW(cachedEngine)) {
            script = cachedEngine;
        } else {
            if (InterlockedCompareExchange(&g_downloading, 1, 0) == 0) {
                wcsncpy(g_statusText, L"Downloading Aura-Background.ps1 engine from GitHub...", 255);
                HANDLE ht = CreateThread(NULL, 0, BootstrapEngineWorker, NULL, 0, NULL);
                if (ht) CloseHandle(ht);
            }
            return;
        }
    }

    WriteLiveBrightness();
    WriteThemeJson();

    char kCols[128] = "", kWds[128] = "", bCols[128] = "", bWds[128] = "", c0[8] = "#38BDF8", c1[8] = "#818CF8";
    size_t o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    for (int i = 0; i < g_cfg.kbd.swatchCount; i++) {
        char hx[8];
        FormatHexColor(g_cfg.kbd.swatches[i], hx);
        if (i == 0) strncpy(c0, hx, 7);
        if (i == 1) strncpy(c1, hx, 7);
        o1 += (size_t)snprintf(kCols + o1, sizeof(kCols) - o1, "%s%s", i ? "," : "", hx);
        o2 += (size_t)snprintf(kWds + o2, sizeof(kWds) - o2, "%s%.3f", i ? "," : "", g_cfg.kbd.widths[i]);
    }
    for (int i = 0; i < g_cfg.bar.swatchCount; i++) {
        char hx[8];
        FormatHexColor(g_cfg.bar.swatches[i], hx);
        o3 += (size_t)snprintf(bCols + o3, sizeof(bCols) - o3, "%s%s", i ? "," : "", hx);
        o4 += (size_t)snprintf(bWds + o4, sizeof(bWds) - o4, "%s%.3f", i ? "," : "", g_cfg.bar.widths[i]);
    }

    wchar_t cmd[2048];
    _snwprintf(cmd, 2047,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\""
        L" -Effect %S -Speed %.2f -Brightness %.3f -Master %.3f -Fps 60 -Quiet"
        L" -OwnerPid %lu -OnExit %S"
        L" -Colors \"%S\" -Widths \"%S\" -Color \"%S\" -Color2 \"%S\""
        L"%s%s -Equalise %s -Layout %s%s"
        L" -BarEffect %S -BarSpeed %.2f -BarBrightness %.3f"
        L" -BarColors \"%S\" -BarWidths \"%S\""
        L"%s%s -BarEqualise %s -BarLayout %s%s%s",
        script,
        kEffects[g_cfg.kbd.effectIdx].token,
        (double)g_cfg.kbd.speed / 10.0,
        (double)g_cfg.kbd.brightness / 100.0,
        (double)g_cfg.masterBrightness / 100.0,
        (unsigned long)GetCurrentProcessId(),
        g_cfg.onExit[0] ? g_cfg.onExit : "off",
        kCols, kWds, c0, c1,
        g_cfg.kbd.mirror ? L" -Mirror" : L"",
        g_cfg.kbd.reverse ? L" -Reverse" : L"",
        g_cfg.kbd.equalise ? L"on" : L"off",
        g_cfg.kbd.loop ? L"loop" : L"across",
        g_cfg.kbd.on ? L"" : L" -KbdOff",
        kEffects[g_cfg.bar.effectIdx].token,
        (double)g_cfg.bar.speed / 10.0,
        (double)g_cfg.bar.brightness / 100.0,
        bCols, bWds,
        g_cfg.bar.mirror ? L" -BarMirror" : L"",
        g_cfg.bar.reverse ? L" -BarReverse" : L"",
        g_cfg.bar.equalise ? L"on" : L"off",
        g_cfg.bar.loop ? L"loop" : L"across",
        g_cfg.bar.on ? L"" : L" -BarOff",
        g_cfg.overlay ? L" -OverlayOn" : L"");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_exeDir, &si, &pi)) {
        CloseHandle(pi.hThread);
        g_engineProc = pi.hProcess;
        g_enginePid  = pi.dwProcessId;
        char pidBuf[32];
        snprintf(pidBuf, sizeof(pidBuf), "%lu\r\n", (unsigned long)g_enginePid);
        WriteAtomicUtf8(g_engPidPath, pidBuf);
        _snwprintf(g_statusText, 255, L"Active  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
                   g_cfg.kbd.on ? kEffects[g_cfg.kbd.effectIdx].label : "Off",
                   g_cfg.bar.on ? kEffects[g_cfg.bar.effectIdx].label : "Off");
    } else {
        wcsncpy(g_statusText, L"Could not launch lighting engine process.", 255);
    }
}

static void TurnAllLightsOff(void) {
    StopEngine(TRUE);
    g_wantOff = TRUE;
    const wchar_t *script = FileExistsW(g_engineScript) ? g_engineScript : NULL;
    static wchar_t cached[MAX_PATH];
    if (!script) {
        _snwprintf(cached, MAX_PATH, L"%s\\Aura-Background.ps1", g_cfgDir);
        if (FileExistsW(cached)) script = cached;
    }
    if (script) {
        wchar_t cmd[768];
        _snwprintf(cmd, 767, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\" -Effect off -Quiet", script);
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_exeDir, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    wcsncpy(g_statusText, L"Lighting turned off  \x00B7  Click Lights toggle to resume", 255);
}

static BOOL QueryAutostart(void) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    wchar_t cmd[] = L"schtasks.exe /Query /TN \"KeyboardLighting\"";
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return FALSE;
    WaitForSingleObject(pi.hProcess, 2500);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (code == 0);
}

static BOOL SetKblAutostart(BOOL enable) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    wchar_t cmd[MAX_PATH * 2];
    if (enable) {
        _snwprintf(cmd, MAX_PATH * 2 - 1,
                   L"schtasks.exe /Create /TN \"KeyboardLighting\" /TR \"\\\"%s\\\" -Silent\" /SC ONLOGON %s /F",
                   g_exePath, g_isAdmin ? L"/RL HIGHEST" : L"");
    } else {
        _snwprintf(cmd, MAX_PATH * 2 - 1, L"schtasks.exe /Delete /TN \"KeyboardLighting\" /F");
    }
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return FALSE;
    WaitForSingleObject(pi.hProcess, 3500);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code == 0) {
        g_autostart = enable;
        return TRUE;
    }
    return FALSE;
}

static void ScheduleLiveApply(void) {
    /* Coalesce bursts but never hold a change back: once a change is queued
     * it goes out within ~40ms even if more edits keep arriving (each flush
     * re-queues the next one). The old code reset the deadline on every
     * mouse move, so drags applied only after you STOPPED - which is what
     * made the app feel laggy instead of real-time. */
    if (!g_applyPending) {
        g_applyPending = TRUE;
        g_applyAtTick  = GetTickCount64() + 40ULL;
    }
}

static void FlushLiveApply(void) {
    g_applyPending = FALSE;
    SavePanelCfg();
    if (g_wantOff) return;
    if (IsEngineAlive()) {
        WriteThemeJson();
        WriteLiveBrightness();
        _snwprintf(g_statusText, 255, L"Active  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
                   g_cfg.kbd.on ? kEffects[g_cfg.kbd.effectIdx].label : "Off",
                   g_cfg.bar.on ? kEffects[g_cfg.bar.effectIdx].label : "Off");
    } else {
        StartEngine();
    }
}

/* Light Bar physical order from the engine's layout.txt: the lamp ids as
   they sit around the real closed loop (the order established in Zones
   Setup), NOT the raw lamp-index order that frame.txt delivers. The ring
   preview must follow this order or a scrolling gradient jumps around the
   circle instead of travelling round it once. */
static int          g_barOrder[64];
static double       g_barQ[64];        /* 0..1 ring fraction, seam gap intact */
static int          g_barLayoutN = 0;
static ULONGLONG    g_layoutReadAt = 0;

static void ParseLayoutTxt(char *txt) {
    int order[64], n = 0, havePos = 0, ring = 0;
    double pos[64];
    for (char *q2 = txt; q2 && *q2; ) {
        char *nl = strchr(q2, '\n');
        if (nl) *nl = 0;
        size_t len = strlen(q2);
        while (len > 0 && (q2[len - 1] == '\r' || q2[len - 1] == ' ')) q2[--len] = 0;
        if (strncmp(q2, "bar=", 4) == 0) {
            const char *v = q2 + 4;
            n = 0;
            while (n < 64 && *v) {
                order[n++] = atoi(v);
                while (*v && *v != ',') v++;
                if (*v == ',') v++;
            }
        } else if (strncmp(q2, "barpos=", 7) == 0) {
            const char *v = q2 + 7;
            havePos = 0;
            while (havePos < 64 && *v) {
                pos[havePos++] = atof(v);
                while (*v && *v != ',') v++;
                if (*v == ',') v++;
            }
        } else if (strncmp(q2, "barring=", 8) == 0) {
            ring = atoi(q2 + 8) != 0;
        }
        q2 = nl ? nl + 1 : NULL;
    }
    if (n < 2) { g_barLayoutN = 0; return; }
    for (int k = 0; k < n; k++) g_barOrder[k] = order[k];
    if (havePos == n && ring) {
        /* barpos is re-normalised 0..1 across the endpoints, which would
           drop the last lamp ONTO the first one's angle. The engine's ring
           map leaves exactly one step of seam room: q = p * (n-1)/n. */
        for (int k = 0; k < n; k++)
            g_barQ[k] = pos[k] * (double)(n - 1) / (double)n;
    } else {
        for (int k = 0; k < n; k++) g_barQ[k] = (double)k / (double)n;
    }
    g_barLayoutN = n;
}

/* ---- M4/M5 hold-to-repeat ------------------------------
   G-Helper's Custom action fires ONCE per press (no key-up, no repeat).
   Three independent signals can reveal the rest of the physical gesture:
     1. The key may ALSO report as a normal keyboard key: Windows then
        sends typematic REPEAT keydowns and a key-up while it is held.
     2. RawInput keyboard reports (catches key usages that never become
        WM_KEYDOWN - VKey 255 vendor keys).
     3. The ASUS input HID stream (VID 0x0B05, report id 0x5A - the very
        stream G-Helper reads): its press frames may REPEAT while held,
        and some models send a RELEASE frame (byte1 == 0, like the ROG
        Ally) or press|0x80. A press+release pair gives TRUE hold with
        no repeats needed at all.
   All three feed one state machine: press starts a paced token stream
   (the engine turns each token into one 5% step), release stops it.
   Learned bindings persist in presets.ini [Keys] M4Scan=/M5Scan=/M4Hid=/
   M5Hid=. Without any hold signal, behavior stays a clean tap per press. */
typedef struct KeyRec { ULONGLONG tick; DWORD scan; BYTE down; BYTE injected; } KeyRec;
static KeyRec    g_keyRing[256];
static int       g_keyRingHead = 0;
typedef struct HidRec { ULONGLONG tick; DWORD code; BYTE rel; } HidRec;
static HidRec    g_hidRing[256];
static int       g_hidRingHead = 0;
static HHOOK     g_kbHook = NULL;
static int       g_scanBound[2] = { 0, 0 };
static DWORD     g_hidCode[2] = { 0, 0 };
static BYTE      g_scanArmed[2] = { 1, 1 };
static BYTE      g_hidDown[2] = { 0, 0 };
static BYTE      g_holdOn[2] = { 0, 0 };
static BYTE      g_hidRelSeen[2] = { 0, 0 };
static ULONGLONG g_sigAt[2] = { 0, 0 };
static ULONGLONG g_scanNext[2] = { 0, 0 };
static int       g_learnCand[2] = { 0, 0 };
static int       g_learnN[2] = { 0, 0 };
static DWORD     g_learnHid[2] = { 0, 0 };
static int       g_learnHidN[2] = { 0, 0 };
static char      g_seenTok[2][32];

static int HasReleaseSrc(int dir) {
    return g_scanBound[dir] != 0 || g_hidRelSeen[dir] >= 2;
}

static void HoldTokenWrite(int dir) {
    char tok[32];
    _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
    tok[31] = '\0';
    WriteAtomicUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, tok);
}

/* One paced token per interval while the gesture is live. */
static void HoldPump(void) {
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < 2; i++) {
        if (!g_holdOn[i]) continue;
        if (!HasReleaseSrc(i) && now - g_sigAt[i] > 300ULL) {
            g_holdOn[i] = 0;
            continue;
        }
        if (now >= g_scanNext[i]) {
            g_scanNext[i] = now + 150;
            HoldTokenWrite(i);
        }
    }
}

static void PressSignal(int dir, int repeat, ULONGLONG now) {
    g_sigAt[dir] = now;
    if (repeat) {
        g_holdOn[dir] = 1;
    } else if (HasReleaseSrc(dir)) {
        /* Release is observable: stream for the whole hold. The press
           itself already stepped once via G-Helper's Custom token. */
        g_holdOn[dir] = 1;
        g_scanNext[dir] = now + 150;
    }
}

static void ReleaseSignal(int dir, ULONGLONG now) {
    (void)now;
    g_holdOn[dir] = 0;
    g_scanArmed[dir] = 1;
    if (g_hidDown[dir]) {
        g_hidDown[dir] = 0;
        if (g_hidRelSeen[dir] < 255) g_hidRelSeen[dir]++;
    }
}

static void OnScanEvent(DWORD scan, int down, int injected, ULONGLONG now) {
    if (injected) return;
    for (int i = 0; i < 2; i++) {
        if (g_scanBound[i] == 0 || (int)scan != g_scanBound[i]) continue;
        if (!down) {
            ReleaseSignal(i, now);
        } else if (g_scanArmed[i]) {
            /* Fresh press: G-Helper's Custom action writes this tap's
               token already. Gate briefly so a duplicate raw-input copy of
               this same down cannot double-step. */
            g_scanArmed[i] = 0;
            g_scanNext[i] = now + 120;
            PressSignal(i, 0, now);
        } else {
            PressSignal(i, 1, now);
        }
    }
}

static void OnRawHid(const BYTE *b, int len, ULONGLONG now) {
    if (len < 2 || b[0] != 0x5A) return;
    DWORD code = b[1];
    int rel = 0, mapped = -1;
    if (code == 0) {
        rel = 1;
        for (int i = 0; i < 2; i++) if (g_hidDown[i]) mapped = i;
    }
    for (int i = 0; i < 2; i++) {
        if (g_hidCode[i] != 0 && code == g_hidCode[i]) mapped = i;
        if (g_hidCode[i] != 0 && code == (g_hidCode[i] | 0x80)) { mapped = i; rel = 1; }
    }
    HidRec *r = &g_hidRing[g_hidRingHead];
    r->tick = now; r->code = code; r->rel = (BYTE)rel;
    g_hidRingHead = (g_hidRingHead + 1) & 255;
    if (mapped >= 0) {
        if (rel) {
            ReleaseSignal(mapped, now);
        } else {
            int repeat = g_hidDown[mapped];
            g_hidDown[mapped] = 1;
            PressSignal(mapped, repeat, now);
        }
    }
}

static void LearnFromRing(int dir) {
    ULONGLONG now = GetTickCount64();
    int cand = 0;
    for (int i = 0; i < 256; i++) {
        int idx = (g_keyRingHead + 255 - i) & 255;
        KeyRec *r = &g_keyRing[idx];
        if (r->tick == 0) break;
        if (now - r->tick > 1500ULL) break;
        if (!r->down || r->injected) continue;
        if (r->scan == 0x1D || r->scan == 0x2A || r->scan == 0x36 || r->scan == 0x38) continue;
        cand = (int)r->scan;
        break;
    }
    if (cand) {
        if (g_learnCand[dir] == cand) {
            g_learnN[dir]++;
            if (g_learnN[dir] >= 2 && g_scanBound[dir] != cand) {
                wchar_t num[16], path[MAX_PATH];
                g_scanBound[dir] = cand;
                g_scanArmed[dir] = 1;
                _snwprintf(num, 15, L"%d", cand);
                num[15] = L'\0';
                _snwprintf(path, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
                WritePrivateProfileStringW(L"Keys", dir == 0 ? L"M4Scan" : L"M5Scan", num, path);
            }
        } else {
            g_learnCand[dir] = cand;
            g_learnN[dir] = 1;
        }
    }
    {
        DWORD hc = 0;
        for (int i = 0; i < 256; i++) {
            int idx = (g_hidRingHead + 255 - i) & 255;
            HidRec *r = &g_hidRing[idx];
            if (r->tick == 0) break;
            if (now - r->tick > 1500ULL) break;
            if (r->rel || r->code == 0) continue;
            hc = r->code;
            break;
        }
        if (hc) {
            if (g_learnHid[dir] == hc) {
                g_learnHidN[dir]++;
                if (g_learnHidN[dir] >= 2 && g_hidCode[dir] != hc) {
                    wchar_t num[16], path[MAX_PATH];
                    g_hidCode[dir] = hc;
                    _snwprintf(num, 15, L"%lu", (unsigned long)hc);
                    num[15] = L'\0';
                    _snwprintf(path, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
                    WritePrivateProfileStringW(L"Keys", dir == 0 ? L"M4Hid" : L"M5Hid", num, path);
                }
            } else {
                g_learnHid[dir] = hc;
                g_learnHidN[dir] = 1;
            }
        }
    }
}

static void WatchBrightTokens(void) {
    for (int dir = 0; dir < 2; dir++) {
        char cur[32] = { 0 };
        DWORD len = 0;
        char *txt = ReadFileUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, &len);
        if (txt) { strncpy(cur, txt, 31); free(txt); }
        if (cur[0] && strcmp(cur, g_seenTok[dir]) != 0) {
            int fresh = g_seenTok[dir][0] != '\0';
            strncpy(g_seenTok[dir], cur, 31);
            if (fresh) LearnFromRing(dir);
        }
    }
}

static LRESULT CALLBACK KbHoldProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN ||
                              wp == WM_KEYUP || wp == WM_SYSKEYUP)) {
        const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
        int down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
        int injected = (k->flags & LLKHF_INJECTED) != 0;
        ULONGLONG now = GetTickCount64();
        KeyRec *r = &g_keyRing[g_keyRingHead];
        r->tick = now; r->scan = k->scanCode;
        r->down = (BYTE)down; r->injected = (BYTE)injected;
        g_keyRingHead = (g_keyRingHead + 1) & 255;
        OnScanEvent(k->scanCode, down, injected, now);
    }
    return CallNextHookEx(g_kbHook, code, wp, lp);
}

static void InitRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid[16];
    UINT n = 0;
    rid[n].usUsagePage = 0x01; rid[n].usUsage = 0x06;
    rid[n].dwFlags = RIDEV_INPUTSINK; rid[n].hwndTarget = hwnd;
    n++;
    UINT num = 0;
    GetRawInputDeviceList(NULL, &num, sizeof(RAWINPUTDEVICELIST));
    if (num > 0 && num < 64) {
        RAWINPUTDEVICELIST list[64];
        num = GetRawInputDeviceList(list, &num, sizeof(RAWINPUTDEVICELIST));
        for (UINT i = 0; i < num && n < 16; i++) {
            RID_DEVICE_INFO info;
            UINT cb = sizeof(info);
            info.cbSize = sizeof(info);
            if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &cb) != (UINT)-1 &&
                info.dwType == RIM_TYPEHID && info.hid.dwVendorId == 0x0B05) {
                UINT j; int dup = 0;
                for (j = 1; j < n; j++)
                    if (rid[j].usUsagePage == info.hid.usUsagePage && rid[j].usUsage == info.hid.usUsage) dup = 1;
                if (!dup) {
                    rid[n].usUsagePage = info.hid.usUsagePage;
                    rid[n].usUsage = info.hid.usUsage;
                    rid[n].dwFlags = RIDEV_INPUTSINK;
                    rid[n].hwndTarget = hwnd;
                    n++;
                }
            }
        }
    }
    RegisterRawInputDevices(rid, n, sizeof(RAWINPUTDEVICE));
}

static void OnRawInputMsg(HWND hwnd, LPARAM lp) {
    (void)hwnd;
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    if (dwSize == 0 || dwSize >= 4096) return;
    BYTE buf[4096];
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) return;
    const RAWINPUT *ri = (const RAWINPUT *)buf;
    ULONGLONG now = GetTickCount64();
    if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        int down = (ri->data.keyboard.Flags & RI_KEY_BREAK) == 0;
        KeyRec *r = &g_keyRing[g_keyRingHead];
        r->tick = now; r->scan = ri->data.keyboard.MakeCode;
        r->down = (BYTE)down; r->injected = 0;
        g_keyRingHead = (g_keyRingHead + 1) & 255;
        OnScanEvent(ri->data.keyboard.MakeCode, down, 0, now);
    } else if (ri->header.dwType == RIM_TYPEHID) {
        OnRawHid(ri->data.hid.bRawData, (int)ri->data.hid.dwSizeHid, now);
    }
}

/* ---- Native-style brightness OSD (sun + bar + percent) -------------
   A small top-centre popup that appears whenever the keyboard brightness
   changes from outside the window (M4/M5, Fn keys, other panels) and
   fades out after ~1.4s - the same feel as the Windows OSD. */
static HWND g_osdHwnd = NULL;
static int  g_osdPct = -1;

static LRESULT CALLBACK OsdWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == 3) { KillTimer(hwnd, 3); ShowWindow(hwnd, SW_HIDE); }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int W = rc.right, H = rc.bottom;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ oldBm = SelectObject(mem, bmp);
        HBRUSH bg = CreateSolidBrush(RGB(32, 35, 42));
        HPEN bgPen = CreatePen(PS_SOLID, 1, RGB(32, 35, 42));
        HGDIOBJ oldBr = SelectObject(mem, bg);
        HGDIOBJ oldPen = SelectObject(mem, bgPen);
        RoundRect(mem, 0, 0, W, H, H / 5, H / 5);
        int pad = H * 22 / 100;
        int sunR = H * 16 / 100;
        int sunCx = pad + sunR * 3, sunCy = H / 2;
        HPEN sunPen = CreatePen(PS_SOLID, H / 22, RGB(232, 236, 245));
        SelectObject(mem, sunPen);
        HBRUSH sunBr = CreateSolidBrush(RGB(232, 236, 245));
        SelectObject(mem, sunBr);
        Ellipse(mem, sunCx - sunR, sunCy - sunR, sunCx + sunR, sunCy + sunR);
        {
            int rayIn = sunR + sunR / 2, rayOut = sunR + sunR + sunR / 2;
            for (int a = 0; a < 8; a++) {
                double th = a * 0.7853981633974483;
                MoveToEx(mem, sunCx + (int)(rayIn * cos(th)), sunCy + (int)(rayIn * sin(th)), NULL);
                LineTo(mem, sunCx + (int)(rayOut * cos(th)), sunCy + (int)(rayOut * sin(th)));
            }
        }
        int barX = sunCx + H * 42 / 100;
        int barY = H * 38 / 100, barH = H * 24 / 100;
        int pctW = H * 38 / 100;
        int barW = W - barX - pctW - pad;
        HBRUSH trBr = CreateSolidBrush(RGB(62, 66, 74));
        HPEN trPen = CreatePen(PS_SOLID, 1, RGB(62, 66, 74));
        SelectObject(mem, trBr);
        SelectObject(mem, trPen);
        RoundRect(mem, barX, barY, barX + barW, barY + barH, barH, barH);
        int pct = g_osdPct;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        int fillW = (barW - 2) * pct / 100;
        if (fillW > 0 && pct > 0) {
            HBRUSH fBr = CreateSolidBrush(RGB(233, 226, 206));
            HPEN fPen = CreatePen(PS_SOLID, 1, RGB(233, 226, 206));
            SelectObject(mem, fBr);
            SelectObject(mem, fPen);
            RoundRect(mem, barX + 1, barY + 1, barX + 1 + fillW, barY + barH - 1, barH, barH);
            DeleteObject(fBr);
            DeleteObject(fPen);
        }
        {
            HFONT font = CreateFontW(-H * 26 / 100, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                     0, 0, 0, 0, 0, L"Segoe UI");
            HGDIOBJ oldF = SelectObject(mem, font);
            wchar_t t[16];
            _snwprintf(t, 15, L"%d%%", pct);
            t[15] = L'\0';
            RECT tr;
            SetRect(&tr, barX + barW + pad / 2, 0, W - pad / 2, H);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(232, 236, 245));
            DrawTextW(mem, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(mem, oldF);
            DeleteObject(font);
        }
        BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBr);
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBm);
        DeleteObject(bg); DeleteObject(bgPen);
        DeleteObject(sunBr); DeleteObject(sunPen);
        DeleteObject(trBr); DeleteObject(trPen);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OsdCreate(HINSTANCE hInst) {
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OsdWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"KBLSuiteOsdWindow";
    RegisterClassExW(&wc);
    g_osdHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                wc.lpszClassName, L"", WS_POPUP,
                                0, 0, 10, 10, NULL, NULL, hInst, NULL);
    if (g_osdHwnd) SetLayeredWindowAttributes(g_osdHwnd, 0, 255, LWA_ALPHA);
}

static void OsdShow(int pct) {
    if (!g_osdHwnd) return;
    HDC sdc = GetDC(NULL);
    int dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(NULL, sdc);
    int W = MulDiv(380, dpi, 96), H = MulDiv(76, dpi, 96);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int x = (sw - W) / 2;
    int y = MulDiv(30, dpi, 96);
    g_osdPct = pct;
    SetWindowPos(g_osdHwnd, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_osdHwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(g_osdHwnd, NULL, FALSE);
    SetTimer(g_osdHwnd, 3, 1400, NULL);
}

static void PollLayoutTxt(void) {
    ULONGLONG now = GetTickCount64();
    if (now - g_layoutReadAt < 500ULL) return;
    g_layoutReadAt = now;
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_layoutPath, &len);
    if (txt) { ParseLayoutTxt(txt); free(txt); }
}

/* Read live lamp frame from Aura-Background.ps1 or synthesize accurate live palette preview */
static void PollLivePreviewFrame(void) {
    PollLayoutTxt();
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_framePath, &len);
    if (txt && len > 8) {
        char *semi = strchr(txt, ';');
        if (semi) {
            int n = atoi(txt);
            const char *hex = semi + 1;
            if (n >= 1 && n <= 64 && strlen(hex) >= (size_t)(n * 6)) {
                for (int i = 0; i < n; i++) {
                    char cbuf[8] = { '#', hex[i*6], hex[i*6+1], hex[i*6+2], hex[i*6+3], hex[i*6+4], hex[i*6+5], 0 };
                    g_liveCols[i] = ParseHexColor(cbuf, RGB(34, 39, 52));
                }
                g_liveKbdN = (n >= 4) ? 4 : n;
                g_liveBarN = (n > 4) ? (n - 4) : 0;
                g_hasLiveFrame = TRUE;
                g_lastFrameTick = GetTickCount64();
                free(txt);
                return;
            }
        }
    }
    free(txt);

    /* Smooth animated preview using exact active settings when off-hardware or starting */
    if (g_hasLiveFrame && GetTickCount64() - g_lastFrameTick < 2500ULL) return;
    g_hasLiveFrame = FALSE;
    g_liveKbdN = 4;
    g_liveBarN = 12;

    double phase = (double)(GetTickCount64() % 10000ULL) / 10000.0;
    for (int z = 0; z < 16; z++) {
        const GroupCfg *g = (z < 4) ? &g_cfg.kbd : &g_cfg.bar;
        double u = (z < 4) ? ((double)z / 4.0) : ((double)(z - 4) / 12.0);
        /* Follow the established ring order so the synthesised gradient
           also travels round the circle instead of jumping at the seam. */
        if (z >= 4) {
            for (int k = 0; k < g_barLayoutN; k++) {
                if (g_barOrder[k] == z) { u = g_barQ[k]; break; }
            }
        }
        if (g_wantOff || !g->on) {
            g_liveCols[z] = RGB(24, 28, 38);
            continue;
        }
        double spd = (double)g->speed / 10.0;
        double pos = fmod(u + phase * spd * (g->reverse ? -1.0 : 1.0) + 10.0, 1.0);
        COLORREF base;
        if (!kEffects[g->effectIdx].usesPalette) {
            double h6 = pos * 6.0;
            int hi = (int)floor(h6) % 6;
            double f = h6 - floor(h6);
            int q = (int)(255.0 * (1.0 - f)), t = (int)(255.0 * f);
            switch (hi) {
            case 0: base = RGB(255, t, 0); break;
            case 1: base = RGB(q, 255, 0); break;
            case 2: base = RGB(0, 255, t); break;
            case 3: base = RGB(0, q, 255); break;
            case 4: base = RGB(t, 0, 255); break;
            default: base = RGB(255, 0, q); break;
            }
        } else {
            int n = g->swatchCount > 0 ? g->swatchCount : 1;
            double scaled = pos * n;
            int i0 = ((int)floor(scaled)) % n;
            int i1 = (i0 + 1) % n;
            double f = scaled - floor(scaled);
            COLORREF c0 = g->swatches[i0], c1 = g->swatches[i1];
            base = RGB(
                (int)(GetRValue(c0) + (GetRValue(c1) - GetRValue(c0)) * f),
                (int)(GetGValue(c0) + (GetGValue(c1) - GetGValue(c0)) * f),
                (int)(GetBValue(c0) + (GetBValue(c1) - GetBValue(c0)) * f));
        }
        double dim = ((double)g_cfg.masterBrightness / 100.0) * ((double)g->brightness / 100.0);
        g_liveCols[z] = RGB((int)(GetRValue(base) * dim),
                            (int)(GetGValue(base) * dim),
                            (int)(GetBValue(base) * dim));
    }
}

/* ============================================================================
 *  Sub-Pixel Anti-Aliased Smooth Rounded UI Renderer (32-bit DIBSection)
 * ============================================================================ */

static inline uint32_t PackRgb(COLORREF c) {
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static inline uint32_t BlendRgb(uint32_t dst, uint32_t src, int a256) {
    if (a256 <= 0) return dst;
    if (a256 >= 256) return src;
    int inv = 256 - a256;
    uint32_t rb = (((dst & 0x00FF00FFu) * (uint32_t)inv) + ((src & 0x00FF00FFu) * (uint32_t)a256)) >> 8;
    uint32_t g  = (((dst & 0x0000FF00u) * (uint32_t)inv) + ((src & 0x0000FF00u) * (uint32_t)a256)) >> 8;
    return (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void FillCanvasRect(uint32_t *px, int W, int H, int x0, int y0, int x1, int y1, COLORREF col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t p = PackRgb(col);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; x++) row[x] = p;
    }
}

static void SmoothRoundRect(uint32_t *px, int W, int H, const RECT *rc, float radius,
                            COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    float x0 = (float)rc->left, y0 = (float)rc->top;
    float x1 = (float)rc->right, y1 = (float)rc->bottom;
    float rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0.5f || rh <= 0.5f) return;
    if (radius * 2.0f > rw) radius = rw * 0.5f;
    if (radius * 2.0f > rh) radius = rh * 0.5f;

    float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    float hx = 0.5f * rw - radius, hy = 0.5f * rh - radius;

    int iy0 = (int)floorf(y0 - 1.0f), iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f), ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP = PackRgb(fillCol), borderP = PackRgb(borderCol);
    BOOL sameBorder = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd = (qx > 0.0f && qy > 0.0f) ? (sqrtf(qx * qx + qy * qy) - radius)
                                                 : ((qx > qy ? qx : qy) - radius);
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) row[x] = c;
            else row[x] = BlendRgb(row[x], c, (int)((0.5f - sd) * 256.0f));
        }
    }
}

static void SmoothCircle(uint32_t *px, int W, int H, float cx, float cy, float radius,
                         COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    if (radius <= 0.5f) return;
    int iy0 = (int)floorf(cy - radius - 1.0f), iy1 = (int)ceilf(cy + radius + 1.0f);
    int ix0 = (int)floorf(cx - radius - 1.0f), ix1 = (int)ceilf(cx + radius + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP = PackRgb(fillCol), borderP = PackRgb(borderCol);
    BOOL sameBorder = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float dy = ((float)y + 0.5f) - cy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float sd = sqrtf(dx * dx + dy * dy) - radius;
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) row[x] = c;
            else row[x] = BlendRgb(row[x], c, (int)((0.5f - sd) * 256.0f));
        }
    }
}

/* Thin annulus: the track that makes the Light Bar read as one closed loop. */
static void SmoothRingTrack(uint32_t *px, int W, int H, float cx, float cy,
                            float radius, float thick, COLORREF col,
                            int clipTop, int clipBottom) {
    if (radius <= 0.5f || thick <= 0.5f) return;
    float outer = radius + thick * 0.5f;
    int iy0 = (int)floorf(cy - outer - 1.0f), iy1 = (int)ceilf(cy + outer + 1.0f);
    int ix0 = (int)floorf(cx - outer - 1.0f), ix1 = (int)ceilf(cx + outer + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t p = PackRgb(col);
    for (int y = iy0; y < iy1; y++) {
        float dy = ((float)y + 0.5f) - cy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float sd = fabsf(sqrtf(dx * dx + dy * dy) - radius) - thick * 0.5f;
            if (sd >= 0.5f) continue;
            if (sd <= -0.5f) row[x] = p;
            else row[x] = BlendRgb(row[x], p, (int)((0.5f - sd) * 256.0f));
        }
    }
}

/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1
   from tools/generate_suite_icons.py (trispoke_sdf_alpha + header
   gradient). The mark is drawn straight at cache resolution, so the
   spinning emblem is razor sharp at any DPI with no bitmap upscaling. */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

static void EmbRasterize(uint32_t *dst, int n, COLORREF bg) {
    (void)bg;
    const double scale = EMB_SDF_SCALE, b = EMB_SDF_BOOST;
    const double pxSize = 2.0 / (double)n;
    const double aa = (pxSize * 1.35 > 1e-4) ? (pxSize * 1.35) : 1e-4;
    const double deg = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double nrm[3] = { 90.0, -150.0, -30.0 };
    for (int j = 0; j < n; j++) {
        double y0 = ((double)j + 0.5) / (double)n;
        double y = -1.0 + 2.0 * y0;
        for (int i = 0; i < n; i++) {
            double x0 = ((double)i + 0.5) / (double)n;
            double x = -1.0 + 2.0 * x0;
            double r = sqrt(x * x + y * y) / scale;
            double ang = atan2(y, x) / deg;
            double dOuter = fabs(r - 0.895) - (0.022 + b * 0.45);
            double dRad   = fabs(r - 0.742) - (0.056 + b * 0.5);
            double dGap = 180.0;
            for (int k = 0; k < 3; k++) {
                double m = fmod(ang - gaps[k] + 180.0, 360.0);
                if (m < 0) m += 360.0;
                double diff = fabs(m - 180.0);
                if (diff < dGap) dGap = diff;
            }
            double dCut  = (12.5 - dGap) * deg * 0.742;
            double dArcs = (dRad > dCut) ? dRad : dCut;
            double dSpokes = 10.0;
            double pMid = 0.052 + b * 0.12, pHw = 0.024 + b * 0.42;
            double sMid = 0.5 * (0.258 + 0.722), sHw = 0.5 * (0.722 - 0.258);
            for (int k = 0; k < 3; k++) {
                double rad = gaps[k] * deg;
                double ux = cos(rad), uy = sin(rad);
                double proj = (x * ux + y * uy) / scale;
                double perp = fabs(-x * uy + y * ux) / scale;
                double dAlong = fabs(proj - sMid) - sHw;
                double dBars  = fabs(perp - pMid) - pHw;
                double dThis  = (dAlong > dBars) ? dAlong : dBars;
                if (dThis < dSpokes) dSpokes = dThis;
            }
            double dHubOut = r - (0.224 + b * 0.25);
            double dTri = -10.0;
            for (int k = 0; k < 3; k++) {
                double rad = nrm[k] * deg;
                double dHalf = (x * cos(rad) + y * sin(rad)) / scale - 0.074;
                if (dHalf > dTri) dTri = dHalf;
            }
            double dHub = (dHubOut > -dTri) ? dHubOut : -dTri;
            double u1 = (dOuter < dArcs) ? dOuter : dArcs;
            double u2 = (dSpokes < dHub) ? dSpokes : dHub;
            double u  = (u1 < u2) ? u1 : u2;
            double alpha = 0.5 - (u * scale) / aa;
            if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
            double t = x0 * 0.3 + y0 * 0.7;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            double fr = 246.0 * (1 - t) + 196.0 * t;
            double fg = 249.0 * (1 - t) + 212.0 * t;
            double fb = 254.0 * (1 - t) + 234.0 * t;
            /* Store colour + REAL alpha in the top byte; the blit blends
               over whatever is behind, so no flat cache box can ever show
               as a demarcation square around the emblem. */
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

/* Emblem cache: rasterized once at 4x by the SDF renderer; every paint
   only rotates it with bilinear sampling - smooth at any angle, and
   nothing at all is allocated per frame. */
typedef struct EmbCache {
    int      sz;        /* on-screen size */
    int      srcSz;     /* 4x supersample */
    uint32_t *src;      /* srcSz * srcSz colour rows, pre-blended on bg */
} EmbCache;
static EmbCache g_embCache = { 0, 0, NULL };

static void EmbCacheEnsure(int sz, COLORREF bg) {
    if (g_embCache.src && g_embCache.sz == sz) return;
    free(g_embCache.src);
    g_embCache.src = NULL;
    g_embCache.sz = 0;
    if (sz < 8 || sz > 512) return;
    int srcSz = sz * 4;
    uint32_t *src = (uint32_t *)malloc((size_t)srcSz * srcSz * sizeof(uint32_t));
    if (!src) return;
    EmbRasterize(src, srcSz, bg);
    g_embCache.sz    = sz;
    g_embCache.srcSz = srcSz;
    g_embCache.src   = src;
}

static void EmbSpinDraw(uint32_t *px, int W, int H, int x, int y, int sz, COLORREF bg) {
    EmbCacheEnsure(sz, bg);
    const uint32_t *src = g_embCache.src;
    if (!src) return;
    int srcSz = g_embCache.srcSz;
    /* One full turn every 30 seconds: slow enough to read as alive,
       slow enough to never pull the eye off the controls. */
    double ang = ((double)(GetTickCount64() % 30000ULL) / 30000.0) * 6.283185307179586;
    double ca = cos(ang), sa = sin(ang);
    double c = (sz - 1) * 0.5;
    double sc = (double)(srcSz - 1) / (double)(sz > 1 ? sz : 1);
    int maxs = srcSz - 1;
    for (int dy = 0; dy < sz; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= H) continue;
        double v = dy - c;
        for (int dx = 0; dx < sz; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= W) continue;
            double u = dx - c;
            double fx = (ca * u + sa * v + c) * sc;
            double fy = (-sa * u + ca * v + c) * sc;
            if (fx < 0) fx = 0; else if (fx > maxs) fx = maxs;
            if (fy < 0) fy = 0; else if (fy > maxs) fy = maxs;
            int x0 = (int)fx, y0 = (int)fy;
            double ax = fx - x0, ay = fy - y0;
            double w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay);
            double w01 = (1 - ax) * ay,       w11 = ax * ay;
            const uint32_t *p00 = &src[y0 * srcSz + x0];
            uint32_t c00 = p00[0];
            uint32_t c10 = (x0 < maxs) ? p00[1] : c00;
            const uint32_t *p01 = (y0 < maxs) ? p00 + srcSz : p00;
            uint32_t c01 = p01[0];
            uint32_t c11 = (x0 < maxs) ? p01[1] : c01;
            double r = (c00 & 255) * w00 + (c10 & 255) * w10 + (c01 & 255) * w01 + (c11 & 255) * w11;
            double g = ((c00 >> 8) & 255) * w00 + ((c10 >> 8) & 255) * w10 +
                       ((c01 >> 8) & 255) * w01 + ((c11 >> 8) & 255) * w11;
            double b = ((c00 >> 16) & 255) * w00 + ((c10 >> 16) & 255) * w10 +
                       ((c01 >> 16) & 255) * w01 + ((c11 >> 16) & 255) * w11;
            double a = ((c00 >> 24) & 255) * w00 + ((c10 >> 24) & 255) * w10 +
                       ((c01 >> 24) & 255) * w01 + ((c11 >> 24) & 255) * w11;
            int ai = (int)(a + 0.5);
            uint32_t d = px[ty * W + tx];
            if (ai >= 255) {
                px[ty * W + tx] = RGB((int)(r + 0.5), (int)(g + 0.5), (int)(b + 0.5));
            } else if (ai > 0) {
                int dr = (int)(d & 255), dg = (int)((d >> 8) & 255), db = (int)((d >> 16) & 255);
                px[ty * W + tx] = RGB(((int)(r + 0.5) * ai + dr * (255 - ai) + 127) / 255,
                                      ((int)(g + 0.5) * ai + dg * (255 - ai) + 127) / 255,
                                      ((int)(b + 0.5) * ai + db * (255 - ai) + 127) / 255);
            }
        }
    }
}

static void DrawSuiteEmblemIcon(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

static void AddHit(RECT r, int kind, int index) {
    if (g_hitCount < 96) {
        g_hits[g_hitCount].rect  = r;
        g_hits[g_hitCount].kind  = kind;
        g_hits[g_hitCount].index = index;
        g_hitCount++;
    }
}

static void DrawSuiteBtn(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                         const wchar_t *label, int style, BOOL hot, int dpi, int ct, int cb) {
    COLORREF fill = hot ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border = hot ? COL_HOVER_RIM : COL_BTN_BORDER;
    COLORREF fg = COL_TEXT;
    if (style == 1) { fill = hot ? COL_ACCENT_HOV : COL_ACCENT_STRONG; border = fill; }
    else if (style == 2) { fill = hot ? RGB(88, 66, 30) : COL_WARN_BG; border = COL_WARN; fg = COL_WARN; }
    SmoothRoundRect(px, W, H, &r, (float)S(9, dpi), fill, border, ct, cb);
    SelectObject(hdc, font);
    SetTextColor(hdc, fg);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void DrawPillSwitch(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                           const wchar_t *label, BOOL isOn, BOOL hot, int dpi, int ct, int cb) {
    SmoothRoundRect(px, W, H, &r, (float)S(9, dpi),
                    hot ? COL_SEC_HOV : COL_SEC_BTN,
                    hot ? COL_HOVER_RIM : COL_BTN_BORDER, ct, cb);
    int tw = S(34, dpi), th = S(18, dpi);
    int tx = r.right - S(10, dpi) - tw;
    int ty = r.top + (r.bottom - r.top - th) / 2;
    RECT trk = { tx, ty, tx + tw, ty + th };
    SmoothRoundRect(px, W, H, &trk, (float)th * 0.5f,
                    isOn ? COL_ACCENT_STRONG : RGB(24, 28, 38),
                    isOn ? COL_ACCENT : COL_BORDER, ct, cb);
    int kd = th - S(6, dpi);
    int kx = isOn ? (trk.right - S(3, dpi) - kd) : (trk.left + S(3, dpi));
    int ky = trk.top + (th - kd) / 2;
    RECT knb = { kx, ky, kx + kd, ky + kd };
    COLORREF kc = isOn ? RGB(255, 255, 255) : COL_MUTED;
    SmoothRoundRect(px, W, H, &knb, (float)kd * 0.5f, kc, kc, ct, cb);

    SelectObject(hdc, font);
    SetTextColor(hdc, isOn ? COL_TEXT : COL_MUTED);
    RECT lr = { r.left + S(12, dpi), r.top, tx - S(6, dpi), r.bottom };
    DrawTextW(hdc, label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void DrawSmoothSlider(HDC hdc, uint32_t *px, int W, int H, HFONT fLabel, HFONT fVal,
                             RECT trackBox, const wchar_t *title, const wchar_t *valStr,
                             double norm01, BOOL hot, int dpi, int ct, int cb) {
    if (norm01 < 0.0) norm01 = 0.0;
    if (norm01 > 1.0) norm01 = 1.0;

    SelectObject(hdc, fLabel);
    SetTextColor(hdc, COL_MUTED);
    RECT lr = { trackBox.left, trackBox.top - S(19, dpi), trackBox.right - S(72, dpi), trackBox.top - S(1, dpi) };
    DrawTextW(hdc, title, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, fVal);
    SetTextColor(hdc, COL_ACCENT);
    RECT vr = { trackBox.right - S(72, dpi), trackBox.top - S(19, dpi), trackBox.right, trackBox.top - S(1, dpi) };
    DrawTextW(hdc, valStr, -1, &vr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int barH = S(8, dpi);
    int cy = (trackBox.top + trackBox.bottom) / 2;
    RECT groove = { trackBox.left, cy - barH / 2, trackBox.right, cy + barH / 2 };
    SmoothRoundRect(px, W, H, &groove, (float)barH * 0.5f, RGB(22, 26, 35), COL_BORDER, ct, cb);

    int thumbR = S(hot ? 9 : 8, dpi);
    int usable = (groove.right - groove.left) - thumbR * 2;
    if (usable < 1) usable = 1;
    int cx = groove.left + thumbR + (int)(norm01 * usable);

    RECT fillR = { groove.left, groove.top, cx, groove.bottom };
    SmoothRoundRect(px, W, H, &fillR, (float)barH * 0.5f, COL_ACCENT_STRONG, COL_ACCENT, ct, cb);

    RECT thumb = { cx - thumbR, cy - thumbR, cx + thumbR, cy + thumbR };
    SmoothRoundRect(px, W, H, &thumb, (float)thumbR, RGB(248, 250, 252), COL_ACCENT, ct, cb);
}

static void RenderKblWindow(HWND hwnd, HDC targetDc) {
    RECT cli;
    GetClientRect(hwnd, &cli);
    int W = cli.right - cli.left, H = cli.bottom - cli.top;
    if (W <= 0 || H <= 0) return;

    int dpi = GetWinDpi(hwnd);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
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
    HFONT fSmall = CreateFontW(-S(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    int M = S(28, dpi);
    int CW = W - M * 2;
    int headerH = S(76, dpi);
    int footerH = S(44, dpi);
    int footerTop = H - footerH;

    FillCanvasRect(px, W, H, 0, 0, W, footerTop, COL_BG);
    FillCanvasRect(px, W, H, 0, footerTop, W, H, COL_FOOTER_BG);

    g_hitCount = 0;

    /* 1. Unified Suite Header (matches Project Hub & Bluetooth Guard) */
    int embSz = S(52, dpi);
    DrawSuiteEmblemIcon(px, W, H, M, (headerH - embSz) / 2, embSz);

    int topH = S(34, dpi), topY = (headerH - topH) / 2;
    RECT btnRst  = { W - M - S(88, dpi), topY, W - M, topY + topH };
    RECT togAuto = { btnRst.left - S(130, dpi), topY, btnRst.left - S(8, dpi), topY + topH };
    RECT togLive = { togAuto.left - S(122, dpi), topY, togAuto.left - S(8, dpi), topY + topH };

    int iLive = g_hitCount; AddHit(togLive, HIT_HDR_LIGHTS, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togLive, L"Lights", !g_wantOff, g_hoverIdx == iLive, dpi, 0, headerH);

    int iAuto = g_hitCount; AddHit(togAuto, HIT_HDR_STARTUP, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togAuto, L"Startup", g_autostart, g_hoverIdx == iAuto, dpi, 0, headerH);

    int iRst = g_hitCount; AddHit(btnRst, HIT_HDR_RESTART, 0);
    DrawSuiteBtn(mem, px, W, H, fBtn, btnRst, L"Restart", 0, g_hoverIdx == iRst, dpi, 0, headerH);

    int hdrLeft = M + embSz + S(14, dpi);
    SelectObject(mem, fTitle);
    SetTextColor(mem, COL_TEXT);
    RECT rHdrT = { hdrLeft, S(13, dpi), togLive.left - S(12, dpi), S(40, dpi) };
    DrawTextW(mem, L"Keyboard Lighting", -1, &rHdrT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    wchar_t subHdr[160];
    _snwprintf(subHdr, 159, L"ROG Strix G16  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
               g_wantOff || !g_cfg.kbd.on ? "Off" : kEffects[g_cfg.kbd.effectIdx].label,
               g_wantOff || !g_cfg.bar.on ? "Off" : kEffects[g_cfg.bar.effectIdx].label);
    SelectObject(mem, fSub);
    SetTextColor(mem, COL_TELEMETRY);
    RECT rHdrS = { hdrLeft, S(40, dpi), togLive.left - S(12, dpi), S(60, dpi) };
    DrawTextW(mem, subHdr, -1, &rHdrS, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillCanvasRect(px, W, H, M, headerH, W - M, headerH + 1, COL_DIVIDER);

    /* Scrollable Cards Region */
    int listTop = headerH + S(12, dpi);
    int listBot = footerTop - S(2, dpi);
    int gap = S(12, dpi);
    int hCard1 = S(180, dpi), hCard2 = S(118, dpi), hCard3 = S(118, dpi), hCard4 = S(148, dpi);
    g_contentH = hCard1 + hCard2 + hCard3 + hCard4 + gap * 3 + S(16, dpi);
    int maxSc = g_contentH - (listBot - listTop);
    if (maxSc < 0) maxSc = 0;
    if (g_scrollY > maxSc) g_scrollY = maxSc;
    if (g_scrollY < 0) g_scrollY = 0;

    int saved = SaveDC(mem);
    IntersectClipRect(mem, 0, listTop, W, listBot);

    int y = listTop - g_scrollY;
    GroupCfg *cg = CurGroup();
    BOOL alive = IsEngineAlive();

    /* ---- CARD 1: Live ROG Strix G16 Preview + Master Brightness ---- */
    RECT c1 = { M, y, M + CW, y + hCard1 };
    SmoothRoundRect(px, W, H, &c1, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    COLORREF stCol = g_wantOff ? COL_IDLE_SLATE : (alive ? COL_GOOD : COL_WARN);
    COLORREF stBg  = g_wantOff ? COL_PILL_MUTED : (alive ? COL_GOOD_BG : COL_WARN_BG);
    const wchar_t *stTxt = g_wantOff ? L"Lights Off" : (alive ? L"Engine Active" : L"Preview Ready");

    RECT bar1 = { c1.left + S(1, dpi), c1.top + S(16, dpi), c1.left + S(5, dpi), c1.bottom - S(16, dpi) };
    SmoothRoundRect(px, W, H, &bar1, (float)S(2, dpi), stCol, stCol, listTop, listBot);

    int icoSz = S(38, dpi);
    HICON hAppIco = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP),
                                      IMAGE_ICON, icoSz, icoSz, LR_DEFAULTCOLOR);
    if (hAppIco) {
        DrawIconEx(mem, c1.left + S(16, dpi), c1.top + S(12, dpi), hAppIco, icoSz, icoSz, 0, NULL, DI_NORMAL);
        DestroyIcon(hAppIco);
    }

    SelectObject(mem, fCard);
    SetTextColor(mem, COL_TEXT);
    RECT c1Title = { c1.left + S(64, dpi), c1.top + S(14, dpi), c1.right - S(150, dpi), c1.top + S(38, dpi) };
    DrawTextW(mem, L"ROG Strix G16 Live Strip", -1, &c1Title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* Status Pill on Card 1 */
    SelectObject(mem, fBadge);
    SIZE bsz = {0, 0};
    GetTextExtentPoint32W(mem, stTxt, (int)wcslen(stTxt), &bsz);
    int pillW = bsz.cx + S(26, dpi), pillH = S(22, dpi);
    RECT pill1 = { c1.right - S(16, dpi) - pillW, c1.top + S(12, dpi), c1.right - S(16, dpi), c1.top + S(12, dpi) + pillH };
    SmoothRoundRect(px, W, H, &pill1, (float)pillH * 0.5f, stBg, stBg, listTop, listBot);
    int d6 = S(6, dpi);
    RECT pDot = { pill1.left + S(9, dpi), pill1.top + (pillH - d6)/2, pill1.left + S(9, dpi) + d6, pill1.top + (pillH - d6)/2 + d6 };
    SmoothRoundRect(px, W, H, &pDot, (float)d6 * 0.5f, stCol, stCol, listTop, listBot);
    RECT pTxt = { pill1.left + S(18, dpi), pill1.top, pill1.right - S(6, dpi), pill1.bottom };
    SetTextColor(mem, stCol);
    DrawTextW(mem, stTxt, -1, &pTxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* 4-Zone Keyboard strip + 12-Zone Light Bar ring. The bar is a closed
       loop on the real machine: its last lamp sits right next to its first,
       so the preview draws it as a ring (clockwise from the top). The old
       straight strip hid the seam and read the zones out of order. */
    int stripY = c1.top + S(82, dpi), stripH = S(28, dpi);
    int innerW = CW - S(32, dpi);
    int splitGap = S(12, dpi);
    int kbdStripW = (innerW - splitGap) * 32 / 100;
    int barStripW = innerW - splitGap - kbdStripW;
    int cellGap = S(3, dpi);

    int kn = g_liveKbdN > 0 ? g_liveKbdN : 4;
    for (int i = 0; i < kn; i++) {
        int x0 = c1.left + S(16, dpi) + (kbdStripW * i) / kn;
        int x1 = c1.left + S(16, dpi) + (kbdStripW * (i + 1)) / kn - cellGap;
        RECT zr = { x0, stripY, x1, stripY + stripH };
        SmoothRoundRect(px, W, H, &zr, (float)S(6, dpi), g_liveCols[i], COL_BORDER, listTop, listBot);
    }

    int bn = g_liveBarN > 0 ? g_liveBarN : 12;
    if (bn < 1) bn = 1;
    int barLeft = c1.left + S(16, dpi) + kbdStripW + splitGap;
    float ringCx = (float)(barLeft + barStripW / 2);
    float ringCy = (float)(c1.top + S(96, dpi));
    float ringR  = (float)S(44, dpi);
    float dotR   = (float)S(9, dpi);
    SmoothRingTrack(px, W, H, ringCx, ringCy, ringR, (float)S(3, dpi),
                    COL_BORDER, listTop, listBot);
    for (int i = 0; i < bn; i++) {
        /* Place each lamp at its established ring angle (layout.txt), so the
           dots show the real loop order the engine's effects travel through.
           Without the layout the ring falls back to even index order. */
        int lamp = kn + i;
        double frac = (double)i / (double)bn;
        if (i < g_barLayoutN) {
            lamp = g_barOrder[i];
            if (lamp < 0 || lamp >= 64) lamp = kn + i;
            frac = g_barQ[i];
        }
        double ang = -1.5707963267948966 + 6.283185307179586 * frac;
        float dx = ringCx + ringR * (float)cos(ang);
        float dy = ringCy + ringR * (float)sin(ang);
        SmoothCircle(px, W, H, dx, dy, dotR,
                     g_liveCols[lamp], COL_BORDER, listTop, listBot);
    }

    /* Master Brightness Slider */
    wchar_t mbVal[16];
    _snwprintf(mbVal, 15, L"%d%%", g_cfg.masterBrightness);
    RECT slMaster = { c1.left + S(16, dpi), c1.top + S(152, dpi), c1.right - S(16, dpi), c1.top + S(176, dpi) };
    int iMas = g_hitCount; AddHit(slMaster, HIT_SLIDER_MASTER, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slMaster, L"Master Brightness (Fn F2 / F3)", mbVal,
                     (double)g_cfg.masterBrightness / 100.0,
                     g_hoverIdx == iMas || g_dragKind == HIT_SLIDER_MASTER, dpi, listTop, listBot);

    y += hCard1 + gap;

    /* ---- CARD 2: Zone Tabs + Effect Picker ---- */
    RECT c2 = { M, y, M + CW, y + hCard2 };
    SmoothRoundRect(px, W, H, &c2, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    RECT segBox = { c2.left + S(16, dpi), c2.top + S(12, dpi), c2.left + S(256, dpi), c2.top + S(44, dpi) };
    SmoothRoundRect(px, W, H, &segBox, (float)S(10, dpi), RGB(24, 28, 38), COL_BORDER, listTop, listBot);
    RECT tab0 = { segBox.left + S(3, dpi), segBox.top + S(3, dpi), (segBox.left + segBox.right)/2 - S(1, dpi), segBox.bottom - S(3, dpi) };
    RECT tab1 = { (segBox.left + segBox.right)/2 + S(1, dpi), segBox.top + S(3, dpi), segBox.right - S(3, dpi), segBox.bottom - S(3, dpi) };
    int iT0 = g_hitCount; AddHit(tab0, HIT_TAB_KBD, 0);
    int iT1 = g_hitCount; AddHit(tab1, HIT_TAB_BAR, 0);
    if (g_tab == 0) {
        SmoothRoundRect(px, W, H, &tab0, (float)S(8, dpi), COL_ACCENT_STRONG, COL_ACCENT_STRONG, listTop, listBot);
        if (g_hoverIdx == iT1) SmoothRoundRect(px, W, H, &tab1, (float)S(8, dpi), COL_SEC_HOV, COL_SEC_HOV, listTop, listBot);
    } else {
        SmoothRoundRect(px, W, H, &tab1, (float)S(8, dpi), COL_ACCENT_STRONG, COL_ACCENT_STRONG, listTop, listBot);
        if (g_hoverIdx == iT0) SmoothRoundRect(px, W, H, &tab0, (float)S(8, dpi), COL_SEC_HOV, COL_SEC_HOV, listTop, listBot);
    }
    SelectObject(mem, fBtn);
    SetTextColor(mem, g_tab == 0 ? RGB(255,255,255) : COL_MUTED);
    DrawTextW(mem, L"Keyboard", -1, &tab0, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(mem, g_tab == 1 ? RGB(255,255,255) : COL_MUTED);
    DrawTextW(mem, L"Light Bar", -1, &tab1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT togZone = { c2.right - S(292, dpi), c2.top + S(12, dpi), c2.right - S(156, dpi), c2.top + S(44, dpi) };
    RECT togLink = { c2.right - S(148, dpi), c2.top + S(12, dpi), c2.right - S(16, dpi),  c2.top + S(44, dpi) };
    int iZO = g_hitCount; AddHit(togZone, HIT_TOG_ZONE_ON, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togZone, L"Zone On", cg->on, g_hoverIdx == iZO, dpi, listTop, listBot);
    int iLK = g_hitCount; AddHit(togLink, HIT_TOG_LINK, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togLink, L"Link Zones", g_cfg.link, g_hoverIdx == iLK, dpi, listTop, listBot);

    RECT pickR = { c2.left + S(16, dpi), c2.top + S(58, dpi), c2.left + S(296, dpi), c2.top + S(98, dpi) };
    int iPk = g_hitCount; AddHit(pickR, HIT_PICK_EFFECT, 0);
    SmoothRoundRect(px, W, H, &pickR, (float)S(10, dpi),
                    g_hoverIdx == iPk ? COL_SEC_HOV : COL_SEC_BTN,
                    g_hoverIdx == iPk ? COL_HOVER_RIM : COL_BTN_BORDER, listTop, listBot);
    wchar_t effLabelW[96];
    _snwprintf(effLabelW, 95, L"Pattern:  %S", kEffects[cg->effectIdx].label);
    SelectObject(mem, fBtn);
    SetTextColor(mem, COL_TEXT);
    RECT pickTxt = { pickR.left + S(14, dpi), pickR.top, pickR.right - S(28, dpi), pickR.bottom };
    DrawTextW(mem, effLabelW, -1, &pickTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT pickChev = { pickR.right - S(28, dpi), pickR.top, pickR.right - S(8, dpi), pickR.bottom };
    SetTextColor(mem, COL_ACCENT);
    DrawTextW(mem, L"\x25BE", -1, &pickChev, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    wchar_t hintW[160];
    _snwprintf(hintW, 159, L"%S", kEffects[cg->effectIdx].hint);
    SelectObject(mem, fSmall);
    SetTextColor(mem, COL_MUTED);
    RECT rHint = { pickR.right + S(14, dpi), pickR.top, c2.right - S(16, dpi), pickR.bottom };
    DrawTextW(mem, hintW, -1, &rHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    y += hCard2 + gap;

    /* ---- CARD 3: Interactive Colour Swatches + Band Width Bars ---- */
    RECT c3 = { M, y, M + CW, y + hCard3 };
    SmoothRoundRect(px, W, H, &c3, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    BOOL usesPal = kEffects[cg->effectIdx].usesPalette != 0;
    SelectObject(mem, fCard);
    SetTextColor(mem, COL_TEXT);
    RECT c3Title = { c3.left + S(16, dpi), c3.top + S(10, dpi), c3.left + S(200, dpi), c3.top + S(32, dpi) };
    DrawTextW(mem, L"Colour Palette", -1, &c3Title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, fSmall);
    SetTextColor(mem, COL_MUTED);
    RECT c3Sub = { c3.left + S(150, dpi), c3.top + S(10, dpi), c3.right - S(16, dpi), c3.top + S(32, dpi) };
    DrawTextW(mem, usesPal ? L"Click a swatch to pick colour  \x00B7  Drag bar beneath to adjust width"
                           : L"Selected pattern generates its own automatic colour palette",
              -1, &c3Sub, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int sx = c3.left + S(16, dpi), sy = c3.top + S(40, dpi), swSz = S(44, dpi);
    for (int i = 0; i < cg->swatchCount; i++) {
        RECT sr = { sx, sy, sx + swSz, sy + swSz };
        int hIdx = g_hitCount;
        if (usesPal) AddHit(sr, HIT_SWATCH_0 + i, i);
        COLORREF fillC = usesPal ? cg->swatches[i] : RGB(45, 50, 64);
        SmoothRoundRect(px, W, H, &sr, (float)S(11, dpi), fillC,
                        (usesPal && g_hoverIdx == hIdx) ? RGB(255, 255, 255) : COL_BTN_BORDER,
                        listTop, listBot);

        RECT wr = { sx, sy + swSz + S(6, dpi), sx + swSz, sy + swSz + S(16, dpi) };
        if (usesPal) AddHit(wr, HIT_WIDTH_0 + i, i);
        SmoothRoundRect(px, W, H, &wr, (float)S(5, dpi), RGB(22, 26, 35), COL_BORDER, listTop, listBot);
        double wt = log(cg->widths[i] / 0.25) / log(16.0);
        if (wt < 0.0) wt = 0.0;
        if (wt > 1.0) wt = 1.0;
        int fw = (int)(wt * swSz);
        if (fw < S(6, dpi)) fw = S(6, dpi);
        RECT wf = { wr.left, wr.top, wr.left + fw, wr.bottom };
        SmoothRoundRect(px, W, H, &wf, (float)S(5, dpi), usesPal ? COL_ACCENT : COL_IDLE_SLATE,
                        usesPal ? COL_ACCENT : COL_IDLE_SLATE, listTop, listBot);

        sx += swSz + S(10, dpi);
    }
    if (usesPal && cg->swatchCount < 8) {
        RECT addR = { sx, sy, sx + S(36, dpi), sy + swSz };
        int hAdd = g_hitCount; AddHit(addR, HIT_SWATCH_ADD, 0);
        DrawSuiteBtn(mem, px, W, H, fCard, addR, L"+", 0, g_hoverIdx == hAdd, dpi, listTop, listBot);
        sx += S(44, dpi);
    }
    if (usesPal && cg->swatchCount > 2) {
        RECT remR = { sx, sy, sx + S(36, dpi), sy + swSz };
        int hRem = g_hitCount; AddHit(remR, HIT_SWATCH_REM, 0);
        DrawSuiteBtn(mem, px, W, H, fCard, remR, L"-", 0, g_hoverIdx == hRem, dpi, listTop, listBot);
    }

    y += hCard3 + gap;

    /* ---- CARD 4: Speed, Zone Brightness & Motion Switches ---- */
    RECT c4 = { M, y, M + CW, y + hCard4 };
    SmoothRoundRect(px, W, H, &c4, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    int halfW = (CW - S(48, dpi)) / 2;
    wchar_t spdStr[16], brtStr[16];
    _snwprintf(spdStr, 15, L"%.1fx", (double)cg->speed / 10.0);
    _snwprintf(brtStr, 15, L"%d%%", cg->brightness);

    RECT slSpd = { c4.left + S(16, dpi), c4.top + S(32, dpi), c4.left + S(16, dpi) + halfW, c4.top + S(56, dpi) };
    int iSpd = g_hitCount; AddHit(slSpd, HIT_SLIDER_SPEED, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slSpd, L"Effect Speed", spdStr,
                     (double)(cg->speed - 1) / 49.0,
                     g_hoverIdx == iSpd || g_dragKind == HIT_SLIDER_SPEED, dpi, listTop, listBot);

    RECT slBrt = { c4.right - S(16, dpi) - halfW, c4.top + S(32, dpi), c4.right - S(16, dpi), c4.top + S(56, dpi) };
    int iBrt = g_hitCount; AddHit(slBrt, HIT_SLIDER_BRIGHT, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slBrt, L"Zone Brightness", brtStr,
                     (double)cg->brightness / 100.0,
                     g_hoverIdx == iBrt || g_dragKind == HIT_SLIDER_BRIGHT, dpi, listTop, listBot);

    int colW = (CW - S(48, dpi)) / 3;
    int r1Y = c4.top + S(66, dpi), r2Y = c4.top + S(104, dpi), thH = S(32, dpi);
    struct { int kind; const wchar_t *lbl; BOOL val; int row; int col; } opts[6] = {
        { HIT_TOG_LOOP,    L"Loop Travel",     cg->loop,      0, 0 },
        { HIT_TOG_EQ,      L"Balance Colours", cg->equalise,  0, 1 },
        { HIT_TOG_MIRROR,  L"Mirror",          cg->mirror,    0, 2 },
        { HIT_TOG_REVERSE, L"Reverse",         cg->reverse,   1, 0 },
        { HIT_TOG_SMOOTH,  L"Steady Colours",  g_cfg.smooth,  1, 1 },
        { HIT_TOG_OVERLAY, L"Battery Alerts",  g_cfg.overlay, 1, 2 }
    };
    for (int o = 0; o < 6; o++) {
        int ox = c4.left + S(16, dpi) + opts[o].col * (colW + S(8, dpi));
        int oy = (opts[o].row == 0) ? r1Y : r2Y;
        RECT orRc = { ox, oy, ox + colW, oy + thH };
        int hO = g_hitCount; AddHit(orRc, opts[o].kind, 0);
        DrawPillSwitch(mem, px, W, H, fBtn, orRc, opts[o].lbl, opts[o].val,
                       g_hoverIdx == hO, dpi, listTop, listBot);
    }

    RestoreDC(mem, saved);

    /* Unified Suite Footer */
    FillCanvasRect(px, W, H, 0, footerTop, W, footerTop + 1, COL_DIVIDER);
    COLORREF fCol = (!g_isAdmin) ? COL_WARN : (g_wantOff ? COL_IDLE_SLATE : COL_GOOD);
    int fdSz = S(8, dpi);
    RECT fdRc = { M, footerTop + (footerH - fdSz)/2, M + fdSz, footerTop + (footerH - fdSz)/2 + fdSz };
    SmoothRoundRect(px, W, H, &fdRc, (float)fdSz * 0.5f, fCol, fCol, footerTop, H);

    RECT quitR = { W - M - S(84, dpi), footerTop + S(7, dpi), W - M, H - S(8, dpi) };
    int iQuit = g_hitCount; AddHit(quitR, HIT_FOOT_QUIT, 0);
    DrawSuiteBtn(mem, px, W, H, fBtn, quitR, L"Quit", 0, g_hoverIdx == iQuit, dpi, footerTop, H);

    int rightLimit = quitR.left - S(8, dpi);
    if (!g_isAdmin) {
        RECT elR = { rightLimit - S(136, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iEl = g_hitCount; AddHit(elR, HIT_FOOT_ELEVATE, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, elR, L"Run as Admin", 2, g_hoverIdx == iEl, dpi, footerTop, H);
        rightLimit = elR.left - S(8, dpi);
    } else if (FileExistsW(g_zonesBat)) {
        RECT znR = { rightLimit - S(116, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iZn = g_hitCount; AddHit(znR, HIT_FOOT_ZONES, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, znR, L"Zones Setup", 0, g_hoverIdx == iZn, dpi, footerTop, H);
        rightLimit = znR.left - S(8, dpi);

        /* Named colour presets: Save snapshots the look, Pattern swaps it */
        RECT svR = { rightLimit - S(74, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iSv = g_hitCount; AddHit(svR, HIT_FOOT_SAVEP, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, svR, L"Save", 0, g_hoverIdx == iSv, dpi, footerTop, H);
        rightLimit = svR.left - S(8, dpi);

        RECT ptR = { rightLimit - S(158, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iPt = g_hitCount; AddHit(ptR, HIT_FOOT_PATTERN, 0);
        wchar_t ptLabel[48];
        if (g_presetIdx >= 0 && g_presetUsed[g_presetIdx])
            _snwprintf(ptLabel, 48, L"Pattern: %s", g_presetName[g_presetIdx]);
        else
            _snwprintf(ptLabel, 48, L"Pattern: -");
        DrawSuiteBtn(mem, px, W, H, fBtn, ptR, ptLabel, 0, g_hoverIdx == iPt, dpi, footerTop, H);
        rightLimit = ptR.left - S(8, dpi);
    }

    SelectObject(mem, fSub);
    SetTextColor(mem, (!g_isAdmin) ? COL_WARN : COL_MUTED);
    RECT fTxt = { M + S(16, dpi), footerTop, rightLimit - S(8, dpi), H };
    const wchar_t *helpTxt = HelpForHit(g_hoverIdx);
    DrawTextW(mem, (!g_isAdmin) ? L"Click 'Run as Admin' to unlock direct ROG Strix G16 USB RGB control"
                                : (helpTxt ? helpTxt : g_statusText),
              -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    /* Floating brightness toast (shown after Fn F2 / F3 presses) */
    if (g_toastAt > GetTickCount64() && g_toastText[0]) {
        SelectObject(mem, fBtn);
        SIZE tsz = { 0, 0 };
        GetTextExtentPoint32W(mem, g_toastText, (int)wcslen(g_toastText), &tsz);
        int tW = tsz.cx + S(40, dpi), tH = S(34, dpi);
        RECT toast = { (W - tW) / 2, footerTop - tH - S(14, dpi), (W + tW) / 2, footerTop - S(14, dpi) };
        SmoothRoundRect(px, W, H, &toast, (float)tH * 0.5f, RGB(28, 33, 45), COL_ACCENT, 0, H);
        SetTextColor(mem, COL_TEXT);
        RECT tt = { toast.left + S(16, dpi), toast.top, toast.right - S(16, dpi), toast.bottom };
        DrawTextW(mem, g_toastText, -1, &tt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    BitBlt(targetDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

    DeleteObject(fTitle); DeleteObject(fSub); DeleteObject(fCard);
    DeleteObject(fBtn); DeleteObject(fBadge); DeleteObject(fSmall);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

/* ============================================================================
 *  Mouse Drag, Color Picker, Menus & Window Procedure
 * ============================================================================ */

static void ApplySliderDrag(int kind, int idx, RECT r, int mouseX) {
    int w = r.right - r.left;
    if (w <= 2) return;
    double t = (double)(mouseX - r.left) / (double)w;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    GroupCfg *cg = CurGroup();
    if (kind == HIT_SLIDER_MASTER) {
        g_cfg.masterBrightness = (int)floor(t * 100.0 + 0.5);
        WriteLiveBrightness();
        ScheduleLiveApply();
    } else if (kind == HIT_SLIDER_SPEED) {
        cg->speed = 1 + (int)floor(t * 49.0 + 0.5);
        SyncLinkedZone();
        ScheduleLiveApply();
    } else if (kind == HIT_SLIDER_BRIGHT) {
        cg->brightness = (int)floor(t * 100.0 + 0.5);
        SyncLinkedZone();
        ScheduleLiveApply();
    } else if (kind >= HIT_WIDTH_0 && kind < HIT_WIDTH_0 + 8 && idx >= 0 && idx < cg->swatchCount) {
        cg->widths[idx] = 0.25 * pow(16.0, t);
        SyncLinkedZone();
        ScheduleLiveApply();
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void OpenEffectPopup(HWND hwnd, RECT anchor) {
    HMENU hMenu = CreatePopupMenu();
    GroupCfg *cg = CurGroup();
    for (int i = 0; i < EFFECT_COUNT; i++) {
        wchar_t lbl[64];
        _snwprintf(lbl, 63, L"%S", kEffects[i].label);
        AppendMenuW(hMenu, MF_STRING | (cg->effectIdx == i ? MF_CHECKED : 0),
                    (UINT_PTR)(IDM_EFF_BASE + i), lbl);
    }
    POINT pt = { anchor.left, anchor.bottom + 2 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

/* ---- live-preview colour picker -------------------------------------
 * The colour dialog now pushes every intermediate choice straight to the
 * lights while it is still open (timer hook reading the dialog's live
 * rgbResult). Choosing a colour is real-time: the keyboard follows the
 * dialog as you browse. Cancel restores whatever was showing before. */
static COLORREF g_pickOrig = 0;
static COLORREF g_pickLast = 0;
static int      g_pickIdx  = -1;
static BOOL     g_pickLive = FALSE;

static UINT_PTR CALLBACK ColorPickerHook(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)lp); /* CHOOSECOLORW */
        SetTimer(hDlg, 0x51C0, 70, NULL);
        return 0;
    }
    if (msg == WM_TIMER && wp == 0x51C0) {
        CHOOSECOLORW *cc = (CHOOSECOLORW *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        GroupCfg *cg = CurGroup();
        if (cc && g_pickIdx >= 0 && g_pickIdx < cg->swatchCount) {
            COLORREF c = cc->rgbResult & 0x00FFFFFF;
            if (c != g_pickLast) {
                g_pickLast = c;
                cg->swatches[g_pickIdx] = c;
                g_pickLive = TRUE;
                SyncLinkedZone();
                ScheduleLiveApply();
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    return 0;
}

static void PickSwatchColor(HWND hwnd, int idx) {
    GroupCfg *cg = CurGroup();
    if (idx < 0 || idx >= cg->swatchCount) return;

    CHOOSECOLORW cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = hwnd;
    cc.rgbResult    = cg->swatches[idx];
    cc.lpCustColors = g_cfg.customColors;
    cc.lpfnHook     = ColorPickerHook;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
    g_pickOrig = cg->swatches[idx];
    g_pickLast = cg->swatches[idx];
    g_pickIdx  = idx;
    g_pickLive = FALSE;
    if (ChooseColorW(&cc)) {
        cg->swatches[idx] = cc.rgbResult;
        SyncLinkedZone();
        FlushLiveApply();
        InvalidateRect(hwnd, NULL, FALSE);
    } else {
        if (g_pickLive) {
            cg->swatches[idx] = g_pickOrig;
            SyncLinkedZone();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            SavePanelCfg();
        }
    }
    g_pickIdx = -1;
}

static void RelaunchElevated(HWND hwnd) {
    StopEngine(TRUE);
    DeleteFileW(g_lockPath);
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize       = sizeof(sei);
    sei.hwnd         = hwnd;
    sei.lpVerb       = L"runas";
    sei.lpFile       = g_exePath;
    sei.lpParameters = L"-NoElevate";
    sei.lpDirectory  = g_exeDir;
    sei.nShow        = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) {
        g_quitting = TRUE;
        DestroyWindow(hwnd);
    } else {
        WriteHeartbeat();
        StartEngine();
    }
}

static void HandleHitAction(HWND hwnd, int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return;
    int kind = g_hits[hitIdx].kind;
    int idx  = g_hits[hitIdx].index;
    RECT rc  = g_hits[hitIdx].rect;
    GroupCfg *cg = CurGroup();

    if (kind == HIT_HDR_LIGHTS) {
        if (g_wantOff) StartEngine();
        else TurnAllLightsOff();
    } else if (kind == HIT_HDR_STARTUP) {
        SetKblAutostart(!g_autostart);
    } else if (kind == HIT_HDR_RESTART) {
        StartEngine();
    } else if (kind == HIT_TAB_KBD) {
        g_tab = 0;
    } else if (kind == HIT_TAB_BAR) {
        g_tab = 1;
    } else if (kind == HIT_TOG_ZONE_ON) {
        cg->on = !cg->on;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_LINK) {
        g_cfg.link = !g_cfg.link;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_PICK_EFFECT) {
        OpenEffectPopup(hwnd, rc);
        return;
    } else if (kind >= HIT_SWATCH_0 && kind < HIT_SWATCH_0 + 8) {
        PickSwatchColor(hwnd, idx);
        return;
    } else if (kind == HIT_SWATCH_ADD && cg->swatchCount < 8) {
        cg->swatches[cg->swatchCount] = RGB(255, 255, 255);
        cg->widths[cg->swatchCount]   = 1.0;
        cg->swatchCount++;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_SWATCH_REM && cg->swatchCount > 2) {
        cg->swatchCount--;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_LOOP) {
        cg->loop = !cg->loop;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_EQ) {
        cg->equalise = !cg->equalise;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_MIRROR) {
        cg->mirror = !cg->mirror;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_REVERSE) {
        cg->reverse = !cg->reverse;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_SMOOTH) {
        g_cfg.smooth = !g_cfg.smooth;
        FlushLiveApply();
    } else if (kind == HIT_TOG_OVERLAY) {
        g_cfg.overlay = !g_cfg.overlay;
        SavePanelCfg();
        if (!g_wantOff) StartEngine();
    } else if (kind == HIT_FOOT_SAVEP) {
        int slot = g_presetIdx;
        if (slot < 0) {
            slot = 0;
            for (int i = 0; i < PRESET_MAX; i++) {
                if (!g_presetUsed[i]) { slot = i; break; }
            }
        }
        SavePresetSlot(slot);
        _snwprintf(g_toastText, 95, L"Saved \x00B7 %s", g_presetName[slot]);
        g_toastAt = GetTickCount64() + 1400ULL;
    } else if (kind == HIT_FOOT_PATTERN) {
        OpenPatternPopup(hwnd, rc);
        return;
    } else if (kind == HIT_FOOT_ZONES) {
        ShellExecuteW(hwnd, L"open", g_zonesBat, NULL, g_exeDir, SW_SHOWNORMAL);
    } else if (kind == HIT_FOOT_ELEVATE) {
        RelaunchElevated(hwnd);
        return;
    } else if (kind == HIT_FOOT_QUIT) {
        g_quitting = TRUE;
        DestroyWindow(hwnd);
        return;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ShowTrayContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Keyboard Lighting");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TOGGLE,
                g_wantOff ? L"Turn Lighting On" : L"Turn Lighting Off");
    AppendMenuW(hMenu, MF_STRING | (g_cfg.kbd.on ? MF_CHECKED : 0), IDM_TRAY_KBD, L"Keyboard Zone Enabled");
    AppendMenuW(hMenu, MF_STRING | (g_cfg.bar.on ? MF_CHECKED : 0), IDM_TRAY_BAR, L"Light Bar Enabled");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_RESTART, L"Restart Lighting Engine");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_autostart ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_FOLDER, L"Open Program Folder");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Keyboard Lighting");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK KblWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        SetTimer(hwnd, 1, 45, NULL);  /* 22 FPS live preview & slider debounce */
        SetTimer(hwnd, 2, 600, NULL); /* Heartbeat, show.flag & engine watchdog */
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_GETMINMAXINFO: {
        int dpi = GetWinDpi(hwnd);
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = S(700, dpi);
        mmi->ptMinTrackSize.y = S(480, dpi);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RenderKblWindow(hwnd, hdc);
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

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            int k = g_hits[i].kind;
            if (PtInRect(&g_hits[i].rect, pt) &&
                (k == HIT_SLIDER_MASTER || k == HIT_SLIDER_SPEED ||
                 k == HIT_SLIDER_BRIGHT || (k >= HIT_WIDTH_0 && k < HIT_WIDTH_0 + 8))) {
                g_dragKind = k;
                g_dragIdx  = g_hits[i].index;
                g_dragRect = g_hits[i].rect;
                SetCapture(hwnd);
                ApplySliderDrag(k, g_dragIdx, g_dragRect, pt.x);
                return 0;
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g_dragKind != HIT_NONE) {
            ApplySliderDrag(g_dragKind, g_dragIdx, g_dragRect, pt.x);
            return 0;
        }
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
        if (g_dragKind != HIT_NONE) {
            g_dragKind = HIT_NONE;
            ReleaseCapture();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) {
                HandleHitAction(hwnd, i);
                break;
            }
        }
        return 0;
    }

    case WM_HOTKEY:
        if (wp == 1) StepMasterBrightness(1);
        else if (wp == 2) StepMasterBrightness(-1);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= IDM_EFF_BASE && id < IDM_EFF_BASE + EFFECT_COUNT) {
            CurGroup()->effectIdx = id - IDM_EFF_BASE;
            SyncLinkedZone();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id >= IDM_PAT_BASE && id < IDM_PAT_BASE + PRESET_MAX) {
            if (g_presetUsed[id - IDM_PAT_BASE])
                ApplyPresetSlot(id - IDM_PAT_BASE);
        } else if (id == IDM_PAT_NEW) {
            int slot = -1;
            for (int i = 0; i < PRESET_MAX; i++) {
                if (!g_presetUsed[i]) { slot = i; break; }
            }
            if (slot < 0) {
                _snwprintf(g_toastText, 95, L"All 8 pattern slots are full - delete one first");
                g_toastAt = GetTickCount64() + 1400ULL;
            } else {
                SavePresetSlot(slot);
                _snwprintf(g_toastText, 95, L"Saved \x00B7 %s", g_presetName[slot]);
                g_toastAt = GetTickCount64() + 1400ULL;
            }
        } else if (id == IDM_PAT_DEL) {
            if (g_presetIdx >= 0 && g_presetUsed[g_presetIdx]) {
                DeletePresetSlot(g_presetIdx);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (id == IDM_TRAY_TOGGLE) {
            if (g_wantOff) StartEngine(); else TurnAllLightsOff();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_KBD) {
            g_cfg.kbd.on = !g_cfg.kbd.on;
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_BAR) {
            g_cfg.bar.on = !g_cfg.bar.on;
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_RESTART) {
            StartEngine();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_AUTOSTART) {
            SetKblAutostart(!g_autostart);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_FOLDER) {
            ShellExecuteW(hwnd, L"open", g_exeDir, NULL, NULL, SW_SHOWNORMAL);
        } else if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_ENGINE_BOOTED:
        if (wp == 1 && !g_wantOff) StartEngine();
        else wcsncpy(g_statusText, L"Could not download Aura-Background.ps1 - check internet.", 255);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_POWERBROADCAST:
        if ((wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) && !g_wantOff) {
            if (IsEngineAlive()) { WriteThemeJson(); WriteLiveBrightness(); }
            else StartEngine();
        }
        return TRUE;

    case WM_INPUT:
        OnRawInputMsg(hwnd, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_TIMER:
        if (wp == 1) {
            if (g_applyPending && GetTickCount64() >= g_applyAtTick) FlushLiveApply();
            PollFnBrightness();
            HoldPump();
            if (IsWindowVisible(hwnd)) {
                PollLivePreviewFrame();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (wp == 2) {
            WriteHeartbeat();
            WatchBrightTokens();
            PollFnBrightness();
            if (FileExistsW(g_showPath)) {
                DeleteFileW(g_showPath);
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
            }
            if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
            }
            if (!g_wantOff && !IsEngineAlive() && InterlockedCompareExchange(&g_downloading, 0, 0) == 0) {
                StartEngine();
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowTrayContextMenu(hwnd);
        }
        return 0;

    case WM_CLOSE:
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            if (!g_trayHinted) {
                g_trayHinted = TRUE;
                g_nid.uFlags = NIF_INFO;
                wcsncpy(g_nid.szInfoTitle, L"Keyboard Lighting is active in tray", 63);
                wcsncpy(g_nid.szInfo, L"Right-click the tray icon any time to adjust zones or quit.", 255);
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        StopEngine(TRUE);
        DeleteFileW(g_lockPath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void InitKblPaths(void) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    wcsncpy(g_exeDir, g_exePath, MAX_PATH - 1);
    wchar_t *sl = wcsrchr(g_exeDir, L'\\');
    if (sl) *sl = L'\0';

    wchar_t localApp[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);

    _snwprintf(g_cfgDir,     MAX_PATH, L"%s\\KeyboardLighting", localApp);
    EnsureDirTree(g_cfgDir);
    _snwprintf(g_panelPath,  MAX_PATH, L"%s\\panel.json", g_cfgDir);
    _snwprintf(g_themePath,  MAX_PATH, L"%s\\theme.json", g_cfgDir);
    _snwprintf(g_livePath,   MAX_PATH, L"%s\\live.txt", g_cfgDir);
    _snwprintf(g_framePath,  MAX_PATH, L"%s\\frame.txt", g_cfgDir);
    _snwprintf(g_layoutPath, MAX_PATH, L"%s\\layout.txt", g_cfgDir);
    _snwprintf(g_lockPath,   MAX_PATH, L"%s\\running.lock", g_cfgDir);
    _snwprintf(g_showPath,   MAX_PATH, L"%s\\show.flag", g_cfgDir);
    _snwprintf(g_engPidPath, MAX_PATH, L"%s\\engine.pid", g_cfgDir);
    _snwprintf(g_stopPath,   MAX_PATH, L"%s\\stop.flag", g_cfgDir);
    _snwprintf(g_brightUpPath,   MAX_PATH, L"%s\\bright_up.flag", g_cfgDir);
    _snwprintf(g_brightDownPath, MAX_PATH, L"%s\\bright_down.flag", g_cfgDir);
    _snwprintf(g_holdModePath,   MAX_PATH, L"%s\\hold_mode.txt", g_cfgDir);
    {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
        g_scanBound[0] = (int)GetPrivateProfileIntW(L"Keys", L"M4Scan", 0, p);
        g_scanBound[1] = (int)GetPrivateProfileIntW(L"Keys", L"M5Scan", 0, p);
        g_hidCode[0] = (DWORD)GetPrivateProfileIntW(L"Keys", L"M4Hid", 0, p);
        g_hidCode[1] = (DWORD)GetPrivateProfileIntW(L"Keys", L"M5Hid", 0, p);
    }
    _snwprintf(g_engineScript, MAX_PATH, L"%s\\Aura-Background.ps1", g_exeDir);
    _snwprintf(g_zonesBat,   MAX_PATH, L"%s\\Zones.bat", g_exeDir);
    if (!FileExistsW(g_zonesBat))
        _snwprintf(g_zonesBat, MAX_PATH, L"%s\\Zones.bat", g_cfgDir);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;
    InitKblPaths();
    g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, KbHoldProc, GetModuleHandleW(NULL), 0);
    OsdCreate(hInst);
    InitRawInput(g_hwnd);
    g_isAdmin = CheckIsAdmin();

    BOOL silent    = (cmdLine && (wcsstr(cmdLine, L"-Silent") != NULL || wcsstr(cmdLine, L"--silent") != NULL));
    BOOL noElevate = (cmdLine && (wcsstr(cmdLine, L"-NoElevate") != NULL || wcsstr(cmdLine, L"--no-elevate") != NULL));

    if (cmdLine && (wcsstr(cmdLine, L"--stop") != NULL || wcsstr(cmdLine, L"-Stop") != NULL)) {
        StopEngine(TRUE);
        KillPidFromFile(g_lockPath);
        return 0;
    }

    /* Brightness taps for the M4/M5 G-Helper 'Custom' actions: a second
       instance writes a one-shot token the engine turns into a paced master
       step and exits immediately - no window, no single-instance bounce.
       The token is a millisecond stamp so a held key's repeat taps are each
       seen; the engine paces them into a smooth sweep. */
    /* Hold-mode switch for the M4/M5 brightness taps: the engine reads
       hold_mode.txt every tick, so this changes behavior with no restart.
       Usage: -HoldMode latch (default) | burst | tap */
    if (cmdLine && wcsstr(cmdLine, L"-HoldMode") != NULL) {
        const wchar_t *m = wcsstr(cmdLine, L"-HoldMode") + 9;
        while (*m == L' ' || *m == L'=') m++;
        const char *mode = "latch";
        if (wcsncmp(m, L"tap", 3) == 0) mode = "tap";
        else if (wcsncmp(m, L"burst", 5) == 0) mode = "burst";
        WriteAtomicUtf8(g_holdModePath, mode);
        return 0;
    }
    if (cmdLine && (wcsstr(cmdLine, L"-BrightUp") != NULL || wcsstr(cmdLine, L"--bright-up") != NULL)) {
        char tok[32];
        _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
        tok[31] = '\0';
        WriteAtomicUtf8(g_brightUpPath, tok);
        return 0;
    }
    if (cmdLine && (wcsstr(cmdLine, L"-BrightDown") != NULL || wcsstr(cmdLine, L"--bright-down") != NULL)) {
        char tok[32];
        _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
        tok[31] = '\0';
        WriteAtomicUtf8(g_brightDownPath, tok);
        return 0;
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!silent) {
            WriteAtomicUtf8(g_showPath, "1");
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    /* Request elevation once for direct ROG Strix G16 USB HID RGB access unless declined */
    if (!g_isAdmin && !noElevate) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize       = sizeof(sei);
        sei.lpVerb       = L"runas";
        sei.lpFile       = g_exePath;
        sei.lpParameters = silent ? L"-Silent -NoElevate" : L"-NoElevate";
        sei.lpDirectory  = g_exeDir;
        sei.nShow        = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) return 0;
        /* User declined UAC - stay open gracefully in preview mode with 'Run as Admin' button! */
        hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    }

    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    /* Ensure any legacy Tray.ps1 process is cleanly closed before taking over */
    KillPidFromFile(g_lockPath);
    LoadPanelCfg();
    LoadPresetIndex();
    g_autostart = QueryAutostart();
    WriteHeartbeat();
    PollLivePreviewFrame();

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = KblWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsKeyboardLightingSuiteWnd";
    RegisterClassExW(&wc);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(820, dpi), S(660, dpi),
                             NULL, NULL, hInst, NULL);
    RegisterBrightnessHotkeys(g_hwnd);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_appIcon;
    wcsncpy(g_nid.szTip, L"Keyboard Lighting - ROG Strix G16", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    StartEngine();

    if (!silent) {
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_showEvent) CloseHandle(g_showEvent);
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
