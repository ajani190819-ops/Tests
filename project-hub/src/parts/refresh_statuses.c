/* Append a live fact to a card's detail line. The strip under each title is
   the only spare real estate on a card, so it carries what actually changes:
   how long the app has been running, and which released build the up-to-date
   verdict was made against. */
static void DetailAppend(int idx, const wchar_t *tail) {
    size_t cur = wcslen(g_cards[idx].detail);
    if (cur < 200) wcsncat(g_cards[idx].detail, tail, 255 - cur);
}

static void DetailAppendUp(int idx, const wchar_t *lockPath) {
    HANDLE hf = CreateFileW(lockPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return;
    FILETIME ft;
    BOOL ok = GetFileTime(hf, NULL, NULL, &ft);
    CloseHandle(hf);
    if (!ok) return;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULONGLONG a = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
    ULONGLONG b = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    if (a <= b) return;
    ULONGLONG mins = (a - b) / 600000000ULL;
    wchar_t tail[48];
    if (mins >= 60)
        _snwprintf(tail, 47, L"  \x00B7  up %llum %02llum", mins / 60, mins % 60);
    else
        _snwprintf(tail, 47, L"  \x00B7  up %llum", mins);
    tail[47] = L'\0';
    DetailAppend(idx, tail);
}

static void DetailAppendBuild(int idx, BOOL upToDate) {
    if (!upToDate || !g_cards[idx].remSha[0]) return;
    wchar_t tail[48];
    _snwprintf(tail, 47, L"  \x00B7  build %.7S", g_cards[idx].remSha);
    tail[47] = L'\0';
    DetailAppend(idx, tail);
}

static void RefreshProjectStatuses(void) {
    char remHub[32], remWave[32], remInfill[32];
    wchar_t remHud[128];
    DWORD remHudSz = 0;

    EnterCriticalSection(&g_cs);
    strncpy(remHub, g_remHubVer, 31); remHub[31] = '\0';
    strncpy(remWave, g_remWaveVer, 31); remWave[31] = '\0';
    strncpy(remInfill, g_remInfillVer, 31); remInfill[31] = '\0';
    wcsncpy(remHud, g_remHudName, 127); remHud[127] = L'\0';
    remHudSz = g_remHudSize;
    LeaveCriticalSection(&g_cs);

    DWORD remBtgSz, remKblSz, remPs1Sz, remDnsSz;
    EnterCriticalSection(&g_cs);
    remBtgSz = g_remBtgSize; remKblSz = g_remKblSize; remPs1Sz = g_remKblPs1Size;
    remDnsSz = g_remDnsSize;
    LeaveCriticalSection(&g_cs);
    /* Each app is installed into the suite folder; until the first update
       lands there, the copy the hub found at startup is the one to measure. */
    const wchar_t *btgDir = g_cards[0].appDir[0] ? g_cards[0].appDir : g_btgDir;
    const wchar_t *kblDir = g_cards[1].appDir[0] ? g_cards[1].appDir : g_kblDir;
    wchar_t btgExeP[MAX_PATH], kblExeP2[MAX_PATH], kblPs1P[MAX_PATH], altP[MAX_PATH];
    _snwprintf(btgExeP, MAX_PATH, L"%s\\BluetoothGuard.exe", btgDir);
    _snwprintf(altP, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
    if (!FileExistsW(btgExeP) && FileExistsW(altP)) wcsncpy(btgExeP, altP, MAX_PATH - 1);
    _snwprintf(kblExeP2, MAX_PATH, L"%s\\KeyboardLighting.exe", kblDir);
    _snwprintf(altP, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    if (!FileExistsW(kblExeP2) && FileExistsW(altP)) wcsncpy(kblExeP2, altP, MAX_PATH - 1);
    _snwprintf(kblPs1P, MAX_PATH, L"%s\\Aura-Background.ps1", kblDir);
    _snwprintf(altP, MAX_PATH, L"%s\\Aura-Background.ps1", g_kblDir);
    if (!FileExistsW(kblPs1P) && FileExistsW(altP)) wcsncpy(kblPs1P, altP, MAX_PATH - 1);
    btgExeP[MAX_PATH - 1] = L'\0'; kblExeP2[MAX_PATH - 1] = L'\0'; kblPs1P[MAX_PATH - 1] = L'\0';
    DWORD locBtgSz = LocalSizeOf(btgExeP);
    const wchar_t *btgLocal = btgExeP;
    if (!locBtgSz && g_btgExtraExe[0]) { locBtgSz = LocalSizeOf(g_btgExtraExe); btgLocal = g_btgExtraExe; }
    BOOL btgKnown = (locBtgSz > 0 && RemoteIdentityKnown(g_remBtgSha, remBtgSz));
    BOOL btgUpd = btgKnown && RemoteDiffers(g_remBtgSha, btgLocal, remBtgSz);
    DWORD locKblSz = LocalSizeOf(kblExeP2);
    DWORD locKblPs1Sz = LocalSizeOf(kblPs1P);
    BOOL kblExeKnown = (locKblSz > 0 && RemoteIdentityKnown(g_remKblSha, remKblSz));
    BOOL kblPs1Known = (locKblPs1Sz > 0 && RemoteIdentityKnown(g_remKblPs1Sha, remPs1Sz));
    BOOL kblKnown = kblExeKnown || kblPs1Known;
    BOOL kblUpd = (kblExeKnown && RemoteDiffers(g_remKblSha, kblExeP2, remKblSz)) ||
                  (kblPs1Known && RemoteDiffers(g_remKblPs1Sha, kblPs1P, remPs1Sz));

    /* The Hub action is not a permanent navigation button. It becomes
       visible only after a completed remote check proves that a newer Hub
       build exists; Update All stays available independently. */
    g_hubUpdAvail = (remHub[0] != '\0' && VerNewer(remHub, HUB_VERSION));
    g_topBtns[1].label = L"Update Hub \x25B2";
    g_topBtns[1].style = BTN_AMBER;
    g_topBtns[1].hidden = !g_hubUpdAvail;

    /* Recompute every button style from a neutral base each refresh. A
       button that turned amber for an update must go back to normal the
       moment the update lands, not stay yellow until the hub restarts. */
    for (int i = 0; i < g_cardCount; i++)
        for (int b = 0; b < g_cards[i].btnCount && b < 4; b++)
            g_cards[i].btns[b].style = BTN_SECONDARY;

    wchar_t path[MAX_PATH];

    /* 1. Bluetooth Guard */
    wcsncpy(path, btgExeP, MAX_PATH - 1);
    path[MAX_PATH - 1] = L'\0';
    BOOL hasBtg = FileHealthyW(path) || (g_btgExtraExe[0] && FileHealthyW(g_btgExtraExe));
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    int blocked = 0, total = 0;
    int btgState = CheckPidLockStatus(btgLock, 12.0, &blocked, &total);
    if (btgState == 1) {
        wcsncpy(g_cards[0].badge, L"Active", 47);
        g_cards[0].badgeKind = BADGE_OK;
        g_cards[0].btns[1].label = L"Open";
        g_cards[0].btns[1].style = BTN_SECONDARY;
        g_cards[0].btns[1].hidden = FALSE;
        _snwprintf(g_cards[0].detail, 255,
                   L"Phone audio protection  \x00B7  %d guarded (%d paired)", blocked, total);
    } else if (hasBtg) {
        wcsncpy(g_cards[0].badge, L"Standby", 47);
        g_cards[0].badgeKind = BADGE_IDLE;
        g_cards[0].btns[1].label = L"Start";
        g_cards[0].btns[1].style = BTN_PRIMARY;
        g_cards[0].btns[1].hidden = FALSE;
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Start to launch", 255);
    } else {
        wcsncpy(g_cards[0].badge, L"Setup Ready", 47);
        g_cards[0].badgeKind = BADGE_WARN;
        g_cards[0].btns[1].hidden = TRUE;   /* nothing to open until installed */
        wcsncpy(g_cards[0].detail, L"Phone audio protection  \x00B7  Click Install to set up", 255);
    }
    /* btns[0] is the Update/Install action: Install when missing, Update when a
       newer build is known, hidden once up to date. */
    if (!hasBtg) {
        g_cards[0].btns[0].label = L"Install";
        g_cards[0].btns[0].style = BTN_PRIMARY;
        g_cards[0].btns[0].hidden = FALSE;
    } else if (btgKnown && btgUpd) {
        g_cards[0].btns[0].label = L"Update";
        g_cards[0].btns[0].style = BTN_AMBER;
        g_cards[0].btns[0].hidden = FALSE;
    } else {
        g_cards[0].btns[0].label = L"Update";
        g_cards[0].btns[0].hidden = TRUE;
    }

    /* show BOTH facts whenever both are known: "Active  ·  Up to Date",
       "Active  ·  Update Ready", "Standby  ·  Up to Date", ... */
    if (hasBtg && btgKnown) {
        wcsncat(g_cards[0].badge, btgUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (btgUpd && (g_cards[0].badgeKind == BADGE_OK || g_cards[0].badgeKind == BADGE_IDLE))
            g_cards[0].badgeKind = BADGE_WARN;
    }
    if (btgState == 1) DetailAppendUp(0, btgLock);
    DetailAppendBuild(0, hasBtg && btgKnown && !btgUpd);

    /* 2. Keyboard Lighting */
    wchar_t kblExe[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(path, MAX_PATH, L"%s\\Tray.ps1", g_kblDir);
    BOOL hasKbl = FileHealthyW(kblExe) || FileHealthyW(path);
    wchar_t kblLock[MAX_PATH];
    _snwprintf(kblLock, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    int kblState = CheckPidLockStatus(kblLock, 12.0, NULL, NULL);
    if (kblState == 2) {
        wcsncpy(g_cards[1].badge, L"Frozen", 47);
        g_cards[1].badgeKind = BADGE_DANGER;
        g_cards[1].btns[3].style = BTN_AMBER;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Unfreeze to restart", 255);
    } else if (kblState == 1) {
        wcsncpy(g_cards[1].badge, L"Active", 47);
        g_cards[1].badgeKind = BADGE_OK;
        g_cards[1].btns[3].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting", 255);
    } else if (hasKbl) {
        wcsncpy(g_cards[1].badge, L"Standby", 47);
        g_cards[1].badgeKind = BADGE_IDLE;
        g_cards[1].btns[3].style = BTN_SECONDARY;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Installed locally", 255);
    } else {
        wcsncpy(g_cards[1].badge, L"Setup Ready", 47);
        g_cards[1].badgeKind = BADGE_WARN;
        wcsncpy(g_cards[1].detail, L"ROG Strix G16 RGB lighting  \x00B7  Click Install to set up", 255);
    }
    /* Open (btns[1]) and Unfreeze (btns[3]) only make sense once installed. */
    g_cards[1].btns[1].hidden = !hasKbl;
    g_cards[1].btns[3].hidden = !hasKbl;
    /* btns[0] is the Update/Install action. */
    if (!hasKbl) {
        g_cards[1].btns[0].label = L"Install";
        g_cards[1].btns[0].style = BTN_PRIMARY;
        g_cards[1].btns[0].hidden = FALSE;
    } else if (kblKnown && kblUpd) {
        g_cards[1].btns[0].label = L"Update";
        g_cards[1].btns[0].style = BTN_AMBER;
        g_cards[1].btns[0].hidden = FALSE;
    } else {
        g_cards[1].btns[0].label = L"Update";
        g_cards[1].btns[0].hidden = TRUE;
    }

    if (kblState == 1) DetailAppendUp(1, kblLock);
    DetailAppendBuild(1, hasKbl && kblKnown && !kblUpd);
    if (hasKbl && kblKnown) {
        wcsncat(g_cards[1].badge, kblUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (kblUpd && (g_cards[1].badgeKind == BADGE_OK || g_cards[1].badgeKind == BADGE_IDLE))
            g_cards[1].badgeKind = BADGE_WARN;
    }

    /* 3. Spatial HUD */
    if (g_hudDisabled) {
        wcsncpy(g_cards[2].badge, L"Disabled", 47);
        g_cards[2].badgeKind = BADGE_IDLE;
        g_cards[2].btns[0].style = BTN_SECONDARY;
        g_cards[2].btns[0].hidden = TRUE;   /* disabled: nothing to update */
        for (int b = 0; b < g_cards[2].btnCount; b++)
            if (g_cards[2].btns[b].id == ID_HUD_DISABLE)
                g_cards[2].btns[b].label = L"Enable";
        wcsncpy(g_cards[2].detail,
                L"Minecraft 26.3 Fabric mod  \x00B7  Disabled - will not install or update", 255);
    } else {
        for (int b = 0; b < g_cards[2].btnCount; b++)
            if (g_cards[2].btns[b].id == ID_HUD_DISABLE)
                g_cards[2].btns[b].label = L"Disable";
        wchar_t searchPat[MAX_PATH], foundJar[128] = L"";
        DWORD foundSize = 0;
        _snwprintf(searchPat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
        WIN32_FIND_DATAW fd;
        HANDLE hf = FindFirstFileW(searchPat, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            wcsncpy(foundJar, fd.cFileName, 127);
            foundSize = fd.nFileSizeLow;
            FindClose(hf);
        }
        if (!foundJar[0]) {
            wcsncpy(g_cards[2].badge, L"Setup Ready", 47);
            g_cards[2].badgeKind = BADGE_WARN;
            g_cards[2].btns[0].label = L"Install";
            g_cards[2].btns[0].style = BTN_PRIMARY;
            g_cards[2].btns[0].hidden = FALSE;   /* not installed: Install */
            wcsncpy(g_cards[2].detail, L"Minecraft 26.3 Fabric mod  \x00B7  Click Install to set up", 255);
        } else if (remHudSz > 0 && foundSize != remHudSz) {
            wcsncpy(g_cards[2].badge, L"Update Ready", 47);
            g_cards[2].badgeKind = BADGE_WARN;
            g_cards[2].btns[0].label = L"Update";
            g_cards[2].btns[0].style = BTN_PRIMARY;
            g_cards[2].btns[0].hidden = FALSE;   /* newer build available */
            _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  Newer build (%s)", remHud);
        } else {
            wcsncpy(g_cards[2].badge, L"Up to Date", 47);
            g_cards[2].badgeKind = BADGE_OK;
            g_cards[2].btns[0].label = L"Update";
            g_cards[2].btns[0].style = BTN_SECONDARY;
            g_cards[2].btns[0].hidden = TRUE;    /* up to date: hide Update */
            _snwprintf(g_cards[2].detail, 255, L"Minecraft 26.3 Fabric mod  \x00B7  %s", foundJar);
        }


    }
    /* Independent plugin state includes both current and legacy files in
       every OrcaSlicer profile, not only the default profile. */
    RefreshOrcaCard(0, remWave);
    RefreshOrcaCard(1, remInfill);

    /* 5. DNS Pilot */
    wchar_t dnsExe[MAX_PATH];
    _snwprintf(dnsExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
    BOOL hasDns = FileHealthyW(dnsExe);
    DWORD locDnsSz = LocalSizeOf(dnsExe);
    BOOL dnsKnown = (locDnsSz > 0 && RemoteIdentityKnown(g_remDnsSha, remDnsSz));
    BOOL dnsUpd = hasDns && dnsKnown && RemoteDiffers(g_remDnsSha, dnsExe, remDnsSz);
    BOOL dnsRunning = (FindWindowW(L"KBLSuiteDnsPilotWindow", NULL) != NULL);
    if (!hasDns) {
        wcsncpy(g_cards[4].badge, L"Setup Ready", 47);
        g_cards[4].badgeKind = BADGE_WARN;
        g_cards[4].btns[1].hidden = TRUE;   /* nothing to open until installed */
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Click Install to set up", 255);
    } else if (dnsRunning) {
        wcsncpy(g_cards[4].badge, L"Active", 47);
        g_cards[4].badgeKind = BADGE_OK;
        g_cards[4].btns[1].label = L"Open";
        g_cards[4].btns[1].style = BTN_SECONDARY;
        g_cards[4].btns[1].hidden = FALSE;
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Running (window or tray)", 255);
    } else {
        wcsncpy(g_cards[4].badge, L"Standby", 47);
        g_cards[4].badgeKind = BADGE_IDLE;
        g_cards[4].btns[1].label = L"Open";
        g_cards[4].btns[1].style = BTN_SECONDARY;
        g_cards[4].btns[1].hidden = FALSE;
        wcsncpy(g_cards[4].detail, L"Fast DNS benchmark & auto-pilot  \x00B7  Installed locally", 255);
    }
    /* btns[0] is the Update/Install action. */
    if (!hasDns) {
        g_cards[4].btns[0].label = L"Install";
        g_cards[4].btns[0].style = BTN_PRIMARY;
        g_cards[4].btns[0].hidden = FALSE;
    } else if (dnsKnown && dnsUpd) {
        g_cards[4].btns[0].label = L"Update";
        g_cards[4].btns[0].style = BTN_AMBER;
        g_cards[4].btns[0].hidden = FALSE;
    } else {
        g_cards[4].btns[0].label = L"Update";
        g_cards[4].btns[0].hidden = TRUE;
    }
    DetailAppendBuild(4, hasDns && dnsKnown && !dnsUpd);
    if (hasDns && dnsKnown) {
        wcsncat(g_cards[4].badge, dnsUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
        if (dnsUpd && (g_cards[4].badgeKind == BADGE_OK || g_cards[4].badgeKind == BADGE_IDLE))
            g_cards[4].badgeKind = BADGE_WARN;
    }

    /* 6. Agent Notes */
    {
        DWORD remNotesSz = g_remNotesSize;
        wchar_t notesExe[MAX_PATH];
        _snwprintf(notesExe, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
        BOOL hasNotes = FileHealthyW(notesExe);
        DWORD locNotesSz = LocalSizeOf(notesExe);
        BOOL notesKnown = (locNotesSz > 0 && RemoteIdentityKnown(g_remNotesSha, remNotesSz));
        BOOL notesUpd = hasNotes && notesKnown && RemoteDiffers(g_remNotesSha, notesExe, remNotesSz);
        if (!hasNotes) {
            wcsncpy(g_cards[5].badge, L"Setup Ready", 47);
            g_cards[5].badgeKind = BADGE_WARN;
            g_cards[5].btns[1].hidden = TRUE;   /* nothing to open until installed */
            wcsncpy(g_cards[5].detail, L"Quick notes straight to the agent  \x00B7  Click Install to set up", 255);
        } else {
            wcsncpy(g_cards[5].badge, L"Ready", 47);
            g_cards[5].badgeKind = BADGE_OK;
            g_cards[5].btns[1].label = L"Open";
            g_cards[5].btns[1].style = BTN_SECONDARY;
            g_cards[5].btns[1].hidden = FALSE;
            wcsncpy(g_cards[5].detail, L"Quick notes straight to the agent  \x00B7  Installed locally", 255);
        }
        /* btns[0] is the Update/Install action. */
        if (!hasNotes) {
            g_cards[5].btns[0].label = L"Install";
            g_cards[5].btns[0].style = BTN_PRIMARY;
            g_cards[5].btns[0].hidden = FALSE;
        } else if (notesKnown && notesUpd) {
            g_cards[5].btns[0].label = L"Update";
            g_cards[5].btns[0].style = BTN_AMBER;
            g_cards[5].btns[0].hidden = FALSE;
        } else {
            g_cards[5].btns[0].label = L"Update";
            g_cards[5].btns[0].hidden = TRUE;
        }
        if (hasNotes && notesKnown) {
            wcsncat(g_cards[5].badge, notesUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (notesUpd && (g_cards[5].badgeKind == BADGE_OK || g_cards[5].badgeKind == BADGE_IDLE))
                g_cards[5].badgeKind = BADGE_WARN;
        }
    }

    /* 7. Audio Switcher */
    {
        DWORD remAsSz = g_remAudioSwitcherSize;
        /* The suite folder is the install target; until the first update lands
           there, report on the copy the hub actually found so the badge, Open
           and Install all describe the same app. */
        const wchar_t *asDir = g_cards[6].appDir[0] ? g_cards[6].appDir : g_audioSwitcherDir;
        wchar_t asExe[MAX_PATH], asAlt[MAX_PATH];
        _snwprintf(asExe, MAX_PATH, L"%s\\AudioSwitcher.exe", asDir);
        _snwprintf(asAlt, MAX_PATH, L"%s\\AudioSwitcher.exe", g_audioSwitcherDir);
        if (!FileExistsW(asExe) && FileExistsW(asAlt)) {
            wcsncpy(asExe, asAlt, MAX_PATH - 1);
            asExe[MAX_PATH - 1] = L'\0';
        }
        BOOL hasAs = FileHealthyW(asExe);
        DWORD locAsSz = LocalSizeOf(asExe);
        BOOL asKnown = (locAsSz > 0 && RemoteIdentityKnown(g_remAudioSwitcherSha, remAsSz));
        BOOL asUpd = hasAs && asKnown && RemoteDiffers(g_remAudioSwitcherSha, asExe, remAsSz);
        BOOL asRunning = (FindWindowW(L"TestsAudioSwitcherWnd", NULL) != NULL);
        if (!hasAs) {
            wcsncpy(g_cards[6].badge, L"Setup Ready", 47);
            g_cards[6].badgeKind = BADGE_WARN;
            g_cards[6].btns[1].hidden = TRUE;   /* nothing to open until installed */
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Click Install to set up", 255);
        } else if (asRunning) {
            wcsncpy(g_cards[6].badge, L"Active", 47);
            g_cards[6].badgeKind = BADGE_OK;
            g_cards[6].btns[1].label = L"Open";
            g_cards[6].btns[1].style = BTN_SECONDARY;
            g_cards[6].btns[1].hidden = FALSE;
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Running (window or tray)", 255);
            AppendInstallFolder(g_cards[6].detail, asDir);
        } else {
            wcsncpy(g_cards[6].badge, L"Standby", 47);
            g_cards[6].badgeKind = BADGE_IDLE;
            g_cards[6].btns[1].label = L"Open";
            g_cards[6].btns[1].style = BTN_SECONDARY;
            g_cards[6].btns[1].hidden = FALSE;
            wcsncpy(g_cards[6].detail, L"Per-app volume & audio output switching  \x00B7  Installed locally", 255);
            AppendInstallFolder(g_cards[6].detail, asDir);
        }
        /* btns[0] is the Update/Install action. */
        if (!hasAs) {
            g_cards[6].btns[0].label = L"Install";
            g_cards[6].btns[0].style = BTN_PRIMARY;
            g_cards[6].btns[0].hidden = FALSE;
        } else if (asKnown && asUpd) {
            g_cards[6].btns[0].label = L"Update";
            g_cards[6].btns[0].style = BTN_AMBER;
            g_cards[6].btns[0].hidden = FALSE;
        } else {
            g_cards[6].btns[0].label = L"Update";
            g_cards[6].btns[0].hidden = TRUE;
        }
        if (hasAs && asKnown) {
            wcsncat(g_cards[6].badge, asUpd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (asUpd && (g_cards[6].badgeKind == BADGE_OK || g_cards[6].badgeKind == BADGE_IDLE))
                g_cards[6].badgeKind = BADGE_WARN;
        }
    }

    /* Catalogue extras: generic installed / update state. Dynamic cards
       must follow the same rule as built-ins: Install while missing, Update
       only for a verified newer remote build, otherwise hide that action. */
    for (int i = BUILTIN_CARD_COUNT; i < g_cardCount; i++) {
        ProjectCard *c = &g_cards[i];
        wchar_t exePath[MAX_PATH], altPath[MAX_PATH];
        _snwprintf(exePath, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
        exePath[MAX_PATH - 1] = L'\0';
        /* a stray copy the hub found at startup still counts as installed */
        _snwprintf(altPath, MAX_PATH - 1, L"%s\\%s", g_exeDir, c->exeName);
        altPath[MAX_PATH - 1] = L'\0';
        if (!FileExistsW(exePath) && FileExistsW(altPath))
            wcsncpy(exePath, altPath, MAX_PATH - 1);
        BOOL has = FileHealthyW(exePath);
        DWORD locSz = LocalSizeOf(exePath);
        BOOL known = (locSz > 0 && RemoteIdentityKnown(c->remSha, c->remSize));
        BOOL upd = has && known && RemoteDiffers(c->remSha, exePath, c->remSize);
        int action = -1, open = -1;
        for (int b = 0; b < c->btnCount; b++) {
            const wchar_t *verb = c->btns[b].verb;
            if (!verb) continue;
            if (action < 0 &&
                (wcscmp(verb, L"install") == 0 || wcscmp(verb, L"update") == 0))
                action = b;
            if (open < 0 && wcscmp(verb, L"open") == 0) open = b;
        }
        if (!has) {
            wcsncpy(c->badge, L"Setup Ready", 47);
            c->badgeKind = BADGE_WARN;
            _snwprintf(c->detail, 255, L"%s  \x00B7  Click Install to set up",
                       c->blurb ? c->blurb : L"Suite app");
        } else {
            wcsncpy(c->badge, L"Ready", 47);
            c->badgeKind = BADGE_OK;
            _snwprintf(c->detail, 255, L"%s  \x00B7  Installed locally",
                       c->blurb ? c->blurb : L"Suite app");
            AppendInstallFolder(c->detail, c->appDir);
        }
        if (action >= 0) {
            BOOL showAction = !has || (known && upd);
            c->btns[action].label = !has ? L"Install" : L"Update";
            c->btns[action].style = !has ? BTN_PRIMARY : (upd ? BTN_AMBER : BTN_SECONDARY);
            c->btns[action].hidden = !showAction;
        }
        if (open >= 0) c->btns[open].hidden = !has;
        if (has && known) {
            wcsncat(c->badge, upd ? L"  \x00B7  Update Ready" : L"  \x00B7  Up to Date", 47);
            if (upd) c->badgeKind = BADGE_WARN;
        }
        c->badge[47] = L'\0';
    }

    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

