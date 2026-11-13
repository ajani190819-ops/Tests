static void WorkerInstallGeneric(int idx, BOOL launchAfter) {
    if (idx < 0 || idx >= g_cardCount) return;
    ProjectCard *c = &g_cards[idx];
    if (!c->exeRepo || !c->exeName) {
        SetFooterStatus(L"Install not configured for this app - check hub configuration.", 0);
        return;
    }
    wchar_t branch[64];
    EnsureActiveBranch(branch, 64);
    EnsureDirTree(c->appDir);

    wchar_t destExe[MAX_PATH], msg[256];
    _snwprintf(destExe, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
    destExe[MAX_PATH - 1] = L'\0';
    _snwprintf(msg, 255, L"Downloading %s from GitHub...", c->title ? c->title : L"app");
    SetFooterStatus(msg, 60);

    /* A tray app still running from a stray copy would hold that copy's exe
       lock and its single-instance mutex, so the fresh suite build could
       neither replace it nor start next to it. */
    StopForeignInstance(L"TestsAudioSwitcherWnd", c->appDir);

    int mirrors = 0;
    if (!InstallRepoFileEverywhere(MIRROR_GENERIC, branch, c->exeRepo, c->appDir,
                                   c->exeName, &mirrors)) {
        SetFooterStatus(L"Could not save the file - close the app if it is open, then retry.", 0);
        return;
    }
    wchar_t desk[MAX_PATH], lnk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        _snwprintf(lnk, MAX_PATH - 1, L"%s\\%s.lnk", desk,
                   c->shortcutName ? c->shortcutName : (c->title ? c->title : L"Suite App"));
        if (RepointDesktopShortcutIfPresent(lnk, destExe, c->appDir,
                                            c->title ? c->title : L"Suite App"))
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    if (launchAfter) {
        ShellExecuteW(NULL, L"open", destExe, c->launchArgs, c->appDir, SW_SHOWNORMAL);
        _snwprintf(msg, 255, L"%s installed and launched!", c->title ? c->title : L"App");
    } else if (mirrors > 0) {
        _snwprintf(msg, 255, L"%s updated in the suite folder and %d other copy(ies).",
                   c->title ? c->title : L"App", mirrors);
    } else {
        _snwprintf(msg, 255, L"%s is up to date.", c->title ? c->title : L"App");
    }
    SetFooterStatus(msg, 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void HandleVerb(int idx, const wchar_t *verb) {
    if (idx < 0 || idx >= g_cardCount || !verb) return;
    ProjectCard *c = &g_cards[idx];
    if (wcscmp(verb, L"folder") == 0) {
        EnsureDirTree(c->appDir);
        ShellExecuteW(g_hwnd, L"open", c->appDir, NULL, NULL, SW_SHOWNORMAL);
    } else if (wcscmp(verb, L"update") == 0 || wcscmp(verb, L"install") == 0) {
        g_dynIdx = idx;
        SetFooterStatus(L"Fetching the latest build from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_DYN);
    } else if (wcscmp(verb, L"open") == 0) {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH - 1, L"%s\\%s", c->appDir, c->exeName);
        if (FileExistsW(p))
            ShellExecuteW(g_hwnd, L"open", p, c->launchArgs, c->appDir, SW_SHOWNORMAL);
        else {
            g_dynIdx = idx;
            SetFooterStatus(L"Fetching the latest build from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_DYN);
        }
    }
}

static void InitCardDefinitions(void) {
    ZeroMemory(g_cards, sizeof(g_cards));

    g_cards[0].resIconId = RES_ICON_BTG;
    g_cards[0].title     = L"Bluetooth Guard";
    g_cards[0].btnCount  = 4;
    g_cards[0].btns[0]   = (ActionBtn){ ID_BTG_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[0].btns[1]   = (ActionBtn){ ID_BTG_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[0].btns[2]   = (ActionBtn){ ID_BTG_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[0].btns[3]   = (ActionBtn){ ID_BTG_STOP,   L"Stop",   BTN_SECONDARY, 62, {0,0,0,0}, NULL, FALSE };

    g_cards[1].resIconId = RES_ICON_KBL;
    g_cards[1].title     = L"Keyboard Lighting";
    g_cards[1].btnCount  = 4;
    g_cards[1].btns[0]   = (ActionBtn){ ID_KBL_UPDATE,  L"Update",   BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[1].btns[1]   = (ActionBtn){ ID_KBL_OPEN,    L"Open",     BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[1].btns[2]   = (ActionBtn){ ID_KBL_FOLDER,  L"Folder",   BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[1].btns[3]   = (ActionBtn){ ID_KBL_RESTART, L"Unfreeze", BTN_SECONDARY, 84, {0,0,0,0}, NULL, FALSE };

    g_cards[2].resIconId = RES_ICON_HUD;
    g_cards[2].title     = L"Spatial HUD";
    g_cards[2].btnCount  = 4;
    g_cards[2].btns[0]   = (ActionBtn){ ID_HUD_UPDATE,  L"Update",  BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[2].btns[1]   = (ActionBtn){ ID_HUD_FOLDER,  L"Folder",  BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[2].btns[2]   = (ActionBtn){ ID_HUD_REMOVE,  L"Remove",  BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[2].btns[3]   = (ActionBtn){ ID_HUD_DISABLE, L"Disable", BTN_SECONDARY, 78, {0,0,0,0}, NULL, FALSE };

    g_cards[3].resIconId = RES_ICON_ORCA;
    g_cards[3].title     = L"OrcaSlicer Plugins";
    g_cards[3].btnCount  = 4;
    g_cards[3].btns[0]   = (ActionBtn){ ID_ORCA_UPDATE,  L"Update",  BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[3].btns[1]   = (ActionBtn){ ID_ORCA_FOLDER,  L"Folder",  BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[3].btns[2]   = (ActionBtn){ ID_ORCA_REMOVE,  L"Remove",  BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[3].btns[3]   = (ActionBtn){ ID_ORCA_DISABLE, L"Disable", BTN_SECONDARY, 78, {0,0,0,0}, NULL, FALSE };

    g_cards[4].resIconId = RES_ICON_DNS;
    g_cards[4].title     = L"DNS Pilot";
    g_cards[4].btnCount  = 3;
    g_cards[4].btns[0]   = (ActionBtn){ ID_DNS_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[4].btns[1]   = (ActionBtn){ ID_DNS_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[4].btns[2]   = (ActionBtn){ ID_DNS_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };

    g_cards[5].resIconId = RES_ICON_NOTES;
    g_cards[5].title     = L"Agent Notes";
    g_cards[5].btnCount  = 3;
    g_cards[5].btns[0]   = (ActionBtn){ ID_NOTES_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[5].btns[1]   = (ActionBtn){ ID_NOTES_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[5].btns[2]   = (ActionBtn){ ID_NOTES_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };

    g_cards[6].resIconId = RES_ICON_AUDIOSWITCHER;
    g_cards[6].title     = L"Audio Switcher";
    g_cards[6].btnCount  = 3;
    g_cards[6].btns[0]   = (ActionBtn){ ID_AUDIOSWITCHER_UPDATE, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    g_cards[6].btns[1]   = (ActionBtn){ ID_AUDIOSWITCHER_OPEN,   L"Open",   BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    g_cards[6].btns[2]   = (ActionBtn){ ID_AUDIOSWITCHER_FOLDER, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };

    /* exeRepo/exeName/appDir so Install/Update/Open/Folder work for built-in
       cards. appDir is the suite folder for every card: it is where an install
       writes and where the status sweep looks, so the two can never disagree. */
    static const struct { const char *repo; const char *name; const wchar_t *dir; } builtinExe[7] = {
        { "bluetooth-guard/BluetoothGuard.exe", "BluetoothGuard.exe", L"BluetoothGuard" },
        { "keyboard-lighting/KeyboardLighting.exe", "KeyboardLighting.exe", L"KeyboardLighting" },
        { NULL, NULL, L"SpatialHudUpdater" },
        { NULL, NULL, L"OrcaPlugins" },
        { "dns-pilot/DnsPilot.exe", "DnsPilot.exe", L"DnsPilot" },
        { "agent-notes/AgentNotes.exe", "AgentNotes.exe", L"AgentNotes" },
        { "audio-switcher/AudioSwitcher.exe", "AudioSwitcher.exe", L"AudioSwitcher" },
    };
    for (int i = 0; i < 7; i++) {
        ProjectCard *c = &g_cards[i];
        if (builtinExe[i].repo) {
            c->exeRepo = PoolAdd(builtinExe[i].repo);
            c->exeName = PoolAdd(builtinExe[i].name);
        }
        if (builtinExe[i].dir && g_suiteDir[0])
            SuiteAppPath(c->appDir, MAX_PATH, builtinExe[i].dir);
    }
}

