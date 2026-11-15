/* ---------------- config + outbox + history ---------------- */

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
}

static void SaveConfig(void) {
    WritePrivateProfileStringW(L"AgentNotes", L"Link", g_link, g_cfgPath);
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

/* Recent notes: newest first. Each entry remembers its outbox file name -
   the file still existing means "Queued", gone means "Sent". The state is
   always live, with no bookkeeping to drift. */
typedef struct {
    wchar_t name[80];
    wchar_t stamp[16];    /* "MM-DD HH:MM" */
    wchar_t snip[80];
} HistEnt;
static HistEnt g_hist[HIST_MAX];
static int     g_histN = 0;

static void HistSave(void) {
    HANDLE h = CreateFileW(g_histPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < g_histN; i++) {
        /* Convert each field to UTF-8 explicitly - %ls in narrow printf
           would route through the ANSI codepage and mangle non-ASCII. */
        char n8[256], s8[64], p8[256];
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].name, -1, n8, sizeof(n8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].stamp, -1, s8, sizeof(s8) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, g_hist[i].snip, -1, p8, sizeof(p8) - 1, NULL, NULL);
        n8[sizeof(n8) - 1] = s8[sizeof(s8) - 1] = p8[sizeof(p8) - 1] = '\0';
        char line[640];
        int n = snprintf(line, sizeof(line) - 1, "%s\t%s\t%s\r\n", n8, s8, p8);
        if (n > 0) {
            DWORD wrote = 0;
            WriteFile(h, line, (DWORD)n, &wrote, NULL);
        }
    }
    CloseHandle(h);
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
                MultiByteToWideChar(CP_UTF8, 0, p, -1, e->name, 79);
                MultiByteToWideChar(CP_UTF8, 0, t1 + 1, -1, e->stamp, 15);
                MultiByteToWideChar(CP_UTF8, 0, t2 + 1, -1, e->snip, 79);
                e->name[79] = e->stamp[15] = e->snip[79] = L'\0';
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
    wcsncpy(e->name, name, 79);   e->name[79] = L'\0';
    wcsncpy(e->stamp, stamp, 15); e->stamp[15] = L'\0';
    wcsncpy(e->snip, snip, 79);   e->snip[79] = L'\0';
    g_histN++;
    HistSave();
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
    wchar_t name[64];
    _snwprintf(name, 63, L"n-%lu-%lu.txt",
               (unsigned long)GetTickCount(), (unsigned long)i);
    name[63] = L'\0';
    _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, name);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *u = (char *)malloc((size_t)n);
    if (u) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, u, n, NULL, NULL);
        DWORD wrote = 0;
        WriteFile(h, u, (DWORD)(n - 1), &wrote, NULL);
        free(u);
    }
    CloseHandle(h);
    if (nameOut && nameCap > 0) {
        wcsncpy(nameOut, name, nameCap - 1);
        nameOut[nameCap - 1] = L'\0';
    }
    return 1;
}

