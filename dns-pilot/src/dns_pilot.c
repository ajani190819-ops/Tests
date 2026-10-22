/* DNS Pilot - benchmark the major public DNS resolvers and steer Windows
   through the fastest ones.

   The dorm/school problem this solves: speed-test websites are blocked or
   unreliable on managed Wi-Fi, so this app measures DNS itself - real UDP
   port-53 queries to every listed resolver, the same path Windows will use
   once the DNS is set. Both IPv4 and IPv6 are ranked separately and the
   winners (plus their secondary) are applied to the active adapter.

   Auto-Pilot re-tests on an interval and switches ONLY when the new best is
   meaningfully faster than what is applied (>=15% and >=8ms) and nothing
   full-screen is in the foreground - so it never resets the connection in
   the middle of what you are doing.

   Setting DNS needs administrator rights; the app says so plainly and can
   relaunch itself elevated for Auto-Pilot sessions. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windowsx.h>
#include <shellapi.h>
#include <math.h>
#include <stdint.h>

#ifndef GGI_MARK_NONEXISTING_GLYPHS
#define GGI_MARK_NONEXISTING_GLYPHS 1
#endif

#define APP_TITLE_W   L"DNS Pilot"
#define APP_DIR_NAME  L"\\DnsPilot"
#define WM_APP_DONE   (WM_APP + 1)
#define SINGLE_MUTEX_NAME L"Local\\Tests_DnsPilot_SingleInstance"

#define COL_BG        RGB(24, 27, 36)
#define COL_CARD      RGB(34, 39, 52)
#define COL_ROW2      RGB(29, 33, 45)
#define COL_TXT       RGB(232, 236, 245)
#define COL_MUTE      RGB(151, 160, 179)
#define COL_ACCENT    RGB(78, 161, 255)
#define COL_OK        RGB(61, 220, 151)
#define COL_WARN      RGB(245, 181, 68)
#define COL_BAD       RGB(255, 107, 107)
#define COL_GOLD      RGB(232, 196, 107)
#define COL_DIV       RGB(48, 55, 72)

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

static HWND        g_hwnd = NULL;
static HFONT       g_fontUI = NULL, g_fontTitle = NULL, g_fontBtn = NULL;

static void WCpy(wchar_t *d, int n, const wchar_t *s) {
    wcsncpy(d, s, (size_t)(n - 1));
    d[n - 1] = L'\0';
}
static int         g_dpi = 96;
static int         g_testing = 0;
static int         g_testedCount = 0;
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

/* ---- DNS query probe: real UDP/53 round trip ---- */
static int BuildQuery(BYTE *buf, WORD id) {
    static const char *labels[3] = { "www", "google", "com" };
    int i = 12;
    buf[0] = (BYTE)(id >> 8); buf[1] = (BYTE)id;
    buf[2] = 0x01; buf[3] = 0x00;   /* RD */
    buf[4] = 0x00; buf[5] = 0x01;   /* 1 question */
    buf[6] = buf[7] = buf[8] = buf[9] = buf[10] = buf[11] = 0;
    for (int k = 0; k < 3; k++) {
        int len = (int)strlen(labels[k]);
        buf[i++] = (BYTE)len;
        memcpy(buf + i, labels[k], (size_t)len);
        i += len;
    }
    buf[i++] = 0;
    buf[i++] = 0x00; buf[i++] = 0x01;   /* A */
    buf[i++] = 0x00; buf[i++] = 0x01;   /* IN */
    return i;
}

static int DnsProbe(const char *ip, int family, int timeoutMs, int *outMs) {
    SOCKET s = socket(family == 6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return 0;
    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    BYTE q[64];
    WORD id = (WORD)((GetTickCount() ^ (size_t)ip) & 0xFFFF);
    int qlen = BuildQuery(q, id);
    int sent = 0;
    if (family == 6) {
        struct sockaddr_in6 a;
        ZeroMemory(&a, sizeof(a));
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(53);
        if (inet_pton(AF_INET6, ip, &a.sin6_addr) != 1) { closesocket(s); return 0; }
        sent = (sendto(s, (const char *)q, qlen, 0, (const struct sockaddr *)&a, sizeof(a)) == qlen);
    } else {
        struct sockaddr_in a;
        ZeroMemory(&a, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons(53);
        if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { closesocket(s); return 0; }
        sent = (sendto(s, (const char *)q, qlen, 0, (const struct sockaddr *)&a, sizeof(a)) == qlen);
    }
    if (!sent) { closesocket(s); return 0; }
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t0);
    char r[512];
    int n = recvfrom(s, r, sizeof(r), 0, NULL, NULL);
    QueryPerformanceCounter(&t1);
    closesocket(s);
    if (n < 12) return 0;
    if ((WORD)(((BYTE)r[0] << 8) | (BYTE)r[1]) != id) return 0;
    if (((BYTE)r[2] & 0x80) == 0) return 0;
    *outMs = (int)((t1.QuadPart - t0.QuadPart) * 1000 / fq.QuadPart);
    return 1;
}

static int DnsProbeTcp(const char *ip, int family, int timeoutMs, int *outMs) {
    SOCKET s = socket(family == 6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;
    int okconn = 0;
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    if (family == 6) {
        struct sockaddr_in6 a;
        ZeroMemory(&a, sizeof(a));
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(53);
        if (inet_pton(AF_INET6, ip, &a.sin6_addr) != 1) { closesocket(s); return 0; }
        okconn = (connect(s, (const struct sockaddr *)&a, sizeof(a)) == 0);
    } else {
        struct sockaddr_in a;
        ZeroMemory(&a, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons(53);
        if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { closesocket(s); return 0; }
        okconn = (connect(s, (const struct sockaddr *)&a, sizeof(a)) == 0);
    }
    if (!okconn) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) { closesocket(s); return 0; }
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(s, &wf);
        struct timeval tvt;
        tvt.tv_sec = timeoutMs / 1000;
        tvt.tv_usec = (timeoutMs % 1000) * 1000;
        if (select(0, NULL, &wf, NULL, &tvt) <= 0) { closesocket(s); return 0; }
        int soe = 0, sl = sizeof(soe);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soe, &sl);
        if (soe != 0) { closesocket(s); return 0; }
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
    BYTE q[64];
    WORD id = (WORD)((GetTickCount() ^ ((size_t)ip * 31)) & 0xFFFF);
    int qlen = BuildQuery(q, id);
    unsigned char lh[2];
    lh[0] = (unsigned char)(qlen >> 8);
    lh[1] = (unsigned char)qlen;
    if (send(s, (const char *)lh, 2, 0) != 2 || send(s, (const char *)q, qlen, 0) != qlen) {
        closesocket(s);
        return 0;
    }
    LARGE_INTEGER t0, t1, fq;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t0);
    if (recv(s, (char *)lh, 2, MSG_WAITALL) != 2) { closesocket(s); return 0; }
    int rlen = ((int)lh[0] << 8) | (int)lh[1];
    if (rlen < 12 || rlen > 512) { closesocket(s); return 0; }
    char r[512];
    if (recv(s, r, rlen, MSG_WAITALL) != rlen) { closesocket(s); return 0; }
    QueryPerformanceCounter(&t1);
    closesocket(s);
    if ((WORD)(((BYTE)r[0] << 8) | (BYTE)r[1]) != id) return 0;
    if (((BYTE)r[2] & 0x80) == 0) return 0;
    *outMs = (int)((t1.QuadPart - t0.QuadPart) * 1000 / fq.QuadPart);
    return 1;
}

static int Median3(int *a) {
    int x = a[0], y = a[1], z = a[2], t;
    if (x > y) { t = x; x = y; y = t; }
    if (y > z) { t = y; y = z; z = t; }
    if (x > y) { t = x; x = y; y = t; }
    return y;
}

static void BuildRows(void) {
    g_rowCount = 0;
    for (int i = 0; i < SERVER_COUNT; i++) {
        const DnsServer *s = &SERVERS[i];
        const char *ips4[2] = { s->v4a, s->v4b };
        const char *ips6[2] = { s->v6a, s->v6b };
        for (int b = 0; b < 2; b++) {
            if (ips4[b] && ips4[b][0]) {
                Row *r = &g_rows[g_rowCount++];
                ZeroMemory(r, sizeof(*r));
                r->srv = i; r->family = 4; r->ip = ips4[b]; r->isBackup = b;
            }
            if (ips6[b] && ips6[b][0]) {
                Row *r = &g_rows[g_rowCount++];
                ZeroMemory(r, sizeof(*r));
                r->srv = i; r->family = 6; r->ip = ips6[b]; r->isBackup = b;
            }
        }
    }
}

/* ---- benchmark run (background thread) ---- */
static DWORD WINAPI TestThread(LPVOID param) {
    (void)param;
    for (int i = 0; i < g_rowCount; i++) {
        Row *r = &g_rows[i];
        int got = 0, strikes = 0;
        r->tested = 1;
        r->ok = 0;
        for (int k = 0; k < 3; k++) {
            int ms = 0;
            if (DnsProbe(r->ip, r->family, 900, &ms)) {
                r->ms[got++] = ms;
                if (got == 3) break;
            } else {
                if (++strikes >= 2) break;
            }
        }
        if (got == 0) {
            /* UDP path failed entirely - many networks filter outbound UDP/53
               but still pass TCP/53, so try that before calling it dead. */
            for (int k = 0; k < 2; k++) {
                int ms = 0;
                if (DnsProbeTcp(r->ip, r->family, 1500, &ms)) {
                    r->ms[got++] = ms;
                    r->usedTcp = 1;
                }
            }
        }
        if (got > 0) {
            r->ok = 1;
            if (got == 3) r->med = Median3(r->ms);
            else {
                int sum = 0;
                for (int k = 0; k < got; k++) sum += r->ms[k];
                r->med = sum / got;
                for (int k = got; k < 3; k++) r->ms[k] = -1;
            }
        } else {
            r->med = -1;
            for (int k = 0; k < 3; k++) r->ms[k] = -1;
        }
        g_testedCount++;
        if ((i & 3) == 3 || i == g_rowCount - 1) InvalidateRect(g_hwnd, NULL, FALSE);
    }
    /* rank within family */
    g_best4 = g_best6 = -1;
    for (int fam = 4; fam <= 6; fam += 2) {
        for (int pass = 0; pass < g_rowCount; pass++) {
            int pick = -1;
            for (int i = 0; i < g_rowCount; i++) {
                Row *r = &g_rows[i];
                if (r->family != fam || !r->ok || r->rank != 0) continue;
                if (pick < 0 || r->med < g_rows[pick].med) pick = i;
            }
            if (pick < 0) break;
            g_rows[pick].rank = pass + 1;
            if (pass == 0) {
                if (fam == 4) g_best4 = pick; else g_best6 = pick;
            }
        }
    }
    g_testing = 0;
    g_lastTestAt = GetTickCount64();
    PostMessageW(g_hwnd, WM_APP_DONE, 0, 0);
    return 0;
}

static void StartTest(int autoRun) {
    if (g_testing) return;
    g_testing = 1;
    g_testedCount = 0;
    g_autoRun = autoRun;
    for (int i = 0; i < g_rowCount; i++) g_rows[i].rank = 0;
    CreateThread(NULL, 0, TestThread, NULL, 0, NULL);
}

/* ---- active adapter discovery (iphlpapi; locale-proof) ---- */
static void FindActiveAdapter(void) {
    ULONG sz = 16 * 1024;
#ifndef GAA_FLAG_INCLUDE_GATEWAY
#define GAA_FLAG_INCLUDE_GATEWAY 0x0080
#endif
    IP_ADAPTER_ADDRESSES *aa = NULL;
    for (int attempt = 0; attempt < 3; attempt++) {
        aa = (IP_ADAPTER_ADDRESSES *)malloc(sz);
        if (!aa) return;
        ULONG rc = GetAdaptersAddresses(AF_UNSPEC,
                                        GAA_FLAG_INCLUDE_GATEWAY | GAA_FLAG_SKIP_ANYCAST |
                                        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                        NULL, aa, &sz);
        if (rc == ERROR_SUCCESS) break;
        free(aa); aa = NULL;
        if (rc != ERROR_BUFFER_OVERFLOW) return;
    }
    if (!aa) return;
    for (IP_ADAPTER_ADDRESSES *p = aa; p; p = p->Next) {
        if (p->OperStatus != IfOperStatusUp) continue;
        if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (!p->FirstGatewayAddress) continue;
        g_ifIndex = p->IfIndex ? p->IfIndex : p->Ipv6IfIndex;
        if (p->FriendlyName) wcsncpy(g_adName, p->FriendlyName, 127);
        g_adName[127] = L'\0';
        break;
    }
    if (!g_ifIndex) {
        for (IP_ADAPTER_ADDRESSES *p = aa; p; p = p->Next) {
            if (p->OperStatus != IfOperStatusUp) continue;
            if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (!p->FirstUnicastAddress) continue;
            g_ifIndex = p->IfIndex ? p->IfIndex : p->Ipv6IfIndex;
            if (p->FriendlyName) wcsncpy(g_adName, p->FriendlyName, 127);
            g_adName[127] = L'\0';
            break;
        }
    }
    free(aa);
}

/* ---- applying DNS (netsh with numeric ifIndex; needs elevation) ---- */
static int RunElevatedCmd(const wchar_t *cmd) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t buf[512];
    wcsncpy(buf, cmd, 511);
    buf[511] = L'\0';
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return 0;
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

static void ApplyDns(int ifIndex, const char *v4a, const char *v4b, const char *v6a, const char *v6b) {
    wchar_t cmd[512], w1[128], w2[128];
    if (ifIndex <= 0) return;
    if (v4a && v4a[0]) {
        Widen(v4a, w1, 128);
        _snwprintf(cmd, 511, L"netsh interface ipv4 set dnsservers %d static %s primary", ifIndex, w1);
        RunElevatedCmd(cmd);
        if (v4b && v4b[0]) {
            Widen(v4b, w2, 128);
            _snwprintf(cmd, 511, L"netsh interface ipv4 add dnsservers %d %s index=2", ifIndex, w2);
            RunElevatedCmd(cmd);
        }
    }
    if (v6a && v6a[0]) {
        Widen(v6a, w1, 128);
        _snwprintf(cmd, 511, L"netsh interface ipv6 set dnsservers %d static %s primary", ifIndex, w1);
        RunElevatedCmd(cmd);
        if (v6b && v6b[0]) {
            Widen(v6b, w2, 128);
            _snwprintf(cmd, 511, L"netsh interface ipv6 add dnsservers %d %s index=2", ifIndex, w2);
            RunElevatedCmd(cmd);
        }
    }
    RunElevatedCmd(L"ipconfig /flushdns");
    if (v4a && v4a[0]) { strncpy(g_curV4, v4a, 63); g_curV4[63] = '\0'; }
    if (v6a && v6a[0]) { strncpy(g_curV6, v6a, 63); g_curV6[63] = '\0'; }
    strncpy(g_lastV4, g_curV4, 63);
    strncpy(g_lastV6, g_curV6, 63);
}

/* ---- elevation ---- */
static int IsElevated(void) {
    HANDLE tok = NULL;
    TOKEN_ELEVATION te;
    DWORD sz = 0;
    int elev = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz))
            elev = te.TokenIsElevated ? 1 : 0;
        CloseHandle(tok);
    }
    return elev;
}

static void RelaunchElevated(const wchar_t *args) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    ShellExecuteW(NULL, L"runas", path, args, NULL, SW_SHOWNORMAL);
}

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

/* ---- Auto live-watch: probe the active DNS + its nearest rivals on a fast
   tick and switch the moment one is CLEARLY and CONSISTENTLY faster. The
   safety rules below exist so switching can never thrash the connection:
     * margin   - the rival must be 20% faster AND at least 20 ms faster,
                  so two near-equal servers never trade places
     * streak   - it must win 3 probe rounds in a row (a real trend, not a
                  one-off blip)
     * cooldown - a 60 s settle gap after every switch
     * verify   - if the new server stops answering within 2 minutes, the
                  previous one is restored automatically
   Switching mid-game is fine by design: a DNS change never touches
   established connections, so downloads, calls and games keep running on
   their current sockets while only new lookups use the new server. */
typedef struct WatchCand {
    char   ip[64];
    int    family;
    int    srv;        /* index into SERVERS, or -1 for the current active */
    int    isPrimary;  /* the currently-active primary of its family */
    double ewma;       /* smoothed latency in ms, -1 = no data yet */
    int    misses;     /* consecutive failed probes */
    int    streak;     /* consecutive rounds beating the active primary */
} WatchCand;

static WatchCand  g_watch[12];
static int        g_watchN = 0;
static volatile LONG g_watchBusy = 0;
static CRITICAL_SECTION g_watchCs;
static ULONGLONG  g_lastSwitchAt = 0;
static ULONGLONG  g_switchedAt = 0;
static int        g_switchFam = 0;
static char       g_prevV4[64] = "", g_prevV6[64] = "";

#define WATCH_MIN_GAIN_MS  20.0
#define WATCH_WIN_RATIO    0.80
#define WATCH_STREAK_NEED  3
#define WATCH_COOLDOWN_MS  (60ULL * 1000ULL)
#define WATCH_VERIFY_MS    (2ULL * 60000ULL)
#define AUTO_FULLTEST_MS   (30ULL * 60000ULL)

static void AddWatch(const char *ip, int family, int srv) {
    if (!ip || !ip[0] || g_watchN >= 12) return;
    for (int i = 0; i < g_watchN; i++)
        if (g_watch[i].family == family && strcmp(g_watch[i].ip, ip) == 0) return;
    WatchCand *c = &g_watch[g_watchN++];
    ZeroMemory(c, sizeof(*c));
    strncpy(c->ip, ip, 63);
    c->family = family;
    c->srv = srv;
    c->ewma = -1;
}

static void RebuildWatchBody(void) {
    g_watchN = 0;
    if (g_curV4[0]) AddWatch(g_curV4, 4, -1);
    if (g_curV6[0]) AddWatch(g_curV6, 6, -1);
    for (int i = 0; i < g_watchN; i++)
        if ((g_watch[i].family == 4 && g_curV4[0] && strcmp(g_watch[i].ip, g_curV4) == 0) ||
            (g_watch[i].family == 6 && g_curV6[0] && strcmp(g_watch[i].ip, g_curV6) == 0))
            g_watch[i].isPrimary = 1;
    for (int fam = 4; fam <= 6; fam += 2) {
        int want = (fam == 4) ? 4 : 2, added = 0;
        for (int rk = 1; added < want && rk <= g_rowCount; rk++) {
            for (int i = 0; i < g_rowCount && added < want; i++) {
                Row *r = &g_rows[i];
                if (r->family == fam && r->ok && !r->isBackup && r->rank == rk) {
                    int before = g_watchN;
                    AddWatch(r->ip, fam, r->srv);
                    if (g_watchN > before) added++;
                }
            }
        }
    }
}

static void RebuildWatch(void) {
    EnterCriticalSection(&g_watchCs);
    RebuildWatchBody();
    LeaveCriticalSection(&g_watchCs);
}

static void WatchSwitch(int fam, const char *ip, int srv, double chalMs, double actMs) {
    const DnsServer *sv = (srv >= 0 && srv < SERVER_COUNT) ? &SERVERS[srv] : NULL;
    const char *v4a = "", *v4b = "", *v6a = "", *v6b = "";
    const char *sec = "";
    if (sv) {
        if (fam == 4) {
            if (sv->v4a[0] && strcmp(sv->v4a, ip) != 0) sec = sv->v4a;
            else if (sv->v4b[0] && strcmp(sv->v4b, ip) != 0) sec = sv->v4b;
        } else {
            if (sv->v6a[0] && strcmp(sv->v6a, ip) != 0) sec = sv->v6a;
            else if (sv->v6b[0] && strcmp(sv->v6b, ip) != 0) sec = sv->v6b;
        }
    }
    if (fam == 4) {
        strncpy(g_prevV4, g_curV4, 63); g_prevV4[63] = '\0';
        v4a = ip; v4b = sec;
    } else {
        strncpy(g_prevV6, g_curV6, 63); g_prevV6[63] = '\0';
        v6a = ip; v6b = sec;
    }
    ApplyDns((int)g_ifIndex, v4a, v4b, v6a, v6b);
    SaveConfig();
    g_lastSwitchAt = GetTickCount64();
    g_switchedAt = g_lastSwitchAt;
    g_switchFam = fam;
    for (int i = 0; i < g_watchN; i++) g_watch[i].streak = 0;
    {
        wchar_t wip[64], wname[64];
        Widen(ip, wip, 64);
        Widen(sv ? sv->name : "rival", wname, 64);
        _snwprintf(g_status, 511,
                   L"Auto-Pilot switched IPv%d to %s %s (%.0f ms vs %.0f ms) - existing connections are unaffected.",
                   fam, wname, wip, chalMs, actMs);
        g_status[511] = L'\0';
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void WatchTickBody(void) {
    if (g_watchN == 0) { RebuildWatch(); if (g_watchN == 0) return; }
    ULONGLONG now = GetTickCount64();

    for (int i = 0; i < g_watchN; i++) {
        WatchCand *c = &g_watch[i];
        int ms = 0;
        if (DnsProbe(c->ip, c->family, 900, &ms)) {
            c->ewma = (c->ewma < 0) ? (double)ms : (c->ewma * 0.6 + (double)ms * 0.4);
            c->misses = 0;
        } else {
            c->misses++;
            if (c->ewma >= 0) c->ewma = c->ewma * 0.8 + 450.0 * 0.2;
        }
    }

    for (int fam = 4; fam <= 6; fam += 2) {
        WatchCand *act = NULL, *best = NULL;
        for (int i = 0; i < g_watchN; i++) {
            WatchCand *c = &g_watch[i];
            if (c->family != fam) continue;
            if (c->isPrimary) act = c;
            else if (c->ewma >= 0 && (!best || c->ewma < best->ewma)) best = c;
        }
        if (!act || !best) continue;
        int wins = (act->ewma >= 0 &&
                    (act->ewma - best->ewma) >= WATCH_MIN_GAIN_MS &&
                    best->ewma <= act->ewma * WATCH_WIN_RATIO);
        for (int i = 0; i < g_watchN; i++) {
            WatchCand *c = &g_watch[i];
            if (c->family != fam || c->isPrimary) continue;
            c->streak = (wins && c == best) ? c->streak + 1 : 0;
        }
        if (wins && best->streak >= WATCH_STREAK_NEED &&
            now - g_lastSwitchAt >= WATCH_COOLDOWN_MS) {
            WatchSwitch(fam, best->ip, best->srv, best->ewma, act->ewma);
            return;
        }
    }

    /* post-switch health check: revert if the new primary dies right away */
    if (g_switchFam && now - g_switchedAt < WATCH_VERIFY_MS) {
        for (int i = 0; i < g_watchN; i++) {
            WatchCand *c = &g_watch[i];
            if (c->family != g_switchFam || !c->isPrimary) continue;
            if (c->misses >= 3) {
                const char *prev = (g_switchFam == 4) ? g_prevV4 : g_prevV6;
                if (prev[0]) {
                    if (g_switchFam == 4) ApplyDns((int)g_ifIndex, prev, "", "", "");
                    else ApplyDns((int)g_ifIndex, "", "", prev, "");
                    SaveConfig();
                    wcsncpy(g_status, L"Auto-Pilot reverted - the new server failed its health check.", 511);
                    g_status[511] = L'\0';
                    g_switchFam = 0;
                    g_lastSwitchAt = GetTickCount64();
                    RebuildWatch();
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
                return;
            }
        }
    }
}

static void WatchTick(void) {
    EnterCriticalSection(&g_watchCs);
    WatchTickBody();
    LeaveCriticalSection(&g_watchCs);
}

static DWORD WINAPI WatchThread(LPVOID param) {
    (void)param;
    WatchTick();
    InterlockedExchange(&g_watchBusy, 0);
    return 0;
}

/* ---- UI ---- */
enum { BTN_TEST = 0, BTN_APPLY, BTN_AUTO, BTN_INTERVAL, BTN_ADMIN, BTN_COUNT };

/* Spinning Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer,
   identical to Project Hub (ported 1:1 from tools/generate_suite_icons.py).
   Drawn straight at cache resolution so it is razor sharp at any DPI. */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

static RECT g_rcEmblem;

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
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

typedef struct EmbCache {
    int      sz;
    int      srcSz;
    uint32_t *src;
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
    /* One full turn every 30 seconds: slow enough to read as alive. */
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

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

/* GDI blit wrapper: rasterize the emblem over the background colour and
   hand the finished square to SetDIBitsToDevice. */
static void PaintEmblem(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    for (int i = 0; i < sz * sz; i++) px[i] = (uint32_t)COL_BG;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;   /* top-down rows */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

static void BtnRect(int i, RECT *r) {
    int x = S(20), y = S(78), h = S(36);
    int w[BTN_COUNT] = { S(140), S(150), S(140), S(150), S(170) };
    for (int k = 0; k < i; k++) x += w[k] + S(10);
    SetRect(r, x, y, x + w[i], y + h);
}

static void FillSolid(HDC dc, const RECT *r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary, int amber) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : (amber ? RGB(84, 70, 38) : COL_CARD);
    COLORREF edge = primary ? COL_ACCENT : (amber ? COL_WARN : COL_DIV);
    if (g_hoverBtn == idx) edge = COL_ACCENT;
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, S(10), S(10));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, amber ? COL_WARN : COL_TXT);
    SelectObject(dc, g_fontBtn);
    DrawTextW(dc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void RowRect(int vis, RECT *r) {
    SetRect(r, S(20), S(146) + vis * S(34), S(960), S(146) + (vis + 1) * S(34));
}

static void PaintAll(HWND hwnd) {
    (void)hwnd;
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    RECT all = rc;
    FillSolid(mem, &all, COL_BG);

    SetBkMode(mem, TRANSPARENT);

    /* Spinning Tri-Spoke Core emblem - identical renderer to Project Hub */
    int embSz = S(52);
    SetRect(&g_rcEmblem, S(28), S(12), S(28) + embSz, S(12) + embSz);
    PaintEmblem(mem, g_rcEmblem.left, g_rcEmblem.top, embSz);

    SetTextColor(mem, COL_TXT);
    SelectObject(mem, g_fontTitle);
    RECT tr;
    SetRect(&tr, S(94), S(12), S(600), S(42));
    DrawTextW(mem, L"DNS Pilot", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTE);
    wchar_t sub[256];
    _snwprintf(sub, 255, L"Benchmarks real DNS queries and routes Windows through the fastest  |  %s  |  ifIndex %d  |  %s",
               g_adName, g_ifIndex, g_elevated ? L"Administrator" : L"not elevated");
    sub[255] = L'\0';
    SetRect(&tr, S(94), S(42), W - S(20), S(64));
    DrawTextW(mem, sub, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    wchar_t autoLabel[48];
    wchar_t intLabel[48];
    _snwprintf(autoLabel, 47, L"Auto-Pilot: %s", g_auto ? L"ON" : L"OFF");
    autoLabel[47] = L'\0';
    if (g_intervalMin == 0) WCpy(intLabel, 48, L"Interval: Auto");
    else _snwprintf(intLabel, 47, L"Every %d min", g_intervalMin);
    intLabel[47] = L'\0';
    DrawBtn(mem, BTN_TEST, L"Test & Rank", 1, 0);
    DrawBtn(mem, BTN_APPLY, L"Apply Fastest", 0, 0);
    DrawBtn(mem, BTN_AUTO, autoLabel, 0, g_auto);
    DrawBtn(mem, BTN_INTERVAL, intLabel, 0, 0);
    DrawBtn(mem, BTN_ADMIN, g_elevated ? L"Running as Admin" : L"Restart as Admin", 0, 0);

    /* table header - columns use the full window width and never overlap */
    int cwid[6] = { S(36), S(190), S(240), S(62), S(112), S(236) };
    int cx[6] = { S(32), S(72), S(272), S(522), S(592), S(712) };
    SetTextColor(mem, COL_MUTE);
    SelectObject(mem, g_fontUI);
    {
        const wchar_t *heads[6] = { L"#", L"Resolver", L"Server IP", L"Family", L"Median", L"Samples" };
        for (int i = 0; i < 6; i++) {
            RECT hr;
            SetRect(&hr, cx[i], S(122), cx[i] + cwid[i], S(146));
            DrawTextW(mem, heads[i], -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    int tableH = H - S(146) - S(52);
    int visRows = tableH / S(34);
    if (visRows < 1) visRows = 1;
    if (g_topRow > g_rowCount - visRows) g_topRow = g_rowCount - visRows;
    if (g_topRow < 0) g_topRow = 0;

    for (int v = 0; v < visRows; v++) {
        int idx = g_topRow + v;
        if (idx >= g_rowCount) break;
        Row *r = &g_rows[idx];
        RECT rr;
        RowRect(v, &rr);
        FillSolid(mem, &rr, (idx & 1) ? COL_ROW2 : COL_CARD);
        if (r->rank == 1) {
            RECT hl = rr;
            HPEN hp = CreatePen(PS_SOLID, 1, COL_GOLD);
            HGDIOBJ op = SelectObject(mem, hp);
            HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
            RoundRect(mem, rr.left + 1, rr.top + 1, rr.right - 1, rr.bottom - 1, S(8), S(8));
            SelectObject(mem, op);
            SelectObject(mem, ob);
            DeleteObject(hp);
            (void)hl;
        }
        wchar_t wname[64], wip[64], wfam[8], wmed[32], wsmp[48];
        Widen(SERVERS[r->srv].name, wname, 64);
        Widen(r->ip, wip, 64);
        _snwprintf(wfam, 7, L"IPv%d", r->family);
        wfam[7] = L'\0';
        if (!r->tested) {
            WCpy(wmed, 32, L"-");
            WCpy(wsmp, 48, L"not tested");
        } else if (!r->ok) {
            WCpy(wmed, 32, L"-");
            WCpy(wsmp, 48, L"timeout / no route");
        } else {
            if (r->usedTcp) _snwprintf(wmed, 31, L"%d ms (TCP)", r->med);
            else _snwprintf(wmed, 31, L"%d ms", r->med);
            wmed[31] = L'\0';
            wchar_t s1[8], s2[8], s3[8];
            if (r->ms[0] >= 0) _snwprintf(s1, 7, L"%d", r->ms[0]); else wcscpy_s(s1, 8, L"-");
            if (r->ms[1] >= 0) _snwprintf(s2, 7, L"%d", r->ms[1]); else wcscpy_s(s2, 8, L"-");
            if (r->ms[2] >= 0) _snwprintf(s3, 7, L"%d", r->ms[2]); else wcscpy_s(s3, 8, L"-");
            _snwprintf(wsmp, 47, L"%s / %s / %s ms", s1, s2, s3);
            wsmp[47] = L'\0';
        }
        wchar_t *cols[6];
        wchar_t wrank[8];
        if (r->tested && r->ok && r->rank > 0) _snwprintf(wrank, 7, L"%d", r->rank);
        else WCpy(wrank, 8, L"");
        cols[0] = wrank; cols[1] = wname; cols[2] = wip; cols[3] = wfam; cols[4] = wmed; cols[5] = wsmp;
        for (int c = 0; c < 6; c++) {
            RECT cr;
            SetRect(&cr, cx[c], rr.top, cx[c] + cwid[c], rr.bottom);
            if (c == 0 && r->rank == 1) SetTextColor(mem, COL_GOLD);
            else if (c == 4 && r->tested && r->ok) SetTextColor(mem, r->rank == 1 ? COL_GOLD : COL_TXT);
            else if (c == 5 && r->tested && !r->ok) SetTextColor(mem, COL_BAD);
            else SetTextColor(mem, c == 1 ? COL_TXT : COL_MUTE);
            if (c == 1 && r->isBackup) {
                wchar_t wnb[80];
                _snwprintf(wnb, 79, L"%s (backup)", wname);
                wnb[79] = L'\0';
                DrawTextW(mem, wnb, -1, &cr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else {
                DrawTextW(mem, cols[c], -1, &cr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
    }

    /* footer */
    RECT fr;
    SetRect(&fr, 0, H - S(52), W, H);
    FillSolid(mem, &fr, COL_CARD);
    SetRect(&fr, S(20), H - S(52), W - S(20), H - S(10));
    SetTextColor(mem, COL_TXT);
    DrawTextW(mem, g_status, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static int HitBtn(int x, int y) {
    POINT pt;
    RECT r;
    pt.x = x; pt.y = y;
    for (int i = 0; i < BTN_COUNT; i++) {
        BtnRect(i, &r);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void UpdateStatusAfterTest(void) {
    if (g_best4 >= 0 && g_best6 >= 0) {
        wchar_t n4[64], n6[64], ip4[64], ip6[64];
        Widen(SERVERS[g_rows[g_best4].srv].name, n4, 64);
        Widen(SERVERS[g_rows[g_best6].srv].name, n6, 64);
        Widen(g_rows[g_best4].ip, ip4, 64);
        Widen(g_rows[g_best6].ip, ip6, 64);
        _snwprintf(g_status, 511, L"Fastest IPv4: %s %s (%d ms)  |  Fastest IPv6: %s %s (%d ms)  |  press Apply Fastest",
                   n4, ip4, g_rows[g_best4].med, n6, ip6, g_rows[g_best6].med);
    } else if (g_best4 >= 0) {
        wchar_t n4[64], ip4[64];
        Widen(SERVERS[g_rows[g_best4].srv].name, n4, 64);
        Widen(g_rows[g_best4].ip, ip4, 64);
        _snwprintf(g_status, 511, L"Fastest IPv4: %s %s (%d ms)  |  no working IPv6 resolver  |  press Apply Fastest",
                   n4, ip4, g_rows[g_best4].med);
    } else {
        wcsncpy(g_status, L"No resolver answered over UDP or TCP port 53 - this network may block outside DNS.", 511);
    }
    {
        int reach = 0;
        wchar_t tail[80];
        for (int i = 0; i < g_rowCount; i++) if (g_rows[i].tested && g_rows[i].ok) reach++;
        _snwprintf(tail, 79, L"  |  %d/%d endpoints reachable", reach, g_rowCount);
        tail[79] = L'\0';
        wcsncat(g_status, tail, 511 - wcslen(g_status));
    }
    g_status[511] = L'\0';
}

static void DoApplyFastest(int announce) {
    if (!g_elevated) {
        wchar_t args[384], wv[5][128];
        const char *pick[5];
        if (g_best4 < 0 && g_best6 < 0) {
            wcsncpy(g_status, L"Run Test & Rank first - nothing to apply.", 511);
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
        pick[0] = g_best4 >= 0 ? g_rows[g_best4].ip : "-";
        pick[1] = (g_best4 >= 0 && SERVERS[g_rows[g_best4].srv].v4b[0]) ? SERVERS[g_rows[g_best4].srv].v4b : "-";
        pick[2] = g_best6 >= 0 ? g_rows[g_best6].ip : "-";
        pick[3] = (g_best6 >= 0 && SERVERS[g_rows[g_best6].srv].v6b[0]) ? SERVERS[g_rows[g_best6].srv].v6b : "-";
        for (int i = 0; i < 4; i++) Widen(pick[i], wv[i], 128);
        _snwprintf(args, 383, L"-ApplyNow %d %s %s %s %s", g_ifIndex, wv[0], wv[1], wv[2], wv[3]);
        args[383] = L'\0';
        RelaunchElevated(args);
        wcsncpy(g_status, L"Approving via the UAC prompt - DNS changes need administrator rights.", 511);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    const char *v4a = g_best4 >= 0 ? g_rows[g_best4].ip : "";
    const char *v4b = (g_best4 >= 0 && SERVERS[g_rows[g_best4].srv].v4b[0]) ? SERVERS[g_rows[g_best4].srv].v4b : "";
    const char *v6a = g_best6 >= 0 ? g_rows[g_best6].ip : "";
    const char *v6b = (g_best6 >= 0 && SERVERS[g_rows[g_best6].srv].v6b[0]) ? SERVERS[g_rows[g_best6].srv].v6b : "";
    ApplyDns((int)g_ifIndex, v4a, v4b, v6a, v6b);
    SaveConfig();
    if (announce) {
        wchar_t n4[64];
        Widen(g_best4 >= 0 ? SERVERS[g_rows[g_best4].srv].name : "", n4, 64);
        _snwprintf(g_status, 511, L"Applied: IPv4 via %s  |  IPv6 via %s  |  DNS cache flushed",
                   g_best4 >= 0 ? n4 : L"(unchanged)",
                   g_best6 >= 0 ? L"tested winner" : L"(unchanged)");
        g_status[511] = L'\0';
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        PaintAll(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b != g_hoverBtn) {
            g_hoverBtn = b;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b == BTN_TEST) {
            StartTest(0);
            wcsncpy(g_status, L"Testing - real DNS queries to every listed resolver...", 511);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_APPLY) {
            DoApplyFastest(1);
        } else if (b == BTN_AUTO) {
            g_auto = !g_auto;
            SaveConfig();
            if (g_auto && !g_elevated) {
                wcsncpy(g_status, L"Auto-Pilot needs administrator rights - click Restart as Admin.", 511);
            } else if (g_auto && g_intervalMin == 0) {
                if (g_lastTestAt == 0 && !g_testing) StartTest(1);
                else RebuildWatch();
                WCpy(g_status, 512,
                     L"Auto-Pilot enabled (live watch) - it switches the instant a rival is clearly "
                     L"and consistently faster. Switching mid-game is safe - existing "
                     L"connections are never touched.");
            } else {
                _snwprintf(g_status, 511, L"Auto-Pilot %s - re-tests every %d min and only switches when clearly faster.",
                           g_auto ? L"enabled" : L"disabled", g_intervalMin);
            }
            g_status[511] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_INTERVAL) {
            int steps[6] = { 0, 15, 30, 60, 120, 300 };
            int cur = 2;
            for (int i = 0; i < 6; i++) if (steps[i] == g_intervalMin) cur = i;
            g_intervalMin = steps[(cur + 1) % 6];
            SaveConfig();
            if (g_intervalMin == 0) {
                WCpy(g_status, 512,
                     L"Auto mode: live watch - switches only when a rival is 20%/20 ms faster, "
                     L"3 rounds in a row, 60 s settle gap, auto-revert if it dies.");
                RebuildWatch();
            } else {
                _snwprintf(g_status, 511, L"Auto-Pilot interval set to %d minutes.", g_intervalMin);
            }
            g_status[511] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_ADMIN) {
            if (!g_elevated) {
                RelaunchElevated(L"-TakeOver");
                DestroyWindow(hwnd);   /* the elevated copy takes over */
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        g_topRow -= (delta > 0) ? 3 : -3;
        if (g_topRow < 0) g_topRow = 0;
        if (g_topRow > g_rowCount - 1) g_topRow = g_rowCount - 1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_APP_DONE:
        UpdateStatusAfterTest();
        if (g_autoRun && g_intervalMin == 0) {
            RebuildWatch();
        } else if (g_autoRun) {
            int better = 0;
            if (g_best4 >= 0) {
                Row *b = &g_rows[g_best4];
                if (g_curV4[0] && strcmp(b->ip, g_curV4) != 0) {
                    better = 1;   /* unknown current latency: only switch if the current is a stale pick */
                } else if (!g_curV4[0]) {
                    better = 1;
                }
            }
            if (better) DoApplyFastest(0);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_TIMER:
        if (wp == 2) {
            InvalidateRect(hwnd, &g_rcEmblem, FALSE);
            return 0;
        }
        if (wp == 1 && g_auto && g_elevated && !g_testing) {
            ULONGLONG now = GetTickCount64();
            if (g_intervalMin == 0) {
                /* live watch on a worker thread: cheap probes every tick and a
                   full re-rank twice an hour - probe timeouts can never stall
                   the UI or the switch path */
                if (InterlockedCompareExchange(&g_watchBusy, 0, 0) == 0) {
                    InterlockedExchange(&g_watchBusy, 1);
                    CreateThread(NULL, 0, WatchThread, NULL, 0, NULL);
                }
                if (g_lastTestAt == 0 || now - g_lastTestAt >= AUTO_FULLTEST_MS) StartTest(1);
            } else {
                if (g_lastTestAt == 0 || now - g_lastTestAt >= (ULONGLONG)g_intervalMin * 60000ULL)
                    StartTest(1);
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        DeleteCriticalSection(&g_watchCs);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev;
    (void)nShow;
    WSADATA wsa;
    int skipSingle = (cmdLine && (wcsstr(cmdLine, L"-ApplyNow") != NULL ||
                                  wcsstr(cmdLine, L"-TakeOver") != NULL));
    if (!skipSingle) {
        HANDLE hMu = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND other = FindWindowW(L"KBLSuiteDnsPilotWindow", NULL);
            if (other) {
                ShowWindow(other, SW_RESTORE);
                SetForegroundWindow(other);
            }
            if (hMu) CloseHandle(hMu);
            return 0;
        }
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxW(NULL, L"Winsock failed to start.", APP_TITLE_W, MB_ICONERROR);
        return 1;
    }

    g_elevated = IsElevated();
    LoadConfig();
    BuildRows();
    FindActiveAdapter();

    /* Elevated one-shot apply from an unelevated session's Apply button. */
    if (cmdLine && wcsstr(cmdLine, L"-ApplyNow") != NULL) {
        int idx = 0;
        wchar_t args[5][128];
        int argc = 0;
        {
            wchar_t *p = cmdLine;
            while (*p && argc < 5) {
                while (*p == L' ') p++;
                if (!*p) break;
                wchar_t *q = p;
                while (*q && *q != L' ') q++;
                {
                    wchar_t save = *q;
                    *q = L'\0';
                    if (wcscmp(p, L"-ApplyNow") != 0) {
                        wcsncpy(args[argc], p, 127);
                        args[argc][127] = L'\0';
                        argc++;
                    }
                    *q = save;
                }
                p = q;
            }
        }
        if (argc >= 1) idx = _wtoi(args[0]);
        {
            char a[5][128];
            (void)argc;
            for (int i = 0; i < 5; i++) {
                if (i < argc && wcscmp(args[i], L"-") != 0)
                    WideCharToMultiByte(CP_UTF8, 0, args[i], -1, a[i], 128, NULL, NULL);
                else
                    a[i][0] = '\0';
            }
            ApplyDns(idx, a[1], a[2], a[3], a[4]);
        }
        return 0;
    }

    g_dpi = GetDeviceCaps(GetDC(NULL), LOGPIXELSX);
    {
        HDC sdc = GetDC(NULL);
        g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
        ReleaseDC(NULL, sdc);
    }
    g_fontUI = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(19), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontBtn = CreateFontW(-S(12), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"KBLSuiteDnsPilotWindow";
    RegisterClassExW(&wc);

    RECT wrc = { 0, 0, S(980), S(760) };
    AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE_W,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             wrc.right - wrc.left, wrc.bottom - wrc.top,
                             NULL, NULL, hInst, NULL);

    /* dark title bar matching the background + smooth corners (dwmapi is
       loaded dynamically so the PE imports stay clean) */
    ApplyModernWindowFrame(g_hwnd);

    ShowWindow(g_hwnd, SW_SHOW);
    InitializeCriticalSection(&g_watchCs);
    SetTimer(g_hwnd, 1, 10000, NULL);
    SetTimer(g_hwnd, 2, 50, NULL);     /* emblem spin repaint */

    /* opening the pilot always kicks off a real test right away */
    StartTest(0);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    WSACleanup();
    return 0;
}
