/* ---------------- net: WinHTTP GET + tile cache ---------------- */

typedef struct {
    char  *data;
    size_t len;
} HttpBuf;

static void HttpBufFree(HttpBuf *b) {
    if (b && b->data) free(b->data);
    if (b) { b->data = NULL; b->len = 0; }
}

/* One GET. Handles the query string explicitly: WinHttpCrackUrl only folds
   the extra info into lpszUrlPath when no extra-info buffer is supplied, and
   every interesting call this app makes carries a query. Returns 1 with a
   NUL-terminated buffer on HTTP 200 (binary-safe: length is separate). */
static int HttpGetBuf(const wchar_t *url, HttpBuf *out) {
    out->data = NULL;
    out->len = 0;
    URL_COMPONENTS uc;
    wchar_t host[160] = L"", urlPath[768] = L"", extra[768] = L"";
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 159;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 767;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 767;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return 0;

    wchar_t fullPath[1536];
    _snwprintf(fullPath, 1535, L"%s%s", urlPath, extra);
    fullPath[1535] = L'\0';

    HINTERNET ses = WinHttpOpen(HTTP_UA, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 8000, 8000, 10000, 20000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, L"GET", fullPath, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (uc.nScheme == INTERNET_SCHEME_HTTPS)
                                           ? WINHTTP_FLAG_SECURE : 0);
    int ok = 0;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    if (req && WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            NULL, &code, &sz, NULL);
        if (code == 200) {
            cap = 65536;
            buf = (char *)malloc(cap);
            DWORD rd = 0;
            while (buf && WinHttpReadData(req, buf + len, (DWORD)(cap - len), &rd) && rd > 0) {
                len += rd;
                if (len + 65536 > cap) {
                    cap *= 2;
                    char *nb = (char *)realloc(buf, cap);
                    if (!nb) { free(buf); buf = NULL; break; }
                    buf = nb;
                }
            }
            if (buf) ok = 1;
        }
    }
    if (req) WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    if (!ok || !buf) { if (buf) free(buf); return 0; }
    buf[len] = '\0';
    out->data = buf;
    out->len = len;
    return 1;
}

/* Tile cache. Names are FNV-1a hashes of the full URL so identical requests
   never refetch, and the two families are pruned on their own clocks:
   basemap tiles barely change (30 days), radar frames are timestamped in
   their path so an old file is simply an old frame (2 days). */
static void CacheNameFor(const wchar_t *prefix, const wchar_t *url, wchar_t *out, int cap) {
    unsigned long key = 2166136261UL;
    char utf[1600];
    WideToUtf8(url, utf, 1600);
    for (const unsigned char *p = (const unsigned char *)utf; *p; p++) {
        key ^= (unsigned long)*p;
        key *= 16777619UL;
    }
    _snwprintf(out, cap - 1, L"%s\\%s%08lx.bin", g_cacheDir, prefix, key);
    out[cap - 1] = L'\0';
}

static int CacheAgeSecOk(const wchar_t *path, long long maxAgeSec) {
    WIN32_FILE_ATTRIBUTE_DATA ad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &ad)) return 0;
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER nowU, ftU;
    nowU.LowPart = nowFt.dwLowDateTime;  nowU.HighPart = nowFt.dwHighDateTime;
    ftU.LowPart = ad.ftLastWriteTime.dwLowDateTime;
    ftU.HighPart = ad.ftLastWriteTime.dwHighDateTime;
    return (long long)((nowU.QuadPart - ftU.QuadPart) / 10000000ULL) <= maxAgeSec;
}

