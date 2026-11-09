/* ---- version-probe cache: conditional GET (ETag / If-Modified-Since) ----
   An unchanged file answers 304 with no body at all, so steady-state checks
   transfer almost nothing instead of re-downloading ~200 KB of hub source
   every cycle just to read one version string. */
typedef struct {
    wchar_t host[48];
    wchar_t path[384];
    wchar_t etag[96];
    wchar_t lastMod[64];
    BYTE   *body;
    DWORD   len;
} HttpCacheEnt;
static HttpCacheEnt g_hcache[8];

static BYTE *HttpGetCached(const wchar_t *host, const wchar_t *urlPath, DWORD *outSize, int *outStatus) {
    HttpCacheEnt *ent = NULL;
    for (int i = 0; i < 8; i++) {
        if (g_hcache[i].path[0] && wcscmp(g_hcache[i].host, host) == 0 &&
            wcscmp(g_hcache[i].path, urlPath) == 0) {
            ent = &g_hcache[i];
            break;
        }
    }
    if (!ent) {
        for (int i = 0; i < 8; i++) {
            if (!g_hcache[i].path[0]) { ent = &g_hcache[i]; break; }
        }
        if (!ent) ent = &g_hcache[0];
        free(ent->body);
        ent->body = NULL;
        ent->len = 0;
        ent->etag[0] = L'\0';
        ent->lastMod[0] = L'\0';
        wcsncpy(ent->host, host, 47); ent->host[47] = L'\0';
        wcsncpy(ent->path, urlPath, 383); ent->path[383] = L'\0';
    }

    wchar_t cond[256] = L"";
    if (ent->etag[0])
        _snwprintf(cond, 255, L"If-None-Match: %s\r\n", ent->etag);
    if (ent->lastMod[0]) {
        size_t used = wcslen(cond);
        _snwprintf(cond + used, 255 - used, L"If-Modified-Since: %s\r\n", ent->lastMod);
    }
    cond[255] = L'\0';

    DWORD sz = 0;
    int st = 0;
    BOOL notMod = FALSE;
    BYTE *data = HttpGetBufferEx(host, urlPath, cond[0] ? cond : NULL, &sz, &st,
                                 ent->etag, ent->lastMod, &notMod);

    if (notMod && ent->body) {
        BYTE *copy = (BYTE *)malloc(ent->len + 1);
        if (!copy) return NULL;
        memcpy(copy, ent->body, ent->len + 1);
        if (outSize) *outSize = ent->len;
        if (outStatus) *outStatus = 200;
        return copy;
    }
    if (data && st == 200) {
        free(ent->body);
        ent->body = (BYTE *)malloc(sz + 1);
        if (ent->body) {
            memcpy(ent->body, data, sz + 1);
            ent->len = sz;
        } else {
            ent->len = 0;
        }
    }
    if (outSize) *outSize = sz;
    if (outStatus) *outStatus = st;
    return data;
}

/* Fetch one repo file into memory so a single download can be written to
   every copy of an app instead of being fetched once per location. */
static BYTE *DownloadRepoBytes(const wchar_t *branch, const wchar_t *relUrl, DWORD *outLen) {
    wchar_t path[512];
    _snwprintf(path, 511, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, branch, relUrl);
    DWORD sz = 0;
    int st = 0;
    BYTE *data = HttpGetBuffer(L"raw.githubusercontent.com", path, &sz, &st);
    if (!data || st != 200 || sz < 8) { free(data); return NULL; }
    if (outLen) *outLen = sz;
    return data;
}

static BOOL DownloadRawRepoFile(const wchar_t *branch, const wchar_t *relUrl, const wchar_t *destPath) {
    wchar_t path[512];
    _snwprintf(path, 511, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, branch, relUrl);
    DWORD sz = 0;
    int st = 0;
    BYTE *data = HttpGetBuffer(L"raw.githubusercontent.com", path, &sz, &st);
    if (!data || st != 200 || sz < 8) {
        free(data);
        return FALSE;
    }
    BOOL ok = WriteBinaryFileAtomic(destPath, data, sz);
    free(data);
    return ok;
}

/* The branch the updater pulls from is the user's choice, remembered in
   %LOCALAPPDATA%\ProjectHub\config.ini so every update check and download
   follows it without any merging. */
static void BranchCfgPath(wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%s\\ProjectHub\\config.ini", g_localApp);
    out[cap - 1] = L'\0';
}

static BOOL LoadBranchCfg(wchar_t *out, size_t cap) {
    wchar_t ini[MAX_PATH];
    BranchCfgPath(ini, MAX_PATH);
    out[0] = L'\0';
    DWORD n = GetPrivateProfileStringW(L"Update", L"Branch", L"", out, (DWORD)cap, ini);
    return n > 0 && out[0];
}

static void SaveBranchCfg(const wchar_t *branch) {
    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    _snwprintf(dir, MAX_PATH - 1, L"%s\\ProjectHub", g_localApp);
    dir[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(dir, NULL);
    BranchCfgPath(ini, MAX_PATH);
    WritePrivateProfileStringW(L"Update", L"Branch", branch, ini);
}

/* Per-app opt-outs. The mod card (and only it, per the owner's choice)
   can be disabled: no install, no update, Update All skips it. */
static int g_hudDisabled = 0;
static int g_orcaDisabled = 0;

static void LoadAppsCfg(void) {
    wchar_t ini[MAX_PATH], buf[8];
    BranchCfgPath(ini, MAX_PATH);
    GetPrivateProfileStringW(L"Apps", L"HudDisabled", L"0", buf, 7, ini);
    g_hudDisabled = (buf[0] == L'1');
    GetPrivateProfileStringW(L"Apps", L"OrcaDisabled", L"0", buf, 7, ini);
    g_orcaDisabled = (buf[0] == L'1');
}

static void SaveAppsCfg(void) {
    wchar_t ini[MAX_PATH];
    BranchCfgPath(ini, MAX_PATH);
    WritePrivateProfileStringW(L"Apps", L"HudDisabled",
                               g_hudDisabled ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Apps", L"OrcaDisabled",
                               g_orcaDisabled ? L"1" : L"0", ini);
}

static void EnsureActiveBranch(wchar_t *outBranch, size_t cap) {
    EnterCriticalSection(&g_cs);
    if (g_activeBranch[0]) {
        wcsncpy(outBranch, g_activeBranch, cap - 1);
        outBranch[cap - 1] = L'\0';
        LeaveCriticalSection(&g_cs);
        return;
    }
    LeaveCriticalSection(&g_cs);

    /* Saved user choice wins; only auto-probe when never chosen.
       But validate the saved branch actually has app binaries - a stale
       saved branch (from an older hub run) would otherwise make Install
       download from a branch where no exes exist, silently failing. */
    wchar_t chosenBuf[64];
    const wchar_t *chosen = NULL;
    if (LoadBranchCfg(chosenBuf, 64)) {
        DWORD sz = 0; int st = 0;
        wchar_t probePath[256];
        _snwprintf(probePath, 255, L"/%s/%s/%s/audio-switcher/AudioSwitcher.exe",
                   REPO_OWNER_W, REPO_NAME_W, chosenBuf);
        BYTE *probe = HttpGetBuffer(L"raw.githubusercontent.com", probePath, &sz, &st);
        free(probe);
        if (st == 200 && sz > 1000) {
            chosen = chosenBuf;
        }
        /* else: saved branch has no exes, fall through to probing below */
    }
    if (!chosen) {
        DWORD sz = 0;
        int st = 0;
        wchar_t probePath[256];
        /* Prefer main: it is where merged, released builds live, so a hub with
           no saved choice serves the builds the user actually shipped. A
           session branch used to win here, and because the fallback name is
           compiled in, a hub built in an older session kept probing that old
           branch, found exes there, and silently served stale builds forever.
           The session branch remains reachable as an explicit choice in the
           branch menu, and as the last resort when main has no exes at all. */
        _snwprintf(probePath, 255, L"/%s/%s/main/audio-switcher/AudioSwitcher.exe",
                   REPO_OWNER_W, REPO_NAME_W);
        BYTE *probe = HttpGetBuffer(L"raw.githubusercontent.com", probePath, &sz, &st);
        free(probe);
        if (st == 200 && sz > 1000) {
            chosen = L"main";
        } else {
            _snwprintf(probePath, 255, L"/%s/%s/%s/audio-switcher/AudioSwitcher.exe",
                       REPO_OWNER_W, REPO_NAME_W, FALLBACK_BRANCH_W);
            probe = HttpGetBuffer(L"raw.githubusercontent.com", probePath, &sz, &st);
            free(probe);
            chosen = (st == 200 && sz > 1000) ? FALLBACK_BRANCH_W : L"main";
        }
    }
    EnterCriticalSection(&g_cs);
    wcsncpy(g_activeBranch, chosen, 63);
    g_activeBranch[63] = L'\0';
    LeaveCriticalSection(&g_cs);

    wcsncpy(outBranch, chosen, cap - 1);
    outBranch[cap - 1] = L'\0';
}

static BOOL ExtractQuotedAfter(const char *hay, const char *key, char *out, size_t cap) {
    const char *p = strstr(hay, key);
    if (!p) return FALSE;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '=')) p++;
    char quote = *p;
    if (quote != '"' && quote != '\'') return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != quote && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* Numeric dotted-version compare: TRUE only when a is genuinely newer than b.
   A stale or older GitHub copy must never claim an update is available - that
   is what left a false "vX.Y.Z is available" nag and a yellow button after an
   update had already landed. */
static BOOL VerNewer(const char *a, const char *b) {
    for (int i = 0; i < 4; i++) {
        int va = 0, vb = 0;
        while (*a >= '0' && *a <= '9') va = va * 10 + (*a++ - '0');
        while (*b >= '0' && *b <= '9') vb = vb * 10 + (*b++ - '0');
        if (va != vb) return va > vb;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return FALSE;
}

/* Resolve directory from an existing .lnk shortcut if the user previously installed elsewhere */
static BOOL ResolveShortcutTargetDir(const wchar_t *lnkPath, wchar_t *outDir) {
    if (!FileExistsW(lnkPath)) return FALSE;
    IShellLinkW *psl = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&psl)) || !psl)
        return FALSE;

    BOOL found = FALSE;
    IPersistFile *ppf = NULL;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        if (SUCCEEDED(IPersistFile_Load(ppf, lnkPath, STGM_READ))) {
            wchar_t target[MAX_PATH] = L"";
            if (SUCCEEDED(IShellLinkW_GetPath(psl, target, MAX_PATH, NULL, 0)) && target[0]) {
                wchar_t *sl = wcsrchr(target, L'\\');
                if (sl) {
                    *sl = L'\0';
                    if (DirExistsW(target)) {
                        wcsncpy(outDir, target, MAX_PATH - 1);
                        outDir[MAX_PATH - 1] = L'\0';
                        found = TRUE;
                    }
                }
            }
        }
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return found;
}

