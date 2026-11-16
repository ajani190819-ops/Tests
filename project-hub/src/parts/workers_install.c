static BOOL WorkerInstallBluetoothGuard(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);
    EnsureDirTree(g_btgDir);

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);

    BOOL btgWasVisible = FALSE;
    {
        HWND wB = FindWindowW(L"TestsBluetoothGuardModernWnd", NULL);
        if (wB && IsWindowVisible(wB)) btgWasVisible = TRUE;
    }

    const wchar_t *stopTarget = FileExistsW(destExe) ? destExe
                              : (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe) ? g_btgExtraExe : NULL);
    if (stopTarget) {
        SetFooterStatus(L"Stopping running Bluetooth Guard before update...", 20);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = stopTarget;
        sei.lpParameters = L"--stop";
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2500);
            CloseHandle(sei.hProcess);
        }
        Sleep(300);
    }
    /* --stop is Bluetooth Guard's graceful route.  Verify every suite and
       stray mirror is really gone before the downloader can replace one. */
    if (!CloseManagedExeCopies(MIRROR_BTG, g_btgDir, L"BluetoothGuard.exe",
                               L"Bluetooth Guard", 0))
        return FALSE;

    SetFooterStatus(L"Downloading latest BluetoothGuard.exe from GitHub...", 60);
    if (!DownloadRawRepoFile(branch, L"bluetooth-guard/BluetoothGuard.exe", destExe)) {
        SetFooterStatus(L"Could not download BluetoothGuard.exe from GitHub.", 0);
        return FALSE;
    }

    /* Also keep any previously launched standalone copy (e.g. in Downloads) in sync! */
    if (g_btgExtraExe[0] && _wcsicmp(g_btgExtraExe, destExe) != 0 &&
        NoProcessRunsImage(g_btgExtraExe)) {
        CopyFileW(destExe, g_btgExtraExe, FALSE);
    }
    /* ... and every other copy found at startup, suite folder included. */
    MirrorFileEverywhere(MIRROR_BTG, destExe);

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
        if (RepointDesktopShortcutIfPresent(lnk, destExe, g_btgDir,
                                            L"Bluetooth Guard - Phone audio protection"))
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || btgWasVisible) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_btgDir, SW_SHOWNORMAL);
    } else {
        /* Start high-priority tray guard immediately in background & register FastBoot logon task */
        ShellExecuteW(NULL, L"open", destExe, L"--silent", g_btgDir, SW_HIDE);
    }
    SetFooterStatus(L"Bluetooth Guard updated with new UI & icon!", 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
    return TRUE;
}

static void WorkerInstallNotes(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);
    EnsureDirTree(g_notesDir);

    /* Agent Notes now stays resident when its ordinary window is closed so it
       can watch for a waiting agent. Ask its explicit Exit command instead;
       the common gate applies that command to every registered mirror and
       then proves all image paths are gone before any write. */
    BOOL wasRunning = FALSE;
    HWND hNotes = FindWindowW(L"AgentNotesWnd", NULL);
    if (hNotes) {
        wasRunning = TRUE;
        SetFooterStatus(L"Stopping Agent Notes for a clean update...", 8);
        PostMessageW(hNotes, WM_COMMAND, MAKEWPARAM(AGENT_NOTES_EXIT_COMMAND, 0), 0);
        Sleep(300);
    }
    if (!CloseManagedExeCopies(MIRROR_NOTES, g_notesDir, L"AgentNotes.exe",
                               L"Agent Notes", AGENT_NOTES_EXIT_COMMAND))
        return;

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
    SetFooterStatus(L"Downloading latest AgentNotes.exe from GitHub...", 60);
    if (!InstallRepoFileEverywhere(MIRROR_NOTES, branch, L"agent-notes/AgentNotes.exe",
                                   g_notesDir, L"AgentNotes.exe", NULL)) {
        SetFooterStatus(L"Could not save AgentNotes.exe - close Agent Notes if it is open, then retry.", 0);
        return;
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Agent Notes.lnk", desk);
        if (RepointDesktopShortcutIfPresent(lnk, destExe, g_notesDir,
                                            L"Agent Notes - quick notes for the agent"))
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || wasRunning) {
        ShellExecuteW(NULL, L"open", destExe, NULL, g_notesDir, SW_SHOWNORMAL);
        SetFooterStatus(wasRunning ? L"Agent Notes updated and reopened!"
                                   : L"Agent Notes installed and launched!", 0);
    } else {
        SetFooterStatus(L"Agent Notes is up to date.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallDnsPilot(BOOL launchAfter) {
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);
    EnsureDirTree(g_dnsDir);

    /* An app that is open when its update starts must come back up on its
       own afterwards. A running pilot also holds its exe lock, so close it
       politely first and relaunch it once the new build is in place. */
    BOOL wasRunning = FALSE, wasVisible = FALSE;
    HWND hPilot = FindWindowW(L"KBLSuiteDnsPilotWindow", NULL);
    if (hPilot) {
        wasRunning = TRUE;
        wasVisible = IsWindowVisible(hPilot) ? TRUE : FALSE;
        SetFooterStatus(L"Stopping running DNS Pilot for clean update...", 8);
        wchar_t pilotExe[MAX_PATH];
        _snwprintf(pilotExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
        if (FileExistsW(pilotExe)) {
            /* --stop quits the app for real; WM_CLOSE only hides it to the
               tray now, so the exe would stay locked. */
            SHELLEXECUTEINFOW sei;
            ZeroMemory(&sei, sizeof(sei));
            sei.cbSize = sizeof(sei);
            sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
            sei.lpFile = pilotExe;
            sei.lpParameters = L"--stop";
            sei.nShow  = SW_HIDE;
            if (ShellExecuteExW(&sei) && sei.hProcess) {
                WaitForSingleObject(sei.hProcess, 2500);
                CloseHandle(sei.hProcess);
            }
        }
        Sleep(300);
    }
    /* DNS Pilot's --stop requests a real tray exit.  Do not trust the launch
       result alone: an old or foreign copy must be absent before any mirror
       is replaced. */
    if (!CloseManagedExeCopies(MIRROR_DNS, g_dnsDir, L"DnsPilot.exe",
                               L"DNS Pilot", 0))
        return;

    wchar_t destExe[MAX_PATH];
    _snwprintf(destExe, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);

    SetFooterStatus(L"Downloading latest DnsPilot.exe from GitHub...", 60);
    if (!InstallRepoFileEverywhere(MIRROR_DNS, branch, L"dns-pilot/DnsPilot.exe",
                                   g_dnsDir, L"DnsPilot.exe", NULL)) {
        SetFooterStatus(L"Could not save DnsPilot.exe - close DNS Pilot if it is open, then retry.", 0);
        return;
    }

    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH, L"%s\\DNS Pilot.lnk", desk);
        if (RepointDesktopShortcutIfPresent(lnk, destExe, g_dnsDir,
                                            L"DNS Pilot - Fast DNS benchmark & auto-pilot"))
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    if (launchAfter || wasRunning) {
        /* restore as it was: a visible window comes back, a trayed pilot
           starts quietly in the tray again instead of popping open */
        BOOL show = (launchAfter || wasVisible);
        ShellExecuteW(NULL, L"open", destExe, show ? NULL : L"-Silent", g_dnsDir,
                      show ? SW_SHOWNORMAL : SW_HIDE);
        SetFooterStatus(wasRunning ? L"DNS Pilot updated and reopened!"
                                   : L"DNS Pilot installed and launched!", 0);
    } else {
        SetFooterStatus(L"DNS Pilot is up to date.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallKeyboardLighting(void) {
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);
    EnsureDirTree(g_kblDir);

    wchar_t kblExe[MAX_PATH], lp[MAX_PATH], uiDll[MAX_PATH];
    _snwprintf(kblExe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    BOOL kblWasInstalled = FileExistsW(kblExe);
    BOOL kblWasVisible = FALSE;
    {
        HWND wK = FindWindowW(L"TestsKeyboardLightingSuiteWnd", NULL);
        if (wK && IsWindowVisible(wK)) kblWasVisible = TRUE;
    }
    SetFooterStatus(L"Stopping running Keyboard Lighting for clean update...", 10);
    if (FileExistsW(kblExe)) {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = kblExe;
        sei.lpParameters = L"--stop";
        sei.nShow  = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 2000);
            CloseHandle(sei.hProcess);
        }
    }
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);
    Sleep(300);
    /* The helper locks take care of the old engine too; the common gate then
       proves no visible, trayed, or stray KeyboardLighting.exe remains before
       we delete or download any of its update files. */
    if (!CloseManagedExeCopies(MIRROR_KBL, g_kblDir, L"KeyboardLighting.exe",
                               L"Keyboard Lighting", 0))
        return;
    _snwprintf(uiDll, MAX_PATH, L"%s\\Ui.dll", g_kblDir);
    DeleteFileW(uiDll);

    const wchar_t *files[] = {
        L"KeyboardLighting.exe", L"Aura-Background.ps1", L"app.ico",
        L"Tray.ps1", L"ui_controls.cs.txt", L"Setup.ps1", L"Setup.bat",
        L"Restart.bat", L"Zones.ps1", L"Zones.bat", L"MyEffect.ps1", L"README.md"
    };
    int count = (int)(sizeof(files) / sizeof(files[0]));
    int dlFail = 0;
    for (int i = 0; i < count; i++) {
        wchar_t msg[256], rel[128], dest[MAX_PATH];
        _snwprintf(msg, 255, L"Downloading Keyboard Lighting (%d/%d): %s...", i + 1, count, files[i]);
        SetFooterStatus(msg, 15 + (i * 80) / count);
        _snwprintf(rel, 127, L"keyboard-lighting/%s", files[i]);
        _snwprintf(dest, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        if (!DownloadRawRepoFile(branch, rel, dest)) dlFail++;
    }
    /* The suite folder copy must never lag behind the folder the hub found
       first (a repo checkout or a shortcut target), or browsing the suite
       shows a build from days ago. */
    for (int i = 0; i < count; i++) {
        wchar_t src2[MAX_PATH];
        _snwprintf(src2, MAX_PATH, L"%s\\%s", g_kblDir, files[i]);
        if (FileExistsW(src2)) MirrorFileEverywhere(MIRROR_KBL, src2);
    }

    if (FileExistsW(kblExe)) {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
            if (RepointDesktopShortcutIfPresent(lnk, kblExe, g_kblDir,
                                                L"Keyboard Lighting - ROG Strix G16 RGB"))
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        }
        /* restore as it was: a visible window comes back to the front,
           anything else (trayed or not running) starts quietly in the tray
           instead of popping the window open */
        if (kblWasVisible || !kblWasInstalled) {
            ShellExecuteW(NULL, L"open", kblExe, NULL, g_kblDir, SW_SHOWNORMAL);
        } else {
            ShellExecuteW(NULL, L"open", kblExe, L"-Silent", g_kblDir, SW_HIDE);
        }
        if (dlFail) {
            /* never claim a clean update when helper files are missing */
            wchar_t warn[256];
            _snwprintf(warn, 255,
                       L"Keyboard Lighting updated, but %d of %d files failed to download - press Update to retry.",
                       dlFail, count);
            warn[255] = L'\0';
            SetFooterStatus(warn, 0);
        } else {
            SetFooterStatus(kblWasVisible || !kblWasInstalled
                                ? L"Keyboard Lighting updated and reopened!"
                                : L"Keyboard Lighting updated - running quietly in the tray.", 0);
        }
    } else {
        SetFooterStatus(L"Could not download KeyboardLighting.exe from GitHub.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallSpatialHud(void) {
    if (g_hudDisabled) {
        SetFooterStatus(L"Spatial HUD is disabled - click Enable on its card first.", 0);
        return;
    }
    SetCheckPhase(TRUE);
    if (!g_remHudPath[0]) WorkerCheckRemote();

    wchar_t dlPath[512], jarName[128];
    EnterCriticalSection(&g_cs);
    wcsncpy(dlPath, g_remHudPath, 511); dlPath[511] = L'\0';
    wcsncpy(jarName, g_remHudName, 127); jarName[127] = L'\0';
    LeaveCriticalSection(&g_cs);

    SetCheckPhase(FALSE);
    if (!dlPath[0] || !jarName[0]) {
        SetFooterStatus(L"No published Spatial HUD JAR found under 'spatial-hud-latest'.", 0);
        return;
    }

    SetFooterStatus(L"Downloading Spatial HUD Fabric JAR...", 60);
    DWORD sz = 0; int st = 0;
    BYTE *jarBytes = HttpGetBuffer(L"github.com", dlPath, &sz, &st);
    if (!jarBytes || st != 200 || sz < 4096 || jarBytes[0] != 'P' || jarBytes[1] != 'K') {
        free(jarBytes);
        SetFooterStatus(L"Spatial HUD download failed or invalid archive.", 0);
        return;
    }

    EnsureDirTree(g_modsDir);

    wchar_t destJar[MAX_PATH];
    _snwprintf(destJar, MAX_PATH, L"%s\\%s", g_modsDir, jarName);
    /* Land the NEW jar first. If the write fails (Minecraft holding a jar,
       disk full, ...) every previously installed jar must survive - only
       after the new jar is safely on disk do we retire the old versions. */
    BOOL ok = WriteBinaryFileAtomic(destJar, jarBytes, sz);
    free(jarBytes);

    if (ok) {
        wchar_t pat[MAX_PATH];
        _snwprintf(pat, MAX_PATH, L"%s\\spatial-hud*.jar", g_modsDir);
        WIN32_FIND_DATAW fd;
        HANDLE hf = FindFirstFileW(pat, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                if (lstrcmpiW(fd.cFileName, jarName) == 0) continue;
                wchar_t oldJar[MAX_PATH];
                _snwprintf(oldJar, MAX_PATH, L"%s\\%s", g_modsDir, fd.cFileName);
                DeleteFileW(oldJar);
            } while (FindNextFileW(hf, &fd));
            FindClose(hf);
        }
        wchar_t msg[300];
        _snwprintf(msg, 299, L"Spatial HUD updated (%s).", jarName);
        SetFooterStatus(msg, 0);
    } else {
        SetFooterStatus(L"Could not write JAR to mods folder (close Minecraft first).", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerInstallOrcaPlugins(void) {
    if (g_orcaDisabled) {
        SetFooterStatus(L"OrcaSlicer Plugins are disabled - click Enable on the card first.", 0);
        return;
    }
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);

    SetFooterStatus(L"Downloading Wave Overhangs & Unlayered Infill...", 40);
    EnsureDirTree(g_orcaBatDir);

    wchar_t batPath[MAX_PATH], tmpWave[MAX_PATH], tmpInf[MAX_PATH];
    _snwprintf(batPath, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    _snwprintf(tmpWave, MAX_PATH, L"%s\\wave_overhangs_orca.py", g_hubCfgDir);
    _snwprintf(tmpInf,  MAX_PATH, L"%s\\unlayered_infill_orca.py", g_hubCfgDir);

    BOOL ok1 = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", batPath);
    BOOL ok2 = DownloadRawRepoFile(branch, L"plugins/wave-overhangs/wave_overhangs_orca.py", tmpWave);
    BOOL ok3 = DownloadRawRepoFile(branch, L"plugins/unlayered-infill/unlayered_infill_orca.py", tmpInf);
    if (!ok2 || !ok3) {
        SetFooterStatus(L"Failed downloading OrcaSlicer plugins from GitHub.", 0);
        return;
    }

    wchar_t searchOrca[MAX_PATH];
    _snwprintf(searchOrca, MAX_PATH, L"%s\\OrcaSlicer*", g_roamApp);
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(searchOrca, &fd);
    int targetsInstalled = 0;

    if (hf != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
                _snwprintf(wDir, MAX_PATH, L"%s\\%s\\orca_plugins\\wave-overhangs", g_roamApp, fd.cFileName);
                _snwprintf(iDir, MAX_PATH, L"%s\\%s\\orca_plugins\\unlayered-infill", g_roamApp, fd.cFileName);
                _snwprintf(legacyDir, MAX_PATH, L"%s\\%s\\plugins", g_roamApp, fd.cFileName);
                EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
                const char *stJson = "{\"enabled\": true}\n";
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
                CopyFileW(tmpInf, dst, FALSE);
                _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
                WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));

                _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
                CopyFileW(tmpWave, dst, FALSE);
                _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
                CopyFileW(tmpInf, dst, FALSE);
                targetsInstalled++;
            }
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }

    if (targetsInstalled == 0) {
        wchar_t wDir[MAX_PATH], iDir[MAX_PATH], legacyDir[MAX_PATH], dst[MAX_PATH], statePath[MAX_PATH];
        _snwprintf(wDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs", g_roamApp);
        _snwprintf(iDir, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill", g_roamApp);
        _snwprintf(legacyDir, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(wDir); EnsureDirTree(iDir); EnsureDirTree(legacyDir);
        const char *stJson = "{\"enabled\": true}\n";
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", wDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", wDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", iDir);
        CopyFileW(tmpInf, dst, FALSE);
        _snwprintf(statePath, MAX_PATH, L"%s\\.install_state.json", iDir);
        WriteBinaryFileAtomic(statePath, (const BYTE *)stJson, (DWORD)strlen(stJson));
        _snwprintf(dst, MAX_PATH, L"%s\\wave_overhangs_orca.py", legacyDir);
        CopyFileW(tmpWave, dst, FALSE);
        _snwprintf(dst, MAX_PATH, L"%s\\unlayered_infill_orca.py", legacyDir);
        CopyFileW(tmpInf, dst, FALSE);
    }

    /* End-state honesty: CopyFileW fails silently while OrcaSlicer holds
       locks, so verify the plugin files actually landed before claiming it. */
    BOOL landed = TRUE;
    if (targetsInstalled == 0) {
        wchar_t chk[MAX_PATH];
        _snwprintf(chk, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py", g_roamApp);
        landed = FileExistsW(chk);
        _snwprintf(chk, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py", g_roamApp);
        landed = landed && FileExistsW(chk);
    }
    if (!landed) {
        SetFooterStatus(L"OrcaSlicer plugin files could not be written - close OrcaSlicer and retry.", 0);
    } else if (!ok1) {
        SetFooterStatus(L"OrcaSlicer plugins updated, but Orca-Plugins.bat failed to download - restart OrcaSlicer to load.", 0);
    } else {
        SetFooterStatus(L"OrcaSlicer plugins updated — restart OrcaSlicer to load.", 0);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void WorkerSelfUpdateHub(void) {
    wchar_t branch[64];
    EnsureProbeRef(branch, 64);
    SetFooterStatus(L"Downloading latest ProjectHub.exe from GitHub...", 50);

    /* A running program cannot close itself and then continue replacing its
       own image.  Stage the fresh EXE under a distinct .pending.exe name;
       the staged process is a tiny handoff worker that waits for this Hub to
       exit, proves this image is no longer live, and only then swaps it. */
    wchar_t pendingExe[MAX_PATH];
    _snwprintf(pendingExe, MAX_PATH, L"%s.pending.exe", g_exePath);
    DeleteFileW(pendingExe);

    /* Hold the bytes so the suite folder copy can be written from the same
       download: a hub launched from Downloads must not leave the suite folder
       holding the previous build. */
    DWORD newLen = 0;
    BYTE *newBytes = DownloadRepoBytes(branch, L"project-hub/ProjectHub.exe", &newLen);
    if (!newBytes || !WriteBinaryFileAtomic(pendingExe, newBytes, newLen)) {
        free(newBytes);
        SetFooterStatus(L"Could not download ProjectHub.exe. No update was applied.", 0);
        return;
    }

    /* This is a separate managed executable too.  WriteBinaryFileAtomic has
       the same exact-image final check as all app installers; if it remains
       live, do not split a Hub update across two copies. */
    if (g_suiteHubExe[0] && _wcsicmp(g_suiteHubExe, g_exePath) != 0) {
        if (!WriteBinaryFileAtomic(g_suiteHubExe, newBytes, newLen)) {
            DeleteFileW(pendingExe);
            free(newBytes);
            SetFooterStatus(L"Project Hub is still running from its suite copy. No update was applied.", 0);
            return;
        }
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
            if (RepointDesktopShortcutIfPresent(lnk, g_suiteHubExe, g_suiteDir,
                                                L"Project Hub - Status & Updates"))
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        }
    }
    free(newBytes);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    SetFooterStatus(L"Project Hub update staged — closing, then applying it...", 100);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_SELFUPD, 0, 0);
}

