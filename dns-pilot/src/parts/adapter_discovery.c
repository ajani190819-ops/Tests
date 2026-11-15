/* ---- active adapter discovery (iphlpapi; locale-proof) ---- */
static void CaptureAdapterExtras(const IP_ADAPTER_ADDRESSES *p);
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
        CaptureAdapterExtras(p);
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
            CaptureAdapterExtras(p);
            break;
        }
    }
    free(aa);
}

static void CopySockIp(const SOCKET_ADDRESS *sa, char *out, int outn, int *isV6) {
    out[0] = '\0';
    if (isV6) *isV6 = 0;
    if (!sa || !sa->lpSockaddr) return;
    if (sa->lpSockaddr->sa_family == AF_INET) {
        const struct sockaddr_in *sv = (const struct sockaddr_in *)sa->lpSockaddr;
        inet_ntop(AF_INET, (void *)&sv->sin_addr, out, outn);
    } else if (sa->lpSockaddr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sv6 = (const struct sockaddr_in6 *)sa->lpSockaddr;
        inet_ntop(AF_INET6, (void *)&sv6->sin6_addr, out, outn);
        if (isV6) *isV6 = 1;
    }
}

static void CaptureAdapterExtras(const IP_ADAPTER_ADDRESSES *p) {
    int isV6 = 0;
    if (p->FirstGatewayAddress)
        CopySockIp(&p->FirstGatewayAddress->Address, g_gwIp, 63, &isV6);
    g_gwIsV6 = isV6;
    /* Prefer a GLOBAL v6 (2000::/3) as the ping source. Windows lists the
       link-local fe80:: address first; using that as the source is what left
       the v6 stats pings showing '-' - a link-local source cannot reach
       global resolvers. Keep the link-local one for fe80 targets. */
    g_srcV6[0] = '\0';
    g_srcV6LL[0] = '\0';
    g_hasGlobalV6 = 0;
    for (const IP_ADAPTER_UNICAST_ADDRESS *u = p->FirstUnicastAddress; u; u = u->Next) {
        char tmp[64];
        if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET6)
            continue;
        CopySockIp(&u->Address, tmp, 63, NULL);
        if (!tmp[0]) continue;
        if (tmp[0] == 'f' && tmp[1] == 'e' && tmp[2] >= '8' && tmp[2] <= 'b') {
            if (!g_srcV6LL[0]) { strncpy(g_srcV6LL, tmp, 63); g_srcV6LL[63] = '\0'; }
        } else if (tmp[0] == '2' || tmp[0] == '3') {
            if (!g_srcV6[0]) { strncpy(g_srcV6, tmp, 63); g_srcV6[63] = '\0'; }
            g_hasGlobalV6 = 1;
        }
    }
    if (!g_srcV6[0] && g_srcV6LL[0]) { strncpy(g_srcV6, g_srcV6LL, 63); g_srcV6[63] = '\0'; }
}

