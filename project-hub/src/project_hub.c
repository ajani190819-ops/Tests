#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define HUB_VERSION       "2.8.32"
#define HUB_VERSION_W     L"2.8.32"
#define APP_TITLE         L"Project Hub"
#define SINGLE_MUTEX_NAME L"Local\\Tests_ProjectHub_SingleInstance_v2"
#define SHOW_EVENT_NAME   L"Local\\Tests_ProjectHub_ShowWindow_v2"

#define REPO_OWNER_W      L"ajani190819-ops"
#define REPO_NAME_W       L"Tests"
#define FALLBACK_BRANCH_W L"arena/e895b94f-tests"

#define WM_HUB_REFRESH    (WM_APP + 10)
#define WM_HUB_STATUS     (WM_APP + 11)
#define WM_HUB_SELFUPD    (WM_APP + 12)

/* Embedded multi-resolution 32-bit RGBA icons in hub.rc */
#define RES_ICON_APP         1
#define RES_ICON_EMBLEM      2
#define RES_ICON_BTG         3
#define RES_ICON_KBL         4
#define RES_ICON_HUD         5
#define RES_ICON_ORCA        6
#define RES_ICON_DNS         8
#define RES_ICON_NOTES       9
#define RES_ICON_AUDIOSWITCHER 10

/* Action IDs */
#define ID_BTN_REFRESH_ALL   1001
#define ID_BTN_SELF_UPDATE   1002
#define ID_BTN_UPDATE_ALL    1003
#define ID_EMBLEM_MENU       1004
#define ID_BTN_BRANCH        1005
#define ID_MENU_SHORTCUT     1011
#define ID_MENU_BRANCH_BASE  1600
#define ID_MENU_BRANCH_CUSTOM 1699
#define ID_MENU_SUITE_DIR    1012

#define ID_BTG_OPEN          1101
#define ID_BTG_UPDATE        1102
#define ID_BTG_STOP          1103
#define ID_BTG_FOLDER        1104

#define ID_KBL_OPEN          1201
#define ID_KBL_RESTART       1202
#define ID_KBL_UPDATE        1203
#define ID_KBL_FOLDER        1204

#define ID_HUD_UPDATE        1301
#define ID_HUD_PICK          1302
#define ID_HUD_FOLDER        1303
#define ID_HUD_DISABLE       1304
#define ID_HUD_REMOVE        1305

#define ID_ORCA_UPDATE       1401
#define ID_ORCA_MENU         1402
#define ID_ORCA_FOLDER       1403


#define ID_DNS_OPEN          1701
#define ID_DNS_UPDATE        1702
#define ID_DNS_FOLDER        1703

#define ID_NOTES_OPEN        1704
#define ID_NOTES_UPDATE      1705
#define ID_NOTES_FOLDER      1706

#define ID_AUDIOSWITCHER_OPEN   1707
#define ID_AUDIOSWITCHER_UPDATE 1708
#define ID_AUDIOSWITCHER_FOLDER 1709

/* Dynamic catalog button ids: ID_DYN_BASE + appIndex*10 + buttonIndex */
#define ID_DYN_BASE          1800
#define MAX_APPS             12

/* Soft Slate Suite Palette (matches Keyboard Lighting, Bluetooth Guard & docs/UI_STYLE_GUIDE.md) */
#define COL_BG               RGB(24, 27, 36)    /* #181B24 */
#define COL_FOOTER_BG        RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER          RGB(48, 55, 72)    /* #303748 */
#define COL_CARD_BG          RGB(34, 39, 52)    /* #222734 */
#define COL_CARD_BORDER      RGB(56, 64, 84)    /* #384054 */
#define COL_BTN_BG           RGB(43, 49, 66)    /* #2B3142 */
#define COL_BTN_HOVER        RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER       RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_BORDER     RGB(96, 112, 144)  /* #607090 */

#define COL_PRIMARY_BG       RGB(37, 99, 235)   /* #2563EB */
#define COL_PRIMARY_HOVER    RGB(59, 130, 246)  /* #3B82F6 */
#define COL_AMBER_BTN        RGB(68, 52, 26)
#define COL_AMBER_BTN_HOVER  RGB(88, 66, 30)

#define COL_TEXT_PRIMARY     RGB(236, 239, 246) /* #ECEFF6 */
#define COL_TEXT_SECONDARY   RGB(154, 163, 184) /* #9AA3B8 */
#define COL_TEXT_MUTED       RGB(116, 126, 148) /* #747E94 */

#define COL_SKY              RGB(56, 189, 248)  /* #38BDF8 */
#define COL_SKY_BG           RGB(24, 48, 68)
#define COL_MINT             RGB(52, 211, 153)  /* #34D399 */
#define COL_MINT_BG          RGB(24, 52, 44)
#define COL_AMBER            RGB(251, 191, 36)  /* #FBBF24 */
#define COL_AMBER_BG         RGB(56, 44, 24)
#define COL_CORAL            RGB(248, 113, 113) /* #F87171 */
#define COL_CORAL_BG         RGB(58, 30, 36)
#define COL_IDLE_SLATE       RGB(108, 120, 144)
#define COL_IDLE_BG          RGB(42, 48, 64)

enum {
    BADGE_IDLE    = 0,
    BADGE_OK      = 1,
    BADGE_WARN    = 2,
    BADGE_SKY     = 3,
    BADGE_DANGER  = 4
};

enum {
    BTN_SECONDARY = 0,
    BTN_AMBER     = 1,
    BTN_PRIMARY   = 2
};

enum {
    TASK_NONE = 0,
    TASK_CHECK_REMOTE,
    TASK_INSTALL_BTG,
    TASK_OPEN_OR_INSTALL_BTG,
    TASK_INSTALL_KBL,
    TASK_INSTALL_HUD,
    TASK_INSTALL_ORCA,
    TASK_INSTALL_DNS,
    TASK_OPEN_OR_INSTALL_DNS,
    TASK_INSTALL_NOTES,
    TASK_OPEN_OR_INSTALL_NOTES,
    TASK_INSTALL_DYN,
    TASK_OPEN_OR_INSTALL_DYN,
    TASK_UPDATE_ALL,
    TASK_SELF_UPDATE
};

typedef struct {
    int            id;
    const wchar_t *label;
    int            style;
    int            widthPx;
    RECT           rc;
    const wchar_t *verb;   /* catalog action: open / update / folder / install / ... */
} ActionBtn;

typedef struct {
    int            resIconId;
    const wchar_t *title;
    wchar_t        badge[48];
    int            badgeKind;
    wchar_t        detail[256];
    ActionBtn      btns[4];
    int            btnCount;
    RECT           rcCard;
    /* catalog-driven app metadata (suite-apps.json) */
    wchar_t        id[24];
    const wchar_t *blurb;
    wchar_t        appDir[MAX_PATH];
    const wchar_t *exeRepo;      /* repo path of the main executable */
    const wchar_t *exeName;      /* file name inside appDir */
    const wchar_t *shortcutName;
    const wchar_t *launchArgs;
    wchar_t        iconFile[MAX_PATH];
    HICON          hIcon;
    DWORD          remSize;
} ProjectCard;

static HINSTANCE        g_hInst       = NULL;
static HWND             g_hwnd        = NULL;
static HFONT            g_fontHero    = NULL;
static HFONT            g_fontSub     = NULL;
static HFONT            g_fontTitle   = NULL;
static HFONT            g_fontBody    = NULL;
static HFONT            g_fontBtn     = NULL;
static HFONT            g_fontBadge   = NULL;
static HICON            g_appIcon     = NULL;
static HICON            g_hdrEmblem   = NULL;
static HICON            g_cardIcons[MAX_APPS];
static HANDLE           g_showEvent   = NULL;
static HANDLE           g_hMutex      = NULL;
static CRITICAL_SECTION g_cs;

static ActionBtn        g_topBtns[4] = {
    { ID_BTN_REFRESH_ALL, L"Refresh",    BTN_SECONDARY, 92,  {0,0,0,0}, NULL },
    { ID_BTN_SELF_UPDATE, L"Update Hub", BTN_AMBER,     112, {0,0,0,0}, NULL },
    { ID_BTN_UPDATE_ALL,  L"Update All", BTN_PRIMARY,   108, {0,0,0,0}, NULL },
    { ID_BTN_BRANCH,      L"Branch \x25BC", BTN_SECONDARY, 96, {0,0,0,0}, NULL }
};
static RECT             g_rcEmblem    = {0, 0, 0, 0};

static ProjectCard      g_cards[MAX_APPS];
static int              g_cardCount = 6;   /* 6 built-ins + catalog extras */
static int              g_dynIdx = 0;      /* target of TASK_*_DYN */
static int              g_hoverId     = 0;
static int              g_scrollY     = 0;
static int              g_maxScrollY  = 0;

static wchar_t          g_exePath[MAX_PATH];
static wchar_t          g_exeDir[MAX_PATH];
static wchar_t          g_localApp[MAX_PATH];
static wchar_t          g_roamApp[MAX_PATH];
static wchar_t          g_suiteDir[MAX_PATH];
static wchar_t          g_hubCfgDir[MAX_PATH];
static wchar_t          g_btgDir[MAX_PATH];
static wchar_t          g_btgExtraExe[MAX_PATH];
static wchar_t          g_kblDir[MAX_PATH];
static wchar_t          g_dnsDir[MAX_PATH];
static wchar_t          g_notesDir[MAX_PATH];
static wchar_t          g_audioSwitcherDir[MAX_PATH];
static wchar_t          g_modsDir[MAX_PATH];
static wchar_t          g_orcaBatDir[MAX_PATH];

/* Shared remote state protected by g_cs */
static wchar_t          g_activeBranch[64]  = L"";
static wchar_t          g_branchMenuNames[96][64];
static int              g_branchMenuN        = 0;
static wchar_t          g_branchPromptOut[64];
static char             g_remHubVer[32]     = "";
static char             g_remWaveVer[32]    = "";
static char             g_remInfillVer[32]  = "";
static wchar_t          g_remHudName[128]   = L"";
static wchar_t          g_remHudPath[512]   = L"";
static DWORD            g_remHudSize        = 0;
static wchar_t          g_statusMsg[512]    = L"Checking installed apps and GitHub updates...";
static int              g_progressVal       = 0;
static BOOL             g_hubUpdAvail       = FALSE;

/* Worker queue */
static int              g_taskQueue[32];
static int              g_qHead             = 0;
static int              g_qTail             = 0;
static HANDLE           g_qSem              = NULL;
static volatile LONG    g_running           = 1;

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
        DWORD cornerPref = 2; /* DWMWCP_ROUND — full smooth Windows 11 rounded corners */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIVIDER;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &cornerPref, sizeof(cornerPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static void RebuildFontsAndIcons(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    if (g_fontHero)  DeleteObject(g_fontHero);
    if (g_fontSub)   DeleteObject(g_fontSub);
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontBody)  DeleteObject(g_fontBody);
    if (g_fontBtn)   DeleteObject(g_fontBtn);
    if (g_fontBadge) DeleteObject(g_fontBadge);

    g_fontHero  = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSub   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBody  = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    if (g_hdrEmblem) { DestroyIcon(g_hdrEmblem); g_hdrEmblem = NULL; }
    int embPx = S(36, dpi);
    g_hdrEmblem = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(RES_ICON_EMBLEM),
                                    IMAGE_ICON, embPx, embPx, LR_DEFAULTCOLOR);

    int cardIcoPx = S(44, dpi);
    const int ids[7] = { RES_ICON_BTG, RES_ICON_KBL, RES_ICON_HUD, RES_ICON_ORCA, RES_ICON_DNS, RES_ICON_NOTES, RES_ICON_AUDIOSWITCHER };
    for (int i = 0; i < 6; i++) {
        if (g_cardIcons[i]) { DestroyIcon(g_cardIcons[i]); g_cardIcons[i] = NULL; }
        g_cardIcons[i] = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(ids[i]),
                                           IMAGE_ICON, cardIcoPx, cardIcoPx, LR_DEFAULTCOLOR);
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

/* Exposed symbol reference kept in sync with suite style tests */

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

static BOOL FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static DWORD LocalSizeOf(const wchar_t *p) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(p, GetFileExInfoStandard, &fad))
        return fad.nFileSizeLow;
    return 0;
}

/* Installed means usable: a 0-byte exe (interrupted copy, AV quarantine
   remnant, ...) must not count as installed, or the update check stays
   silent while the app cannot even open. */
static BOOL FileHealthyW(const wchar_t *p) {
    return FileExistsW(p) && LocalSizeOf(p) > 0;
}

static BOOL DirExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
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

static volatile LONG g_checkActive = 0;
static DWORD g_remBtgSize = 0, g_remKblSize = 0, g_remKblPs1Size = 0, g_remDnsSize = 0, g_remNotesSize = 0, g_remAudioSwitcherSize = 0;
static RECT           g_rcFooterDot = { 0, 0, 0, 0 };

/* Quiet update-check phase: the periodic version check gets a small
   pulsing footer dot instead of the full-width progress sweep and
   step-by-step status churn. */
static void SetCheckPhase(BOOL on) {
    InterlockedExchange(&g_checkActive, on ? 1 : 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_STATUS, 0, 0);
}

static void SetFooterStatus(const wchar_t *msg, int pct) {
    EnterCriticalSection(&g_cs);
    wcsncpy(g_statusMsg, msg, 511);
    g_statusMsg[511] = L'\0';
    g_progressVal = pct;
    LeaveCriticalSection(&g_cs);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_STATUS, 0, 0);
}

static char *ReadTextFileUtf8(const wchar_t *path, DWORD *outLen) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(hf, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 8 * 1024 * 1024) { CloseHandle(hf); return NULL; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { CloseHandle(hf); return NULL; }
    DWORD rd = 0;
    if (!ReadFile(hf, buf, sz, &rd, NULL)) { free(buf); CloseHandle(hf); return NULL; }
    buf[rd] = '\0';
    CloseHandle(hf);
    if (outLen) *outLen = rd;
    return buf;
}

static BOOL WriteBinaryFileAtomic(const wchar_t *dest, const BYTE *data, DWORD len) {
    wchar_t dir[MAX_PATH];
    wcsncpy(dir, dest, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) { *slash = L'\0'; EnsureDirTree(dir); }

    wchar_t tmp[MAX_PATH];
    _snwprintf(tmp, MAX_PATH, L"%s.tmp", dest);
    HANDLE hf = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;
    DWORD wr = 0;
    BOOL ok = WriteFile(hf, data, len, &wr, NULL) && (wr == len);
    CloseHandle(hf);
    if (!ok) { DeleteFileW(tmp); return FALSE; }
    if (!MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return FALSE;
    }
    /* Strip the Mark of the Web so Windows does not re-prompt SmartScreen
       every time the user opens a freshly updated app. */
    {
        wchar_t ads[MAX_PATH + 24];
        _snwprintf(ads, MAX_PATH + 23, L"%s:Zone.Identifier", dest);
        DeleteFileW(ads);
    }
    return TRUE;
}

/* ---- HTTP layer: one shared WinHTTP session + per-host connection --------
   Every request runs on the single hub worker thread, so plain lazy handles
   are safe. Reusing the session and connection means steady-state checks
   keep a warm TLS channel instead of paying fresh handshakes for every
   probe (this used to open 8 brand-new sessions per 10-second cycle). */
static HINTERNET g_httpSess = NULL;
/* One connection handle PER HOST. WinHttpConnect bakes the hostname into
   the handle, so sharing one slot across hosts sends later requests to
   whatever host connected first (that made the github.com jar download
   land on raw.githubusercontent.com and 404 - "download failed"). */
typedef struct { const wchar_t *host; HINTERNET conn; } HostConn;
static HostConn g_httpConns[] = {
    { L"raw.githubusercontent.com", NULL },
    { L"api.github.com", NULL },
    { L"github.com", NULL },
    { NULL, NULL }
};

static HINTERNET HttpSession(void) {
    if (!g_httpSess) {
        g_httpSess = WinHttpOpen(L"Tests-ProjectHub/2.3",
                                 WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (g_httpSess) WinHttpSetTimeouts(g_httpSess, 8000, 10000, 15000, 30000);
    }
    return g_httpSess;
}

static HINTERNET HttpConnect(const wchar_t *host) {
    for (HostConn *s = g_httpConns; s->host; s++) {
        if (_wcsicmp(s->host, host) != 0) continue;
        if (!s->conn) {
            HINTERNET sess = HttpSession();
            if (!sess) return NULL;
            s->conn = WinHttpConnect(sess, s->host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        }
        return s->conn;
    }
    return NULL;
}

static BOOL HttpHeadSize(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize) {
    *outSize = 0;
    HINTERNET hConnect = HttpConnect(host);
    if (!hConnect) return FALSE;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"HEAD", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) return FALSE;
    BOOL ok = WinHttpSendRequest(hReq, L"User-Agent: Tests-ProjectHub/2.3\r\n", (DWORD)-1L,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(hReq, NULL);
    if (ok) {
        DWORD cl = 0, cls = sizeof(cl);
        if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &cl, &cls, WINHTTP_NO_HEADER_INDEX))
            *outSize = cl;
    }
    WinHttpCloseHandle(hReq);
    return ok && *outSize > 0;
}

static BYTE *HttpGetBufferEx(const wchar_t *host, const wchar_t *urlPath, const wchar_t *extraHdr,
                             DWORD *outSize, int *outStatus,
                             wchar_t *outEtag, wchar_t *outLastMod, BOOL *outNotMod) {
    if (outSize) *outSize = 0;
    if (outStatus) *outStatus = 0;
    if (outNotMod) *outNotMod = FALSE;

    HINTERNET hConnect = HttpConnect(host);
    if (!hConnect) return NULL;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) return NULL;

    wchar_t hdrs[768];
    _snwprintf(hdrs, 767, L"User-Agent: Tests-ProjectHub/2.3\r\nAccept: */*\r\n%s",
               extraHdr ? extraHdr : L"");
    hdrs[767] = L'\0';

    BOOL sent = WinHttpSendRequest(hReq, hdrs, (DWORD)-1L,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    DWORD statusCode = 0, scSize = sizeof(statusCode);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);
    if (outStatus) *outStatus = (int)statusCode;

    if (statusCode == 304) {
        if (outNotMod) *outNotMod = TRUE;
        WinHttpCloseHandle(hReq);
        return NULL;
    }
    if (statusCode != 200) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    if (outEtag && outLastMod) {
        outEtag[0] = L'\0';
        outLastMod[0] = L'\0';
        DWORD n = 95;
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_ETAG, WINHTTP_HEADER_NAME_BY_INDEX,
                            outEtag, &n, WINHTTP_NO_HEADER_INDEX);
        n = 63;
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_LAST_MODIFIED, WINHTTP_HEADER_NAME_BY_INDEX,
                            outLastMod, &n, WINHTTP_NO_HEADER_INDEX);
    }

    DWORD cap = 65536, len = 0;
    BYTE *buf = (BYTE *)malloc(cap);
    if (!buf) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail) || avail == 0) break;
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            BYTE *nb = (BYTE *)realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
        }
        DWORD rd = 0;
        if (!WinHttpReadData(hReq, buf + len, avail, &rd) || rd == 0) break;
        len += rd;
    }

    WinHttpCloseHandle(hReq);

    if (buf) {
        buf[len] = 0;
        if (outSize) *outSize = len;
    }
    return buf;
}

static BYTE *HttpGetBuffer(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    return HttpGetBufferEx(host, urlPath, NULL, outSize, outStatus, NULL, NULL, NULL);
}

/* ---- version-probe cache: conditional GET (ETag / If-Modified-Since) ----
   An unchanged file answers 304 with no body at all, so steady-state checks
   transfer almost nothing instead of re-downloading ~200 KB of hub source
   every cycle just to read one version string. */
typedef struct {
    wchar_t host[48];
    wchar_t path[384];
    wchar_t etag[96];
    wchar_t lastMod[64];
    BYTE   *body;
    DWORD   len;
} HttpCacheEnt;
static HttpCacheEnt g_hcache[8];

static BYTE *HttpGetCached(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    HttpCacheEnt *ent = NULL;
    for (int i = 0; i < 8; i++) {
        if (g_hcache[i].path[0] && wcscmp(g_hcache[i].host, host) == 0 &&
            wcscmp(g_hcache[i].path, urlPath) == 0) {
            ent = &g_hcache[i];
            break;
        }
    }
    if (!ent) {
        for (int i = 0; i < 8; i++) {
            if (!g_hcache[i].path[0]) { ent = &g_hcache[i]; break; }
        }
        if (!ent) ent = &g_hcache[0];
        free(ent->body);
        ent->body = NULL;
        ent->len = 0;
        ent->etag[0] = L'\0';
        ent->lastMod[0] = L'\0';
        wcsncpy(ent->host, host, 47); ent->host[47] = L'\0';
        wcsncpy(ent->path, urlPath, 383); ent->path[383] = L'\0';
    }

    wchar_t cond[256] = L"";
    if (ent->etag[0])
        _snwprintf(cond, 255, L"If-None-Match: %s\r\n", ent->etag);
    if (ent->lastMod[0]) {
        size_t used = wcslen(cond);
        _snwprintf(cond + used, 255 - used, L"If-Modified-Since: %s\r\n", ent->lastMod);
    }
    cond[255] = L'\0';

    DWORD sz = 0;
    int st = 0;
    BOOL notMod = FALSE;
    BYTE *data = HttpGetBufferEx(host, urlPath, cond[0] ? cond : NULL, &sz, &st,
                                 ent->etag, ent->lastMod, &notMod);

    if (notMod && ent->body) {
        BYTE *copy = (BYTE *)malloc(ent->len + 1);
        if (!copy) return NULL;
        memcpy(copy, ent->body, ent->len + 1);
        if (outSize) *outSize = ent->len;
        if (outStatus) *outStatus = 200;
        return copy;
    }
    if (data && st == 200) {
        free(ent->body);
        ent->body = (BYTE *)malloc(sz + 1);
        if (ent->body) {
            memcpy(ent->body, data, sz + 1);
            ent->len = sz;
        } else {
            ent->len = 0;
        }
    }
    if (outSize) *outSize = sz;
    if (outStatus) *outStatus = st;
    return data;
}

static BOOL DownloadRawRepoFile(const wchar_t *branch, const wchar_t *relUrl, const wchar_t *destPath) {
    wchar_t path[512];
    _snwprintf(path, 511, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, branch, relUrl);
    DWORD sz = 0;
    int st = 0;
    BYTE *data = HttpGetBuffer(L"raw.githubusercontent.com", path, &sz, &st);
    if (!data || st != 200 || sz < 8) {
        free(data);
        return FALSE;
    }
    BOOL ok = WriteBinaryFileAtomic(destPath, data, sz);
    free(data);
    return ok;
}

/* The branch the updater pulls from is the user's choice, remembered in
   %LOCALAPPDATA%\ProjectHub\config.ini so every update check and download
   follows it without any merging. */
static void BranchCfgPath(wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%s\\ProjectHub\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static BOOL LoadBranchCfg(wchar_t *out, size_t cap) {
    wchar_t ini[MAX_PATH];
    BranchCfgPath(ini, MAX_PATH);
    out[0] = L'\0';
    DWORD n = GetPrivateProfileStringW(L"Update", L"Branch", L"", out, (DWORD)cap, ini);
    return n > 0 && out[0];
}

static void SaveBranchCfg(const wchar_t *branch) {
    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    _snwprintf(dir, MAX_PATH - 1, L"%s\\ProjectHub", g_localApp);
    dir[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(dir, NULL);
    BranchCfgPath(ini, MAX_PATH);
    WritePrivateProfileStringW(L"Update", L"Branch", branch, ini);
}

/* Per-app opt-outs. The mod card (and only it, per the owner's choice)
   can be disabled: no install, no update, Update All skips it. */
static int g_hudDisabled = 0;

static void LoadAppsCfg(void) {
    wchar_t ini[MAX_PATH], buf[8];
    BranchCfgPath(ini, MAX_PATH);
    GetPrivateProfileStringW(L"Apps", L"HudDisabled", L"0", buf, 7, ini);
    g_hudDisabled = (buf[0] == L'1');
}

static void SaveAppsCfg(void) {
    wchar_t ini[MAX_PATH];
    BranchCfgPath(ini, MAX_PATH);
    WritePrivateProfileStringW(L"Apps", L"HudDisabled",
                               g_hudDisabled ? L"1" : L"0", ini);
}

static void EnsureActiveBranch(wchar_t *outBranch, size_t cap) {
    EnterCriticalSection(&g_cs);
    if (g_activeBranch[0]) {
        wcsncpy(outBranch, g_activeBranch, cap - 1);
        outBranch[cap - 1] = L'\0';
        LeaveCriticalSection(&g_cs);
        return;
    }
    LeaveCriticalSection(&g_cs);

    /* Saved user choice wins; only auto-probe when never chosen. */
    wchar_t chosenBuf[64];
    const wchar_t *chosen;
    if (LoadBranchCfg(chosenBuf, 64)) {
        chosen = chosenBuf;
    } else {
        DWORD sz = 0;
        int st = 0;
        wchar_t probePath[256];
        _snwprintf(probePath, 255, L"/%s/%s/main/bluetooth-guard/README.md", REPO_OWNER_W, REPO_NAME_W);
        BYTE *probe = HttpGetBuffer(L"raw.githubusercontent.com", probePath, &sz, &st);
        free(probe);
        chosen = (st == 200 && sz > 20) ? L"main" : FALLBACK_BRANCH_W;
    }
    EnterCriticalSection(&g_cs);
    wcsncpy(g_activeBranch, chosen, 63);
    g_activeBranch[63] = L'\0';
    LeaveCriticalSection(&g_cs);

    wcsncpy(outBranch, chosen, cap - 1);
    outBranch[cap - 1] = L'\0';
}

static BOOL ExtractQuotedAfter(const char *hay, const char *key, char *out, size_t cap) {
    const char *p = strstr(hay, key);
    if (!p) return FALSE;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '=')) p++;
    char quote = *p;
    if (quote != '"' && quote != '\'') return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != quote && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* Numeric dotted-version compare: TRUE only when a is genuinely newer than b.
   A stale or older GitHub copy must never claim an update is available - that
   is what left a false "vX.Y.Z is available" nag and a yellow button after an
   update had already landed. */
static BOOL VerNewer(const char *a, const char *b) {
    for (int i = 0; i < 4; i++) {
        int va = 0, vb = 0;
        while (*a >= '0' && *a <= '9') va = va * 10 + (*a++ - '0');
        while (*b >= '0' && *b <= '9') vb = vb * 10 + (*b++ - '0');
        if (va != vb) return va > vb;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return FALSE;
}

/* Resolve directory from an existing .lnk shortcut if the user previously installed elsewhere */
static BOOL ResolveShortcutTargetDir(const wchar_t *lnkPath, wchar_t *outDir) {
    if (!FileExistsW(lnkPath)) return FALSE;
    IShellLinkW *psl = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&psl)) || !psl)
        return FALSE;

    BOOL found = FALSE;
    IPersistFile *ppf = NULL;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        if (SUCCEEDED(IPersistFile_Load(ppf, lnkPath, STGM_READ))) {
            wchar_t target[MAX_PATH] = L"";
            if (SUCCEEDED(IShellLinkW_GetPath(psl, target, MAX_PATH, NULL, 0)) && target[0]) {
                wchar_t *sl = wcsrchr(target, L'\\');
                if (sl) {
                    *sl = L'\0';
                    if (DirExistsW(target)) {
                        wcsncpy(outDir, target, MAX_PATH - 1);
                        outDir[MAX_PATH - 1] = L'\0';
                        found = TRUE;
                    }
                }
            }
        }
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return found;
}

static void InitAllPaths(void) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    wcsncpy(g_exeDir, g_exePath, MAX_PATH - 1);
    wchar_t *sl = wcsrchr(g_exeDir, L'\\');
    if (sl) *sl = L'\0';

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, g_localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", g_localApp, MAX_PATH);
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, g_roamApp)))
        GetEnvironmentVariableW(L"APPDATA", g_roamApp, MAX_PATH);

    _snwprintf(g_hubCfgDir, MAX_PATH, L"%s\\ProjectHub", g_localApp);
    _snwprintf(g_suiteDir,  MAX_PATH, L"%s\\Programs\\TestsSuite", g_localApp);
    EnsureDirTree(g_hubCfgDir);
    EnsureDirTree(g_suiteDir);

    wchar_t desk[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk);

    wchar_t parent[MAX_PATH];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    wchar_t *psl = wcsrchr(parent, L'\\');
    if (psl) *psl = L'\0';

    wchar_t cand[MAX_PATH], lnk[MAX_PATH];

    /* 1. Locate existing BluetoothGuard installation (running process, same folder, sibling, shortcut, or suite dir) */
    g_btgDir[0] = L'\0';
    g_btgExtraExe[0] = L'\0';
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    char *lockTxt = ReadTextFileUtf8(btgLock, NULL);
    if (lockTxt) {
        unsigned long pid = strtoul(lockTxt, NULL, 10);
        free(lockTxt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            if (hp) {
                wchar_t procPath[MAX_PATH] = L"";
                DWORD plen = MAX_PATH;
                if (QueryFullProcessImageNameW(hp, 0, procPath, &plen) && FileExistsW(procPath)) {
                    wcsncpy(g_btgExtraExe, procPath, MAX_PATH - 1);
                }
                CloseHandle(hp);
            }
        }
    }

    _snwprintf(cand, MAX_PATH, L"%s\\BluetoothGuard.exe", g_exeDir);
    if (FileExistsW(cand)) {
        wcsncpy(g_btgDir, g_exeDir, MAX_PATH - 1);
    } else {
        _snwprintf(cand, MAX_PATH, L"%s\\bluetooth-guard\\BluetoothGuard.exe", parent);
        if (FileExistsW(cand)) {
            _snwprintf(g_btgDir, MAX_PATH, L"%s\\bluetooth-guard", parent);
        } else if (desk[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
            ResolveShortcutTargetDir(lnk, g_btgDir);
        }
    }
    if (!g_btgDir[0]) _snwprintf(g_btgDir, MAX_PATH, L"%s\\BluetoothGuard", g_suiteDir);

    /* 2. Locate existing Keyboard Lighting installation (sibling, Desktop shortcut, Start Menu shortcut, or suite dir) */
    g_kblDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH, L"%s\\keyboard-lighting\\Tray.ps1", parent);
    if (FileExistsW(cand)) {
        _snwprintf(g_kblDir, MAX_PATH, L"%s\\keyboard-lighting", parent);
    } else {
        if (desk[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
            ResolveShortcutTargetDir(lnk, g_kblDir);
        }
        if (!g_kblDir[0]) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Microsoft\\Windows\\Start Menu\\Programs\\Keyboard Lighting.lnk", g_roamApp);
            ResolveShortcutTargetDir(lnk, g_kblDir);
        }
    }
    if (!g_kblDir[0]) _snwprintf(g_kblDir, MAX_PATH, L"%s\\KeyboardLighting", g_suiteDir);

    /* 2b. Locate existing DNS Pilot installation (sibling of the hub, repo
       checkout, or the suite dir where Install puts it) */
    g_dnsDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH, L"%s\\DnsPilot.exe", g_exeDir);
    if (FileExistsW(cand)) {
        wcsncpy(g_dnsDir, g_exeDir, MAX_PATH - 1);
    } else {
        _snwprintf(cand, MAX_PATH, L"%s\\dns-pilot\\DnsPilot.exe", parent);
        if (FileExistsW(cand)) {
            _snwprintf(g_dnsDir, MAX_PATH, L"%s\\dns-pilot", parent);
        }
    }
    if (!g_dnsDir[0]) _snwprintf(g_dnsDir, MAX_PATH, L"%s\\DnsPilot", g_suiteDir);

    /* 2c. Locate Agent Notes (sibling of the hub, repo checkout, or suite dir) */
    g_notesDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH, L"%s\\AgentNotes.exe", g_exeDir);
    if (FileExistsW(cand)) {
        wcsncpy(g_notesDir, g_exeDir, MAX_PATH - 1);
    } else {
        _snwprintf(cand, MAX_PATH, L"%s\\agent-notes\\AgentNotes.exe", parent);
        if (FileExistsW(cand)) {
            _snwprintf(g_notesDir, MAX_PATH, L"%s\\agent-notes", parent);
        }
    }
    if (!g_notesDir[0]) _snwprintf(g_notesDir, MAX_PATH, L"%s\\AgentNotes", g_suiteDir);

    /* 2d. Locate Audio Switcher (sibling of the hub, repo checkout, or suite dir) */
    g_audioSwitcherDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH, L"%s\\AudioSwitcher.exe", g_exeDir);
    if (FileExistsW(cand)) {
        wcsncpy(g_audioSwitcherDir, g_exeDir, MAX_PATH - 1);
    } else {
        _snwprintf(cand, MAX_PATH, L"%s\\audio-switcher\\AudioSwitcher.exe", parent);
        if (FileExistsW(cand)) {
            wcsncpy(g_audioSwitcherDir, parent, MAX_PATH - 1);
        }
    }
    if (!g_audioSwitcherDir[0]) _snwprintf(g_audioSwitcherDir, MAX_PATH, L"%s\\AudioSwitcher", g_suiteDir);

    /* 3. OrcaSlicer plugins */
    _snwprintf(cand, MAX_PATH, L"%s\\Orca-Plugins.bat", parent);
    if (FileExistsW(cand)) wcsncpy(g_orcaBatDir, parent, MAX_PATH - 1);
    else _snwprintf(g_orcaBatDir, MAX_PATH, L"%s\\OrcaPlugins", g_suiteDir);

    /* 4. Spatial HUD mods folder */
    wchar_t hudTargetTxt[MAX_PATH];
    _snwprintf(hudTargetTxt, MAX_PATH, L"%s\\SpatialHudUpdater\\target-directory.txt", g_localApp);
    char *savedMods = ReadTextFileUtf8(hudTargetTxt, NULL);
    if (savedMods) {
        unsigned char *s = (unsigned char *)savedMods;
        if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) s += 3;
        size_t n = strlen((char *)s);
        while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = '\0';
        if (n > 2) MultiByteToWideChar(CP_UTF8, 0, (char *)s, -1, g_modsDir, MAX_PATH);
        free(savedMods);
    }
    if (!g_modsDir[0]) {
        _snwprintf(g_modsDir, MAX_PATH, L"%s\\ModrinthApp\\profiles\\F5W\\mods", g_roamApp);
    }
}

static int CheckPidLockStatus(const wchar_t *lockFile, double maxAgeSec, int *outField2, int *outField3) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(lockFile, GetFileExInfoStandard, &fad)) return 0;

    char *txt = ReadTextFileUtf8(lockFile, NULL);
    if (!txt) return 0;
    unsigned long pid = 0;
    int f1 = 0, f2 = 0, f3 = 0;
    sscanf(txt, "%lu|%d|%d|%d", &pid, &f1, &f2, &f3);
    free(txt);
    if (outField2) *outField2 = f2;
    if (outField3) *outField3 = f3;
    if (pid == 0) return 0;

    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hp) return 0;
    DWORD exitCode = 0;
    BOOL alive = GetExitCodeProcess(hp, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hp);
    if (!alive) return 0;

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER uNow, uMod;
    uNow.LowPart = nowFt.dwLowDateTime; uNow.HighPart = nowFt.dwHighDateTime;
    uMod.LowPart = fad.ftLastWriteTime.dwLowDateTime; uMod.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    double ageSec = (uNow.QuadPart > uMod.QuadPart)
                  ? (double)(uNow.QuadPart - uMod.QuadPart) / 10000000.0
                  : 0.0;
    return (ageSec > maxAgeSec) ? 2 : 1;
}

static BOOL CreateShortcutLink(const wchar_t *lnkPath, const wchar_t *targetExe,
                               const wchar_t *workDir, const wchar_t *desc) {
    IShellLinkW *psl = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IShellLinkW, (void **)&psl);
    if (FAILED(hr) || !psl) return FALSE;
    IShellLinkW_SetPath(psl, targetExe);
    IShellLinkW_SetWorkingDirectory(psl, workDir);
    IShellLinkW_SetIconLocation(psl, targetExe, 0);
    IShellLinkW_SetDescription(psl, desc);

    IPersistFile *ppf = NULL;
    BOOL ok = FALSE;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        ok = SUCCEEDED(IPersistFile_Save(ppf, lnkPath, TRUE));
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return ok;
}

/* ---------------- dynamic app catalog (suite-apps.json) ----------------
   New suite apps are described in suite-apps.json on the active branch.
   The hub fetches it during its quiet check and builds cards for any entry
   it does not already know - so adding an app means dropping its files and
   one JSON entry, never a hub update. */
static char *g_catalogJson = NULL;

static void EnqueueTask(int task);

/* Load a .ico at the exact card-icon draw size. The assets ship 256px
   sources; LR_DEFAULTSIZE would pin 32px and blur when the card scales
   it up to S(44, dpi). */
static HICON LoadIconFilePx(const wchar_t *path) {
    int dpi = GetWinDpi(g_hwnd);
    int px = S(44, dpi);
    return (HICON)LoadImageW(NULL, path, IMAGE_ICON, px, px, LR_LOADFROMFILE);
}

static const wchar_t *PoolAdd(const char *utf8);

/* Cached download of a catalog icon. The cache file name carries a short
   hash of the catalog's icon path, so when an app's icon changes in the
   catalog the hub fetches the new art exactly once instead of showing the
   old cached .ico forever. On a failed download the card falls back to its
   embedded resource / letter tile. */
static void LoadCardIcon(ProjectCard *c, const char *icon) {
    wchar_t iconsDir[MAX_PATH];
    unsigned long key = 2166136261UL; /* FNV-1a of the catalog icon path */
    for (const unsigned char *p = (const unsigned char *)icon; *p; p++) {
        key ^= (unsigned long)*p;
        key *= 16777619UL;
    }
    _snwprintf(iconsDir, MAX_PATH - 1, L"%s\\icons", g_localApp);
    CreateDirectoryW(iconsDir, NULL);
    _snwprintf(c->iconFile, MAX_PATH - 1, L"%s\\%s-%08lx.ico", iconsDir, c->id, key);
    if (!FileExistsW(c->iconFile)) {
        wchar_t branch[64];
        EnsureActiveBranch(branch, 64);
        if (!DownloadRawRepoFile(branch, PoolAdd(icon), c->iconFile))
            c->iconFile[0] = L'\0';
    }
    if (c->iconFile[0])
        c->hIcon = LoadIconFilePx(c->iconFile);
}

static wchar_t g_strPool[12288];
static int     g_strPoolUsed = 0;

static const wchar_t *PoolAdd(const char *utf8) {
    wchar_t *w = &g_strPool[g_strPoolUsed];
    int cap = (int)(sizeof(g_strPool) / sizeof(wchar_t)) - g_strPoolUsed;
    if (cap < 8) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8 ? utf8 : "", -1, w, cap);
    if (n <= 0) return L"";
    g_strPoolUsed += n;
    return w;
}

static int JsonGet(const char *obj, const char *key, char *out, int cap) {
    char pat[64];
    out[0] = '\0';
    _snprintf(pat, sizeof(pat) - 1, "\"%s\"", key);
    pat[sizeof(pat) - 1] = '\0';
    const char *p = strstr(obj, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i + 1 < cap) {
        if (*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

static int BuiltInSlot(const char *id) {
    static const char *known[7] = { "btguard", "kbl", "hud", "orca", "dnspilot", "agentnotes", "audioswitcher" };
    for (int i = 0; i < 6; i++) if (strcmp(id, known[i]) == 0) return i;
    return -1;
}

static int BuiltInBtnId(int slot, const char *verb) {
    /* verb -> the control id the existing WM_COMMAND switch understands */
    static const struct { const char *verb; int id; } map[25] = {
        { "open",      ID_BTG_OPEN },     { "update",    ID_BTG_UPDATE },
        { "stop",      ID_BTG_STOP },     { "folder",    ID_BTG_FOLDER },
        { "open",      ID_KBL_OPEN },     { "unfreeze",  ID_KBL_RESTART },
        { "update",    ID_KBL_UPDATE },   { "folder",    ID_KBL_FOLDER },
        { "updatemod", ID_HUD_UPDATE },   { "modsdir",   ID_HUD_PICK },
        { "folder",    ID_HUD_FOLDER },   { "disable",   ID_HUD_DISABLE },
        { "remove",    ID_HUD_REMOVE },
        { "update",    ID_ORCA_UPDATE },  { "branch",    ID_ORCA_MENU },
        { "folder",    ID_ORCA_FOLDER },
        { "open",      ID_DNS_OPEN },     { "update",    ID_DNS_UPDATE },
        { "folder",    ID_DNS_FOLDER },
        { "open",      ID_NOTES_OPEN },   { "update",    ID_NOTES_UPDATE },
        { "folder",    ID_NOTES_FOLDER },
        { "open",      ID_AUDIOSWITCHER_OPEN },  { "update",    ID_AUDIOSWITCHER_UPDATE },
        { "folder",    ID_AUDIOSWITCHER_FOLDER }
    };
    static const int off[8] = { 0, 4, 8, 13, 16, 19, 22, 25 };
    if (slot < 0 || slot > 5) return 0;
    for (int i = off[slot]; i < off[slot + 1]; i++)
        if (strcmp(map[i].verb, verb) == 0) return map[i].id;
    return 0;
}

static void FillBuiltInDef(int slot, const char *id, const char *title,
                           const char *blurb, const char *icon, const char *buttons) {
    if (slot < 0 || slot >= g_cardCount) return;
    ProjectCard *c = &g_cards[slot];
    MultiByteToWideChar(CP_UTF8, 0, id, -1, c->id, 23);
    if (title[0]) c->title = PoolAdd(title);
    if (blurb[0]) c->blurb = PoolAdd(blurb);
    if (buttons[0]) {
        char list[256];
        strncpy(list, buttons, sizeof(list) - 1);
        list[sizeof(list) - 1] = '\0';
        int n = 0;
        char *bp = list;
        while (*bp && n < 4) {
            char verb[24] = "", label[32] = "";
            int wpx = 74, k = 0;
            while (*bp && *bp != ':' && *bp != ',' && k < 23) verb[k++] = *bp++;
            verb[k] = '\0';
            if (*bp == ':') bp++;
            k = 0;
            while (*bp && *bp != ':' && *bp != ',' && k < 31) label[k++] = *bp++;
            label[k] = '\0';
            if (*bp == ':') {
                bp++;
                wpx = 0;
                while (*bp >= '0' && *bp <= '9') wpx = wpx * 10 + (*bp++ - '0');
                if (wpx < 40 || wpx > 160) wpx = 74;
            }
            while (*bp && *bp != ',') bp++;
            if (*bp == ',') bp++;
            int btnId = verb[0] ? BuiltInBtnId(slot, verb) : 0;
            if (!btnId) continue;
            c->btns[n].id = btnId;
            c->btns[n].label = PoolAdd(label[0] ? label : verb);
            c->btns[n].style = BTN_SECONDARY;
            c->btns[n].widthPx = wpx;
            c->btns[n].verb = PoolAdd(verb);
            ZeroMemory(&c->btns[n].rc, sizeof(RECT));
            n++;
        }
        if (n > 0) c->btnCount = n;
    }
    /* Disable/Remove are hub features of the mod card: always present, even
       if a stale catalog copy arrives without those verbs (the CDN in front
       of raw.githubusercontent can serve older files for a few minutes).
       Make room by dropping Folder first - the least essential of the four. */
    if (slot == 2) {
        static const struct { int id; const wchar_t *label; const wchar_t *verb; int w; } must[2] = {
            { ID_HUD_DISABLE, L"Disable", L"disable", 78 },
            { ID_HUD_REMOVE,  L"Remove",  L"remove",  74 }
        };
        for (int m = 0; m < 2; m++) {
            int have = 0, drop = -1;
            for (int b = 0; b < c->btnCount; b++) {
                if (c->btns[b].id == must[m].id) have = 1;
                if (c->btns[b].id == ID_HUD_FOLDER) drop = b;
            }
            if (have) continue;
            if (drop >= 0) {
                memmove(&c->btns[drop], &c->btns[drop + 1],
                        (size_t)(c->btnCount - drop - 1) * sizeof(ActionBtn));
                c->btnCount--;
            }
            if (c->btnCount >= 4) continue;
            ActionBtn *btn = &c->btns[c->btnCount];
            ZeroMemory(btn, sizeof(*btn));
            btn->id = must[m].id;
            btn->label = must[m].label;
            btn->style = BTN_SECONDARY;
            btn->widthPx = must[m].w;
            btn->verb = must[m].verb;
            c->btnCount++;
        }
    }
    if (icon[0])
        LoadCardIcon(c, icon);
}

static void AddCatalogCard(const char *obj) {
    char id[32], title[64], blurb[128], exe[128], dir[64], icon[128];
    char shortcut[64], buttons[256], args[64];
    if (!JsonGet(obj, "id", id, sizeof(id))) return;
    if (!JsonGet(obj, "exe", exe, sizeof(exe))) exe[0] = '\0';
    JsonGet(obj, "title", title, sizeof(title));
    JsonGet(obj, "blurb", blurb, sizeof(blurb));
    JsonGet(obj, "dir", dir, sizeof(dir));
    JsonGet(obj, "icon", icon, sizeof(icon));
    JsonGet(obj, "shortcut", shortcut, sizeof(shortcut));
    JsonGet(obj, "buttons", buttons, sizeof(buttons));
    JsonGet(obj, "launchArgs", args, sizeof(args));
    if (BuiltInSlot(id) >= 0) {
        /* built-in card: refresh its definition from the catalog */
        FillBuiltInDef(BuiltInSlot(id), id, title, blurb, icon, buttons);
        return;
    }
    if (!exe[0]) return;
    if (g_cardCount >= MAX_APPS) return;

    ProjectCard *c = &g_cards[g_cardCount];
    ZeroMemory(c, sizeof(*c));
    MultiByteToWideChar(CP_UTF8, 0, id, -1, c->id, 23);
    c->title = PoolAdd(title[0] ? title : id);
    c->blurb = PoolAdd(blurb[0] ? blurb : "Suite app");
    c->exeRepo = PoolAdd(exe);
    {
        const char *bs = strrchr(exe, '/');
        c->exeName = PoolAdd(bs ? bs + 1 : exe);
    }
    c->shortcutName = PoolAdd(shortcut[0] ? shortcut : (title[0] ? title : id));
    c->launchArgs = args[0] ? PoolAdd(args) : NULL;

    /* resolve the install dir the same way the built-ins do */
    wchar_t cand[MAX_PATH], parent[MAX_PATH], wtmp[64];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    parent[MAX_PATH - 1] = L'\0';
    {
        wchar_t *slash = wcsrchr(parent, L'\\');
        if (slash) *slash = L'\0';
    }
    c->appDir[0] = L'\0';
    _snwprintf(cand, MAX_PATH - 1, L"%s\\%s", g_exeDir, c->exeName);
    if (FileExistsW(cand)) wcsncpy(c->appDir, g_exeDir, MAX_PATH - 1);
    else {
        const char *slash = strrchr(exe, '/');
        if (slash) {
            char folder[64];
            size_t fl = (size_t)(slash - exe);
            if (fl >= sizeof(folder)) fl = sizeof(folder) - 1;
            memcpy(folder, exe, fl);
            folder[fl] = '\0';
            MultiByteToWideChar(CP_UTF8, 0, folder, -1, wtmp, 63);
            _snwprintf(cand, MAX_PATH - 1, L"%s\\%s", parent, wtmp);
            if (FileExistsW(cand)) wcsncpy(c->appDir, cand, MAX_PATH - 1);
        }
    }
    if (!c->appDir[0]) {
        MultiByteToWideChar(CP_UTF8, 0, dir[0] ? dir : (title[0] ? title : id), -1, wtmp, 63);
        _snwprintf(c->appDir, MAX_PATH - 1, L"%s\\%s", g_suiteDir, wtmp);
    }

    /* buttons: "verb:Label:width,verb:Label:width" */
    if (!buttons[0])
        strncpy(buttons, "install:Install:68,update:Update:74,folder:Folder:68",
                sizeof(buttons) - 1);
    c->btnCount = 0;
    char *bp = buttons;
    while (*bp && c->btnCount < 4) {
        char verb[24] = "", label[32] = "";
        int wpx = 74, n = 0;
        while (*bp && *bp != ':' && *bp != ',' && n < 23) verb[n++] = *bp++;
        verb[n] = '\0';
        if (*bp == ':') bp++;
        n = 0;
        while (*bp && *bp != ':' && *bp != ',' && n < 31) label[n++] = *bp++;
        label[n] = '\0';
        if (*bp == ':') {
            bp++;
            wpx = 0;
            while (*bp >= '0' && *bp <= '9') wpx = wpx * 10 + (*bp++ - '0');
            if (wpx < 40 || wpx > 160) wpx = 74;
        }
        while (*bp && *bp != ',') bp++;
        if (*bp == ',') bp++;
        if (!verb[0]) continue;
        ActionBtn *btn = &c->btns[c->btnCount];
        btn->id = ID_DYN_BASE + g_cardCount * 10 + c->btnCount;
        btn->label = PoolAdd(label[0] ? label : verb);
        btn->style = BTN_SECONDARY;
        btn->widthPx = wpx;
        btn->verb = PoolAdd(verb);
        c->btnCount++;
    }

    /* icon: cached download of the catalog icon, letter tile until it lands */
    if (icon[0])
        LoadCardIcon(c, icon);
    g_cardCount++;
}

static void RebuildExtraCards(const char *json) {
    /* Built-in titles revert to their built-in literals first: any of them
       missing from the catalog must not point at wiped pool strings. */
    static const wchar_t *fallbackTitles[7] = {
        L"Bluetooth Guard", L"Keyboard Lighting", L"Spatial HUD",
        L"OrcaSlicer Plugins", L"DNS Pilot", L"Agent Notes", L"Audio Switcher" };
    for (int i = 0; i < 6; i++) g_cards[i].title = fallbackTitles[i];
    /* Shrink first so a repaint mid-rebuild never reads half-built cards. */
    g_cardCount = 6;
    for (int i = 6; i < MAX_APPS; i++) {
        if (g_cards[i].hIcon) { DestroyIcon(g_cards[i].hIcon); g_cards[i].hIcon = NULL; }
        ZeroMemory(&g_cards[i], sizeof(g_cards[i]));
    }
    g_strPoolUsed = 0;
    const char *apps = json ? strstr(json, "\"apps\"") : NULL;
    const char *p = apps ? strchr(apps, '[') : NULL;
    while (p && *p && *p != ']') {
        const char *b = strchr(p, '{');
        if (!b) break;
        int depth = 0;
        const char *q = b;
        for (; *q; q++) {
            if (*q == '{') depth++;
            else if (*q == '}') { depth--; if (depth == 0) break; }
        }
        if (*q != '}') break;
        size_t len = (size_t)(q - b) + 1;
        if (len < 1024) {
            char obj[1024];
            memcpy(obj, b, len);
            obj[len] = '\0';
            AddCatalogCard(obj);
        }
        p = q + 1;
    }
}

static void WorkerInstallGeneric(int idx, BOOL launchAfter) {
    if (idx < 0 || idx >= g_cardCount) return;
    ProjectCard *c = &g_cards[idx];
    if (!c->exeRepo || !c->exeName) return;
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(c->appDir);

    wchar_t destExe[MAX_PATH], msg[256];
    _snwprintf(destExe, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
    _snwprintf(msg, 255, L"Downloading %s from GitHub...", c->title ? c->title : L"app");
    SetFooterStatus(msg, 60);
    if (!DownloadRawRepoFile(branch, c->exeRepo, destExe)) {
        SetFooterStatus(L"Could not save the file - close the app if it is open, then retry.", 0);
        return;
    }
    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH - 1, L"%s\\%s.lnk", desk,
                   c->shortcutName ? c->shortcutName : (c->title ? c->title : L"Suite App"));
        CreateShortcutLink(lnk, destExe, c->appDir, c->title ? c->title : L"Suite App");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    if (launchAfter) {
        ShellExecuteW(NULL, L"open", destExe, c->launchArgs, c->appDir, SW_SHOWNORMAL);
        _snwprintf(msg, 255, L"%s installed and launched!", c->title ? c->title : L"App");
    } else {
        _snwprintf(msg, 255, L"%s is up to date.", c->title ? c->title : L"App");
    }
    SetFooterStatus(msg, 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void HandleVerb(int idx, const wchar_t *verb) {
    if (idx < 0 || idx >= g_cardCount || !verb) return;
    ProjectCard *c = &g_cards[idx];
    if (wcscmp(verb, L"folder") == 0) {
        EnsureDirTree(c->appDir);
        ShellExecuteW(g_hwnd, L"open", c->appDir, NULL, NULL, SW_SHOWNORMAL);
    } else if (wcscmp(verb, L"update") == 0 || wcscmp(verb, L"install") == 0) {
        g_dynIdx = idx;
        SetFooterStatus(L"Fetching the latest build from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_DYN);
    } else if (wcscmp(verb, L"open") == 0) {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
        if (FileExistsW(p))
            ShellExecuteW(g_hwnd, L"open", p, c->launchArgs, c->appDir, SW_SHOWNORMAL);
        else {
            g_dynIdx = idx;
            SetFooterStatus(L"Fetching the latest build from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_DYN);
        }
    }
}

static void InitCardDefinitions(void) {
    ZeroMemory(g_cards, sizeof(g_cards));

    g_cards[0].resIconId = RES_ICON_BTG;
    g_cards[0].title     = L"Bluetooth Guard";
    g_cards[0].btnCount  = 4;
    g_cards[0].btns[0]   = (ActionBtn){ ID_BTG_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL };
    g_cards[0].btns[1]   = (ActionBtn){ ID_BTG_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL };
    g_cards[0].btns[2]   = (ActionBtn){ ID_BTG_STOP,   L"Stop",   BTN_SECONDARY, 62, {0,0,0,0}, NULL };
    g_cards[0].btns[3]   = (ActionBtn){ ID_BTG_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL };

    g_cards[1].resIconId = RES_ICON_KBL;
    g_cards[1].title     = L"Keyboard Lighting";
    g_cards[1].btnCount  = 4;
    g_cards[1].btns[0]   = (ActionBtn){ ID_KBL_OPEN,    L"Open",     BTN_SECONDARY, 68, {0,0,0,0}, NULL };
    g_cards[1].btns[1]   = (ActionBtn){ ID_KBL_RESTART, L"Unfreeze", BTN_SECONDARY, 84, {0,0,0,0}, NULL };
    g_cards[1].btns[2]   = (ActionBtn){ ID_KBL_UPDATE,  L"Update",   BTN_SECONDARY, 74, {0,0,0,0}, NULL };
    g_cards[1].btns[3]   = (ActionBtn){ ID_KBL_FOLDER,  L"Folder",   BTN_SECONDARY, 68, {0,0,0,0}, NULL };

    g_cards[2].resIconId = RES_ICON_HUD;
    g_cards[2].title     = L"Spatial HUD";
    g_cards[2].btnCount  = 4;
    g_cards[2].btns[0]   = (ActionBtn){ ID_HUD_UPDATE,  L"Update Mod", BTN_SECONDARY, 98, {0,0,0,0}, NULL };
    g_cards[2].btns[1]   = (ActionBtn){ ID_HUD_DISABLE, L"Disable",    BTN_SECONDARY, 78, {0,0,0,0}, NULL };
    g_cards[2].btns[2]   = (ActionBtn){ ID_HUD_PICK,    L"Mods Dir",   BTN_SECONDARY, 84, {0,0,0,0}, NULL };
    g_cards[2].btns[3]   = (ActionBtn){ ID_HUD_REMOVE,  L"Remove",     BTN_SECONDARY, 74, {0,0,0,0}, NULL };

    g_cards[3].resIconId = RES_ICON_ORCA;
    g_cards[3].title     = L"OrcaSlicer Plugins";
    g_cards[3].btnCount  = 3;
    g_cards[3].btns[0]   = (ActionBtn){ ID_ORCA_UPDATE, L"Update", BTN_SECONDARY, 78, {0,0,0,0}, NULL };
    g_cards[3].btns[1]   = (ActionBtn){ ID_ORCA_MENU,   L"Branch", BTN_SECONDARY, 78, {0,0,0,0}, NULL };
    g_cards[3].btns[2]   = (ActionBtn){ ID_ORCA_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL };

    g_cards[4].resIconId = RES_ICON_DNS;
    g_cards[4].title     = L"DNS Pilot";
    g_cards[4].btnCount  = 3;
    g_cards[4].btns[0]   = (ActionBtn){ ID_DNS_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL };
    g_cards[4].btns[1]   = (ActionBtn){ ID_DNS_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL };
    g_cards[4].btns[2]   = (ActionBtn){ ID_DNS_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL };

    g_cards[5].resIconId = RES_ICON_NOTES;
    g_cards[5].title     = L"Agent Notes";
    g_cards[5].btnCount  = 3;
    g_cards[5].btns[0]   = (ActionBtn){ ID_NOTES_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL };
    g_cards[5].btns[1]   = (ActionBtn){ ID_NOTES_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL };
    g_cards[5].btns[2]   = (ActionBtn){ ID_NOTES_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL };

    g_cards[6].resIconId = RES_ICON_AUDIOSWITCHER;
    g_cards[6].title     = L"Audio Switcher";
    g_cards[6].btnCount  = 3;
    g_cards[6].btns[0]   = (ActionBtn){ ID_AUDIOSWITCHER_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL };
    g_cards[6].btns[1]   = (ActionBtn){ ID_AUDIOSWITCHER_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL };
    g_cards[6].btns[2]   = (ActionBtn){ ID_AUDIOSWITCHER_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL };
}

static void RefreshProjectStatuses(void) {
    char remHub[32], remWave[32], remInfill[32];
    wchar_t remHud[128];
    DWORD remHudSz = 0;

    EnterCriticalSection(&g_cs);
    strncpy(remHub, g_remHubVer, 31); remHub[31] = '\0';
    strncpy(remWave, g_remWaveVer, 31); remWave[31] = '\0';
    strncpy(remInfill, g_remInfillVer, 31); remInfill[31] = '\0';
    wcsncpy(remHud, g_remHudName, 127); remHud[127] = L'\0';
    remHudSz = g_remHudSize;
    LeaveCriticalSection(&g_cs);

    DWORD remBtgSz, remKblSz, remPs1Sz, remDnsSz;
    EnterCriticalSection(&g_cs);
    remBtgSz = g_remBtgSize; remKblSz = g_remKblSize; remPs1Sz = g_remKblPs1Size;
    remDnsSz = g_remDnsSize;
    LeaveCriticalSection(&g_cs);
    wchar_t btgExeP[MAX_PATH], kblExeP2[MAX_PATH], kblPs1P[MAX_PATH];
    _snwprintf(btgExeP, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    _snwprintf(kblExeP2, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(kblPs1P, MAX_PATH, L"%s\\Aura-Background.ps1", g_kblDir);
    DWORD locBtgSz = LocalSizeOf(btgExeP);
    if (!locBtgSz && g_btgExtraExe[0]) locBtgSz = LocalSizeOf(g_btgExtraExe);
    BOOL btgKnown = (remBtgSz > 0 && locBtgSz > 0);
    BOOL btgUpd = btgKnown && locBtgSz != remBtgSz;
    DWORD locKblSz = LocalSizeOf(kblExeP2), locPs1Sz = LocalSizeOf(kblPs1P);
    BOOL kblKnown = (remKblSz > 0 && locKblSz > 0);
    BOOL kblUpd = kblKnown && locKblSz != remKblSz;
    if (!kblUpd && remPs1Sz > 0 && locPs1Sz > 0) kblUpd = (locPs1Sz != remPs1Sz);

    g_hubUpdAvail = (remHub[0] != '\0' && VerNewer(remHub, HUB_VERSION));
    g_topBtns[1].label = g_hubUpdAvail ? L"Update Hub \x25B2" : L"Update Hub";
    g_topBtns[1].style = g_hubUpdAvail ? BTN_AMBER : BTN_SECONDARY;

    /* Recompute every button style from a neutral base each refresh. A
       button that turned amber for an update must go back to normal the
       moment the update lands, not stay yellow until the hub restarts. */
    for (int i = 0; i < g_cardCount; i++)
        for (int b = 0; b < g_cards[i].btnCount && b < 4; b++)
            g_cards[i].btns[b].style = BTN_SECONDARY;

    wchar_t path[MAX_PATH];

    /* 1. Bluetooth Guard */
    _snwprintf(path, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    BOOL hasBtg = FileHealthyW(path) || (g_btgExtraExe[0] && FileHealthyW(g_btgExtraExe));
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    int blocked = 0, total = 0;
    int btgState = CheckPidLockStatus(btgLock, 12.0, &blocked, &total);
    if (btgState == 1) {
        wcsncpy(g_cards[0].badge, L"Active", 47);
        g_cards[0].badgeKind = BADGE_OK;
        g_cards[0].btns[0].label = L"Open";
        g_cards[0].btns[0].style = BTN_SECONDARY;
        _snwprintf(g_cards[0].detail, 255,
                   L"Phone audio protection  \x00B7  %d guarded (%d paired)", blocked, total);
    } else if (hasBtg) {
        wcsncpy(g_cards[0].badge, L"Standby", 47);
        g_cards[0].badgeKind = BADGE_IDLE;
        g_cards[0].btns[0].label = L"Start";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Start to launch", 255);
    } else {
        wcsncpy(g_cards[0].badge, L"Setup Ready", 47);
        g_cards[0].badgeKind = BADGE_WARN;
        g_cards[0].btns[0].label = L"Install";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Install to set up", 255);
    }

    /* show BOTH facts whenever both are known: "Active  ·  Up to Date",
       "Active  ·  Update Ready", "Standby  ·  Up to Date", ... */
    if (hasBtg && btgKnown) {
        wcsncat(g_cards[0].badge, btgUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (btgUpd) {
            if (g_cards[0].badgeKind == BADGE_OK || g_cards[0].badgeKind == BADGE_IDLE)
                g_cards[0].badgeKind = BADGE_WARN;
            g_cards[0].btns[1].style = BTN_AMBER;
        }
    }

    /* 2. Keyboard Lighting */
    wchar_t kblExe[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(path, MAX_PATH, L"%s\\Tray.ps1", g_kblDir);
    BOOL hasKbl = FileHealthyW(kblExe) || FileHealthyW(path);
    wchar_t kblLock[MAX_PATH];
    _snwprintf(kblLock, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    int kblState = CheckPidLockStatus(kblLock, 12.0, NULL, NULL);
    if (kblState == 2) {
        wcsncpy(g_cards[1].badge, L"Frozen", 47);
        g_cards[1].badgeKind = BADGE_DANGER;
        g_cards[1].btns[1].style = BTN_AMBER;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Unfreeze to restart", 255);
    } else if (kblState == 1) {
        wcsncpy(g_cards[1].badge, L"Active", 47);
        g_cards[1].badgeKind = BADGE_OK;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting", 255);
    } else if (hasKbl) {
        wcsncpy(g_cards[1].badge, L"Standby", 47);
        g_cards[1].badgeKind = BADGE_IDLE;
        g_cards[1].btns[1].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Installed locally", 255);
    } else {
        wcsncpy(g_cards[1].badge, L"Setup Ready", 47);
        g_cards[1].badgeKind = BADGE_WARN;
        g_cards[1].btns[2].style = BTN_PRIMARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Update to install", 255);
    }

    if (hasKbl && kblKnown) {
        wcsncat(g_cards[1].badge, kblUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (kblUpd) {
            if (g_cards[1].badgeKind == BADGE_OK || g_cards[1].badgeKind == BADGE_IDLE)
                g_cards[1].badgeKind = BADGE_WARN;
            g_cards[1].btns[2].style = BTN_AMBER;
        }
    }

    /* 3. Spatial HUD */
    if (g_hudDisabled) {
        wcsncpy(g_cards[2].badge, L"Disabled", 47);
        g_cards[2].badgeKind = BADGE_IDLE;
        g_cards[2].btns[0].style = BTN_SECONDARY;
        for (int b = 0; b < g_cards[2].btnCount; b++)
            if (g_cards[2].btns[b].id == ID_HUD_DISABLE)
                g_cards[2].btns[b].label = L"Enable";
        wcsncpy(g_cards[2].detail,
                L"Minecraft 26.3 Fabric mod  \x00B7  Disabled - will not install or update", 255);
    } else {
        for (int b = 0; b < g_cards[2].btnCount; b++)
            if (g_cards[2].btns[b].id == ID_HUD_DISABLE)
                g_cards[2].btns[b].label = L"Disable";
        wchar_t searchPat[MAX_PATH], foundJar[128] = L"";
        DWORD foundSize = 0;
        _snwprintf(searchPat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
        WIN32_FIND_DATAW fd;
        HANDLE hf = FindFirstFileW(searchPat, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            wcsncpy(foundJar, fd.cFileName, 127);
            foundSize = fd.nFileSizeLow;
            FindClose(hf);
        }
        if (!foundJar[0]) {
            wcsncpy(g_cards[2].badge, L"Update Ready", 47);
            g_cards[2].badgeKind = BADGE_WARN;
            g_cards[2].btns[0].style = BTN_PRIMARY;
            wcsncpy(g_cards[2].detail, L"Minecraft 26.3 Fabric mod  \x00B7  Click Update Mod to install", 255);
        } else if (remHudSz > 0 && foundSize != remHudSz) {
            wcsncpy(g_cards[2].badge, L"Update Ready", 47);
            g_cards[2].badgeKind = BADGE_WARN;
            g_cards[2].btns[0].style = BTN_PRIMARY;
            _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  Newer build (%s)", remHud);
        } else {
            wcsncpy(g_cards[2].badge, L"Up to Date", 47);
            g_cards[2].badgeKind = BADGE_OK;
            g_cards[2].btns[0].style = BTN_SECONDARY;
            _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  %s", foundJar);
        }


    }
    /* 4. OrcaSlicer Plugins */
    char locWave[32] = "", locInfill[32] = "";
    const wchar_t *probeRel[] = {
        L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py",
        L"%s\\OrcaSlicer\\plugins\\wave_overhangs_orca.py"
    };
    for (int i = 0; i < 2 && !locWave[0]; i++) {
        _snwprintf(path, MAX_PATH, probeRel[i], g_roamApp);
        char *py = ReadTextFileUtf8(path, NULL);
        if (py) { ExtractQuotedAfter(py, "PLUGIN_VERSION", locWave, sizeof(locWave)); free(py); }
    }
    const wchar_t *probeInf[] = {
        L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py",
        L"%s\\OrcaSlicer\\plugins\\unlayered_infill_orca.py"
    };
    for (int i = 0; i < 2 && !locInfill[0]; i++) {
        _snwprintf(path, MAX_PATH, probeInf[i], g_roamApp);
        char *py = ReadTextFileUtf8(path, NULL);
        if (py) { ExtractQuotedAfter(py, "PLUGIN_VERSION", locInfill, sizeof(locInfill)); free(py); }
    }
    if (!locWave[0] && !locInfill[0]) {
        wcsncpy(g_cards[3].badge, L"Update Ready", 47);
        g_cards[3].badgeKind = BADGE_WARN;
        _snwprintf(g_cards[3].detail, 255, L"Wave Overhangs %S & Unlayered Infill %S available",
                   remWave[0] ? remWave : "latest", remInfill[0] ? remInfill : "latest");
    } else {
        BOOL upd = (remWave[0] && VerNewer(remWave, locWave)) ||
                   (remInfill[0] && VerNewer(remInfill, locInfill));
        wcsncpy(g_cards[3].badge, upd ? L"Update Ready" : L"Up to Date", 47);
        g_cards[3].badgeKind = upd ? BADGE_WARN : BADGE_OK;
        g_cards[3].btns[0].style = upd ? BTN_PRIMARY : BTN_SECONDARY;
        _snwprintf(g_cards[3].detail, 255, L"Wave Overhangs %S & Unlayered Infill %S",
                   locWave[0] ? locWave : "none", locInfill[0] ? locInfill : "none");
    }

    /* 5. DNS Pilot */
    wchar_t dnsExe[MAX_PATH];
    _snwprintf(dnsExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
    BOOL hasDns = FileHealthyW(dnsExe);
    DWORD locDnsSz = LocalSizeOf(dnsExe);
    BOOL dnsKnown = (remDnsSz > 0 && locDnsSz > 0);
    BOOL dnsUpd = hasDns && dnsKnown && locDnsSz != remDnsSz;
    BOOL dnsRunning = (FindWindowW(L"KBLSuiteDnsPilotWindow", NULL) != NULL);
    g_cards[4].btns[1].style = BTN_SECONDARY;
    if (!hasDns) {
        wcsncpy(g_cards[4].badge, L"Setup Ready", 47);
        g_cards[4].badgeKind = BADGE_WARN;
        g_cards[4].btns[0].label = L"Install";
        g_cards[4].btns[0].style = BTN_PRIMARY;
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Click Install to set up", 255);
    } else if (dnsRunning) {
        wcsncpy(g_cards[4].badge, L"Active", 47);
        g_cards[4].badgeKind = BADGE_OK;
        g_cards[4].btns[0].label = L"Open";
        g_cards[4].btns[0].style = BTN_SECONDARY;
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Running (window or tray)", 255);
    } else {
        wcsncpy(g_cards[4].badge, L"Standby", 47);
        g_cards[4].badgeKind = BADGE_IDLE;
        g_cards[4].btns[0].label = L"Open";
        g_cards[4].btns[0].style = BTN_SECONDARY;
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Installed locally", 255);
    }
    if (hasDns && dnsKnown) {
        wcsncat(g_cards[4].badge, dnsUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (dnsUpd) {
            if (g_cards[4].badgeKind == BADGE_OK || g_cards[4].badgeKind == BADGE_IDLE)
                g_cards[4].badgeKind = BADGE_WARN;
            g_cards[4].btns[1].style = BTN_AMBER;
        }
    }

    /* 6. Agent Notes */
    {
        DWORD remNotesSz = g_remNotesSize;
        wchar_t notesExe[MAX_PATH];
        _snwprintf(notesExe, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
        BOOL hasNotes = FileHealthyW(notesExe);
        DWORD locNotesSz = LocalSizeOf(notesExe);
        BOOL notesKnown = (remNotesSz > 0 && locNotesSz > 0);
        BOOL notesUpd = hasNotes && notesKnown && locNotesSz != remNotesSz;
        g_cards[5].btns[1].style = BTN_SECONDARY;
        if (!hasNotes) {
            wcsncpy(g_cards[5].badge, L"Setup Ready", 47);
            g_cards[5].badgeKind = BADGE_WARN;
            g_cards[5].btns[0].label = L"Install";
            g_cards[5].btns[0].style = BTN_PRIMARY;
            wcsncpy(g_cards[5].detail, L"Quick notes straight to the agent  \x00B7  Click Install to set up", 255);
        } else {
            wcsncpy(g_cards[5].badge, L"Ready", 47);
            g_cards[5].badgeKind = BADGE_OK;
            g_cards[5].btns[0].label = L"Open";
            g_cards[5].btns[0].style = BTN_SECONDARY;
            wcsncpy(g_cards[5].detail, L"Quick notes straight to the agent  \x00B7  Installed locally", 255);
        }
        if (hasNotes && notesKnown) {
            wcsncat(g_cards[5].badge, notesUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (notesUpd) {
                if (g_cards[5].badgeKind == BADGE_OK || g_cards[5].badgeKind == BADGE_IDLE)
                    g_cards[5].badgeKind = BADGE_WARN;
                g_cards[5].btns[1].style = BTN_AMBER;
            }
        }
    }

    /* 7. Audio Switcher */
    {
        DWORD remAsSz = g_remAudioSwitcherSize;
        wchar_t asExe[MAX_PATH];
        _snwprintf(asExe, MAX_PATH, L"%s\\AudioSwitcher.exe", g_audioSwitcherDir);
        BOOL hasAs = FileHealthyW(asExe);
        DWORD locAsSz = LocalSizeOf(asExe);
        BOOL asKnown = (remAsSz > 0 && locAsSz > 0);
        BOOL asUpd = hasAs && asKnown && locAsSz != remAsSz;
        BOOL asRunning = (FindWindowW(L"TestsAudioSwitcherWnd", NULL) != NULL);
        g_cards[6].btns[1].style = BTN_SECONDARY;
        if (!hasAs) {
            wcsncpy(g_cards[6].badge, L"Setup Ready", 47);
            g_cards[6].badgeKind = BADGE_WARN;
            g_cards[6].btns[0].label = L"Install";
            g_cards[6].btns[0].style = BTN_PRIMARY;
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Click Install to set up", 255);
        } else if (asRunning) {
            wcsncpy(g_cards[6].badge, L"Active", 47);
            g_cards[6].badgeKind = BADGE_OK;
            g_cards[6].btns[0].label = L"Open";
            g_cards[6].btns[0].style = BTN_SECONDARY;
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Running (window or tray)", 255);
        } else {
            wcsncpy(g_cards[6].badge, L"Standby", 47);
            g_cards[6].badgeKind = BADGE_IDLE;
            g_cards[6].btns[0].label = L"Open";
            g_cards[6].btns[0].style = BTN_SECONDARY;
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Installed locally", 255);
        }
        if (hasAs && asKnown) {
            wcsncat(g_cards[6].badge, asUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (asUpd) {
                if (g_cards[6].badgeKind == BADGE_OK || g_cards[6].badgeKind == BADGE_IDLE)
                    g_cards[6].badgeKind = BADGE_WARN;
                g_cards[6].btns[1].style = BTN_AMBER;
            }
        }
    }

    /* 7+. Catalog extras: generic installed / update state */
    for (int i = 6; i < g_cardCount; i++) {
        ProjectCard *c = &g_cards[i];
        wchar_t exePath[MAX_PATH];
        _snwprintf(exePath, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
        BOOL has = FileHealthyW(exePath);
        DWORD locSz = LocalSizeOf(exePath);
        BOOL known = (c->remSize > 0 && locSz > 0);
        BOOL upd = has && known && locSz != c->remSize;
        if (!has) {
            wcsncpy(c->badge, L"Setup Ready", 47);
            c->badgeKind = BADGE_WARN;
            for (int b = 0; b < c->btnCount; b++)
                if (c->btns[b].verb &&
                    (wcscmp(c->btns[b].verb, L"install") == 0 ||
                     wcscmp(c->btns[b].verb, L"open") == 0)) {
                    c->btns[b].style = BTN_PRIMARY;
                    break;
                }
            _snwprintf(c->detail, 255, L"%s  \x00B7  Click Install to set up",
                       c->blurb ? c->blurb : L"Suite app");
        } else {
            wcsncpy(c->badge, L"Ready", 47);
            c->badgeKind = BADGE_OK;
            _snwprintf(c->detail, 255, L"%s  \x00B7  Installed locally",
                       c->blurb ? c->blurb : L"Suite app");
        }
        if (has && known) {
            wcsncat(c->badge, upd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (upd) {
                c->badgeKind = BADGE_WARN;
                for (int b = 0; b < c->btnCount; b++)
                    if (c->btns[b].verb && wcscmp(c->btns[b].verb, L"update") == 0) {
                        c->btns[b].style = BTN_AMBER;
                        break;
                    }
            }
        }
        c->badge[47] = L'\0';
    }

    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static void WorkerCheckRemote(void) {
    wchar_t branch[64];
    SetCheckPhase(TRUE);
    EnsureActiveBranch(branch, 64);

    wchar_t p[256];
    DWORD sz = 0; int st = 0;

    _snwprintf(p, 255, L"/%s/%s/%s/project-hub/src/project_hub.c", REPO_OWNER_W, REPO_NAME_W, branch);
    char *hubSrc = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
    if (hubSrc && st == 200) {
        char hv[32] = "";
        if (ExtractQuotedAfter(hubSrc, "#define HUB_VERSION", hv, sizeof(hv))) {
            EnterCriticalSection(&g_cs);
            strncpy(g_remHubVer, hv, 31);
            LeaveCriticalSection(&g_cs);
        }
    }
    free(hubSrc);

    _snwprintf(p, 255, L"/%s/%s/%s/plugins.json", REPO_OWNER_W, REPO_NAME_W, branch);
    char *manifest = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
    if (manifest && st == 200) {
        char wv[32] = "", iv[32] = "";
        const char *wpos = strstr(manifest, "\"wave-overhangs\"");
        if (wpos) ExtractQuotedAfter(wpos, "\"version\"", wv, sizeof(wv));
        const char *ipos = strstr(manifest, "\"unlayered-infill\"");
        if (ipos) ExtractQuotedAfter(ipos, "\"version\"", iv, sizeof(iv));
        EnterCriticalSection(&g_cs);
        if (wv[0]) strncpy(g_remWaveVer, wv, 31);
        if (iv[0]) strncpy(g_remInfillVer, iv, 31);
        LeaveCriticalSection(&g_cs);
    }
    free(manifest);
    /* check continues - footer dot keeps pulsing, no step churn */

    /* Dynamic app catalog: new suite apps appear on refresh with no hub
       update needed. Built-in card ids are skipped here. The minute-
       granularity cache buster keeps the raw.githubusercontent CDN from
       serving stale copies of the catalog. */
    _snwprintf(p, 255, L"/%s/%s/%s/suite-apps.json?t=%llu", REPO_OWNER_W, REPO_NAME_W, branch,
               (unsigned long long)(GetTickCount64() / 60000ULL));
    char *catalog = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
    if (catalog && st == 200) {
        RebuildExtraCards(catalog);
        EnterCriticalSection(&g_cs);
        free(g_catalogJson);
        g_catalogJson = catalog;
        LeaveCriticalSection(&g_cs);
        catalog = NULL;
    }
    free(catalog);

    /* Remote build sizes: the installed file differing from the branch copy
       is what 'Update Ready' means for the EXE apps. */
    {
        DWORD szBtg = 0, szKbl = 0, szPs1 = 0, szDns = 0, szNotes = 0, szAs = 0;
        _snwprintf(p, 255, L"/%s/%s/%s/bluetooth-guard/BluetoothGuard.exe", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szBtg);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/KeyboardLighting.exe", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szKbl);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/Aura-Background.ps1", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szPs1);
        _snwprintf(p, 255, L"/%s/%s/%s/dns-pilot/DnsPilot.exe", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szDns);
        _snwprintf(p, 255, L"/%s/%s/%s/agent-notes/AgentNotes.exe", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szNotes);
        _snwprintf(p, 255, L"/%s/%s/%s/audio-switcher/AudioSwitcher.exe", REPO_OWNER_W, REPO_NAME_W, branch);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szAs);
        /* Always record the result: a failed HEAD means "unknown" (0), never
           "keep the old number". Stale cached sizes are what left Update Ready
           badges stuck yellow after an update already said up to date. */
        EnterCriticalSection(&g_cs);
        g_remBtgSize = szBtg;
        g_remKblSize = szKbl;
        g_remKblPs1Size = szPs1;
        g_remDnsSize = szDns;
        g_remNotesSize = szNotes;
        g_remAudioSwitcherSize = szAs;
        LeaveCriticalSection(&g_cs);
    }
    /* generic catalog extras: remote size of each app's main exe */
    for (int i = 6; i < g_cardCount; i++) {
        DWORD szX = 0;
        if (g_cards[i].exeRepo) {
            _snwprintf(p, 255, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, branch, g_cards[i].exeRepo);
            HttpHeadSize(L"raw.githubusercontent.com", p, &szX);
        }
        g_cards[i].remSize = szX;
    }

    _snwprintf(p, 255, L"/repos/%s/%s/releases/tags/spatial-hud-latest", REPO_OWNER_W, REPO_NAME_W);
    EnterCriticalSection(&g_cs);
    g_remHudSize = 0;   /* unknown until this lookup succeeds - never stale */
    LeaveCriticalSection(&g_cs);
    char *rel = (char *)HttpGetCached(L"api.github.com", p, &sz, &st);
    if (rel && st == 200) {
        const char *dlPos = strstr(rel, "\"browser_download_url\"");
        if (dlPos) {
            char urlUtf8[512] = "";
            if (ExtractQuotedAfter(dlPos, "\"browser_download_url\"", urlUtf8, sizeof(urlUtf8)) &&
                strncmp(urlUtf8, "https://github.com/", 19) == 0) {
                const char *slashPath = urlUtf8 + 18;
                const char *baseName = strrchr(urlUtf8, '/');
                DWORD assetSz = 0;
                const char *szPos = strstr(rel, "\"size\"");
                if (szPos) {
                    const char *colon = strchr(szPos, ':');
                    if (colon) assetSz = (DWORD)strtoul(colon + 1, NULL, 10);
                }
                EnterCriticalSection(&g_cs);
                MultiByteToWideChar(CP_UTF8, 0, slashPath, -1, g_remHudPath, 511);
                if (baseName) MultiByteToWideChar(CP_UTF8, 0, baseName + 1, -1, g_remHudName, 127);
                g_remHudSize = assetSz;
                LeaveCriticalSection(&g_cs);
            }
        }
    }
    free(rel);

    char remHubLocal[32] = "";
    EnterCriticalSection(&g_cs);
    strncpy(remHubLocal, g_remHubVer, 31);
    LeaveCriticalSection(&g_cs);

    SetCheckPhase(FALSE);
    if (remHubLocal[0] && VerNewer(remHubLocal, HUB_VERSION)) {
        wchar_t updMsg[256];
        _snwprintf(updMsg, 255,
                   L"Project Hub v%S is available on GitHub  ·  Click 'Update Hub' above",
                   remHubLocal);
        SetFooterStatus(updMsg, 0);
    } else {
        SetFooterStatus(L"All suite apps verified  ·  GitHub connected", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void StopProcessFromLock(const wchar_t *lockPath) {
    char *txt = ReadTextFileUtf8(lockPath, NULL);
    if (txt) {
        unsigned long pid = strtoul(txt, NULL, 10);
        free(txt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
        }
    }
    DeleteFileW(lockPath);
}

static BOOL WorkerInstallBluetoothGuard(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_btgDir);

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);

    BOOL btgWasVisible = FALSE;
    {
        HWND wB = FindWindowW(L"TestsBluetoothGuardModernWnd", NULL);
        if (wB && IsWindowVisible(wB)) btgWasVisible = TRUE;
    }

    const wchar_t *stopTarget = FileExistsW(destExe) ? destExe
                              : (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe) ? g_btgExtraExe : NULL);
    if (stopTarget) {
        SetFooterStatus(L"Stopping running Bluetooth Guard before update...", 20);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = stopTarget;
        sei.lpParameters = L"--stop";
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2500);
            CloseHandle(sei.hProcess);
        }
        Sleep(300);
    }

    SetFooterStatus(L"Downloading latest BluetoothGuard.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"bluetooth-guard/BluetoothGuard.exe", destExe)) {
        SetFooterStatus(L"Could not download BluetoothGuard.exe from GitHub.", 0);
        return FALSE;
    }

    /* Also keep any previously launched standalone copy (e.g. in Downloads) in sync! */
    if (g_btgExtraExe[0] && _wcsicmp(g_btgExtraExe, destExe) != 0) {
        CopyFileW(destExe, g_btgExtraExe, FALSE);
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
        CreateShortcutLink(lnk, destExe, g_btgDir, L"Bluetooth Guard - Phone audio protection");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || btgWasVisible) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_btgDir, SW_SHOWNORMAL);
    } else {
        /* Start high-priority tray guard immediately in background & register FastBoot logon task */
        ShellExecuteW(NULL, L"open", destExe, L"--silent", g_btgDir, SW_HIDE);
    }
    SetFooterStatus(L"Bluetooth Guard updated with new UI & icon!", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
    return TRUE;
}

static void WorkerInstallNotes(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_notesDir);

    /* close an open notes window first so the exe is not locked */
    BOOL wasRunning = FALSE;
    HWND hNotes = FindWindowW(L"AgentNotesWnd", NULL);
    if (hNotes) {
        wasRunning = TRUE;
        SetFooterStatus(L"Closing Agent Notes for a clean update...", 8);
        PostMessageW(hNotes, WM_CLOSE, 0, 0);
        Sleep(300);
    }

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
    SetFooterStatus(L"Downloading latest AgentNotes.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"agent-notes/AgentNotes.exe", destExe)) {
        SetFooterStatus(L"Could not save AgentNotes.exe - close Agent Notes if it is open, then retry.", 0);
        return;
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Agent Notes.lnk", desk);
        CreateShortcutLink(lnk, destExe, g_notesDir, L"Agent Notes - quick notes for the agent");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || wasRunning) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_notesDir, SW_SHOWNORMAL);
        SetFooterStatus(wasRunning ? L"Agent Notes updated and reopened!"
                                   : L"Agent Notes installed and launched!", 0);
    } else {
        SetFooterStatus(L"Agent Notes is up to date.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallDnsPilot(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_dnsDir);

    /* An app that is open when its update starts must come back up on its
       own afterwards. A running pilot also holds its exe lock, so close it
       politely first and relaunch it once the new build is in place. */
    BOOL wasRunning = FALSE, wasVisible = FALSE;
    HWND hPilot = FindWindowW(L"KBLSuiteDnsPilotWindow", NULL);
    if (hPilot) {
        wasRunning = TRUE;
        wasVisible = IsWindowVisible(hPilot) ? TRUE : FALSE;
        SetFooterStatus(L"Stopping running DNS Pilot for clean update...", 8);
        wchar_t pilotExe[MAX_PATH];
        _snwprintf(pilotExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
        if (FileExistsW(pilotExe)) {
            /* --stop quits the app for real; WM_CLOSE only hides it to the
               tray now, so the exe would stay locked. */
            SHELLEXECUTEINFOW sei;
            ZeroMemory(&sei, sizeof(sei));
            sei.cbSize = sizeof(sei);
            sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
            sei.lpFile = pilotExe;
            sei.lpParameters = L"--stop";
            sei.nShow  = SW_HIDE;
            if (ShellExecuteExW(&sei) && sei.hProcess) {
                WaitForSingleObject(sei.hProcess, 2500);
                CloseHandle(sei.hProcess);
            }
        }
        Sleep(300);
    }

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);

    SetFooterStatus(L"Downloading latest DnsPilot.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"dns-pilot/DnsPilot.exe", destExe)) {
        SetFooterStatus(L"Could not save DnsPilot.exe - close DNS Pilot if it is open, then retry.", 0);
        return;
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\DNS Pilot.lnk", desk);
        CreateShortcutLink(lnk, destExe, g_dnsDir, L"DNS Pilot - Fast DNS benchmark & auto-pilot");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || wasRunning) {
        /* restore as it was: a visible window comes back, a trayed pilot
           starts quietly in the tray again instead of popping open */
        BOOL show = (launchAfter || wasVisible);
        ShellExecuteW(NULL, L"open", destExe, show ? NULL : L"-Silent", g_dnsDir,
                      show ? SW_SHOWNORMAL : SW_HIDE);
        SetFooterStatus(wasRunning ? L"DNS Pilot updated and reopened!"
                                   : L"DNS Pilot installed and launched!", 0);
    } else {
        SetFooterStatus(L"DNS Pilot is up to date.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallKeyboardLighting(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(g_kblDir);

    wchar_t kblExe[MAX_PATH], lp[MAX_PATH], uiDll[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    BOOL kblWasInstalled = FileExistsW(kblExe);
    BOOL kblWasVisible = FALSE;
    {
        HWND wK = FindWindowW(L"TestsKeyboardLightingSuiteWnd", NULL);
        if (wK && IsWindowVisible(wK)) kblWasVisible = TRUE;
    }
    SetFooterStatus(L"Stopping running Keyboard Lighting for clean update...", 10);
    if (FileExistsW(kblExe)) {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = kblExe;
        sei.lpParameters = L"--stop";
        sei.nShow  = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2000);
            CloseHandle(sei.hProcess);
        }
    }
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);
    Sleep(300);
    _snwprintf(uiDll, MAX_PATH, L"%s\\Ui.dll", g_kblDir);
    DeleteFileW(uiDll);

    const wchar_t *files[] = {
        L"KeyboardLighting.exe", L"Aura-Background.ps1", L"app.ico",
        L"Tray.ps1", L"ui_controls.cs.txt", L"Setup.ps1", L"Setup.bat",
        L"Restart.bat", L"Zones.ps1", L"Zones.bat", L"MyEffect.ps1", L"README.md"
    };
    int count = (int)(sizeof(files) / sizeof(files[0]));
    int dlFail = 0;
    for (int i = 0; i < count; i++) {
        wchar_t msg[256], rel[128], dest[MAX_PATH];
        _snwprintf(msg, 255, L"Downloading Keyboard Lighting (%d/%d): %s...", i + 1, count, files[i]);
        SetFooterStatus(msg, 15 + (i * 80) / count);
        _snwprintf(rel, 127, L"keyboard-lighting/%s", files[i]);
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        if (!DownloadRawRepoFile(branch, rel, dest)) dlFail++;
    }

    if (FileExistsW(kblExe)) {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
            CreateShortcutLink(lnk, kblExe, g_kblDir, L"Keyboard Lighting - ROG Strix G16 RGB");
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        }
        /* restore as it was: a visible window comes back to the front,
           anything else (trayed or not running) starts quietly in the tray
           instead of popping the window open */
        if (kblWasVisible || !kblWasInstalled) {
            ShellExecuteW(NULL, L"open", kblExe, NULL, g_kblDir, SW_SHOWNORMAL);
        } else {
            ShellExecuteW(NULL, L"open", kblExe, L"-Silent", g_kblDir, SW_HIDE);
        }
        if (dlFail) {
            /* never claim a clean update when helper files are missing */
            wchar_t warn[256];
            _snwprintf(warn, 255,
                       L"Keyboard Lighting updated, but %d of %d files failed to download - press Update to retry.",
                       dlFail, count);
            warn[255] = L'\0';
            SetFooterStatus(warn, 0);
        } else {
            SetFooterStatus(kblWasVisible || !kblWasInstalled
                                ? L"Keyboard Lighting updated and reopened!"
                                : L"Keyboard Lighting updated - running quietly in the tray.", 0);
        }
    } else {
        SetFooterStatus(L"Could not download KeyboardLighting.exe from GitHub.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallSpatialHud(void) {
    if (g_hudDisabled) {
        SetFooterStatus(L"Spatial HUD is disabled - click Enable on its card first.", 0);
        return;
    }
    SetCheckPhase(TRUE);
    if (!g_remHudPath[0]) WorkerCheckRemote();

    wchar_t dlPath[512], jarName[128];
    EnterCriticalSection(&g_cs);
    wcsncpy(dlPath, g_remHudPath, 511); dlPath[511] = L'\0';
    wcsncpy(jarName, g_remHudName, 127); jarName[127] = L'\0';
    LeaveCriticalSection(&g_cs);

    SetCheckPhase(FALSE);
    if (!dlPath[0] || !jarName[0]) {
        SetFooterStatus(L"No published Spatial HUD JAR found under 'spatial-hud-latest'.", 0);
        return;
    }

    SetFooterStatus(L"Downloading Spatial HUD Fabric JAR...", 60);
    DWORD sz = 0; int st = 0;
    BYTE *jarBytes = HttpGetBuffer(L"github.com", dlPath, &sz, &st);
    if (!jarBytes || st != 200 || sz < 4096 || jarBytes[0] != 'P' || jarBytes[1] != 'K') {
        free(jarBytes);
        SetFooterStatus(L"Spatial HUD download failed or invalid archive.", 0);
        return;
    }

    EnsureDirTree(g_modsDir);

    wchar_t destJar[MAX_PATH];
    _snwprintf(destJar, MAX_PATH, L"%s\\%s", g_modsDir, jarName);
    /* Land the NEW jar first. If the write fails (Minecraft holding a jar,
       disk full, ...) every previously installed jar must survive - only
       after the new jar is safely on disk do we retire the old versions. */
    BOOL ok = WriteBinaryFileAtomic(destJar, jarBytes, sz);
    free(jarBytes);

    if (ok) {
        wchar_t pat[MAX_PATH];
        _snwprintf(pat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
        WIN32_FIND_DATAW fd;
        HANDLE hf = FindFirstFileW(pat, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                if (lstrcmpiW(fd.cFileName, jarName) == 0) continue;
                wchar_t oldJar[MAX_PATH];
                _snwprintf(oldJar, MAX_PATH, L"%s\\%s", g_modsDir, fd.cFileName);
                DeleteFileW(oldJar);
            } while (FindNextFileW(hf, &fd));
            FindClose(hf);
        }
        wchar_t msg[300];
        _snwprintf(msg, 299, L"Spatial HUD updated (%s).", jarName);
        SetFooterStatus(msg, 0);
    } else {
        SetFooterStatus(L"Could not write JAR to mods folder (close Minecraft first).", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallOrcaPlugins(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);

    SetFooterStatus(L"Downloading Wave Overhangs & Unlayered Infill...", 40);
    EnsureDirTree(g_orcaBatDir);

    wchar_t batPath[MAX_PATH], tmpWave[MAX_PATH], tmpInf[MAX_PATH];
    _snwprintf(batPath, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    _snwprintf(tmpWave, MAX_PATH, L"%s\\wave_overhangs_orca.py", g_hubCfgDir);
    _snwprintf(tmpInf,  MAX_PATH, L"%s\\unlayered_infill_orca.py", g_hubCfgDir);

    BOOL ok1 = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", batPath);
    BOOL ok2 = DownloadRawRepoFile(branch, L"plugins/wave-overhangs/wave_overhangs_orca.py", tmpWave);
    BOOL ok3 = DownloadRawRepoFile(branch, L"plugins/unlayered-infill/unlayered_infill_orca.py", tmpInf);
    if (!ok2 || !ok3) {
        SetFooterStatus(L"Failed downloading OrcaSlicer plugins from GitHub.", 0);
        return;
    }

    wchar_t searchOrca[MAX_PATH];
    _snwprintf(searchOrca, MAX_PATH, L"%s\\OrcaSlicer*", g_roamApp);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(searchOrca, &fd);
    int targetsInstalled = 0;

    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
                _snwprintf(wDir, MAX_PATH, L"%s\\%s\\orca_plugins\\wave-overhangs", g_roamApp, fd.cFileName);
                _snwprintf(iDir, MAX_PATH, L"%s\\%s\\orca_plugins\\unlayered-infill", g_roamApp, fd.cFileName);
                _snwprintf(legacyDir, MAX_PATH, L"%s\\%s\\plugins", g_roamApp, fd.cFileName);
                EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
                const char *stJson = "{\"enabled\": true}\n";
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
                CopyFileW(tmpInf, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
                CopyFileW(tmpInf, dst, FALSE);
                targetsInstalled++;
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }

    if (targetsInstalled == 0) {
        wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
        _snwprintf(wDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs", g_roamApp);
        _snwprintf(iDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill", g_roamApp);
        _snwprintf(legacyDir, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);
        const char *stJson = "{\"enabled\": true}\n";
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
        CopyFileW(tmpInf, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
        CopyFileW(tmpInf, dst, FALSE);
    }

    /* End-state honesty: CopyFileW fails silently while OrcaSlicer holds
       locks, so verify the plugin files actually landed before claiming it. */
    BOOL landed = TRUE;
    if (targetsInstalled == 0) {
        wchar_t chk[MAX_PATH];
        _snwprintf(chk, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py", g_roamApp);
        landed = FileExistsW(chk);
        _snwprintf(chk, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py", g_roamApp);
        landed = landed && FileExistsW(chk);
    }
    if (!landed) {
        SetFooterStatus(L"OrcaSlicer plugin files could not be written - close OrcaSlicer and retry.", 0);
    } else if (!ok1) {
        SetFooterStatus(L"OrcaSlicer plugins updated, but Orca-Plugins.bat failed to download - restart OrcaSlicer to load.", 0);
    } else {
        SetFooterStatus(L"OrcaSlicer plugins updated — restart OrcaSlicer to load.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerSelfUpdateHub(void) {
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    SetFooterStatus(L"Downloading latest ProjectHub.exe from GitHub...", 50);

    wchar_t newExe[MAX_PATH], oldExe[MAX_PATH];
    _snwprintf(newExe, MAX_PATH, L"%s.new", g_exePath);
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    if (!DownloadRawRepoFile(branch, L"project-hub/ProjectHub.exe", newExe)) {
        SetFooterStatus(L"Could not download ProjectHub.exe from GitHub.", 0);
        return;
    }

    /* The running image can be renamed away but not overwritten. The second
       move must therefore never be allowed to fail with the old exe already
       renamed - that is how ProjectHub.exe could disappear from disk.
       Retry through transient locks (AV scans love fresh downloads), and if
       it still fails put the old exe back so the hub always stays present. */
    /* Transient locks (AV scans love fresh downloads) can bounce this
       rename a few times - retry before giving up and keeping the hub. */
    BOOL renamed = FALSE;
    for (int i = 0; i < 5 && !renamed; i++) {
        renamed = MoveFileExW(g_exePath, oldExe, MOVEFILE_REPLACE_EXISTING);
        if (!renamed) Sleep(200);
    }
    if (!renamed) {
        DeleteFileW(newExe);
        SetFooterStatus(L"Could not replace ProjectHub.exe on disk.", 0);
        return;
    }

    BOOL swapped = FALSE;
    for (int i = 0; i < 10 && !swapped; i++) {
        swapped = MoveFileExW(newExe, g_exePath, MOVEFILE_REPLACE_EXISTING);
        if (!swapped) Sleep(300);
    }
    if (!swapped) {
        for (int i = 0; i < 10; i++) {
            if (MoveFileExW(oldExe, g_exePath, MOVEFILE_REPLACE_EXISTING)) break;
            Sleep(300);
        }
        if (FileExistsW(g_exePath)) {
            DeleteFileW(newExe);
            SetFooterStatus(L"Could not install the update - kept the current Project Hub.", 0);
        } else {
            SetFooterStatus(L"Update failed - ProjectHub.exe.old still holds the last good hub.", 0);
        }
        return;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    SetFooterStatus(L"Project Hub updated — relaunching...", 100);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_SELFUPD, 0, 0);
}

static void EnqueueTask(int task) {
    EnterCriticalSection(&g_cs);
    int next = (g_qTail + 1) % 32;
    if (next != g_qHead) {
        g_taskQueue[g_qTail] = task;
        g_qTail = next;
        ReleaseSemaphore(g_qSem, 1, NULL);
    }
    LeaveCriticalSection(&g_cs);
}

static DWORD WINAPI HubWorkerThread(LPVOID unused) {
    (void)unused;
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        if (WaitForSingleObject(g_qSem, INFINITE) != WAIT_OBJECT_0) break;
        if (InterlockedCompareExchange(&g_running, 1, 1) == 0) break;

        int task = TASK_NONE;
        EnterCriticalSection(&g_cs);
        if (g_qHead != g_qTail) {
            task = g_taskQueue[g_qHead];
            g_qHead = (g_qHead + 1) % 32;
        }
        LeaveCriticalSection(&g_cs);

        switch (task) {
        case TASK_CHECK_REMOTE:        WorkerCheckRemote(); break;
        case TASK_INSTALL_BTG:         WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_OPEN_OR_INSTALL_BTG: WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_INSTALL_KBL:         WorkerInstallKeyboardLighting(); break;
        case TASK_INSTALL_HUD:         WorkerInstallSpatialHud(); break;
        case TASK_INSTALL_ORCA:        WorkerInstallOrcaPlugins(); break;
        case TASK_INSTALL_DNS:         WorkerInstallDnsPilot(FALSE); break;
        case TASK_OPEN_OR_INSTALL_DNS: WorkerInstallDnsPilot(TRUE); break;
        case TASK_INSTALL_NOTES:         WorkerInstallNotes(FALSE); break;
        case TASK_OPEN_OR_INSTALL_NOTES: WorkerInstallNotes(TRUE); break;
        case TASK_INSTALL_DYN:           WorkerInstallGeneric(g_dynIdx, FALSE); break;
        case TASK_OPEN_OR_INSTALL_DYN:   WorkerInstallGeneric(g_dynIdx, TRUE); break;
        case TASK_UPDATE_ALL:
            WorkerInstallBluetoothGuard(FALSE);
            WorkerInstallKeyboardLighting();
            WorkerInstallOrcaPlugins();
            for (int i = 6; i < g_cardCount; i++) WorkerInstallGeneric(i, FALSE);
            WorkerInstallSpatialHud();
            WorkerInstallDnsPilot(FALSE);
            WorkerCheckRemote();
            SetFooterStatus(L"All suite apps updated!", 0);
            break;
        case TASK_SELF_UPDATE:         WorkerSelfUpdateHub(); break;
        default: break;
        }
    }
    return 0;
}

static void RestartKeyboardLightingCleanly(void) {
    wchar_t lp[MAX_PATH];
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);

    wchar_t exe[MAX_PATH], rstBat[MAX_PATH];
    _snwprintf(exe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(rstBat, MAX_PATH, L"%s\\Restart.bat", g_kblDir);
    if (FileExistsW(exe))
        ShellExecuteW(g_hwnd, L"open", exe, NULL, g_kblDir, SW_SHOWNORMAL);
    else if (FileExistsW(rstBat))
        ShellExecuteW(g_hwnd, L"open", rstBat, NULL, g_kblDir, SW_SHOWNORMAL);
    else
        EnqueueTask(TASK_INSTALL_KBL);
    SetFooterStatus(L"Restarted Keyboard Lighting cleanly.", 0);
}

static INT_PTR PromptTextInput(HWND hwnd, const wchar_t *label, const wchar_t *preset,
                               wchar_t *out, int outCap, BOOL withBrowse,
                               const wchar_t *okLabel);

static void NormalizeDirPath(wchar_t *s) {
    /* People paste from Explorer: quotes, stray spaces and a trailing
       backslash all happen. Clean them so the path is usable as-is. */
    int n = 0;
    while (s[n] == L' ' || s[n] == L'\t') n++;
    if (n > 0) memmove(s, s + n, (wcslen(s + n) + 1) * sizeof(wchar_t));
    n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t')) s[--n] = L'\0';
    if (n >= 2 && s[0] == L'"' && s[n - 1] == L'"') {
        memmove(s, s + 1, (size_t)(n - 2) * sizeof(wchar_t));
        s[n - 2] = L'\0';
    }
    n = (int)wcslen(s);
    while (n > 3 && (s[n - 1] == L'\\' || s[n - 1] == L'/')) s[--n] = L'\0';
}

static void SaveModsDirFromInput(wchar_t *input) {
    NormalizeDirPath(input);
    if (!input[0]) return;
    DWORD attr = GetFileAttributesW(input);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetFooterStatus(L"That is a file - enter the folder that contains your mods.", 0);
        return;
    }
    wcsncpy(g_modsDir, input, MAX_PATH - 1);
    g_modsDir[MAX_PATH - 1] = L'\0';
    EnsureDirTree(g_modsDir);
    wchar_t saveDir[MAX_PATH], saveFile[MAX_PATH];
    _snwprintf(saveDir, MAX_PATH, L"%s\\SpatialHudUpdater", g_localApp);
    EnsureDirTree(saveDir);
    _snwprintf(saveFile, MAX_PATH, L"%s\\target-directory.txt", saveDir);
    char utf8[MAX_PATH * 3];
    int n = WideCharToMultiByte(CP_UTF8, 0, g_modsDir, -1, utf8, sizeof(utf8), NULL, NULL);
    if (n > 1) WriteBinaryFileAtomic(saveFile, (const BYTE *)utf8, (DWORD)(n - 1));
    SetFooterStatus(L"Mods folder set.", 0);
    RefreshProjectStatuses();
}

static void PickSpatialHudModsFolder(void) {
    wchar_t input[MAX_PATH];
    wcsncpy(input, g_modsDir, MAX_PATH - 1);
    input[MAX_PATH - 1] = L'\0';
    INT_PTR r = PromptTextInput(g_hwnd,
                        L"Mods folder path - paste or type it, or click Browse:",
                        input, input, MAX_PATH, TRUE, L"Use Folder");
    if (r == -1) {
        /* Dialog could not be created - never leave the button dead. */
        BROWSEINFOW bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.hwndOwner = g_hwnd;
        bi.lpszTitle = L"Select your Minecraft / Modrinth 'mods' folder:";
        bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            if (SHGetPathFromIDListW(pidl, input) && input[0])
                SaveModsDirFromInput(input);
            CoTaskMemFree(pidl);
        }
        return;
    }
    if (r != IDOK) return;
    SaveModsDirFromInput(input);
}

/* ============================================================================
 * Sub-Pixel Anti-Aliased Smooth Rounded UI Renderer (32-bit DIBSection)
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

    int iy0 = (int)floorf(y0 - 1.0f);
    int iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f);
    int ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP   = PackRgb(fillCol);
    uint32_t borderP = PackRgb(borderCol);
    BOOL sameBorder  = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd;
            if (qx > 0.0f && qy > 0.0f) {
                sd = sqrtf(qx * qx + qy * qy) - radius;
            } else {
                sd = (qx > qy ? qx : qy) - radius;
            }
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) {
                row[x] = c;
            } else {
                int cov = (int)((0.5f - sd) * 256.0f);
                row[x] = BlendRgb(row[x], c, cov);
            }
        }
    }
}

static void DrawActionBtn(HDC hdc, uint32_t *px, int W, int H, const ActionBtn *btn,
                          int dpi, int clipTop, int clipBottom) {
    BOOL hot = (g_hoverId == btn->id);
    COLORREF bg, border, fg = COL_TEXT_PRIMARY;
    if (btn->style == BTN_PRIMARY) {
        bg     = hot ? COL_PRIMARY_HOVER : COL_PRIMARY_BG;
        border = bg;
    } else if (btn->style == BTN_AMBER) {
        bg     = hot ? COL_AMBER_BTN_HOVER : COL_AMBER_BTN;
        border = COL_AMBER;
        fg     = COL_AMBER;
    } else {
        bg     = hot ? COL_BTN_HOVER : COL_BTN_BG;
        border = hot ? COL_HOVER_BORDER : COL_BTN_BORDER;
    }
    SmoothRoundRect(px, W, H, &btn->rc, (float)S(7, dpi), bg, border, clipTop, clipBottom);
    SelectObject(hdc, g_fontBtn);
    SetTextColor(hdc, fg);
    RECT tr = btn->rc;
    DrawTextW(hdc, btn->label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void ComputeLayout(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int cardH = S(80, dpi);
    int gap = S(11, dpi);

    int embSz = S(52, dpi);
    int embY = (headerH - embSz) / 2;
    SetRect(&g_rcEmblem, pad, embY, pad + embSz, embY + embSz);

    int bx = w - pad;
    int topBtnH = S(35, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    for (int i = 3; i >= 0; i--) {
        int bw = S(g_topBtns[i].widthPx, dpi);
        bx -= bw;
        SetRect(&g_topBtns[i].rc, bx, topBtnY, bx + bw, topBtnY + topBtnH);
        bx -= S(9, dpi);
    }

    int contentH = g_cardCount * cardH + (g_cardCount > 1 ? (g_cardCount - 1) : 0) * gap + S(24, dpi);
    int viewH = h - headerH - footerH;
    g_maxScrollY = (contentH > viewH) ? (contentH - viewH) : 0;
    if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
    if (g_scrollY < 0) g_scrollY = 0;

    int startY = headerH + ((viewH > contentH) ? (viewH - contentH) / 2 + S(12, dpi) : S(12, dpi)) - g_scrollY;
    int btnH = S(30, dpi);
    for (int c = 0; c < g_cardCount; c++) {
        SetRect(&g_cards[c].rcCard, pad, startY, w - pad, startY + cardH);
        int rx = w - pad - S(16, dpi);
        int by = startY + cardH - S(12, dpi) - btnH;
        for (int b = g_cards[c].btnCount - 1; b >= 0; b--) {
            int bw = S(g_cards[c].btns[b].widthPx, dpi);
            rx -= bw;
            SetRect(&g_cards[c].btns[b].rc, rx, by, rx + bw, by + btnH);
            rx -= S(7, dpi);
        }
        startY += cardH + gap;
    }
}

static int HitTestButtons(POINT pt, int headerBottom, int footerTop) {
    if (PtInRect(&g_rcEmblem, pt)) return ID_EMBLEM_MENU;
    for (int i = 0; i < 4; i++) {
        if (PtInRect(&g_topBtns[i].rc, pt)) return g_topBtns[i].id;
    }
    if (pt.y < headerBottom || pt.y > footerTop) return 0;
    for (int c = 0; c < g_cardCount; c++) {
        for (int b = 0; b < g_cards[c].btnCount; b++) {
            if (PtInRect(&g_cards[c].btns[b].rc, pt)) return g_cards[c].btns[b].id;
        }
    }
    return 0;
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). The
   square is filled with the canvas background first - a raw COLORREF fill
   would swap red/blue in the 32bpp DIB and show as a box. */
static void PaintEmblemBlit(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    uint32_t bgP = ((uint32_t)GetRValue(COL_BG) << 16) |
                   ((uint32_t)GetGValue(COL_BG) << 8) |
                   (uint32_t)GetBValue(COL_BG);
    for (int i = 0; i < sz * sz; i++) px[i] = bgP;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

/* Static-frame cache for the spin fast-path: the 50 ms emblem timer blits
   this snapshot plus the spinning square instead of re-rendering every
   card, pill and row of text 20 times a second. */
static HBITMAP g_frameCache = NULL;
static int    g_fcW = 0, g_fcH = 0;

static void PaintHubWindow(HWND hwnd, HDC targetDc, const RECT *updRc) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    if (w <= 0 || h <= 0) return;

    ComputeLayout(hwnd);

    /* Spin fast-path: an emblem-only update rect plus a cached frame means
       we blit the snapshot and repaint just the spinning square - no card
       sweeps, no per-pixel roundrects, no text at 20 fps. */
    if (updRc && g_frameCache && g_fcW == w && g_fcH == h &&
        updRc->right > updRc->left && updRc->bottom > updRc->top &&
        g_rcEmblem.right > g_rcEmblem.left &&
        updRc->left >= g_rcEmblem.left && updRc->top >= g_rcEmblem.top &&
        updRc->right <= g_rcEmblem.right && updRc->bottom <= g_rcEmblem.bottom) {
        HDC cacheDc = CreateCompatibleDC(targetDc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(targetDc, 0, 0, w, h, cacheDc, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
        PaintEmblemBlit(targetDc, g_rcEmblem.left, g_rcEmblem.top,
                        g_rcEmblem.right - g_rcEmblem.left);
        return;
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; /* top-down 32bpp */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC hdc = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(hdc); return; }
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);
    SetBkMode(hdc, TRANSPARENT);

    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int footerTop = h - footerH;

    /* Soft low-glare slate canvas */
    FillCanvasRect(px, w, h, 0, 0, w, footerTop, COL_BG);
    FillCanvasRect(px, w, h, 0, footerTop, w, h, COL_FOOTER_BG);

    /* Clipped project cards */
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, 0, headerH + 1, w, footerTop - 1);

    int cardIcoSz = S(44, dpi);
    for (int c = 0; c < g_cardCount; c++) {
        const ProjectCard *card = &g_cards[c];
        if (card->rcCard.bottom < headerH || card->rcCard.top > footerTop) continue;

        SmoothRoundRect(px, w, h, &card->rcCard, (float)S(10, dpi),
                        COL_CARD_BG, COL_CARD_BORDER, headerH + 1, footerTop - 1);

        COLORREF tagFg = COL_TEXT_SECONDARY, tagBg = COL_IDLE_BG, barCol = COL_IDLE_SLATE;
        if (card->badgeKind == BADGE_OK) {
            tagFg = COL_MINT; tagBg = COL_MINT_BG; barCol = COL_MINT;
        } else if (card->badgeKind == BADGE_WARN) {
            tagFg = COL_AMBER; tagBg = COL_AMBER_BG; barCol = COL_AMBER;
        } else if (card->badgeKind == BADGE_SKY) {
            tagFg = COL_SKY; tagBg = COL_SKY_BG; barCol = COL_SKY;
        } else if (card->badgeKind == BADGE_DANGER) {
            tagFg = COL_CORAL; tagBg = COL_CORAL_BG; barCol = COL_CORAL;
        }

        /* Soft inset rounded accent capsule on left of card */
        RECT barRc = {
            card->rcCard.left + S(1, dpi),
            card->rcCard.top + S(16, dpi),
            card->rcCard.left + S(5, dpi),
            card->rcCard.bottom - S(16, dpi)
        };
        SmoothRoundRect(px, w, h, &barRc, (float)S(2, dpi), barCol, barCol, headerH + 1, footerTop - 1);

        /* Catalog icon first (download_icons), then embedded, letter tile */
        {
            HICON ico = card->hIcon ? card->hIcon : g_cardIcons[c];
            int ix = card->rcCard.left + S(18, dpi);
            int iy = (card->rcCard.top + card->rcCard.bottom - cardIcoSz) / 2;
            if (ico) {
                DrawIconEx(hdc, ix, iy, ico, cardIcoSz, cardIcoSz, 0, NULL, DI_NORMAL);
            } else if (card->title && card->title[0]) {
                RECT tile = { ix, iy, ix + cardIcoSz, iy + cardIcoSz };
                SmoothRoundRect(px, w, h, &tile, (float)S(10, dpi),
                                COL_SKY_BG, COL_SKY, headerH + 1, footerTop - 1);
                wchar_t lt[2];
                lt[0] = card->title[0];
                lt[1] = L'\0';
                SelectObject(hdc, g_fontBadge);
                SetTextColor(hdc, COL_SKY);
                DrawTextW(hdc, lt, -1, &tile, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        /* Smooth rounded status pill badge on top right */
        SelectObject(hdc, g_fontBadge);
        const wchar_t *badgeTxt = card->badge[0] ? card->badge : L"Checking";
        SIZE bsz = {0, 0};
        GetTextExtentPoint32W(hdc, badgeTxt, (int)wcslen(badgeTxt), &bsz);
        int pillH = S(23, dpi);
        int pillW = bsz.cx + S(28, dpi);
        RECT pillRc = {
            card->rcCard.right - S(16, dpi) - pillW,
            card->rcCard.top + S(11, dpi),
            card->rcCard.right - S(16, dpi),
            card->rcCard.top + S(11, dpi) + pillH
        };
        SmoothRoundRect(px, w, h, &pillRc, (float)pillH * 0.5f, tagBg, tagBg, headerH + 1, footerTop - 1);

        int dotD = S(6, dpi);
        RECT dotRc = {
            pillRc.left + S(9, dpi),
            pillRc.top + (pillH - dotD) / 2,
            pillRc.left + S(9, dpi) + dotD,
            pillRc.top + (pillH - dotD) / 2 + dotD
        };
        SmoothRoundRect(px, w, h, &dotRc, (float)dotD * 0.5f, tagFg, tagFg, headerH + 1, footerTop - 1);

        RECT pillTxtRc = { pillRc.left + S(19, dpi), pillRc.top, pillRc.right - S(8, dpi), pillRc.bottom };
        SetTextColor(hdc, tagFg);
        DrawTextW(hdc, badgeTxt, -1, &pillTxtRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        /* Clean Title Case card title + calm summary */
        int textLeft = card->rcCard.left + S(74, dpi);
        SelectObject(hdc, g_fontTitle);
        SetTextColor(hdc, COL_TEXT_PRIMARY);
        RECT titleRc = { textLeft, card->rcCard.top + S(13, dpi), pillRc.left - S(12, dpi), card->rcCard.top + S(37, dpi) };
        DrawTextW(hdc, card->title, -1, &titleRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        int firstBtnLeft = (card->btnCount > 0) ? card->btns[0].rc.left : (card->rcCard.right - S(16, dpi));
        SelectObject(hdc, g_fontBody);
        SetTextColor(hdc, COL_TEXT_SECONDARY);
        RECT detRc = { textLeft, card->rcCard.top + S(39, dpi), firstBtnLeft - S(14, dpi), card->rcCard.bottom - S(12, dpi) };
        DrawTextW(hdc, card->detail, -1, &detRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        for (int b = 0; b < card->btnCount; b++) {
            DrawActionBtn(hdc, px, w, h, &card->btns[b], dpi, headerH + 1, footerTop - 1);
        }
    }

    RestoreDC(hdc, saved);

    /* Header: Suite Tri-Spoke Core Emblem + natural title + calm subtitle + rounded buttons */
    int embSz = S(52, dpi);
    DrawWayfinderEmblem(px, w, h, g_rcEmblem.left, g_rcEmblem.top, embSz);

    int hdrTextLeft = g_rcEmblem.right + S(14, dpi);
    int hdrRightLimit = g_topBtns[0].rc.left - S(14, dpi);
    SelectObject(hdc, g_fontHero);
    SetTextColor(hdc, COL_TEXT_PRIMARY);
    RECT hTitle = { hdrTextLeft, S(13, dpi), hdrRightLimit, S(40, dpi) };
    DrawTextW(hdc, L"Project Hub", -1, &hTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, COL_TEXT_MUTED);
    RECT hSub = { hdrTextLeft, S(40, dpi), hdrRightLimit, S(60, dpi) };
    {
        wchar_t subBuf[160];
        EnterCriticalSection(&g_cs);
        _snwprintf(subBuf, 159, L"Suite Control  \x00B7  v" HUB_VERSION_W L"  \x00B7  %s",
                   g_activeBranch[0] ? g_activeBranch : L"auto");
        subBuf[159] = L'\0';
        LeaveCriticalSection(&g_cs);
        DrawTextW(hdc, subBuf, -1, &hSub,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    for (int i = 0; i < 4; i++) {
        DrawActionBtn(hdc, px, w, h, &g_topBtns[i], dpi, 0, headerH);
    }

    FillCanvasRect(px, w, h, pad, headerH, w - pad, headerH + 1, COL_DIVIDER);
    FillCanvasRect(px, w, h, 0, footerTop, w, footerTop + 1, COL_DIVIDER);

    wchar_t footMsg[512];
    int pct = 0;
    EnterCriticalSection(&g_cs);
    wcsncpy(footMsg, g_statusMsg, 511); footMsg[511] = L'\0';
    pct = g_progressVal;
    LeaveCriticalSection(&g_cs);

    BOOL checking = InterlockedCompareExchange(&g_checkActive, 0, 0) != 0;

    /* The full-width progress sweep is reserved for real downloads. The
       periodic update check just pulses a small dot so the footer stays
       readable and nothing sweeps across the window. */
    if (!checking && pct > 0 && pct < 100) {
        int fillW = (w * pct) / 100;
        FillCanvasRect(px, w, h, 0, footerTop - 1, fillW, footerTop + S(2, dpi), COL_SKY);
    }

    int fdSz = S(8, dpi);
    COLORREF dotCol;
    if (checking) {
        double ph = (double)(GetTickCount64() % 1400ULL) / 1400.0;
        double tri = (ph < 0.5) ? (ph * 2.0) : (2.0 - ph * 2.0);
        fdSz = S(5, dpi) + (int)(tri * S(5, dpi) + 0.5);
        int tr = (int)(tri * 255.0);
        int dr = 42 + ((GetRValue(COL_SKY) - 42) * tr) / 255;
        int dg = 84 + ((GetGValue(COL_SKY) - 84) * tr) / 255;
        int db = 130 + ((GetBValue(COL_SKY) - 130) * tr) / 255;
        dotCol = RGB(dr, dg, db);
    } else {
        dotCol = g_hubUpdAvail ? COL_AMBER : ((pct > 0 && pct < 100) ? COL_SKY : COL_MINT);
    }
    RECT fDot = { pad, footerTop + (footerH - fdSz) / 2, pad + fdSz, footerTop + (footerH - fdSz) / 2 + fdSz };
    SmoothRoundRect(px, w, h, &fDot, (float)fdSz * 0.5f, dotCol, dotCol, footerTop, h);
    g_rcFooterDot.left = pad - S(4, dpi);
    g_rcFooterDot.top = footerTop;
    g_rcFooterDot.right = pad + S(18, dpi);
    g_rcFooterDot.bottom = h;

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, g_hubUpdAvail ? COL_AMBER : COL_TEXT_SECONDARY);
    RECT fTxt = { pad + S(16, dpi), footerTop, w - pad, h };
    DrawTextW(hdc, footMsg, -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    /* snapshot the composed frame for the spin fast-path */
    if (!g_frameCache || g_fcW != w || g_fcH != h) {
        if (g_frameCache) DeleteObject(g_frameCache);
        g_frameCache = CreateCompatibleBitmap(targetDc, w, h);
        g_fcW = w;
        g_fcH = h;
    }
    if (g_frameCache) {
        HDC cacheDc = CreateCompatibleDC(targetDc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(cacheDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
    }

    BitBlt(targetDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

static void ShowUtilityMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SHORTCUT,  L"Pin Project Hub Shortcut to Desktop");
    AppendMenuW(hMenu, MF_STRING, ID_BTN_SELF_UPDATE, L"Self-Update Project Hub Now");
    AppendMenuW(hMenu, MF_STRING, ID_BTN_BRANCH, L"Download Branch...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SUITE_DIR, L"Open Managed Suite Folder");

    POINT pt = { g_rcEmblem.left, g_rcEmblem.bottom + 4 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

/* ---- download-branch picker ------------------------------------------- */

static void FetchBranchNames(void) {
    g_branchMenuN = 0;
    DWORD sz = 0;
    int st = 0;
    BYTE *buf = HttpGetBuffer(L"api.github.com",
                              L"/repos/ajani190819-ops/Tests/branches?per_page=100",
                              &sz, &st);
    if (!buf || st != 200) { free(buf); return; }
    char *s = (char *)buf;
    while (g_branchMenuN < 96) {
        char *q = strstr(s, "\"name\":\"");
        if (!q) break;
        q += 8;
        char *e = strchr(q, '"');
        if (!e || (e - q) >= 64) break;
        int i;
        for (i = 0; q + i < e; i++)
            g_branchMenuNames[g_branchMenuN][i] = (wchar_t)(unsigned char)q[i];
        g_branchMenuNames[g_branchMenuN][i] = L'\0';
        g_branchMenuN++;
        s = e + 1;
    }
    free(buf);
}

static void EnsureBranchInMenu(const wchar_t *name) {
    int i;
    for (i = 0; i < g_branchMenuN; i++)
        if (_wcsicmp(g_branchMenuNames[i], name) == 0) return;
    if (g_branchMenuN >= 96) return;
    wcsncpy(g_branchMenuNames[g_branchMenuN], name, 63);
    g_branchMenuNames[g_branchMenuN][63] = L'\0';
    g_branchMenuN++;
}

/* Generic one-field text prompt (in-memory dialog template: the C struct has
   no members for the trailing menu/class/title). Optional Browse button
   fills the edit via the standard folder picker - so a path can be pasted,
   typed, or browsed in one place. */
typedef struct {
    const wchar_t *preset;
    wchar_t      *out;
    int           outCap;
    BOOL          withBrowse;
} PromptCtx;

static INT_PTR CALLBACK TextPromptProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    PromptCtx *ctx = (PromptCtx *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    switch (msg) {
    case WM_INITDIALOG:
        ctx = (PromptCtx *)lp;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);
        if (ctx && ctx->preset && ctx->preset[0]) SetDlgItemTextW(hDlg, 201, ctx->preset);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == 202 && ctx && ctx->withBrowse) {
            BROWSEINFOW bi;
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = hDlg;
            bi.lpszTitle = L"Select the folder:";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t picked[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, picked) && picked[0]) {
                    /* Selecting a folder in the picker is the decision:
                       accept immediately - no second click needed. */
                    wcsncpy(ctx->out, picked, ctx->outCap - 1);
                    ctx->out[ctx->outCap - 1] = L'\0';
                    EndDialog(hDlg, IDOK);
                }
                CoTaskMemFree(pidl);
            }
            return TRUE;
        }
        if (LOWORD(wp) == IDOK && ctx && ctx->out) {
            GetDlgItemTextW(hDlg, 201, ctx->out, ctx->outCap - 1);
            ctx->out[ctx->outCap - 1] = L'\0';
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR PromptTextInput(HWND hwnd, const wchar_t *label, const wchar_t *preset,
                               wchar_t *out, int outCap, BOOL withBrowse,
                               const wchar_t *okLabel) {
    static BYTE buf[1400];
    ZeroMemory(buf, sizeof(buf));
    BYTE *p = buf;
    int nItems = withBrowse ? 5 : 4;
    int cx = withBrowse ? 320 : 240;
    int cy = withBrowse ? 92 : 78;
    *(DWORD *)p = DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU; p += 4;
    *(DWORD *)p = 0;             p += 4;   /* dwExtendedStyle (required!) */
    *(WORD *)p = (WORD)nItems; p += 2;
    *(WORD *)p = 10;   p += 2;
    *(WORD *)p = 10;   p += 2;
    *(WORD *)p = (WORD)cx; p += 2;
    *(WORD *)p = (WORD)cy; p += 2;
    *(WORD *)p = 0;    p += 2;
    *(WORD *)p = 0;    p += 2;
    *(WCHAR *)p = 0;   p += 2;
    *(WORD *)p = 9;    p += 2;
    {
        static const WCHAR face[] = L"Segoe UI";
        CopyMemory(p, face, sizeof(face));
        p += sizeof(face);
    }
    struct { DWORD style; short x, y, cx, cy; WORD id; WORD cls; const WCHAR *title; } items[5] = {
        { WS_CHILD | WS_VISIBLE | SS_LEFT, 8, 12, (short)(cx - 16), 12, 101, 0x0082, label },
        { WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
          8, 28, (short)(cx - 16), 14, 201, 0x0081, L"" },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
          8, (short)(cy - 26), 56, 16, 202, 0x0080, L"Browse..." },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
          (short)(withBrowse ? cx - 126 : 128), (short)(cy - 26), 56, 16, IDOK, 0x0080, okLabel },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
          (short)(withBrowse ? cx - 64 : 182), (short)(cy - 26), 48, 16, IDCANCEL, 0x0080, L"Cancel" }
    };
    /* Without Browse the dialog is label, edit, OK, Cancel - skip item 2. */
    const int order = withBrowse ? 1 : 0;
    const int seq[2][5] = { { 0, 1, 3, 4, -1 }, { 0, 1, 2, 3, 4 } };
    for (int i = 0; i < nItems; i++) {
        int it = seq[order][i];
        p = (BYTE *)(((ULONG_PTR)p + 3) & ~(ULONG_PTR)3);
        *(DWORD *)p = items[it].style; p += 4;
        *(DWORD *)p = 0;             p += 4;
        *(WORD *)p = (WORD)items[it].x;  p += 2;
        *(WORD *)p = (WORD)items[it].y;  p += 2;
        *(WORD *)p = (WORD)items[it].cx; p += 2;
        *(WORD *)p = (WORD)items[it].cy; p += 2;
        *(WORD *)p = items[it].id;    p += 2;
        *(WORD *)p = 0xFFFF;         p += 2;
        *(WORD *)p = items[it].cls;   p += 2;
        {
            size_t n = wcslen(items[it].title) + 1;
            CopyMemory(p, items[it].title, n * sizeof(WCHAR));
            p += n * sizeof(WCHAR);
        }
        *(WORD *)p = 0;              p += 2;
    }
    PromptCtx ctx = { preset, out, outCap > 0 ? outCap : 1, withBrowse };
    if (out != preset) out[0] = L'\0';   /* keep the preset when buffers alias */
    return DialogBoxIndirectParamW(g_hInst, (DLGTEMPLATE *)buf, hwnd,
                                   TextPromptProc, (LPARAM)&ctx);
}

static INT_PTR PromptBranchName(HWND hwnd, const wchar_t *preset) {
    return PromptTextInput(hwnd, L"GitHub branch name:", preset,
                           g_branchPromptOut, 64, FALSE, L"OK");
}

static void SetActiveBranch(HWND hwnd, const wchar_t *branch) {
    if (!branch || !branch[0]) return;
    EnterCriticalSection(&g_cs);
    wcsncpy(g_activeBranch, branch, 63);
    g_activeBranch[63] = L'\0';
    LeaveCriticalSection(&g_cs);
    SaveBranchCfg(branch);
    {
        wchar_t msg[160];
        _snwprintf(msg, 159, L"Download branch set to '%s' - refreshing update checks.", branch);
        msg[159] = L'\0';
        SetFooterStatus(msg, 0);
    }
    RefreshProjectStatuses();
    EnqueueTask(TASK_CHECK_REMOTE);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ShowBranchMenu(HWND hwnd) {
    FetchBranchNames();
    if (g_branchMenuN == 0) {
        EnsureBranchInMenu(L"main");
        EnsureBranchInMenu(FALLBACK_BRANCH_W);
    }
    EnsureBranchInMenu(L"main");
    EnsureBranchInMenu(FALLBACK_BRANCH_W);
    {
        wchar_t active[64];
        EnsureActiveBranch(active, 64);
        EnsureBranchInMenu(active);
    }
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Download updates from:");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    {
        wchar_t active[64];
        EnsureActiveBranch(active, 64);
        for (int i = 0; i < g_branchMenuN; i++) {
            UINT flags = MF_STRING;
            if (_wcsicmp(g_branchMenuNames[i], active) == 0) flags |= MF_CHECKED;
            AppendMenuW(hMenu, flags, ID_MENU_BRANCH_BASE + i, g_branchMenuNames[i]);
        }
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_BRANCH_CUSTOM, L"Other branch...");
    {
        POINT pt = { g_topBtns[3].rc.left, g_topBtns[3].rc.bottom + 4 };
        ClientToScreen(hwnd, &pt);
        TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                       pt.x, pt.y, 0, hwnd, NULL);
    }
    DestroyMenu(hMenu);
}

static void HandleActionCommand(HWND hwnd, int id) {
    wchar_t p[MAX_PATH];
    /* dynamic branch-menu entries carry an index into the fetched list */
    if (id >= ID_MENU_BRANCH_BASE && id < ID_MENU_BRANCH_CUSTOM) {
        int idx = id - ID_MENU_BRANCH_BASE;
        if (idx >= 0 && idx < g_branchMenuN)
            SetActiveBranch(hwnd, g_branchMenuNames[idx]);
        return;
    }
    if (id >= ID_DYN_BASE && id < ID_DYN_BASE + MAX_APPS * 10) {
        int aidx = (id - ID_DYN_BASE) / 10;
        int bidx = (id - ID_DYN_BASE) % 10;
        if (aidx >= 0 && aidx < g_cardCount && bidx >= 0 && bidx < g_cards[aidx].btnCount)
            HandleVerb(aidx, g_cards[aidx].btns[bidx].verb);
        return;
    }
    switch (id) {
    case ID_BTN_REFRESH_ALL:
        RefreshProjectStatuses();
        EnqueueTask(TASK_CHECK_REMOTE);
        break;
    case ID_BTN_SELF_UPDATE:
        EnqueueTask(TASK_SELF_UPDATE);
        break;
    case ID_BTN_UPDATE_ALL:
        EnqueueTask(TASK_UPDATE_ALL);
        break;
    case ID_BTN_BRANCH:
        ShowBranchMenu(hwnd);
        break;
    case ID_MENU_BRANCH_CUSTOM: {
        wchar_t cur[64];
        EnsureActiveBranch(cur, 64);
        if (PromptBranchName(hwnd, cur) == IDOK && g_branchPromptOut[0])
            SetActiveBranch(hwnd, g_branchPromptOut);
        break;
    }
    case ID_EMBLEM_MENU:
        ShowUtilityMenu(hwnd);
        break;
    case ID_MENU_SHORTCUT: {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
            if (CreateShortcutLink(lnk, g_exePath, g_exeDir, L"Project Hub - Status & Updates")) {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
                SetFooterStatus(L"Pinned 'Project Hub' shortcut to Desktop.", 0);
            }
        }
        break;
    }
    case ID_MENU_SUITE_DIR:
        EnsureDirTree(g_suiteDir);
        ShellExecuteW(hwnd, L"open", g_suiteDir, NULL, NULL, SW_SHOWNORMAL);
        break;


    case ID_BTG_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_btgDir, SW_SHOWNORMAL);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, NULL, NULL, SW_SHOWNORMAL);
        else EnqueueTask(TASK_OPEN_OR_INSTALL_BTG);
        break;
    case ID_BTG_UPDATE:
        EnqueueTask(TASK_INSTALL_BTG);
        break;
    case ID_BTG_STOP:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, L"--stop", g_btgDir, SW_HIDE);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, L"--stop", NULL, SW_HIDE);
        SetFooterStatus(L"Stopped Bluetooth Guard.", 0);
        break;
    case ID_BTG_FOLDER:
        EnsureDirTree(g_btgDir);
        ShellExecuteW(hwnd, L"open", g_btgDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_DNS_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_dnsDir, SW_SHOWNORMAL);
        else {
            SetFooterStatus(L"DNS Pilot: fetching latest DnsPilot.exe from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_DNS);
        }
        break;
    case ID_DNS_UPDATE:
        SetFooterStatus(L"DNS Pilot: fetching latest DnsPilot.exe from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_DNS);
        break;
    case ID_DNS_FOLDER:
        EnsureDirTree(g_dnsDir);
        ShellExecuteW(hwnd, L"open", g_dnsDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_NOTES_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_notesDir, SW_SHOWNORMAL);
        else {
            SetFooterStatus(L"Agent Notes: fetching latest AgentNotes.exe from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_NOTES);
        }
        break;
    case ID_NOTES_UPDATE:
        SetFooterStatus(L"Agent Notes: fetching latest AgentNotes.exe from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_NOTES);
        break;
    case ID_NOTES_FOLDER:
        EnsureDirTree(g_notesDir);
        ShellExecuteW(hwnd, L"open", g_notesDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_KBL_OPEN: {
        wchar_t showFlag[MAX_PATH];
        _snwprintf(showFlag, MAX_PATH, L"%s\\KeyboardLighting\\show.flag", g_localApp);
        WriteBinaryFileAtomic(showFlag, (const BYTE *)"1", 1);
        _snwprintf(p, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_kblDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_KBL);
        break;
    }
    case ID_KBL_RESTART:
        RestartKeyboardLightingCleanly();
        break;
    case ID_KBL_UPDATE:
        EnqueueTask(TASK_INSTALL_KBL);
        break;
    case ID_KBL_FOLDER:
        EnsureDirTree(g_kblDir);
        ShellExecuteW(hwnd, L"open", g_kblDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_HUD_UPDATE:
        EnqueueTask(TASK_INSTALL_HUD);
        break;
    case ID_HUD_PICK:
        PickSpatialHudModsFolder();
        break;
    case ID_HUD_FOLDER:
        EnsureDirTree(g_modsDir);
        ShellExecuteW(hwnd, L"open", g_modsDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_HUD_DISABLE:
        g_hudDisabled = !g_hudDisabled;
        SaveAppsCfg();
        SetFooterStatus(g_hudDisabled
                            ? L"Spatial HUD disabled - it will not be installed or updated."
                            : L"Spatial HUD enabled.", 0);
        RefreshProjectStatuses();
        break;

    case ID_HUD_REMOVE: {
        wchar_t pat[MAX_PATH + 32], path[MAX_PATH + 64];
        WIN32_FIND_DATAW rfd;
        int removed = 0, blocked = 0;
        _snwprintf(pat, MAX_PATH + 31, L"%s\\spatial-hud*.jar", g_modsDir);
        HANDLE rh = FindFirstFileW(pat, &rfd);
        if (rh != INVALID_HANDLE_VALUE) {
            do {
                _snwprintf(path, MAX_PATH + 63, L"%s\\%s", g_modsDir, rfd.cFileName);
                if (DeleteFileW(path)) removed++;
                else blocked++;
            } while (FindNextFileW(rh, &rfd));
            FindClose(rh);
        }
        if (removed > 0 && blocked == 0)
            SetFooterStatus(L"Mod removed from your mods folder.", 0);
        else if (removed > 0)
            SetFooterStatus(L"Some mod files removed - close Minecraft to remove the rest.", 0);
        else if (blocked > 0)
            SetFooterStatus(L"Could not remove the mod - close Minecraft first, then retry.", 0);
        else
            SetFooterStatus(L"Nothing to remove - the mod is not in your mods folder.", 0);
        RefreshProjectStatuses();
        break;
    }

    case ID_ORCA_UPDATE:
        EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_MENU:
        _snwprintf(p, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_orcaBatDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_FOLDER:
        _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins", g_roamApp);
        if (!DirExistsW(p)) _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(p);
        ShellExecuteW(hwnd, L"open", p, NULL, NULL, SW_SHOWNORMAL);
        break;

    }
}

static LRESULT CALLBACK HubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        RebuildFontsAndIcons(hwnd);
        LoadAppsCfg();
        InitCardDefinitions();
        RefreshProjectStatuses();
        SetTimer(hwnd, 1, 2000, NULL);
        SetTimer(hwnd, 2, 50, NULL);    /* emblem spin repaint */
        EnqueueTask(TASK_CHECK_REMOTE);
        return 0;

    case WM_DPICHANGED: {
        RebuildFontsAndIcons(hwnd);
        const RECT *suggested = (const RECT *)lp;
        if (suggested) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        int dpi = GetWinDpi(hwnd);
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = S(720, dpi);
        mmi->ptMinTrackSize.y = S(460, dpi);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintHubWindow(hwnd, hdc, &ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int dpi = GetWinDpi(hwnd);
        g_scrollY -= (delta > 0) ? S(36, dpi) : -S(36, dpi);
        if (g_scrollY < 0) g_scrollY = 0;
        if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
        if (hit != g_hoverId) {
            g_hoverId = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hoverId != 0) {
            g_hoverId = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hoverId != 0) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONUP: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
        if (hit != 0) HandleActionCommand(hwnd, hit);
        return 0;
    }

    case WM_RBUTTONUP:
        ShowUtilityMenu(hwnd);
        return 0;

    case WM_COMMAND:
        HandleActionCommand(hwnd, LOWORD(wp));
        return 0;

    case WM_HUB_REFRESH:
        RefreshProjectStatuses();
        return 0;

    case WM_HUB_STATUS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_HUB_SELFUPD: {
        /* Hand the single-instance lock over BEFORE spawning the new build.
           If this old process still holds it, the new hub sees a duplicate
           launch, forwards to us and exits - and once we exit too nothing is
           left running. */
        if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = NULL; }
        HINSTANCE hi = ShellExecuteW(NULL, L"open", g_exePath, NULL, g_exeDir, SW_SHOWNORMAL);
        if ((INT_PTR)hi <= 32) {
            SetFooterStatus(L"Project Hub updated - please reopen it.", 0);
            return 0;   /* stay up rather than vanish with no successor */
        }
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_TIMER:
        if (wp == 2) {
            /* Emblem spin only - keep this light so the animated mark
               never costs a full status sweep. */
            if (g_rcEmblem.right > g_rcEmblem.left)
                InvalidateRect(hwnd, &g_rcEmblem, FALSE);
            if (InterlockedCompareExchange(&g_checkActive, 0, 0) != 0)
                InvalidateRect(hwnd, &g_rcFooterDot, FALSE);
            return 0;
        }
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        RefreshProjectStatuses();
        {
            /* Every 5th tick = 10s - one third of the emblem's 30s
             * rotation: quietly re-check GitHub for Hub & app updates so
             * "Update Hub" lights up on its own. */
            static int tickN = 0;
            if (++tickN >= 30) { tickN = 0; EnqueueTask(TASK_CHECK_REMOTE); }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;
    g_hInst = hInst;

    /* A hub that just updated itself spawns the new build while the old
       process is still shutting down and holding this mutex. Wait briefly
       for the lock instead of instantly treating the fresh copy as a second
       click and quitting - that race is why the hub never reopened after an
       update. A genuine duplicate launch still forwards and exits. */
    BOOL solo = FALSE;
    for (int mi = 0; mi < 12; mi++) {
        g_hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
        if (GetLastError() != ERROR_ALREADY_EXISTS) { solo = TRUE; break; }
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
        HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
        if (ev) { SetEvent(ev); CloseHandle(ev); }
        Sleep(250);
    }
    if (!solo) return 0;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitializeCriticalSection(&g_cs);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    g_qSem      = CreateSemaphoreW(NULL, 0, 64, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    InitAllPaths();
    wchar_t oldExe[MAX_PATH];
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = HubWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsProjectHubNativeWnd";
    RegisterClassExW(&wc);

    HANDLE hWorker = CreateThread(NULL, 0, HubWorkerThread, NULL, 0, NULL);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(860, dpi), S(700, dpi),
                             NULL, NULL, hInst, NULL);

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_running, 0);
    ReleaseSemaphore(g_qSem, 1, NULL);
    if (hWorker) { WaitForSingleObject(hWorker, 1500); CloseHandle(hWorker); }
    CloseHandle(g_qSem);
    if (g_showEvent) CloseHandle(g_showEvent);
    if (g_hMutex) CloseHandle(g_hMutex);
    DeleteCriticalSection(&g_cs);
    CoUninitialize();
    return 0;
}
