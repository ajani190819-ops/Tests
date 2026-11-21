/* Read one line's value from a plain key=value file. The released marker
   is two such lines: the commit that holds finished work, and the hub
   version that went with it. */
static int ExtractLineAfter(const char *src, const char *key, char *out, int cap) {
    const char *k = strstr(src, key);
    if (!k) return 0;
    k += strlen(key);
    int i = 0;
    while (k[i] && k[i] != '\r' && k[i] != '\n' && i < cap - 1) { out[i] = k[i]; i++; }
    out[i] = '\0';
    return i > 0;
}

#define TREE_RETRY_MS          300000ULL
#define TREE_MUTABLE_CACHE_MS  300000ULL

/* A released ref is an immutable commit. Keep its tree response in memory:
   polling the Git Trees API every minute burns the unauthenticated API quota,
   after which the old code kept comparing against a stale SHA and ignored its
   size fallback. Mutable refs (branches without a release marker) are refreshed
   conditionally every five minutes. */
static char *g_treeJsonCache = NULL;
static wchar_t g_treeCacheRef[64];
static BOOL g_treeCacheImmutable = FALSE;
static ULONGLONG g_treeCacheAt = 0;
static ULONGLONG g_treeRetryAt = 0;

static char *CopyTreeText(const char *src, DWORD len) {
    char *copy = (char *)malloc((size_t)len + 1);
    if (!copy) return NULL;
    memcpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

static BOOL TreeResponseComplete(const char *json) {
    const char *p = NULL, *scan = json;
    if (!json) return FALSE;
    /* The root flag follows the tree array, so it is the last matching token;
       this avoids treating a file literally named "truncated" as the flag. */
    while ((scan = strstr(scan, "\"truncated\"")) != NULL) {
        p = scan;
        scan += 11;
    }
    if (!p || !(p = strchr(p, ':'))) return FALSE;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return strncmp(p, "false", 5) == 0;
}

static char *FetchRemoteTree(const wchar_t *probeRef, BOOL immutable,
                             DWORD *outSize, int *outStatus) {
    ULONGLONG now = GetTickCount64();
    if (outSize) *outSize = 0;
    if (outStatus) *outStatus = 0;

    if (_wcsicmp(g_treeCacheRef, probeRef) != 0 || g_treeCacheImmutable != immutable) {
        free(g_treeJsonCache);
        g_treeJsonCache = NULL;
        wcsncpy(g_treeCacheRef, probeRef, 63);
        g_treeCacheRef[63] = L'\0';
        g_treeCacheImmutable = immutable;
        g_treeCacheAt = 0;
        g_treeRetryAt = 0;
    }
    if (g_treeJsonCache && immutable) {
        DWORD n = (DWORD)strlen(g_treeJsonCache);
        if (outSize) *outSize = n;
        if (outStatus) *outStatus = 200;
        return CopyTreeText(g_treeJsonCache, n);
    }
    if (g_treeJsonCache && !immutable && now - g_treeCacheAt < TREE_MUTABLE_CACHE_MS) {
        DWORD n = (DWORD)strlen(g_treeJsonCache);
        if (outSize) *outSize = n;
        if (outStatus) *outStatus = 200;
        return CopyTreeText(g_treeJsonCache, n);
    }
    if (g_treeRetryAt && now < g_treeRetryAt) return NULL;

    wchar_t path[512];
    _snwprintf(path, 511, L"/repos/%s/%s/git/trees/%s?recursive=1",
               REPO_OWNER_W, REPO_NAME_W, probeRef);
    path[511] = L'\0';
    DWORD size = 0;
    int status = 0;
    char *data = (char *)(immutable
        ? HttpGetBuffer(L"api.github.com", path, &size, &status)
        : HttpGetCached(L"api.github.com", path, &size, &status));
    if (data && status == 200 && TreeResponseComplete(data)) {
        char *cache = CopyTreeText(data, size);
        if (cache) {
            free(g_treeJsonCache);
            g_treeJsonCache = cache;
            g_treeCacheAt = now;
            g_treeRetryAt = 0;
        } else {
            g_treeRetryAt = now + TREE_RETRY_MS;
        }
        if (outSize) *outSize = size;
        if (outStatus) *outStatus = status;
        return data;
    }
    free(data);
    g_treeRetryAt = now + TREE_RETRY_MS;
    if (outStatus) *outStatus = status;
    return NULL;
}

static void ClearRemoteBuildHashes(void) {
    g_remBtgSha[0] = '\0';
    g_remKblSha[0] = '\0';
    g_remKblPs1Sha[0] = '\0';
    g_remDnsSha[0] = '\0';
    g_remNotesSha[0] = '\0';
    g_remAudioSwitcherSha[0] = '\0';
}

/* Caller holds g_cs. Never leave identities from one selected ref active while
   the next ref is being probed or after a selection changes mid-request. */
static void ClearRemoteProbeResults(void) {
    ClearRemoteBuildHashes();
    g_remBtgSize = g_remKblSize = g_remKblPs1Size = 0;
    g_remDnsSize = g_remNotesSize = g_remAudioSwitcherSize = 0;
    g_remHubVer[0] = '\0';
    g_remWaveVer[0] = '\0';
    g_remInfillVer[0] = '\0';
    g_remHudName[0] = L'\0';
    g_remHudPath[0] = L'\0';
    g_remHudSize = 0;
    for (int i = 6; i < g_cardCount; i++) {
        g_cards[i].remSize = 0;
        g_cards[i].remSha[0] = '\0';
    }
}

static void WorkerCheckRemote(void) {
    wchar_t branch[64], probeRef[64] = L"";
    BOOL releasedRef = FALSE;
    char markerHubVer[32] = "";
    SetCheckPhase(TRUE);
    RefreshAutomaticBranch();
    EnsureActiveBranch(branch, 64);

    /* Resolve this check to one snapshot. A release marker names an immutable
       commit; when it is unavailable, deliberately fall back to the selected
       branch tip rather than retaining a previous check's commit SHA. */
    DWORD msz = 0; int mst = 0;
    wchar_t murl[256];
    _snwprintf(murl, 255, L"/%s/%s/%s/.agent/RELEASED?t=%llu", REPO_OWNER_W, REPO_NAME_W, branch,
               (unsigned long long)(GetTickCount64() / 60000ULL));
    char *marker = (char *)HttpGetCached(L"raw.githubusercontent.com", murl, &msz, &mst);
    int hubFromMarker = 0;
    if (marker && mst == 200) {
        char sha[48] = "";
        if (ExtractLineAfter(marker, "commit=", sha, sizeof(sha))) {
            int oksha = strlen(sha) == 40;
            for (int i = 0; sha[i]; i++) {
                char c = sha[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) oksha = 0;
            }
            if (oksha) {
                _snwprintf(probeRef, 63, L"%S", sha);
                releasedRef = TRUE;
            }
        }
        if (ExtractLineAfter(marker, "hub=", markerHubVer, sizeof(markerHubVer)))
            hubFromMarker = 1;
    }
    free(marker);
    if (!probeRef[0]) _snwprintf(probeRef, 63, L"%s", branch);

    /* Branch-menu changes can happen while this worker is downloading. Use
       the captured ref for every request in this pass, and don't overwrite a
       newer selection when the pass finishes. */
    EnterCriticalSection(&g_cs);
    BOOL refChanged = _wcsicmp(g_probeRef, probeRef) != 0;
    if (refChanged) ClearRemoteProbeResults();
    if (_wcsicmp(g_activeBranch, branch) == 0) {
        wcsncpy(g_probeRef, probeRef, 63);
        g_probeRef[63] = L'\0';
    }
    if (markerHubVer[0]) {
        strncpy(g_remHubVer, markerHubVer, 31);
        g_remHubVer[31] = '\0';
    }
    LeaveCriticalSection(&g_cs);

    wchar_t p[512];
    DWORD sz = 0; int st = 0;

    if (!hubFromMarker) {
        /* Fallback only: a marker without a hub line. Reading the version
           from branch-tip source is what made the hub announce half-finished
           work, so it must never run when the marker answered. */
        _snwprintf(p, 255, L"/%s/%s/%s/project-hub/src/project_hub.c", REPO_OWNER_W, REPO_NAME_W, branch);
        char *hubSrc = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
        if (hubSrc && st == 200) {
            char hv[32] = "";
            if (ExtractQuotedAfter(hubSrc, "#define HUB_VERSION", hv, sizeof(hv))) {
                EnterCriticalSection(&g_cs);
                strncpy(g_remHubVer, hv, 31);
                LeaveCriticalSection(&g_cs);
            }
        }
        free(hubSrc);
    }

    _snwprintf(p, 255, L"/%s/%s/%s/plugins.json", REPO_OWNER_W, REPO_NAME_W, probeRef);
    char *manifest = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
    if (manifest && st == 200) {
        char wv[32] = "", iv[32] = "";
        const char *wpos = strstr(manifest, "\"wave-overhangs\"");
        if (wpos) ExtractQuotedAfter(wpos, "\"version\"", wv, sizeof(wv));
        const char *ipos = strstr(manifest, "\"unlayered-infill\"");
        if (ipos) ExtractQuotedAfter(ipos, "\"version\"", iv, sizeof(iv));
        EnterCriticalSection(&g_cs);
        if (wv[0]) strncpy(g_remWaveVer, wv, 31);
        if (iv[0]) strncpy(g_remInfillVer, iv, 31);
        LeaveCriticalSection(&g_cs);
    }
    free(manifest);
    /* check continues - footer dot keeps pulsing, no step churn */

    /* Dynamic app catalog: new suite apps appear on refresh with no hub
       update needed. Built-in card ids are skipped here. The minute-
       granularity cache buster keeps the raw.githubusercontent CDN from
       serving stale copies of the catalog. */
    _snwprintf(p, 255, L"/%s/%s/%s/suite-apps.json?t=%llu", REPO_OWNER_W, REPO_NAME_W, probeRef,
               (unsigned long long)(GetTickCount64() / 60000ULL));
    char *catalog = (char *)HttpGetCached(L"raw.githubusercontent.com", p, &sz, &st);
    if (catalog && st == 200) {
        RebuildExtraCards(catalog);
        EnterCriticalSection(&g_cs);
        free(g_catalogJson);
        g_catalogJson = catalog;
        LeaveCriticalSection(&g_cs);
        catalog = NULL;
    }
    free(catalog);

    /* Remote content identity is immutable for a released commit. Fetch once
       per probe ref and reuse it; a failed tree request clears the old hashes
       so the raw-file sizes below are a real fallback, not masked by stale SHA. */
    char *treeJson = FetchRemoteTree(probeRef, releasedRef, NULL, NULL);
    if (!treeJson) {
        EnterCriticalSection(&g_cs);
        ClearRemoteBuildHashes();
        LeaveCriticalSection(&g_cs);
    }

    /* Remote build sizes: kept as the fallback identity when the trees API is
       unreachable (rate limit, offline). */
    {
        DWORD szBtg = 0, szKbl = 0, szPs1 = 0, szDns = 0, szNotes = 0, szAs = 0;
        _snwprintf(p, 255, L"/%s/%s/%s/bluetooth-guard/BluetoothGuard.exe", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szBtg);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/KeyboardLighting.exe", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szKbl);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/Aura-Background.ps1", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szPs1);
        _snwprintf(p, 255, L"/%s/%s/%s/dns-pilot/DnsPilot.exe", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szDns);
        _snwprintf(p, 255, L"/%s/%s/%s/agent-notes/AgentNotes.exe", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szNotes);
        _snwprintf(p, 255, L"/%s/%s/%s/audio-switcher/AudioSwitcher.exe", REPO_OWNER_W, REPO_NAME_W, probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szAs);
        /* Always record the result: a failed HEAD means "unknown" (0), never
           "keep the old number". Stale cached sizes are what left Update Ready
           badges stuck yellow after an update already said up to date. */
        EnterCriticalSection(&g_cs);
        g_remBtgSize = szBtg;
        g_remKblSize = szKbl;
        g_remKblPs1Size = szPs1;
        g_remDnsSize = szDns;
        g_remNotesSize = szNotes;
        g_remAudioSwitcherSize = szAs;
        if (treeJson) {
            TreeShaFor(treeJson, "bluetooth-guard/BluetoothGuard.exe", g_remBtgSha);
            TreeShaFor(treeJson, "keyboard-lighting/KeyboardLighting.exe", g_remKblSha);
            TreeShaFor(treeJson, "keyboard-lighting/Aura-Background.ps1", g_remKblPs1Sha);
            TreeShaFor(treeJson, "dns-pilot/DnsPilot.exe", g_remDnsSha);
            TreeShaFor(treeJson, "agent-notes/AgentNotes.exe", g_remNotesSha);
            TreeShaFor(treeJson, "audio-switcher/AudioSwitcher.exe", g_remAudioSwitcherSha);
        }
        LeaveCriticalSection(&g_cs);
    }
    /* generic catalog extras: remote size of each app's main exe */
    for (int i = 6; i < g_cardCount; i++) {
        DWORD szX = 0;
        if (g_cards[i].exeRepo) {
            _snwprintf(p, 255, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, probeRef, g_cards[i].exeRepo);
            HttpHeadSize(L"raw.githubusercontent.com", p, &szX);
        }
        g_cards[i].remSize = szX;
        g_cards[i].remSha[0] = '\0';
        if (treeJson && g_cards[i].exeRepo) {
            char rp[256];
            WideCharToMultiByte(CP_UTF8, 0, g_cards[i].exeRepo, -1, rp, 256, NULL, NULL);
            TreeShaFor(treeJson, rp, g_cards[i].remSha);
        }
    }
    free(treeJson);

    /* The mod jar is published per branch: CI republishes
       spatial-hud-build-<branch> on every successful push, while
       spatial-hud-latest is only main's curated pointer. Querying only the
       latter made a branch's jar invisible no matter which branch was
       selected - a branch carrying mod fixes still reported the mod up to
       date. Follow the selected branch first, fall back to the pointer. */
    wchar_t tagBranch[96];
    {
        int ti = 0;
        const wchar_t *pre = L"spatial-hud-build-";
        for (int i = 0; pre[i] && ti < 95; i++) tagBranch[ti++] = pre[i];
        for (int i = 0; branch[i] && ti < 95; i++)
            tagBranch[ti++] = (branch[i] == L'/') ? L'-' : branch[i];
        tagBranch[ti] = L'\0';
    }
    const wchar_t *hudTags[2] = { tagBranch, L"spatial-hud-latest" };
    EnterCriticalSection(&g_cs);
    g_remHudSize = 0;   /* unknown until this lookup succeeds - never stale */
    LeaveCriticalSection(&g_cs);
    char *rel = NULL;
    for (int t = 0; t < 2; t++) {
        _snwprintf(p, 255, L"/repos/%s/%s/releases/tags/%s",
                   REPO_OWNER_W, REPO_NAME_W, hudTags[t]);
        free(rel);
        rel = (char *)HttpGetCached(L"api.github.com", p, &sz, &st);
        if (rel && st == 200) break;
    }
    if (rel && st == 200) {
        const char *dlPos = strstr(rel, "\"browser_download_url\"");
        if (dlPos) {
            char urlUtf8[512] = "";
            if (ExtractQuotedAfter(dlPos, "\"browser_download_url\"", urlUtf8, sizeof(urlUtf8)) &&
                strncmp(urlUtf8, "https://github.com/", 19) == 0) {
                const char *slashPath = urlUtf8 + 18;
                const char *baseName = strrchr(urlUtf8, '/');
                DWORD assetSz = 0;
                const char *szPos = strstr(rel, "\"size\"");
                if (szPos) {
                    const char *colon = strchr(szPos, ':');
                    if (colon) assetSz = (DWORD)strtoul(colon + 1, NULL, 10);
                }
                EnterCriticalSection(&g_cs);
                MultiByteToWideChar(CP_UTF8, 0, slashPath, -1, g_remHudPath, 511);
                if (baseName) MultiByteToWideChar(CP_UTF8, 0, baseName + 1, -1, g_remHudName, 127);
                g_remHudSize = assetSz;
                LeaveCriticalSection(&g_cs);
            }
        }
    }
    free(rel);

    BOOL probeStillCurrent;
    EnterCriticalSection(&g_cs);
    probeStillCurrent = (_wcsicmp(g_activeBranch, branch) == 0 &&
                         _wcsicmp(g_probeRef, probeRef) == 0);
    if (!probeStillCurrent) ClearRemoteProbeResults();
    LeaveCriticalSection(&g_cs);
    if (!probeStillCurrent) {
        SetCheckPhase(FALSE);
        if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
        return;
    }

    char remHubLocal[32] = "";
    EnterCriticalSection(&g_cs);
    strncpy(remHubLocal, g_remHubVer, 31);
    remHubLocal[31] = '\0';
    LeaveCriticalSection(&g_cs);

    BOOL allBuildIdsKnown =
        RemoteIdentityKnown(g_remBtgSha, g_remBtgSize) &&
        RemoteIdentityKnown(g_remKblSha, g_remKblSize) &&
        RemoteIdentityKnown(g_remKblPs1Sha, g_remKblPs1Size) &&
        RemoteIdentityKnown(g_remDnsSha, g_remDnsSize) &&
        RemoteIdentityKnown(g_remNotesSha, g_remNotesSize) &&
        RemoteIdentityKnown(g_remAudioSwitcherSha, g_remAudioSwitcherSize);
    for (int i = BUILTIN_CARD_COUNT; i < g_cardCount; i++) {
        if (g_cards[i].exeRepo &&
            !RemoteIdentityKnown(g_cards[i].remSha, g_cards[i].remSize))
            allBuildIdsKnown = FALSE;
    }

    wchar_t refLabel[12] = L"branch tip";
    if (releasedRef) {
        wcsncpy(refLabel, probeRef, 7);
        refLabel[7] = L'\0';
    }
    SetCheckPhase(FALSE);
    if (remHubLocal[0] && VerNewer(remHubLocal, HUB_VERSION)) {
        wchar_t updMsg[256];
        _snwprintf(updMsg, 255,
                   L"Project Hub v%S is available  ·  checked %s @ %s",
                   remHubLocal, branch, refLabel);
        SetFooterStatus(updMsg, 0);
    } else if (!allBuildIdsKnown) {
        wchar_t checkMsg[256];
        _snwprintf(checkMsg, 255,
                   L"App update check incomplete  ·  %s @ %s  ·  retrying",
                   branch, refLabel);
        SetFooterStatus(checkMsg, 0);
    } else {
        wchar_t checkMsg[256];
        _snwprintf(checkMsg, 255,
                   L"App identities checked  ·  %s @ %s",
                   branch, refLabel);
        SetFooterStatus(checkMsg, 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void StopProcessFromLock(const wchar_t *lockPath) {
    char *txt = ReadTextFileUtf8(lockPath, NULL);
    if (txt) {
        unsigned long pid = strtoul(txt, NULL, 10);
        free(txt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
        }
    }
    DeleteFileW(lockPath);
}

