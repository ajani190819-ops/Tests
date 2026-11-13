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
#include <bcrypt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define HUB_VERSION       "2.8.43"
#define HUB_VERSION_W     L"2.8.43"
#define APP_TITLE         L"Project Hub"
#define SINGLE_MUTEX_NAME L"Local\\Tests_ProjectHub_SingleInstance_v2"
#define SHOW_EVENT_NAME   L"Local\\Tests_ProjectHub_ShowWindow_v2"

#define REPO_OWNER_W      L"ajani190819-ops"
#define REPO_NAME_W       L"Tests"
#define FALLBACK_BRANCH_W L"arena/e6c1980c-tests"

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
#define ID_MENU_HUB_TOSUITE  1013

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
#define ID_ORCA_DISABLE      1404
#define ID_ORCA_FOLDER       1403
#define ID_ORCA_REMOVE       1405


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
    BOOL           hidden; /* set TRUE to skip this button in layout & drawing */
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
    char           remSha[41];   /* git blob SHA-1 of the remote build, when known */
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
    { ID_BTN_REFRESH_ALL, L"Refresh",    BTN_SECONDARY, 92,  {0,0,0,0}, NULL, FALSE },
    { ID_BTN_SELF_UPDATE, L"Update Hub", BTN_AMBER,     112, {0,0,0,0}, NULL, FALSE },
    { ID_BTN_UPDATE_ALL,  L"Update All", BTN_PRIMARY,   108, {0,0,0,0}, NULL, FALSE },
    { ID_BTN_BRANCH,      L"Branch \x25BC", BTN_SECONDARY, 96, {0,0,0,0}, NULL, FALSE }
};
static RECT             g_rcEmblem    = {0, 0, 0, 0};

static ProjectCard      g_cards[MAX_APPS];
static int              g_cardCount = 7;   /* 7 built-ins + catalog extras */
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

/* The suite folder is the one place a user browses to see the suite, so it
   is where every install writes. Any OTHER copy that already exists on disk
   (next to a hub that was launched from Downloads, or inside a repo
   checkout) is registered here and receives the very same bytes, so no copy
   can be left behind at an older build. */
#define MAX_MIRRORS 6
static wchar_t          g_suiteHubExe[MAX_PATH];
static wchar_t          g_mirrorDirs[7][MAX_MIRRORS][MAX_PATH];
static int              g_mirrorCount[7];

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
    for (int i = 0; i < 7; i++) {
        if (g_cardIcons[i]) { DestroyIcon(g_cardIcons[i]); g_cardIcons[i] = NULL; }
        g_cardIcons[i] = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(ids[i]),
                                           IMAGE_ICON, cardIcoPx, cardIcoPx, LR_DEFAULTCOLOR);
    }
}


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so project_hub.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/emblem.c"
#include "parts/content_identity.c"
#include "parts/http_layer.c"
#include "parts/version_probe_cache.c"
#include "parts/install_folder.c"
#include "parts/catalog_icons_json.c"
#include "parts/builtin_card_text.c"
#include "parts/card_defs_verbs.c"
#include "parts/refresh_statuses.c"
#include "parts/workers_remote.c"
#include "parts/workers_install.c"
#include "parts/enqueue_restart_prompts.c"
#include "parts/ui_renderer.c"
#include "parts/branch_picker_entry.c"
