/* ---- download-branch picker ------------------------------------------- */

static void FetchBranchNames(void) {
    g_branchMenuN = 0;
    DWORD sz = 0;
    int st = 0;
    BYTE *buf = HttpGetBuffer(L"api.github.com",
                              L"/repos/ajani190819-ops/Tests/branches?per_page=100",
                              &sz, &st);
    if (!buf || st != 200) { free(buf); return; }
    char *s = (char *)buf;
    while (g_branchMenuN < 96) {
        char *q = strstr(s, "\"name\":\"");
        if (!q) break;
        q += 8;
        char *e = strchr(q, '"');
        if (!e || (e - q) >= 64) break;
        int i;
        for (i = 0; q + i < e; i++)
            g_branchMenuNames[g_branchMenuN][i] = (wchar_t)(unsigned char)q[i];
        g_branchMenuNames[g_branchMenuN][i] = L'\0';
        g_branchMenuN++;
        s = e + 1;
    }
    free(buf);
}

static void EnsureBranchInMenu(const wchar_t *name) {
    int i;
    for (i = 0; i < g_branchMenuN; i++)
        if (_wcsicmp(g_branchMenuNames[i], name) == 0) return;
    if (g_branchMenuN >= 96) return;
    wcsncpy(g_branchMenuNames[g_branchMenuN], name, 63);
    g_branchMenuNames[g_branchMenuN][63] = L'\0';
    g_branchMenuN++;
}

/* Generic one-field text prompt (in-memory dialog template: the C struct has
   no members for the trailing menu/class/title). Optional Browse button
   fills the edit via the standard folder picker - so a path can be pasted,
   typed, or browsed in one place. */
typedef struct {
    const wchar_t *preset;
    wchar_t      *out;
    int           outCap;
    BOOL          withBrowse;
} PromptCtx;

static INT_PTR CALLBACK TextPromptProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    PromptCtx *ctx = (PromptCtx *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    switch (msg) {
    case WM_INITDIALOG:
        ctx = (PromptCtx *)lp;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);
        if (ctx && ctx->preset && ctx->preset[0]) SetDlgItemTextW(hDlg, 201, ctx->preset);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == 202 && ctx && ctx->withBrowse) {
            BROWSEINFOW bi;
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = hDlg;
            bi.lpszTitle = L"Select the folder:";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t picked[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, picked) && picked[0]) {
                    /* Selecting a folder in the picker is the decision:
                       accept immediately - no second click needed. */
                    wcsncpy(ctx->out, picked, ctx->outCap - 1);
                    ctx->out[ctx->outCap - 1] = L'\0';
                    EndDialog(hDlg, IDOK);
                }
                CoTaskMemFree(pidl);
            }
            return TRUE;
        }
        if (LOWORD(wp) == IDOK && ctx && ctx->out) {
            GetDlgItemTextW(hDlg, 201, ctx->out, ctx->outCap - 1);
            ctx->out[ctx->outCap - 1] = L'\0';
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR PromptTextInput(HWND hwnd, const wchar_t *label, const wchar_t *preset,
                               wchar_t *out, int outCap, BOOL withBrowse,
                               const wchar_t *okLabel) {
    static BYTE buf[1400];
    ZeroMemory(buf, sizeof(buf));
    BYTE *p = buf;
    int nItems = withBrowse ? 5 : 4;
    int cx = withBrowse ? 320 : 240;
    int cy = withBrowse ? 92 : 78;
    *(DWORD *)p = DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU; p += 4;
    *(DWORD *)p = 0;             p += 4;   /* dwExtendedStyle (required!) */
    *(WORD *)p = (WORD)nItems; p += 2;
    *(WORD *)p = 10;   p += 2;
    *(WORD *)p = 10;   p += 2;
    *(WORD *)p = (WORD)cx; p += 2;
    *(WORD *)p = (WORD)cy; p += 2;
    *(WORD *)p = 0;    p += 2;
    *(WORD *)p = 0;    p += 2;
    *(WCHAR *)p = 0;   p += 2;
    *(WORD *)p = 9;    p += 2;
    {
        static const WCHAR face[] = L"Segoe UI";
        CopyMemory(p, face, sizeof(face));
        p += sizeof(face);
    }
    struct { DWORD style; short x, y, cx, cy; WORD id; WORD cls; const WCHAR *title; } items[5] = {
        { WS_CHILD | WS_VISIBLE | SS_LEFT, 8, 12, (short)(cx - 16), 12, 101, 0x0082, label },
        { WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
          8, 28, (short)(cx - 16), 14, 201, 0x0081, L"" },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
          8, (short)(cy - 26), 56, 16, 202, 0x0080, L"Browse..." },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
          (short)(withBrowse ? cx - 126 : 128), (short)(cy - 26), 56, 16, IDOK, 0x0080, okLabel },
        { WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
          (short)(withBrowse ? cx - 64 : 182), (short)(cy - 26), 48, 16, IDCANCEL, 0x0080, L"Cancel" }
    };
    /* Without Browse the dialog is label, edit, OK, Cancel - skip item 2. */
    const int order = withBrowse ? 1 : 0;
    const int seq[2][5] = { { 0, 1, 3, 4, -1 }, { 0, 1, 2, 3, 4 } };
    for (int i = 0; i < nItems; i++) {
        int it = seq[order][i];
        p = (BYTE *)(((ULONG_PTR)p + 3) & ~(ULONG_PTR)3);
        *(DWORD *)p = items[it].style; p += 4;
        *(DWORD *)p = 0;             p += 4;
        *(WORD *)p = (WORD)items[it].x;  p += 2;
        *(WORD *)p = (WORD)items[it].y;  p += 2;
        *(WORD *)p = (WORD)items[it].cx; p += 2;
        *(WORD *)p = (WORD)items[it].cy; p += 2;
        *(WORD *)p = items[it].id;    p += 2;
        *(WORD *)p = 0xFFFF;         p += 2;
        *(WORD *)p = items[it].cls;   p += 2;
        {
            size_t n = wcslen(items[it].title) + 1;
            CopyMemory(p, items[it].title, n * sizeof(WCHAR));
            p += n * sizeof(WCHAR);
        }
        *(WORD *)p = 0;              p += 2;
    }
    PromptCtx ctx = { preset, out, outCap > 0 ? outCap : 1, withBrowse };
    if (out != preset) out[0] = L'\0';   /* keep the preset when buffers alias */
    return DialogBoxIndirectParamW(g_hInst, (DLGTEMPLATE *)buf, hwnd,
                                   TextPromptProc, (LPARAM)&ctx);
}

static INT_PTR PromptBranchName(HWND hwnd, const wchar_t *preset) {
    return PromptTextInput(hwnd, L"GitHub branch name:", preset,
                           g_branchPromptOut, 64, FALSE, L"OK");
}

static void SetActiveBranch(HWND hwnd, const wchar_t *branch) {
    if (!branch || !branch[0]) return;
    EnterCriticalSection(&g_cs);
    wcsncpy(g_activeBranch, branch, 63);
    g_activeBranch[63] = L'\0';
    LeaveCriticalSection(&g_cs);
    SaveBranchCfg(branch);
    {
        wchar_t msg[160];
        _snwprintf(msg, 159, L"Download branch set to '%s' - refreshing update checks.", branch);
        msg[159] = L'\0';
        SetFooterStatus(msg, 0);
    }
    RefreshProjectStatuses();
    EnqueueTask(TASK_CHECK_REMOTE);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ShowBranchMenu(HWND hwnd) {
    FetchBranchNames();
    if (g_branchMenuN == 0) {
        EnsureBranchInMenu(L"main");
        EnsureBranchInMenu(FALLBACK_BRANCH_W);
    }
    EnsureBranchInMenu(L"main");
    EnsureBranchInMenu(FALLBACK_BRANCH_W);
    {
        wchar_t active[64];
        EnsureActiveBranch(active, 64);
        EnsureBranchInMenu(active);
    }
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Download updates from:");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    {
        wchar_t active[64];
        EnsureActiveBranch(active, 64);
        for (int i = 0; i < g_branchMenuN; i++) {
            UINT flags = MF_STRING;
            if (_wcsicmp(g_branchMenuNames[i], active) == 0) flags |= MF_CHECKED;
            AppendMenuW(hMenu, flags, ID_MENU_BRANCH_BASE + i, g_branchMenuNames[i]);
        }
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_BRANCH_CUSTOM, L"Other branch...");
    {
        POINT pt = { g_topBtns[3].rc.left, g_topBtns[3].rc.bottom + 4 };
        ClientToScreen(hwnd, &pt);
        TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                       pt.x, pt.y, 0, hwnd, NULL);
    }
    DestroyMenu(hMenu);
}

static void HandleActionCommand(HWND hwnd, int id) {
    wchar_t p[MAX_PATH];
    /* dynamic branch-menu entries carry an index into the fetched list */
    if (id >= ID_MENU_BRANCH_BASE && id < ID_MENU_BRANCH_CUSTOM) {
        int idx = id - ID_MENU_BRANCH_BASE;
        if (idx >= 0 && idx < g_branchMenuN)
            SetActiveBranch(hwnd, g_branchMenuNames[idx]);
        return;
    }
    if (id >= ID_DYN_BASE && id < ID_DYN_BASE + MAX_APPS * 10) {
        int aidx = (id - ID_DYN_BASE) / 10;
        int bidx = (id - ID_DYN_BASE) % 10;
        if (aidx >= 0 && aidx < g_cardCount && bidx >= 0 && bidx < g_cards[aidx].btnCount)
            HandleVerb(aidx, g_cards[aidx].btns[bidx].verb);
        return;
    }
    switch (id) {
    case ID_BTN_REFRESH_ALL:
        RefreshProjectStatuses();
        EnqueueTask(TASK_CHECK_REMOTE);
        break;
    case ID_BTN_SELF_UPDATE:
        EnqueueTask(TASK_SELF_UPDATE);
        break;
    case ID_BTN_UPDATE_ALL:
        EnqueueTask(TASK_UPDATE_ALL);
        break;
    case ID_BTN_BRANCH:
        ShowBranchMenu(hwnd);
        break;
    case ID_BTN_SETTINGS:
        OpenSettingsWindow(hwnd);
        break;
    case ID_MENU_BRANCH_CUSTOM: {
        wchar_t cur[64];
        EnsureActiveBranch(cur, 64);
        if (PromptBranchName(hwnd, cur) == IDOK && g_branchPromptOut[0])
            SetActiveBranch(hwnd, g_branchPromptOut);
        break;
    }
    case ID_EMBLEM_MENU:
        ShowUtilityMenu(hwnd);
        break;
    case ID_MENU_SHORTCUT: {
        wchar_t desk[MAX_PATH], lnk[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
            _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
            if (CreateShortcutLink(lnk, g_exePath, g_exeDir, L"Project Hub - Status & Updates")) {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
                SetFooterStatus(L"Pinned 'Project Hub' shortcut to Desktop.", 0);
            }
        }
        break;
    }
    case ID_MENU_SUITE_DIR:
        EnsureDirTree(g_suiteDir);
        ShellExecuteW(hwnd, L"open", g_suiteDir, NULL, NULL, SW_SHOWNORMAL);
        break;
    case ID_MENU_HUB_TOSUITE: {
        /* One click to make the suite folder copy of the hub the real one:
           write the running build there and repoint the Desktop shortcut, so
           the next launch comes from the folder the user browses. */
        EnsureDirTree(g_suiteDir);
        if (CopyFileW(g_exePath, g_suiteHubExe, FALSE)) {
            wchar_t desk[MAX_PATH], lnk[MAX_PATH];
            BOOL repointed = FALSE;
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
                _snwprintf(lnk, MAX_PATH, L"%s\\Project Hub.lnk", desk);
                repointed = RepointDesktopShortcutIfPresent(lnk, g_suiteHubExe, g_suiteDir,
                                                            L"Project Hub - Status & Updates");
                if (repointed) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
            }
            wchar_t m[MAX_PATH + 96];
            if (repointed)
                _snwprintf(m, MAX_PATH + 95, L"Project Hub installed into %s - the Desktop shortcut now points there.", g_suiteDir);
            else
                _snwprintf(m, MAX_PATH + 95, L"Project Hub installed into %s - no Desktop shortcut was created; pin one from the emblem menu if you want one.", g_suiteDir);
            m[MAX_PATH + 95] = L'\0';
            SetFooterStatus(m, 0);
        } else {
            SetFooterStatus(L"Could not write ProjectHub.exe into the suite folder - close the other hub copy and retry.", 0);
        }
        break;
    }


    case ID_BTG_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_btgDir, SW_SHOWNORMAL);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, NULL, NULL, SW_SHOWNORMAL);
        else EnqueueTask(TASK_OPEN_OR_INSTALL_BTG);
        break;
    case ID_BTG_UPDATE:
        EnqueueTask(TASK_INSTALL_BTG);
        break;
    case ID_BTG_STOP:
        _snwprintf(p, MAX_PATH, L"%s\\BluetoothGuard.exe", g_btgDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, L"--stop", g_btgDir, SW_HIDE);
        else if (g_btgExtraExe[0] && FileExistsW(g_btgExtraExe))
            ShellExecuteW(hwnd, L"open", g_btgExtraExe, L"--stop", NULL, SW_HIDE);
        SetFooterStatus(L"Stopped Bluetooth Guard.", 0);
        break;
    case ID_BTG_FOLDER:
        EnsureDirTree(g_btgDir);
        ShellExecuteW(hwnd, L"open", g_btgDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_DNS_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\DnsPilot.exe", g_dnsDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_dnsDir, SW_SHOWNORMAL);
        else {
            SetFooterStatus(L"DNS Pilot: fetching latest DnsPilot.exe from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_DNS);
        }
        break;
    case ID_DNS_UPDATE:
        SetFooterStatus(L"DNS Pilot: fetching latest DnsPilot.exe from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_DNS);
        break;
    case ID_DNS_FOLDER:
        EnsureDirTree(g_dnsDir);
        ShellExecuteW(hwnd, L"open", g_dnsDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_NOTES_OPEN:
        _snwprintf(p, MAX_PATH, L"%s\\AgentNotes.exe", g_notesDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_notesDir, SW_SHOWNORMAL);
        else {
            SetFooterStatus(L"Agent Notes: fetching latest AgentNotes.exe from GitHub...", 25);
            EnqueueTask(TASK_OPEN_OR_INSTALL_NOTES);
        }
        break;
    case ID_NOTES_UPDATE:
        SetFooterStatus(L"Agent Notes: fetching latest AgentNotes.exe from GitHub...", 25);
        EnqueueTask(TASK_INSTALL_NOTES);
        break;
    case ID_NOTES_FOLDER:
        EnsureDirTree(g_notesDir);
        ShellExecuteW(hwnd, L"open", g_notesDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_KBL_OPEN: {
        wchar_t showFlag[MAX_PATH];
        _snwprintf(showFlag, MAX_PATH, L"%s\\KeyboardLighting\\show.flag", g_localApp);
        WriteBinaryFileAtomic(showFlag, (const BYTE *)"1", 1);
        _snwprintf(p, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_kblDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_KBL);
        break;
    }
    case ID_KBL_RESTART:
        RestartKeyboardLightingCleanly();
        break;
    case ID_KBL_UPDATE:
        EnqueueTask(TASK_INSTALL_KBL);
        break;
    case ID_KBL_FOLDER:
        EnsureDirTree(g_kblDir);
        ShellExecuteW(hwnd, L"open", g_kblDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_HUD_UPDATE:
        EnqueueTask(TASK_INSTALL_HUD);
        break;
    case ID_HUD_PICK:
        PickSpatialHudModsFolder();
        break;
    case ID_HUD_FOLDER:
        EnsureDirTree(g_modsDir);
        ShellExecuteW(hwnd, L"open", g_modsDir, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_HUD_DISABLE:
        g_hudDisabled = !g_hudDisabled;
        SaveAppsCfg();
        SetFooterStatus(g_hudDisabled
                            ? L"Spatial HUD disabled - it will not be installed or updated."
                            : L"Spatial HUD enabled.", 0);
        RefreshProjectStatuses();
        break;

    case ID_HUD_REMOVE: {
        wchar_t pat[MAX_PATH + 32], path[MAX_PATH + 64];
        WIN32_FIND_DATAW rfd;
        int removed = 0, blocked = 0;
        _snwprintf(pat, MAX_PATH + 31, L"%s\\spatial-hud*.jar", g_modsDir);
        HANDLE rh = FindFirstFileW(pat, &rfd);
        if (rh != INVALID_HANDLE_VALUE) {
            do {
                _snwprintf(path, MAX_PATH + 63, L"%s\\%s", g_modsDir, rfd.cFileName);
                if (DeleteFileW(path)) removed++;
                else blocked++;
            } while (FindNextFileW(rh, &rfd));
            FindClose(rh);
        }
        if (removed > 0 && blocked == 0)
            SetFooterStatus(L"Mod removed from your mods folder.", 0);
        else if (removed > 0)
            SetFooterStatus(L"Some mod files removed - close Minecraft to remove the rest.", 0);
        else if (blocked > 0)
            SetFooterStatus(L"Could not remove the mod - close Minecraft first, then retry.", 0);
        else
            SetFooterStatus(L"Nothing to remove - the mod is not in your mods folder.", 0);
        RefreshProjectStatuses();
        break;
    }

    case ID_ORCA_UPDATE:
        EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_MENU:
        _snwprintf(p, MAX_PATH, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
        if (FileExistsW(p)) ShellExecuteW(hwnd, L"open", p, NULL, g_orcaBatDir, SW_SHOWNORMAL);
        else EnqueueTask(TASK_INSTALL_ORCA);
        break;
    case ID_ORCA_FOLDER:
        _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\orca_plugins", g_roamApp);
        if (!DirExistsW(p)) _snwprintf(p, MAX_PATH, L"%s\\OrcaSlicer\\plugins", g_roamApp);
        EnsureDirTree(p);
        ShellExecuteW(hwnd, L"open", p, NULL, NULL, SW_SHOWNORMAL);
        break;

    case ID_ORCA_DISABLE:
        g_orcaDisabled = !g_orcaDisabled;
        SaveAppsCfg();
        SetFooterStatus(g_orcaDisabled
                            ? L"OrcaSlicer Plugins disabled - will not be installed or updated."
                            : L"OrcaSlicer Plugins enabled.", 0);
        RefreshProjectStatuses();
        break;

    case ID_ORCA_REMOVE: {
        /* Delete exactly what WorkerInstallOrcaPlugins writes, in every
           %APPDATA%\OrcaSlicer* profile: the two orca_plugins packages (files
           + their folders) and the two legacy top-level plugin files. */
        wchar_t base[MAX_PATH], pat[MAX_PATH], dirp[MAX_PATH], t[MAX_PATH];
        _snwprintf(base, MAX_PATH, L"%s", g_roamApp);
        _snwprintf(pat, MAX_PATH, L"%s\\OrcaSlicer*", base);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pat, &fd);
        int removed = 0, blocked = 0;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                _snwprintf(dirp, MAX_PATH, L"%s\\%s", base, fd.cFileName);
                const wchar_t *rel[] = {
                    L"\\orca_plugins\\wave-overhangs\\wave_overhangs_orca.py",
                    L"\\orca_plugins\\wave-overhangs\\.install_state.json",
                    L"\\orca_plugins\\unlayered-infill\\unlayered_infill_orca.py",
                    L"\\orca_plugins\\unlayered-infill\\.install_state.json",
                    L"\\plugins\\wave_overhangs_orca.py",
                    L"\\plugins\\unlayered_infill_orca.py"
                };
                for (int k = 0; k < 6; k++) {
                    _snwprintf(t, MAX_PATH, L"%s%s", dirp, rel[k]);
                    if (DeleteFileW(t)) removed++;
                    else if (FileExistsW(t)) blocked++;
                }
                _snwprintf(t, MAX_PATH, L"%s\\orca_plugins\\wave-overhangs", dirp);
                RemoveDirectoryW(t);
                _snwprintf(t, MAX_PATH, L"%s\\orca_plugins\\unlayered-infill", dirp);
                RemoveDirectoryW(t);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        if (removed > 0 && blocked == 0)
            SetFooterStatus(L"OrcaSlicer Plugins removed.", 0);
        else if (removed == 0 && blocked == 0)
            SetFooterStatus(L"OrcaSlicer Plugins not installed - nothing to remove.", 0);
        else if (removed > 0 && blocked > 0)
            SetFooterStatus(L"Partially removed OrcaSlicer Plugins - close OrcaSlicer and try again.", 2);
        else
            SetFooterStatus(L"Could not remove OrcaSlicer Plugins - close OrcaSlicer and try again.", 2);
        RefreshProjectStatuses();
        break;
    }

    case ID_AUDIOSWITCHER_OPEN: {
        /* appDir is the suite folder (where Update writes). Until the first
           update lands there, the copy the hub actually found is the one to
           launch - opening nothing would look like a broken button. */
        wchar_t pp[MAX_PATH];
        const wchar_t *asDir = g_cards[6].appDir;
        _snwprintf(pp, MAX_PATH - 1, L"%s\\%s", asDir, g_cards[6].exeName);
        if (!FileExistsW(pp) && g_audioSwitcherDir[0] && _wcsicmp(g_audioSwitcherDir, asDir) != 0) {
            asDir = g_audioSwitcherDir;
            _snwprintf(pp, MAX_PATH - 1, L"%s\\%s", asDir, g_cards[6].exeName);
        }
        if (FileExistsW(pp))
            ShellExecuteW(hwnd, L"open", pp, NULL, asDir, SW_SHOWNORMAL);
        else
            HandleVerb(6, L"install");
        break;
    }
    case ID_AUDIOSWITCHER_UPDATE:
        HandleVerb(6, L"update");
        break;
    case ID_AUDIOSWITCHER_FOLDER: {
        /* Show the folder that holds the exe today; the suite folder is created
           and becomes the target the moment Update is clicked. */
        const wchar_t *asDir = g_cards[6].appDir;
        wchar_t pp[MAX_PATH];
        _snwprintf(pp, MAX_PATH - 1, L"%s\\%s", asDir, g_cards[6].exeName);
        if (!FileExistsW(pp) && g_audioSwitcherDir[0]) asDir = g_audioSwitcherDir;
        EnsureDirTree(asDir);
        ShellExecuteW(hwnd, L"open", asDir, NULL, NULL, SW_SHOWNORMAL);
        break;
    }

    }
}

static LRESULT CALLBACK HubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        RebuildFontsAndIcons(hwnd);
        LoadAppsCfg();
        InitCardDefinitions();
        RefreshProjectStatuses();
        SetTimer(hwnd, 1, 2000, NULL);
        SetTimer(hwnd, 2, 50, NULL);    /* emblem spin repaint */
        EnqueueTask(TASK_CHECK_REMOTE);
        return 0;

    case WM_DPICHANGED: {
        RebuildFontsAndIcons(hwnd);
        const RECT *suggested = (const RECT *)lp;
        if (suggested) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        int dpi = GetWinDpi(hwnd);
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = S(720, dpi);
        mmi->ptMinTrackSize.y = S(460, dpi);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintHubWindow(hwnd, hdc, &ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int dpi = GetWinDpi(hwnd);
        g_scrollY -= (delta > 0) ? S(36, dpi) : -S(36, dpi);
        if (g_scrollY < 0) g_scrollY = 0;
        if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
        if (hit != g_hoverId) {
            g_hoverId = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hoverId != 0) {
            g_hoverId = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hoverId != 0) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_LBUTTONUP: {
        int dpi = GetWinDpi(hwnd);
        RECT cli;
        GetClientRect(hwnd, &cli);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTestButtons(pt, S(76, dpi), cli.bottom - S(42, dpi));
        if (hit != 0) HandleActionCommand(hwnd, hit);
        return 0;
    }

    case WM_RBUTTONUP:
        ShowUtilityMenu(hwnd);
        return 0;

    case WM_COMMAND:
        HandleActionCommand(hwnd, LOWORD(wp));
        return 0;

    case WM_HUB_REFRESH:
        RefreshProjectStatuses();
        return 0;

    case WM_HUB_STATUS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_HUB_SELFUPD: {
        /* Hand the single-instance lock over BEFORE spawning the new build.
           If this old process still holds it, the new hub sees a duplicate
           launch, forwards to us and exits - and once we exit too nothing is
           left running. */
        if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = NULL; }
        HINSTANCE hi = ShellExecuteW(NULL, L"open", g_exePath, NULL, g_exeDir, SW_SHOWNORMAL);
        if ((INT_PTR)hi <= 32) {
            SetFooterStatus(L"Project Hub updated - please reopen it.", 0);
            return 0;   /* stay up rather than vanish with no successor */
        }
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_TIMER:
        if (wp == 2) {
            /* Emblem spin only - keep this light so the animated mark
               never costs a full status sweep. */
            if (g_rcEmblem.right > g_rcEmblem.left)
                InvalidateRect(hwnd, &g_rcEmblem, FALSE);
            if (InterlockedCompareExchange(&g_checkActive, 0, 0) != 0)
                InvalidateRect(hwnd, &g_rcFooterDot, FALSE);
            return 0;
        }
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        RefreshProjectStatuses();
        {
            /* Every 5th tick = 10s - one third of the emblem's 30s
             * rotation: quietly re-check GitHub for Hub & app updates so
             * "Update Hub" lights up on its own. */
            static int tickN = 0;
            if (++tickN >= 30) { tickN = 0; EnqueueTask(TASK_CHECK_REMOTE); }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;
    g_hInst = hInst;

    /* A hub that just updated itself spawns the new build while the old
       process is still shutting down and holding this mutex. Wait briefly
       for the lock instead of instantly treating the fresh copy as a second
       click and quitting - that race is why the hub never reopened after an
       update. A genuine duplicate launch still forwards and exits. */
    BOOL solo = FALSE;
    for (int mi = 0; mi < 12; mi++) {
        g_hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
        if (GetLastError() != ERROR_ALREADY_EXISTS) { solo = TRUE; break; }
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
        HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
        if (ev) { SetEvent(ev); CloseHandle(ev); }
        Sleep(250);
    }
    if (!solo) return 0;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitializeCriticalSection(&g_cs);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    g_qSem      = CreateSemaphoreW(NULL, 0, 64, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    InitAllPaths();
    RemoveLegacyDesktopShortcuts();
    wchar_t oldExe[MAX_PATH];
    _snwprintf(oldExe, MAX_PATH, L"%s.old", g_exePath);
    DeleteFileW(oldExe);

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = HubWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsProjectHubNativeWnd";
    RegisterClassExW(&wc);

    HANDLE hWorker = CreateThread(NULL, 0, HubWorkerThread, NULL, 0, NULL);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(860, dpi), S(700, dpi),
                             NULL, NULL, hInst, NULL);

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_running, 0);
    ReleaseSemaphore(g_qSem, 1, NULL);
    if (hWorker) { WaitForSingleObject(hWorker, 1500); CloseHandle(hWorker); }
    CloseHandle(g_qSem);
    if (g_showEvent) CloseHandle(g_showEvent);
    if (g_hMutex) CloseHandle(g_hMutex);
    DeleteCriticalSection(&g_cs);
    CoUninitialize();
    return 0;
}
