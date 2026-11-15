static void EnqueueTask(int task) {
    EnterCriticalSection(&g_cs);
    int next = (g_qTail + 1) % 32;
    if (next != g_qHead) {
        g_taskQueue[g_qTail] = task;
        g_qTail = next;
        ReleaseSemaphore(g_qSem, 1, NULL);
    }
    LeaveCriticalSection(&g_cs);
}

static DWORD WINAPI HubWorkerThread(LPVOID unused) {
    (void)unused;
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        if (WaitForSingleObject(g_qSem, INFINITE) != WAIT_OBJECT_0) break;
        if (InterlockedCompareExchange(&g_running, 1, 1) == 0) break;

        int task = TASK_NONE;
        EnterCriticalSection(&g_cs);
        if (g_qHead != g_qTail) {
            task = g_taskQueue[g_qHead];
            g_qHead = (g_qHead + 1) % 32;
        }
        LeaveCriticalSection(&g_cs);

        switch (task) {
        case TASK_CHECK_REMOTE:        WorkerCheckRemote(); break;
        case TASK_INSTALL_BTG:         WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_OPEN_OR_INSTALL_BTG: WorkerInstallBluetoothGuard(TRUE); break;
        case TASK_INSTALL_KBL:         WorkerInstallKeyboardLighting(); break;
        case TASK_INSTALL_HUD:         WorkerInstallSpatialHud(); break;
        case TASK_INSTALL_ORCA:        WorkerInstallOrcaPlugins(); break;
        case TASK_INSTALL_DNS:         WorkerInstallDnsPilot(FALSE); break;
        case TASK_OPEN_OR_INSTALL_DNS: WorkerInstallDnsPilot(TRUE); break;
        case TASK_INSTALL_NOTES:         WorkerInstallNotes(FALSE); break;
        case TASK_OPEN_OR_INSTALL_NOTES: WorkerInstallNotes(TRUE); break;
        case TASK_INSTALL_DYN:           WorkerInstallGeneric(g_dynIdx, FALSE); break;
        case TASK_OPEN_OR_INSTALL_DYN:   WorkerInstallGeneric(g_dynIdx, TRUE); break;
        case TASK_UPDATE_ALL:
            /* Every built-in app has to be refreshed here or its suite folder
               keeps the old build: slots 0-5 have dedicated workers (each with
               its own MIRROR_ set), and slots 6+ (Audio Switcher and catalog
               cards) go through the generic worker. Agent Notes
               (slot 5) was the one dedicated worker missing from this list, so
               "Update All" left its suite copy stale while every other app
               updated. */
            WorkerInstallBluetoothGuard(FALSE);
            WorkerInstallKeyboardLighting();
            WorkerInstallOrcaPlugins();
            WorkerInstallSpatialHud();
            WorkerInstallDnsPilot(FALSE);
            WorkerInstallNotes(FALSE);
            for (int i = 6; i < g_cardCount; i++) WorkerInstallGeneric(i, FALSE);
            WorkerCheckRemote();
            SetFooterStatus(L"All suite apps updated!", 0);
            break;
        case TASK_SELF_UPDATE:         WorkerSelfUpdateHub(); break;
        default: break;
        }
    }
    return 0;
}

static void RestartKeyboardLightingCleanly(void) {
    wchar_t lp[MAX_PATH];
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\running.lock", g_localApp);
    StopProcessFromLock(lp);
    _snwprintf(lp, MAX_PATH, L"%s\\KeyboardLighting\\engine.pid", g_localApp);
    StopProcessFromLock(lp);

    wchar_t exe[MAX_PATH], rstBat[MAX_PATH];
    _snwprintf(exe, MAX_PATH, L"%s\\KeyboardLighting.exe", g_kblDir);
    _snwprintf(rstBat, MAX_PATH, L"%s\\Restart.bat", g_kblDir);
    if (FileExistsW(exe))
        ShellExecuteW(g_hwnd, L"open", exe, NULL, g_kblDir, SW_SHOWNORMAL);
    else if (FileExistsW(rstBat))
        ShellExecuteW(g_hwnd, L"open", rstBat, NULL, g_kblDir, SW_SHOWNORMAL);
    else
        EnqueueTask(TASK_INSTALL_KBL);
    SetFooterStatus(L"Restarted Keyboard Lighting cleanly.", 0);
}

static INT_PTR PromptTextInput(HWND hwnd, const wchar_t *label, const wchar_t *preset,
                               wchar_t *out, int outCap, BOOL withBrowse,
                               const wchar_t *okLabel);

static void NormalizeDirPath(wchar_t *s) {
    /* People paste from Explorer: quotes, stray spaces and a trailing
       backslash all happen. Clean them so the path is usable as-is. */
    int n = 0;
    while (s[n] == L' ' || s[n] == L'\t') n++;
    if (n > 0) memmove(s, s + n, (wcslen(s + n) + 1) * sizeof(wchar_t));
    n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t')) s[--n] = L'\0';
    if (n >= 2 && s[0] == L'"' && s[n - 1] == L'"') {
        memmove(s, s + 1, (size_t)(n - 2) * sizeof(wchar_t));
        s[n - 2] = L'\0';
    }
    n = (int)wcslen(s);
    while (n > 3 && (s[n - 1] == L'\\' || s[n - 1] == L'/')) s[--n] = L'\0';
}

static void SaveModsDirFromInput(wchar_t *input) {
    NormalizeDirPath(input);
    if (!input[0]) return;
    DWORD attr = GetFileAttributesW(input);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SetFooterStatus(L"That is a file - enter the folder that contains your mods.", 0);
        return;
    }
    wcsncpy(g_modsDir, input, MAX_PATH - 1);
    g_modsDir[MAX_PATH - 1] = L'\0';
    EnsureDirTree(g_modsDir);
    wchar_t saveDir[MAX_PATH], saveFile[MAX_PATH];
    _snwprintf(saveDir, MAX_PATH, L"%s\\SpatialHudUpdater", g_localApp);
    EnsureDirTree(saveDir);
    _snwprintf(saveFile, MAX_PATH, L"%s\\target-directory.txt", saveDir);
    char utf8[MAX_PATH * 3];
    int n = WideCharToMultiByte(CP_UTF8, 0, g_modsDir, -1, utf8, sizeof(utf8), NULL, NULL);
    if (n > 1) WriteBinaryFileAtomic(saveFile, (const BYTE *)utf8, (DWORD)(n - 1));
    SetFooterStatus(L"Mods folder set.", 0);
    RefreshProjectStatuses();
}

static void PickSpatialHudModsFolder(void) {
    wchar_t input[MAX_PATH];
    wcsncpy(input, g_modsDir, MAX_PATH - 1);
    input[MAX_PATH - 1] = L'\0';
    INT_PTR r = PromptTextInput(g_hwnd,
                        L"Mods folder path - paste or type it, or click Browse:",
                        input, input, MAX_PATH, TRUE, L"Use Folder");
    if (r == -1) {
        /* Dialog could not be created - never leave the button dead. */
        BROWSEINFOW bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.hwndOwner = g_hwnd;
        bi.lpszTitle = L"Select your Minecraft / Modrinth 'mods' folder:";
        bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            if (SHGetPathFromIDListW(pidl, input) && input[0])
                SaveModsDirFromInput(input);
            CoTaskMemFree(pidl);
        }
        return;
    }
    if (r != IDOK) return;
    SaveModsDirFromInput(input);
}

