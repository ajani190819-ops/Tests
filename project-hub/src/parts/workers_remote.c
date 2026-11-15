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

static void WorkerCheckRemote(void) {
    wchar_t branch[64];
    SetCheckPhase(TRUE);
    EnsureActiveBranch(branch, 64);

    /* The released marker is the ONLY read taken from the branch tip. Every
       other probe below uses the commit the marker blesses, so half-finished
       work is invisible and the owner hears about an update once, when a
       task is done. No marker (other people's branches, offline) falls back
       to today's behaviour. */
    DWORD msz = 0; int mst = 0;
    wchar_t murl[256];
    _snwprintf(murl, 255, L"/%s/%s/%s/.agent/RELEASED?t=%llu", REPO_OWNER_W, REPO_NAME_W, branch,
               (unsigned long long)(GetTickCount64() / 60000ULL));
    char *marker = (char *)HttpGetCached(L"raw.githubusercontent.com", murl, &msz, &mst);
    int hubFromMarker = 0;
    if (marker && mst == 200) {
        char sha[48] = "";
        if (ExtractLineAfter(marker, "commit=", sha, sizeof(sha))) {
            int oksha = strlen(sha) >= 7;
            for (int i = 0; sha[i]; i++) {
                char c = sha[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) oksha = 0;
            }
            if (oksha) _snwprintf(g_probeRef, 63, L"%S", sha);
        }
        char hv[32] = "";
        if (ExtractLineAfter(marker, "hub=", hv, sizeof(hv))) {
            EnterCriticalSection(&g_cs);
            strncpy(g_remHubVer, hv, 31);
            LeaveCriticalSection(&g_cs);
            hubFromMarker = 1;
        }
    }
    free(marker);
    if (!g_probeRef[0]) _snwprintf(g_probeRef, 63, L"%s", branch);

    wchar_t p[256];
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

    _snwprintf(p, 255, L"/%s/%s/%s/plugins.json", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
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
    _snwprintf(p, 255, L"/%s/%s/%s/suite-apps.json?t=%llu", REPO_OWNER_W, REPO_NAME_W, g_probeRef,
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

    /* Remote content identity, fetched once for every path below. */
    char *treeJson = NULL;
    {
        wchar_t tp[512];
        DWORD tsz = 0;
        int tst = 0;
        _snwprintf(tp, 511, L"/repos/%s/%s/git/trees/%s?recursive=1",
                   REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        tp[511] = L'\0';
        treeJson = (char *)HttpGetBuffer(L"api.github.com", tp, &tsz, &tst);
        if (!(treeJson && tst == 200)) { free(treeJson); treeJson = NULL; }
    }

    /* Remote build sizes: kept as the fallback identity when the trees API is
       unreachable (rate limit, offline). */
    {
        DWORD szBtg = 0, szKbl = 0, szPs1 = 0, szDns = 0, szNotes = 0, szAs = 0;
        _snwprintf(p, 255, L"/%s/%s/%s/bluetooth-guard/BluetoothGuard.exe", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szBtg);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/KeyboardLighting.exe", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szKbl);
        _snwprintf(p, 255, L"/%s/%s/%s/keyboard-lighting/Aura-Background.ps1", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szPs1);
        _snwprintf(p, 255, L"/%s/%s/%s/dns-pilot/DnsPilot.exe", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szDns);
        _snwprintf(p, 255, L"/%s/%s/%s/agent-notes/AgentNotes.exe", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
        HttpHeadSize(L"raw.githubusercontent.com", p, &szNotes);
        _snwprintf(p, 255, L"/%s/%s/%s/audio-switcher/AudioSwitcher.exe", REPO_OWNER_W, REPO_NAME_W, g_probeRef);
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
            _snwprintf(p, 255, L"/%s/%s/%s/%s", REPO_OWNER_W, REPO_NAME_W, g_probeRef, g_cards[i].exeRepo);
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

    char remHubLocal[32] = "";
    EnterCriticalSection(&g_cs);
    strncpy(remHubLocal, g_remHubVer, 31);
    LeaveCriticalSection(&g_cs);

    SetCheckPhase(FALSE);
    if (remHubLocal[0] && VerNewer(remHubLocal, HUB_VERSION)) {
        wchar_t updMsg[256];
        _snwprintf(updMsg, 255,
                   L"Project Hub v%S is available on GitHub  ·  Click 'Update Hub' above",
                   remHubLocal);
        SetFooterStatus(updMsg, 0);
    } else {
        SetFooterStatus(L"All suite apps verified  ·  GitHub connected", 0);
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

