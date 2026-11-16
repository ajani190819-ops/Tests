/* ---------------- config + outbox + receipt history ---------------- */

static void InitPaths(void) {
    wchar_t appdata[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    _snwprintf(g_cfgDir, MAX_PATH - 1, L"%s\\AgentNotes", appdata);
    CreateDirectoryW(g_cfgDir, NULL);
    _snwprintf(g_cfgPath, MAX_PATH - 1, L"%s\\config.ini", g_cfgDir);
    _snwprintf(g_histPath, MAX_PATH - 1, L"%s\\history.txt", g_cfgDir);
    _snwprintf(g_outboxDir, MAX_PATH - 1, L"%s\\outbox", g_cfgDir);
    CreateDirectoryW(g_outboxDir, NULL);
}

static void LoadConfig(void) {
    GetPrivateProfileStringW(L"AgentNotes", L"Link", L"", g_link, 255, g_cfgPath);
    g_waitSound = GetPrivateProfileIntW(L"AgentNotes", L"WaitSound", 0, g_cfgPath) ? 1 : 0;
    GetPrivateProfileStringW(L"AgentNotes", L"SeenWait", L"", g_seenWait, 79, g_cfgPath);
}

static void SaveConfig(void) {
    WritePrivateProfileStringW(L"AgentNotes", L"Link", g_link, g_cfgPath);
    WritePrivateProfileStringW(L"AgentNotes", L"WaitSound", g_waitSound ? L"1" : L"0", g_cfgPath);
    WritePrivateProfileStringW(L"AgentNotes", L"SeenWait", g_seenWait, g_cfgPath);
}

static int OutboxCount(void) {
    int n = 0;
    wchar_t pat[MAX_PATH + 16];
    WIN32_FIND_DATAW fd;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return n;
}

static int FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

/* Recent notes, newest first. A surviving outbox file is Queued. Once the
   receiver has flushed the note and returned its durable receipt, `receipt`
   makes the first receipt stage explicit: Saved. `read` becomes true only
   after the agent runs its required inbox checkpoint acknowledgement. */
typedef struct {
    wchar_t name[80];
    wchar_t stamp[16];    /* "MM-DD HH:MM" */
    wchar_t snip[80];
    wchar_t receipt[80];  /* opaque receiver token; blank for queued/legacy */
    int read;
} HistEnt;
static HistEnt g_hist[HIST_MAX];
static int     g_histN = 0;

static int HistSave(void) {
    HANDLE h = CreateFileW(g_histPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int ok = 1;
    for (int i = 0; i < g_histN; i++) {
        /* Convert each field to UTF-8 explicitly - %ls in narrow printf
           would route through the ANSI codepage and mangle non-ASCII. */
        char n8[256], s8[64], p8[256], r8[256];
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].name, -1, n8, sizeof(n8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].stamp, -1, s8, sizeof(s8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].snip, -1, p8, sizeof(p8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].receipt, -1, r8, sizeof(r8) - 1, NULL, NULL);
        n8[sizeof(n8) - 1] = s8[sizeof(s8) - 1] = p8[sizeof(p8) - 1] = '\0';
        r8[sizeof(r8) - 1] = '\0';
        char line[960];
        int n = snprintf(line, sizeof(line) - 1, "%s\t%s\t%s\t%s\t%d\r\n",
                         n8, s8, p8, r8, g_hist[i].read ? 1 : 0);
        if (n <= 0 || n >= (int)sizeof(line)) { ok = 0; continue; }
        DWORD wrote = 0;
        if (!WriteFile(h, line, (DWORD)n, &wrote, NULL) || wrote != (DWORD)n) ok = 0;
    }
    CloseHandle(h);
    return ok;
}

static void HistLoad(void) {
    HANDLE h = CreateFileW(g_histPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, NULL), rd = 0;
    if (sz > 200000) sz = 200000;
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { CloseHandle(h); return; }
    ReadFile(h, buf, sz, &rd, NULL);
    buf[rd] = '\0';
    CloseHandle(h);
    g_histN = 0;
    char *p = buf;
    while (*p && g_histN < HIST_MAX) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        char *t1 = strchr(p, '\t');
        if (t1) {
            *t1 = '\0';
            char *t2 = strchr(t1 + 1, '\t');
            if (t2) {
                *t2 = '\0';
                HistEnt *e = &g_hist[g_histN];
                ZeroMemory(e, sizeof(*e));
                MultiByteToWideChar(CP_UTF8, 0, p, -1, e->name, 79);
                MultiByteToWideChar(CP_UTF8, 0, t1 + 1, -1, e->stamp, 15);
                char *t3 = strchr(t2 + 1, '\t');
                if (t3) {
                    *t3 = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, t2 + 1, -1, e->snip, 79);
                    char *t4 = strchr(t3 + 1, '\t');
                    if (t4) {
                        *t4 = '\0';
                        MultiByteToWideChar(CP_UTF8, 0, t3 + 1, -1, e->receipt, 79);
                        e->read = atoi(t4 + 1) ? 1 : 0;
                    }
                } else {
                    /* Pre-receipt history remains readable; it simply cannot
                       claim a receipt that its older app version never stored. */
                    MultiByteToWideChar(CP_UTF8, 0, t2 + 1, -1, e->snip, 79);
                }
                e->name[79] = e->stamp[15] = e->snip[79] = e->receipt[79] = L'\0';
                g_histN++;
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}

static void HistAdd(const wchar_t *name, const wchar_t *stamp, const wchar_t *snip) {
    if (g_histN == HIST_MAX) g_histN--;
    memmove(&g_hist[1], &g_hist[0], (size_t)g_histN * sizeof(HistEnt));
    HistEnt *e = &g_hist[0];
    ZeroMemory(e, sizeof(*e));
    wcsncpy(e->name, name, 79);   e->name[79] = L'\0';
    wcsncpy(e->stamp, stamp, 15); e->stamp[15] = L'\0';
    wcsncpy(e->snip, snip, 79);   e->snip[79] = L'\0';
    g_histN++;
    HistSave();
}

/* Store the receiver's opaque durable-inbox receipt before the source outbox
   file is deleted. If this write fails, the note remains queued; retrying its
   stable key is idempotent at the receiver, so neither a lost receipt nor a
   duplicate inbox entry is silently possible. */
static int HistMarkSaved(const wchar_t *name, const wchar_t *receipt) {
    if (!name || !receipt || !receipt[0]) return 0;
    for (int i = 0; i < g_histN; i++) {
        if (wcscmp(g_hist[i].name, name) == 0) {
            wcsncpy(g_hist[i].receipt, receipt, 79);
            g_hist[i].receipt[79] = L'\0';
            g_hist[i].read = 0;
            return HistSave();
        }
    }
    return 0;
}

static int HistMarkReceiptRead(const wchar_t *receipt) {
    if (!receipt || !receipt[0]) return 0;
    for (int i = 0; i < g_histN; i++) {
        if (wcscmp(g_hist[i].receipt, receipt) == 0 && !g_hist[i].read) {
            g_hist[i].read = 1;
            return 1;
        }
    }
    return 0;
}

static void HistSnip(const wchar_t *text, wchar_t *out, int cap) {
    int n = 0;
    while (text[n] && n + 1 < cap) n++;
    for (int i = 0; i < n; i++) {
        wchar_t c = text[i];
        out[i] = (c == L'\r' || c == L'\n' || c == L'\t') ? L' ' : c;
    }
    out[n] = L'\0';
}

static int OutboxAdd(const wchar_t *text, wchar_t *nameOut, int nameCap) {
    wchar_t path[MAX_PATH + 32];
    static LONG seq = 0;
    LONG i = InterlockedIncrement(&seq);
    unsigned char random[4] = { 0, 0, 0, 0 };
    if (BCryptGenRandom(NULL, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        DWORD fallback = GetTickCount() ^ GetCurrentProcessId() ^ (DWORD)i;
        memcpy(random, &fallback, sizeof(random));
    }
    wchar_t name[80];
    _snwprintf(name, 79, L"n-%08lx-%08lx-%08lx-%02x%02x%02x%02x.txt",
               (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount(),
               (unsigned long)i, (unsigned)random[0], (unsigned)random[1],
               (unsigned)random[2], (unsigned)random[3]);
    name[79] = L'\0';
    _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, name);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (n <= 1) { CloseHandle(h); DeleteFileW(path); return 0; }
    char *u = (char *)malloc((size_t)n);
    if (!u) { CloseHandle(h); DeleteFileW(path); return 0; }
    WideCharToMultiByte(CP_UTF8, 0, text, -1, u, n, NULL, NULL);
    DWORD wrote = 0;
    int ok = WriteFile(h, u, (DWORD)(n - 1), &wrote, NULL) && wrote == (DWORD)(n - 1);
    free(u);
    CloseHandle(h);
    if (!ok) { DeleteFileW(path); return 0; }
    if (nameOut && nameCap > 0) {
        wcsncpy(nameOut, name, nameCap - 1);
        nameOut[nameCap - 1] = L'\0';
    }
    return 1;
}
