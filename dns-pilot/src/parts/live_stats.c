/* ---- live stats strip: passive throughput + 1 Hz ping (tiny footprint) ---- */
#define STATS_PING_TIMEOUT 600
#define PING_NONE ((DWORD)-1)

static DWORD PingV4Once(const char *ip, DWORD timeoutMs) {
    ULONG addr = inet_addr(ip);
    if (addr == INADDR_NONE) return PING_NONE;
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return PING_NONE;
    char req[8] = "KBLsui";
    char reply[sizeof(ICMP_ECHO_REPLY) + 16];
    ZeroMemory(reply, sizeof(reply));
    DWORD ms = PING_NONE;
    if (IcmpSendEcho(h, addr, req, 6, NULL, reply, sizeof(reply), timeoutMs) > 0) {
        ICMP_ECHO_REPLY *r = (ICMP_ECHO_REPLY *)reply;
        if (r->Status == 0) ms = r->RoundTripTime;
    }
    IcmpCloseHandle(h);
    return ms;
}

static DWORD PingV6Once(const char *ip, const char *src, DWORD timeoutMs) {
    if (!ip || !ip[0]) return PING_NONE;
    HANDLE h = Icmp6CreateFile();
    if (h == INVALID_HANDLE_VALUE) return PING_NONE;
    struct sockaddr_in6 dst, src6;
    ZeroMemory(&dst, sizeof(dst));
    ZeroMemory(&src6, sizeof(src6));
    dst.sin6_family = AF_INET6;
    src6.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, ip, &dst.sin6_addr) != 1) {
        IcmpCloseHandle(h);
        return PING_NONE;
    }
    /* Pick the source to match the destination scope: fe80 targets need the
       link-local source + interface scope, global targets need a global
       source (never the link-local one Windows lists first). */
    int dstIsLL = (ip[0] == 'f' && ip[1] == 'e' && ip[2] >= '8' && ip[2] <= 'b');
    const char *useSrc = src;
    if (dstIsLL) {
        if (g_srcV6LL[0]) useSrc = g_srcV6LL;
        dst.sin6_scope_id = (u_long)g_ifIndex;
    } else {
        if (!g_hasGlobalV6) { IcmpCloseHandle(h); return PING_NONE; }
    }
    if (!useSrc || !useSrc[0] || inet_pton(AF_INET6, useSrc, &src6.sin6_addr) != 1) {
        IcmpCloseHandle(h);
        return PING_NONE;
    }
    char req[8] = "KBLsui";
    char reply[sizeof(ICMPV6_ECHO_REPLY) + 48];
    ZeroMemory(reply, sizeof(reply));
    DWORD ms = PING_NONE;
    if (Icmp6SendEcho2(h, NULL, NULL, NULL, &src6, &dst, req, 6, NULL,
                      reply, sizeof(reply), timeoutMs) > 0) {
        ICMPV6_ECHO_REPLY *r = (ICMPV6_ECHO_REPLY *)reply;
        if (r->Status == 0) ms = r->RoundTripTime;
    }
    IcmpCloseHandle(h);
    return ms;
}

static void FmtMs(DWORD ms, wchar_t *out, int n) {
    if (ms == PING_NONE) WCpy(out, n, L"-");
    else if (ms == 0) WCpy(out, n, L"<1 ms");
    else {
        _snwprintf(out, n - 1, L"%lu ms", (unsigned long)ms);
        out[n - 1] = L'\0';
    }
}

static void FmtMbps(double mbps, wchar_t *out, int n) {
    if (mbps < 0.0) WCpy(out, n, L"-");
    else if (mbps >= 100.0) _snwprintf(out, n - 1, L"%.0f Mbps", mbps);
    else if (mbps >= 10.0)  _snwprintf(out, n - 1, L"%.1f Mbps", mbps);
    else _snwprintf(out, n - 1, L"%.2f Mbps", mbps);
    out[n - 1] = L'\0';
}

