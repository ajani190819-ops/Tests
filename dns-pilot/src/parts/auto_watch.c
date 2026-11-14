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
    if (InterlockedCompareExchange(&g_exitStop, 0, 0)) return;
    if (g_watchN == 0) { RebuildWatch(); if (g_watchN == 0) return; }
    ULONGLONG now = GetTickCount64();

    for (int i = 0; i < g_watchN; i++) {
        WatchCand *c = &g_watch[i];
        int ms = 0;
        if (InterlockedCompareExchange(&g_exitStop, 0, 0)) return;
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

