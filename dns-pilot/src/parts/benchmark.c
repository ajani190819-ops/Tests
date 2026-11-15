/* ---- benchmark run (background thread) ---- */
static void ProbeRow(Row *r) {
    int got = 0, strikes = 0;
    r->tested = 1;
    r->ok = 0;
    /* No global IPv6 on this network: probing every v6 resolver would only
       burn timeout budget and show a wall of mystery rows. Leave them
       'not tested' and let the status line say why. */
    if (r->family == 6 && !g_hasGlobalV6) {
        r->tested = 0;
        InterlockedIncrement((volatile LONG *)&g_testedCount);
        if ((g_testedCount & 3) == 3 || g_testedCount >= g_rowCount)
            InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    for (int k = 0; k < 3; k++) {
        if (InterlockedCompareExchange(&g_exitStop, 0, 0)) break;
        int ms = 0;
        /* First sample keeps the full budget so live resolvers rank on a
           fair 900 ms window; follow-ups shrink so dead endpoints (common
           on networks that block outside DNS) cost far less wall time. */
        int tmo = (k == 0) ? 900 : 500;
        if (DnsProbe(r->ip, r->family, tmo, &ms)) {
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
            if (InterlockedCompareExchange(&g_exitStop, 0, 0)) break;
            int ms = 0;
            if (DnsProbeTcp(r->ip, r->family, 700, &ms)) {
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
    InterlockedIncrement((volatile LONG *)&g_testedCount);
    if ((g_testedCount & 3) == 3 || g_testedCount >= g_rowCount)
        InvalidateRect(g_hwnd, NULL, FALSE);
}

/* Rows are probed by a small pool of workers that claim the next index
   atomically. 28 endpoints x dead-timeouts used to run serially and take
   minutes on networks that block outside DNS; in parallel the whole test
   finishes in a few seconds and live resolvers rank exactly the same. */
static DWORD WINAPI TestWorker(LPVOID param) {
    (void)param;
    for (;;) {
        if (InterlockedCompareExchange(&g_exitStop, 0, 0)) break;
        int i = (int)InterlockedIncrement(&g_testNext) - 1;
        if (i >= g_rowCount) break;
        ProbeRow(&g_rows[i]);
    }
    return 0;
}

static DWORD WINAPI TestThread(LPVOID param) {
    (void)param;
    InterlockedExchange(&g_testNext, 0);
    int workers = (g_rowCount < 8) ? g_rowCount : 8;
    HANDLE hs[8];
    int nH = 0;
    for (int i = 0; i < workers; i++) {
        HANDLE h = CreateThread(NULL, 0, TestWorker, NULL, 0, NULL);
        if (h) hs[nH++] = h;
    }
    for (int i = 0; i < nH; i++) {
        WaitForSingleObject(hs[i], INFINITE);
        CloseHandle(hs[i]);
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
    HANDLE hT = CreateThread(NULL, 0, TestThread, NULL, 0, NULL);
    if (hT) CloseHandle(hT);
}

