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
                   L"Session link not answering - re-paste the current link if it changed  |  %d queued.",
                   left);
    } else {
        g_lastOk = 1;
        wcsncpy(g_status, L"Nothing to send.", 255);
    }
    g_status[255] = L'\0';
    InvalidateRect(g_hwnd, NULL, FALSE);
    return 0;
}

static void StartFlush(void) {
    if (g_sending) return;
    g_sending = 1;
    HANDLE h = CreateThread(NULL, 0, FlushThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else g_sending = 0;
}

