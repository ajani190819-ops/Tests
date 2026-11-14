/* ---------------- HTTP to the agent session ---------------- */

static int HttpPostNote(const wchar_t *link, const wchar_t *path,
                        const char *body, int bodyLen) {
    URL_COMPONENTS uc;
    wchar_t host[128] = L"", urlPath[256] = L"";
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 127;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 255;
    if (!WinHttpCrackUrl(link, 0, 0, &uc)) return 0;

    HINTERNET ses = WinHttpOpen(L"AgentNotes/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 6000, 6000, 10000, 15000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, L"POST", path, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (uc.nScheme == INTERNET_SCHEME_HTTPS)
                                           ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return 0; }
    int ok = 0;
    if (WinHttpSendRequest(req, L"Content-Type: text/plain; charset=utf-8", (DWORD)-1L,
                           (LPVOID)body, (DWORD)bodyLen, (DWORD)bodyLen, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(req,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
                            WINHTTP_NO_HEADER_INDEX);
        ok = (code == 200);
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static int FlushOutbox(int *delivered) {
    wchar_t pat[MAX_PATH + 16], path[MAX_PATH + 32];
    WIN32_FIND_DATAW fd;
    int sent = 0, failed = 0;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, fd.cFileName);
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) { failed++; continue; }
        DWORD sz = GetFileSize(f, NULL), rd = 0;
        if (sz > 60000) sz = 60000;
        char *buf = (char *)malloc((size_t)sz + 1);
        if (!buf) { CloseHandle(f); failed++; continue; }
        ReadFile(f, buf, sz, &rd, NULL);
        buf[rd] = '\0';
        CloseHandle(f);
        if (rd > 0 && g_link[0] && HttpPostNote(g_link, L"/note", buf, (int)rd)) {
            DeleteFileW(path);
            sent++;
        } else {
            failed++;
        }
        free(buf);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (delivered) *delivered = sent;
    return failed;
}

static DWORD WINAPI FlushThread(LPVOID unused) {
    (void)unused;
    int sent = 0;
    int failed = FlushOutbox(&sent);
    int left = OutboxCount();
    g_sending = 0;
    if (sent > 0 && failed == 0) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        _snwprintf(g_lastSent, 15, L"%02d:%02d", st.wHour, st.wMinute);
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d note%s  |  last note sent %s  |  inbox is clear.",
                   sent, sent == 1 ? L"" : L"s", g_lastSent);
    } else if (sent > 0) {
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d  |  %d still queued - will retry automatically.",
                   sent, left);
    } else if (failed > 0) {
        g_lastOk = 0;
        _snwprintf(g_status, 255,
                   L"Agent offline - %d queued note%s stay safe here and deliver "
                   L"the moment a session is live.",
                   left, left == 1 ? L"" : L"s");
    } else {
        g_lastOk = 1;
        wcsncpy(g_status, L"Nothing to send.", 255);
    }
    g_status[255] = L'\0';
    InvalidateRect(g_hwnd, NULL, FALSE);
    return 0;
}

/* ---------------- session link discovery ----------------
   The agent commits .agent/AGENT_LINK.txt - the public address of its
   workspace - to the active branch every turn it works. Polling that one
   file is what kills the paste-a-new-link ritual: the app adopts the
   discovered address ONLY when its /health probe answers, so a stale file,
   a dead session or a half-written commit can never replace a link that
   works. With no hub config to read the branch from, discovery stays quiet
   and the saved link keeps doing its job. */
static int HttpGetText(const wchar_t *url, char *out, int cap) {
    URL_COMPONENTS uc;
    wchar_t host[128] = L"", urlPath[256] = L"";
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 127;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 255;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return 0;
    HINTERNET ses = WinHttpOpen(L"AgentNotes/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 5000, 5000, 8000, 10000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, L"GET", urlPath, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (uc.nScheme == INTERNET_SCHEME_HTTPS)
                                           ? WINHTTP_FLAG_SECURE : 0);
    int len = 0;
    if (req && WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            NULL, &code, &sz, NULL);
        if (code == 200) {
            DWORD rd = 0;
            while (len < cap - 1 &&
                   WinHttpReadData(req, out + len, (DWORD)(cap - 1 - len), &rd) && rd > 0)
                len += (int)rd;
        }
    }
    out[len] = '\0';
    if (req) WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return len;
}

static int HealthOk(const wchar_t *link) {
    wchar_t url[320];
    _snwprintf(url, 319, L"%s/health", link);
    url[319] = L'\0';
    char buf[64];
    return HttpGetText(url, buf, 64) > 0;
}

int LinkDiscoveryTick(void) {
    wchar_t cfg[MAX_PATH], branch[64] = L"";
    GetEnvironmentVariableW(L"LOCALAPPDATA", cfg, MAX_PATH);
    wcsncat(cfg, L"\\ProjectHub\\config.ini", MAX_PATH - wcslen(cfg) - 1);
    GetPrivateProfileStringW(L"Update", L"Branch", L"", branch, 63, cfg);
    if (!branch[0]) return 0;
    wchar_t url[384];
    _snwprintf(url, 383,
               L"https://raw.githubusercontent.com/ajani190819-ops/Tests/%s/"
               L".agent/AGENT_LINK.txt?t=%llu", branch,
               (unsigned long long)(GetTickCount64() / 60000ULL));
    url[383] = L'\0';
    char buf[512];
    if (HttpGetText(url, buf, 512) <= 0) return 0;
    int end = 0;
    while (buf[end] && buf[end] != '\r' && buf[end] != '\n') end++;
    buf[end] = '\0';
    while (end > 0 && (buf[end - 1] == ' ' || buf[end - 1] == '\t')) buf[--end] = '\0';
    if (end < 8 || strncmp(buf, "http", 4) != 0) return 0;
    wchar_t cand[256];
    if (MultiByteToWideChar(CP_UTF8, 0, buf, -1, cand, 256) <= 0) return 0;
    cand[255] = L'\0';
    if (wcscmp(cand, g_link) == 0) return 0;
    if (!HealthOk(cand)) return 0;
    wcsncpy(g_link, cand, 255);
    g_link[255] = L'\0';
    SaveConfig();
    return 1;
}

static void StartFlush(void) {
    if (g_sending) return;
    g_sending = 1;
    HANDLE h = CreateThread(NULL, 0, FlushThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else g_sending = 0;
}

