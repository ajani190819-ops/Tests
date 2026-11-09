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

