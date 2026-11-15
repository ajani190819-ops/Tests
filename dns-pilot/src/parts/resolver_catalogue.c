/* ---- resolver catalogue: primary + secondary per family ---- */
typedef struct DnsServer {
    const char *name;
    const char *v4a, *v4b;
    const char *v6a, *v6b;
} DnsServer;

static const DnsServer SERVERS[] = {
    { "Cloudflare",         "1.1.1.1",         "1.0.0.1",         "2606:4700:4700::1111", "2606:4700:4700::1001" },
    { "Cloudflare Security","1.1.1.2",         "1.0.0.2",         "2606:4700:4700::1112", "2606:4700:4700::1002" },
    { "Cloudflare Family",  "1.1.1.3",         "1.0.0.3",         "2606:4700:4700::1113", "2606:4700:4700::1003" },
    { "Google Public DNS",  "8.8.8.8",         "8.8.4.4",         "2001:4860:4860::8888", "2001:4860:4860::8844" },
    { "Quad9",              "9.9.9.9",         "149.112.112.112", "2620:fe::fe",          "2620:fe::9" },
    { "Quad9 Unfiltered",   "9.9.9.10",        "149.112.112.10",  "2620:fe::10",          "" },
    { "OpenDNS",            "208.67.222.222",  "208.67.220.220",  "2620:119:35::35",      "2620:119:35::36" },
    { "AdGuard DNS",        "94.140.14.14",    "94.140.15.15",    "2a10:50c0::ad1:ff",    "2a10:50c0::ad2:ff" },
    { "AdGuard Unfiltered", "94.140.14.140",   "94.140.15.140",   "2a10:50c0::ad1:0:ff",  "2a10:50c0::ad2:0:ff" },
    { "ControlD Free",      "76.76.2.0",       "76.76.10.0",      "2602:fcdc:0:2::2",     "2602:fcdc:0:3::2" },
    { "Mullvad DNS",        "194.242.2.2",     "",                "2a07:e340::2",         "" },
    { "Mullvad Adblock",    "194.242.2.3",     "",                "2a07:e340::3",         "" },
    { "CleanBrowsing",      "185.228.168.168", "185.228.169.168", "2a0d:2a00:1::1",       "2a0d:2a00:2::1" },
    { "DNS.SB",             "185.222.222.222", "45.11.45.11",     "2a09::",               "2a11::" },
    { "AliDNS",             "223.5.5.5",       "223.6.6.6",       "2400:3200::1",         "2400:3200:baba::1" },
    { "Quad101",            "101.101.101.101", "101.102.101.101", "2001:de0::1",          "2001:de0::2" },
};
#define SERVER_COUNT ((int)(sizeof(SERVERS) / sizeof(SERVERS[0])))

typedef struct Row {
    int         srv;      /* index into SERVERS */
    int         family;   /* 4 or 6 */
    const char *ip;       /* primary or secondary literal */
    int         isBackup;
    int         tested;
    int         ok;
    int         usedTcp;
    int         ms[3];
    int         med;
    int         rank;     /* 1-based within family after a run */
} Row;

static Row g_rows[128];
static int g_rowCount = 0;

#define WM_TRAYICON     (WM_APP + 2)
#define IDM_TRAY_OPEN   4001
#define IDM_TRAY_TEST   4002
#define IDM_TRAY_AUTO   4003
#define IDM_TRAY_QUIT   4004

static HWND        g_hwnd = NULL;
static HICON       g_hIcon = NULL;
static NOTIFYICONDATAW g_nid;
static UINT        g_exitMsg = 0;
static int         g_quitting = 0;
static int         g_trayHinted = 0;
static HFONT       g_fontUI = NULL, g_fontTitle = NULL, g_fontBtn = NULL;

static void WCpy(wchar_t *d, int n, const wchar_t *s) {
    wcsncpy(d, s, (size_t)(n - 1));
    d[n - 1] = L'\0';
}
static int         g_dpi = 96;
static int         g_testing = 0;
static int         g_testedCount = 0;
static volatile LONG g_exitStop = 0;   /* set on quit: threads bail out fast */
static volatile LONG g_testNext = 0;   /* next row index claimed by a probe worker */
static int         g_topRow = 0;
static int         g_hoverBtn = -1;
static ULONGLONG   g_lastTestAt = 0;
static int         g_elevated = 0;
static int         g_auto = 0;
static int         g_intervalMin = 30;
static int         g_autoRun = 0;
static wchar_t     g_status[512] = L"Press Test & Rank to benchmark the listed resolvers.";
static wchar_t     g_adName[128] = L"(no adapter found)";
static DWORD       g_ifIndex = 0;
static char        g_curV4[64] = "";
static char        g_curV6[64] = "";
/* Live stats strip: passive adapter throughput + 1 Hz ICMP ping */
static CRITICAL_SECTION g_statsCs;
static wchar_t      g_statsLine[160] = L"GW -  |  DNS -  |  \x2193 - Mbps  |  \x2191 - Mbps";
static volatile LONG g_statsRun = 1;
static char         g_gwIp[64] = "";   /* default gateway, v4 or v6 */
static int          g_gwIsV6 = 0;
static char         g_srcV6[64] = "";  /* adapter's own v6 for ICMP6 source */
static char         g_srcV6LL[64] = "";/* link-local v6 (fe80::) for LL targets */
static int          g_hasGlobalV6 = 0; /* adapter has a global (2000::/3) v6 */
static wchar_t     g_cfgPath[MAX_PATH];
static char        g_lastV4[64] = "", g_lastV6[64] = "";
static int         g_best4 = -1, g_best6 = -1;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

/* Title bar, border and corners in the exact same colours as Project Hub:
   the caption strip matches the background instead of the OS grey. */
static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD cornerPref = 2; /* DWMWCP_ROUND - full smooth Windows 11 rounded corners */
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

static void Widen(const char *a, wchar_t *w, int n) {
    if (!a) { w[0] = L'\0'; return; }
    MultiByteToWideChar(CP_UTF8, 0, a, -1, w, n);
    w[n - 1] = L'\0';
}

