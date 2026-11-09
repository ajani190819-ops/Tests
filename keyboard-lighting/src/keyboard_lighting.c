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


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so keyboard_lighting.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/named_colour_presets.c"
#include "parts/brightness_hotkeys.c"
#include "parts/engine_bootstrap.c"
#include "parts/m4_m5_hold.c"
#include "parts/brightness_osd.c"
#include "parts/ui_renderer.c"
#include "parts/emblem.c"
#include "parts/mouse_drag_menus.c"
#include "parts/picker_window_entry.c"
