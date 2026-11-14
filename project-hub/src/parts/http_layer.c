/* ---- HTTP layer: one shared WinHTTP session + per-host connection --------
   Every request runs on the single hub worker thread, so plain lazy handles
   are safe. Reusing the session and connection means steady-state checks
   keep a warm TLS channel instead of paying fresh handshakes for every
   probe (this used to open 8 brand-new sessions per 10-second cycle). */
static HINTERNET g_httpSess = NULL;
/* One connection handle PER HOST. WinHttpConnect bakes the hostname into
   the handle, so sharing one slot across hosts sends later requests to
   whatever host connected first (that made the github.com jar download
   land on raw.githubusercontent.com and 404 - "download failed"). */
typedef struct { const wchar_t *host; HINTERNET conn; } HostConn;
static HostConn g_httpConns[] = {
    { L"raw.githubusercontent.com", NULL },
    { L"api.github.com", NULL },
    { L"github.com", NULL },
    { NULL, NULL }
};

static HINTERNET HttpSession(void) {
    if (!g_httpSess) {
        g_httpSess = WinHttpOpen(L"Tests-ProjectHub/2.3",
                                 WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (g_httpSess) WinHttpSetTimeouts(g_httpSess, 8000, 10000, 15000, 30000);
    }
    return g_httpSess;
}

static HINTERNET HttpConnect(const wchar_t *host) {
    for (HostConn *s = g_httpConns; s->host; s++) {
        if (_wcsicmp(s->host, host) != 0) continue;
        if (!s->conn) {
            HINTERNET sess = HttpSession();
            if (!sess) return NULL;
            s->conn = WinHttpConnect(sess, s->host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        }
        return s->conn;
    }
    return NULL;
}

static BOOL HttpHeadSize(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize) {
    *outSize = 0;
    HINTERNET hConnect = HttpConnect(host);
    if (!hConnect) return FALSE;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"HEAD", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) return FALSE;
    BOOL ok = WinHttpSendRequest(hReq, L"User-Agent: Tests-ProjectHub/2.3\r\n", (DWORD)-1L,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(hReq, NULL);
    if (ok) {
        DWORD cl = 0, cls = sizeof(cl);
        if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &cl, &cls, WINHTTP_NO_HEADER_INDEX))
            *outSize = cl;
    }
    WinHttpCloseHandle(hReq);
    return ok && *outSize > 0;
}

static BYTE *HttpGetBufferEx(const wchar_t *host, const wchar_t *urlPath, const wchar_t *extraHdr,
                             DWORD *outSize, int *outStatus,
                             wchar_t *outEtag, wchar_t *outLastMod, BOOL *outNotMod) {
    if (outSize) *outSize = 0;
    if (outStatus) *outStatus = 0;
    if (outNotMod) *outNotMod = FALSE;

    HINTERNET hConnect = HttpConnect(host);
    if (!hConnect) return NULL;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) return NULL;

    wchar_t hdrs[768];
    _snwprintf(hdrs, 767, L"User-Agent: Tests-ProjectHub/2.3\r\nAccept: */*\r\n%s",
               extraHdr ? extraHdr : L"");
    hdrs[767] = L'\0';

    BOOL sent = WinHttpSendRequest(hReq, hdrs, (DWORD)-1L,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    DWORD statusCode = 0, scSize = sizeof(statusCode);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);
    if (outStatus) *outStatus = (int)statusCode;

    if (statusCode == 304) {
        if (outNotMod) *outNotMod = TRUE;
        WinHttpCloseHandle(hReq);
        return NULL;
    }
    if (statusCode != 200) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    if (outEtag && outLastMod) {
        outEtag[0] = L'\0';
        outLastMod[0] = L'\0';
        DWORD n = 95;
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_ETAG, WINHTTP_HEADER_NAME_BY_INDEX,
                            outEtag, &n, WINHTTP_NO_HEADER_INDEX);
        n = 63;
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_LAST_MODIFIED, WINHTTP_HEADER_NAME_BY_INDEX,
                            outLastMod, &n, WINHTTP_NO_HEADER_INDEX);
    }

    DWORD cap = 65536, len = 0;
    BYTE *buf = (BYTE *)malloc(cap);
    if (!buf) {
        WinHttpCloseHandle(hReq);
        return NULL;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail) || avail == 0) break;
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            BYTE *nb = (BYTE *)realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
        }
        DWORD rd = 0;
        if (!WinHttpReadData(hReq, buf + len, avail, &rd) || rd == 0) break;
        len += rd;
    }

    WinHttpCloseHandle(hReq);

    if (buf) {
        buf[len] = 0;
        if (outSize) *outSize = len;
    }
    return buf;
}

static BYTE *HttpGetBuffer(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    return HttpGetBufferEx(host, urlPath, NULL, outSize, outStatus, NULL, NULL, NULL);
}

