/* ============================================================================
 *  Win32 window + UI + tray
 * ============================================================================ */

static HINSTANCE g_hInst = NULL;
static HWND   g_hwnd = NULL;
static HICON  g_app_icon = NULL;
static NOTIFYICONDATAW g_nid = {0};
static BOOL   g_quitting = FALSE;
static BOOL   g_silent = FALSE;
/* Single-instance "show the window" signalling. A second launch (the Hub's
   Open button, a desktop shortcut, a double-click) cannot create a second
   window, so it sets this named event and a watcher thread marshals the
   request onto the UI thread. An event is used instead of a cross-process
   PostMessage because the tray instance is normally elevated (the logon task
   runs RunLevel Highest) and UIPI silently drops a message from a lower-
   integrity launcher; a named event is not subject to UIPI. */
static HANDLE g_showEvent = NULL;
static HANDLE g_showThread = NULL;
static volatile BOOL g_showThreadRun = FALSE;

#define PANEL_X      24
#define PANEL_Y      80
#define PANEL_W      640

static HWND g_btn_refresh = NULL;

typedef struct {
    HWND hwnd_btn;
    HWND hwnd_vol;
    HWND hwnd_mute;
    HWND hwnd_label;
    HWND hwnd_voltxt;
    int  endpoint_idx;
} EndpointCtrl;

static EndpointCtrl g_endpoint_ctrls[32];

typedef struct {
    HWND hwnd_label;
    HWND hwnd_vol;
    HWND hwnd_mute;
    HWND hwnd_voltxt;
    int  session_idx;
} SessionCtrl;

static SessionCtrl g_session_ctrls[128];

static int g_dpi = 96;
static int S(int x) { return MulDiv(x, g_dpi, 96); }

/* Spinning emblem */
static RECT   g_rcEmblem   = {0, 0, 0, 0};

