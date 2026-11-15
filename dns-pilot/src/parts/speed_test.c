/* ---- speed test: active line-capacity measurement (Cloudflare endpoints) ---- */
static volatile LONG g_speedBusy = 0;

static int SpeedTransfer(int upload, LONGLONG want, double *mbps) {
    /* One HTTPS transfer to speed.cloudflare.com. The rate is timed from
       after the first chunk so TCP + TLS setup does not drag it down. */
    HINTERNET ses = WinHttpOpen(L"DnsPilot-Speed/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 8000, 8000, 15000, 60000);
    HINTERNET con = WinHttpConnect(ses, L"speed.cloudflare.com",
                                   INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, upload ? L"POST" : L"GET",
                                       upload ? L"/__up" : L"/__down?bytes=67108864",
                                       NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return 0; }
    LARGE_INTEGER t0, t1, fq;
    LONGLONG done = 0;
    int started = 0, ok = 0;
    char chunk[65536];
    ZeroMemory(chunk, sizeof(chunk));
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t0);
    t1 = t0;
    if (upload) {
        if (WinHttpSendRequest(req, L"Content-Type: application/octet-stream", (DWORD)-1L,
                               WINHTTP_NO_REQUEST_DATA, 0, (DWORD)want, 0)) {
            while (done < want) {
                DWORD n = (DWORD)((want - done) > (LONGLONG)sizeof(chunk)
                                      ? sizeof(chunk) : (DWORD)(want - done));
                DWORD wrote = 0;
                if (!WinHttpWriteData(req, chunk, n, &wrote)) break;
                done += wrote;
                if (!started) { started = 1; QueryPerformanceCounter(&t0); }
            }
            QueryPerformanceCounter(&t1);
            WinHttpReceiveResponse(req, NULL);
        }
    } else {
        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, NULL)) {
            for (;;) {
                DWORD n = 0;
                if (!WinHttpReadData(req, chunk, sizeof(chunk), &n)) break;
                if (n == 0) break;
                done += n;
                if (!started) { started = 1; QueryPerformanceCounter(&t0); }
                if (done >= want) break;
            }
            QueryPerformanceCounter(&t1);
        }
    }
    if (done >= want && started) {
        double secs = (double)(t1.QuadPart - t0.QuadPart) / (double)fq.QuadPart;
        if (secs > 0.001) { *mbps = (double)done * 8.0 / secs / 1e6; ok = 1; }
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static DWORD WINAPI SpeedThread(LPVOID param) {
    (void)param;
    double dn = -1.0, up = -1.0;
    wchar_t d[48], u[48];
    wcsncpy(g_status, L"Speed test: downloading 64 MB ...", 511);
    InvalidateRect(g_hwnd, NULL, FALSE);
    SpeedTransfer(0, 64LL * 1024 * 1024, &dn);
    if (!InterlockedCompareExchange(&g_exitStop, 0, 0)) {
        wcsncpy(g_status, L"Speed test: uploading 16 MB ...", 511);
        InvalidateRect(g_hwnd, NULL, FALSE);
        SpeedTransfer(1, 16LL * 1024 * 1024, &up);
    }
    if (dn < 0.0 && up < 0.0) {
        wcsncpy(g_status,
                L"Speed test failed - could not reach speed.cloudflare.com (network or firewall).",
                511);
    } else {
        FmtMbps(dn, d, 48);
        FmtMbps(up, u, 48);
        _snwprintf(g_status, 511,
                   L"Line speed: down %s   up %s   (single stream, 64 MB down / 16 MB up via speed.cloudflare.com)",
                   d, u);
        g_status[511] = L'\0';
    }
    InterlockedExchange(&g_speedBusy, 0);
    InvalidateRect(g_hwnd, NULL, FALSE);
    return 0;
}

static void StartSpeedTest(void) {
    if (InterlockedCompareExchange(&g_speedBusy, 1, 0) != 0) return;
    HANDLE h = CreateThread(NULL, 0, SpeedThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else InterlockedExchange(&g_speedBusy, 0);
}

static DWORD WINAPI StatsThread(LPVOID unused) {
    (void)unused;
    ULONGLONG lastTick = 0, lastIn = 0, lastOut = 0;
    double downEwma = -1.0, upEwma = -1.0;
    while (g_statsRun) {
        Sleep(1000);
        if (!g_statsRun) break;
        if (IsIconic(g_hwnd) || !IsWindowVisible(g_hwnd)) continue;  /* free when hidden */

        /* 1) passive throughput on the active adapter (zero extra traffic) */
        MIB_IFROW row;
        ZeroMemory(&row, sizeof(row));
        row.dwIndex = g_ifIndex;
        if (g_ifIndex && GetIfEntry(&row) == NO_ERROR) {
            ULONGLONG inOct = row.dwInOctets, outOct = row.dwOutOctets;
            ULONGLONG tick = GetTickCount64();
            if (lastTick && tick > lastTick) {
                ULONGLONG dt = tick - lastTick;
                ULONGLONG dIn  = (inOct  >= lastIn)  ? (inOct  - lastIn)  : (inOct  + (1ULL << 32) - lastIn);
                ULONGLONG dOut = (outOct >= lastOut) ? (outOct - lastOut) : (outOct + (1ULL << 32) - lastOut);
                double secs = (double)dt / 1000.0;
                if (secs > 0.05) {
                    double din  = (double)dIn  * 8.0 / secs / 1e6;
                    double dout = (double)dOut * 8.0 / secs / 1e6;
                    downEwma = (downEwma < 0) ? din  : downEwma * 0.6 + din  * 0.4;
                    upEwma   = (upEwma   < 0) ? dout : upEwma   * 0.6 + dout * 0.4;
                }
            }
            lastIn = inOct; lastOut = outOct; lastTick = tick;
        }

        /* 2) one small ping each to the router and the active DNS server */
        DWORD gwMs = PING_NONE, dnsMs = PING_NONE;
        if (g_gwIp[0])
            gwMs = g_gwIsV6 ? PingV6Once(g_gwIp, g_srcV6, STATS_PING_TIMEOUT)
                            : PingV4Once(g_gwIp, STATS_PING_TIMEOUT);
        if (g_curV4[0])
            dnsMs = PingV4Once(g_curV4, STATS_PING_TIMEOUT);
        else if (g_curV6[0])
            dnsMs = PingV6Once(g_curV6, g_srcV6, STATS_PING_TIMEOUT);

        /* 3) build the one-line strip */
        wchar_t wGw[16], wDns[16], wDown[24], wUp[24], line[160];
        FmtMs(gwMs, wGw, 16);
        FmtMs(dnsMs, wDns, 16);
        FmtMbps(downEwma, wDown, 24);
        FmtMbps(upEwma, wUp, 24);
        _snwprintf(line, 159, L"GW %s  |  DNS %s  |  \x2193 %s  |  \x2191 %s",
                   wGw, wDns, wDown, wUp);
        line[159] = L'\0';
        EnterCriticalSection(&g_statsCs);
        wcsncpy(g_statsLine, line, 159);
        LeaveCriticalSection(&g_statsCs);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
    return 0;
}

