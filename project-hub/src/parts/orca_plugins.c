/* Independent Hub views of the existing Orca packages. The legacy catalogue
   id "orca", package ids, Python filenames and capability names stay stable.
   No plugin algorithm, preset reference or user configuration is renamed. */
typedef struct {
    int card, updateId, folderId, removeId, disableId, task;
    const wchar_t *name, *packageId, *file, *repo;
} HubOrcaPlugin;

static const HubOrcaPlugin g_orcaPlugins[2] = {
    { 3, ID_ORCA_UPDATE, ID_ORCA_FOLDER, ID_ORCA_REMOVE, ID_ORCA_DISABLE,
      TASK_INSTALL_ORCA_WAVE, L"Undahang", L"wave-overhangs",
      L"wave_overhangs_orca.py", L"plugins/wave-overhangs/wave_overhangs_orca.py" },
    { 7, ID_INFILL_UPDATE, ID_INFILL_FOLDER, ID_INFILL_REMOVE, ID_INFILL_DISABLE,
      TASK_INSTALL_ORCA_INFILL, L"Unlayered Infill", L"unlayered-infill",
      L"unlayered_infill_orca.py", L"plugins/unlayered-infill/unlayered_infill_orca.py" }
};

static void RefreshProjectStatuses(void);

static BOOL OrcaPluginDisabled(int plugin) {
    return plugin == 0 ? g_orcaDisabled : g_infillDisabled;
}

static void InitOrcaCard(int plugin) {
    const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
    ProjectCard *c = &g_cards[spec->card];
    BOOL initialized = c->btnCount == 4 && c->btns[0].id == spec->updateId;
    ActionBtn previous[4];
    memcpy(previous, c->btns, sizeof(previous));
    c->resIconId = plugin == 0 ? RES_ICON_UNDAHANG : RES_ICON_ORCA;
    c->category = HUB_GROUP_ORCA;
    c->title = spec->name;
    c->blurb = plugin == 0 ? L"Wave overhang paths" : L"Nonplanar internal infill";
    wcscpy(c->id, plugin == 0 ? L"orca" : L"unlayered-infill");
    c->exeRepo = c->exeName = c->launchArgs = NULL;
    c->shortcutName = spec->name;
    c->btnCount = 4;
    c->btns[0] = (ActionBtn){ spec->updateId, L"Update", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    c->btns[1] = (ActionBtn){ spec->folderId, L"Folder", BTN_SECONDARY, 68, {0,0,0,0}, NULL, FALSE };
    c->btns[2] = (ActionBtn){ spec->removeId, L"Remove", BTN_SECONDARY, 74, {0,0,0,0}, NULL, FALSE };
    c->btns[3] = (ActionBtn){ spec->disableId, L"Disable", BTN_SECONDARY, 78, {0,0,0,0}, NULL, FALSE };
    /* A background catalogue refresh must not briefly reveal Update on a
       current/disabled card while the rest of the remote check is in flight. */
    if (initialized) {
        for (int b = 0; b < 4; b++) {
            c->btns[b].hidden = previous[b].hidden;
            c->btns[b].style = previous[b].style;
            c->btns[b].label = previous[b].label;
        }
    } else c->btns[0].hidden = TRUE;
}

static void OrcaPackageDir(const HubOrcaPlugin *spec, const wchar_t *profile,
                            wchar_t *out) {
    _snwprintf(out, MAX_PATH - 1, L"%s\\%s\\orca_plugins\\%s", g_roamApp, profile, spec->packageId);
    out[MAX_PATH - 1] = L'\0';
}

static void OrcaLegacyFile(const HubOrcaPlugin *spec, const wchar_t *profile,
                            wchar_t *out) {
    _snwprintf(out, MAX_PATH - 1, L"%s\\%s\\plugins\\%s", g_roamApp, profile, spec->file);
    out[MAX_PATH - 1] = L'\0';
}

static HANDLE FindOrcaProfiles(WIN32_FIND_DATAW *data) {
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH - 1, L"%s\\OrcaSlicer*", g_roamApp);
    pattern[MAX_PATH - 1] = L'\0';
    return FindFirstFileW(pattern, data);
}

typedef struct {
    int files;
    BOOL unknown, needsUpdate, newer, mixed;
    char version[32];
    wchar_t folder[MAX_PATH];
} HubOrcaStatus;

static void ProbeOrcaFile(const wchar_t *file, const char *remote, HubOrcaStatus *status) {
    if (!FileExistsW(file)) return;
    status->files++;
    char version[32] = "";
    char *text = ReadTextFileUtf8(file, NULL);
    if (text) { ExtractQuotedAfter(text, "PLUGIN_VERSION", version, sizeof(version)); free(text); }
    if (!version[0]) { status->unknown = TRUE; return; }
    if (remote && remote[0]) {
        if (VerNewer(remote, version)) status->needsUpdate = TRUE;
        if (VerNewer(version, remote)) status->newer = TRUE;
    }
    if (!status->version[0]) strcpy(status->version, version);
    else if (strcmp(status->version, version) != 0) status->mixed = TRUE;
}

static HubOrcaStatus ProbeOrcaPlugin(int plugin, const char *remote) {
    const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
    HubOrcaStatus status;
    ZeroMemory(&status, sizeof(status));
    WIN32_FIND_DATAW data;
    HANDLE find = FindOrcaProfiles(&data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wchar_t dir[MAX_PATH], file[MAX_PATH];
            OrcaPackageDir(spec, data.cFileName, dir);
            _snwprintf(file, MAX_PATH - 1, L"%s\\%s", dir, spec->file);
            file[MAX_PATH - 1] = L'\0';
            if (!status.folder[0] && FileExistsW(file)) wcscpy(status.folder, dir);
            ProbeOrcaFile(file, remote, &status);
            OrcaLegacyFile(spec, data.cFileName, file);
            if (!status.folder[0] && FileExistsW(file)) {
                wcscpy(status.folder, file);
                wchar_t *slash = wcsrchr(status.folder, L'\\');
                if (slash) *slash = L'\0';
            }
            ProbeOrcaFile(file, remote, &status);
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    return status;
}

static void RefreshOrcaCard(int plugin, const char *remote) {
    const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
    ProjectCard *c = &g_cards[spec->card];
    HubOrcaStatus status = ProbeOrcaPlugin(plugin, remote);
    BOOL disabled = OrcaPluginDisabled(plugin);
    BOOL known = remote && remote[0];
    c->btns[0].label = status.files ? L"Update" : L"Install";
    c->btns[0].hidden = disabled || (status.files && (!known || !status.needsUpdate));
    c->btns[0].style = !status.files ? BTN_PRIMARY : (status.needsUpdate ? BTN_AMBER : BTN_SECONDARY);
    c->btns[2].hidden = !status.files;
    c->btns[3].label = disabled ? L"Enable" : L"Disable";
    c->badgeKind = BADGE_OK;
    const wchar_t *badge = L"Up to Date";
    if (disabled) { badge = L"Disabled"; c->badgeKind = BADGE_IDLE; }
    else if (!status.files) { badge = L"Setup Ready"; c->badgeKind = BADGE_WARN; }
    else if (status.needsUpdate) { badge = L"Update Ready"; c->badgeKind = BADGE_WARN; }
    else if (status.unknown) { badge = L"Version unknown"; c->badgeKind = BADGE_WARN; }
    else if (!known) { badge = L"Installed"; c->badgeKind = BADGE_SKY; }
    else if (status.newer) { badge = L"Newer installed"; c->badgeKind = BADGE_SKY; }
    wcsncpy(c->badge, badge, 47); c->badge[47] = L'\0';
    if (disabled)
        _snwprintf(c->detail, 255, L"Hub installs/updates paused; existing plugin settings retained");
    else if (!status.files)
        _snwprintf(c->detail, 255, L"%s - Click Install to set up", c->blurb);
    else
        _snwprintf(c->detail, 255, L"%s%S%s", status.mixed ? L"Mixed installations; " : L"Installed ",
                   status.version[0] ? status.version : "version unknown",
                   known ? L" - restart OrcaSlicer after changes" : L" - update check pending");
    c->detail[255] = L'\0';
}

/* Never reset an existing host enable choice or user settings during update.
   CREATE_NEW also preserves a state file created concurrently by the host. */
static BOOL EnsureNewOrcaState(const wchar_t *dir) {
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH - 1, L"%s\\.install_state.json", dir);
    path[MAX_PATH - 1] = L'\0';
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS;
    }
    const char json[] = "{\"enabled\": true}\n";
    DWORD written = 0;
    BOOL ok = WriteFile(file, json, (DWORD)strlen(json), &written, NULL) &&
              written == strlen(json) && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) DeleteFileW(path);
    return ok;
}

static BOOL InstallOrcaProfile(const HubOrcaPlugin *spec, const wchar_t *profile,
                                const wchar_t *source) {
    wchar_t dir[MAX_PATH], file[MAX_PATH], legacy[MAX_PATH];
    OrcaPackageDir(spec, profile, dir);
    EnsureDirTree(dir);
    _snwprintf(file, MAX_PATH - 1, L"%s\\%s", dir, spec->file);
    file[MAX_PATH - 1] = L'\0';
    BOOL modernOk = CopyFileW(source, file, FALSE);
    OrcaLegacyFile(spec, profile, legacy);
    wchar_t parent[MAX_PATH];
    wcscpy(parent, legacy);
    wchar_t *slash = wcsrchr(parent, L'\\');
    if (slash) *slash = L'\0';
    EnsureDirTree(parent);
    BOOL legacyOk = CopyFileW(source, legacy, FALSE);
    BOOL stateOk = modernOk && EnsureNewOrcaState(dir);
    return modernOk && legacyOk && stateOk;
}

static void WorkerInstallOrcaPlugin(int plugin) {
    if (plugin < 0 || plugin >= 2) return;
    const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
    if (OrcaPluginDisabled(plugin)) {
        SetFooterStatus(L"This Orca plugin is disabled for Hub installs/updates. Enable its card first.", 0);
        return;
    }
    wchar_t branch[64], source[MAX_PATH], bat[MAX_PATH], message[256];
    EnsureProbeRef(branch, 64);
    _snwprintf(message, 255, L"Downloading %s...", spec->name);
    message[255] = L'\0'; SetFooterStatus(message, 40);
    _snwprintf(source, MAX_PATH - 1, L"%s\\%s", g_hubCfgDir, spec->file);
    source[MAX_PATH - 1] = L'\0';
    if (!DownloadRawRepoFile(branch, spec->repo, source)) {
        SetFooterStatus(L"Plugin download failed; no installed plugin was changed.", 0);
        return;
    }
    /* Retain the shared legacy menu/installer route, not a second visible card. */
    EnsureDirTree(g_orcaBatDir);
    _snwprintf(bat, MAX_PATH - 1, L"%s\\Orca-Plugins.bat", g_orcaBatDir);
    bat[MAX_PATH - 1] = L'\0';
    BOOL batOk = DownloadRawRepoFile(branch, L"Orca-Plugins.bat", bat);
    WIN32_FIND_DATAW data;
    HANDLE find = FindOrcaProfiles(&data);
    int profiles = 0, failed = 0;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            profiles++;
            if (!InstallOrcaProfile(spec, data.cFileName, source)) failed++;
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    if (!profiles && !InstallOrcaProfile(spec, L"OrcaSlicer", source)) failed++;
    _snwprintf(message, 255, failed
        ? L"%s was not fully written - close OrcaSlicer and retry. Other plugin untouched."
        : (batOk ? L"%s updated - restart OrcaSlicer to load."
                 : L"%s updated; legacy menu download failed. Restart OrcaSlicer to load."), spec->name);
    message[255] = L'\0';
    SetFooterStatus(message, failed ? 2 : 0);
    if (g_hwnd) PostMessageW(g_hwnd, WM_HUB_REFRESH, 0, 0);
}

static void RemoveOrcaPlugin(int plugin) {
    const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
    WIN32_FIND_DATAW data;
    HANDLE find = FindOrcaProfiles(&data);
    int removed = 0, blocked = 0;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wchar_t dir[MAX_PATH], files[3][MAX_PATH];
            OrcaPackageDir(spec, data.cFileName, dir);
            _snwprintf(files[0], MAX_PATH - 1, L"%s\\%s", dir, spec->file);
            _snwprintf(files[1], MAX_PATH - 1, L"%s\\.install_state.json", dir);
            OrcaLegacyFile(spec, data.cFileName, files[2]);
            files[0][MAX_PATH - 1] = files[1][MAX_PATH - 1] = L'\0';
            for (int i = 0; i < 3; i++) {
                /* A locked script still belongs to the installed package;
                   keep its host enable state until removal actually succeeds. */
                if (i == 1 && FileExistsW(files[0])) continue;
                if (DeleteFileW(files[i])) removed++;
                else if (FileExistsW(files[i])) blocked++;
            }
            /* Nonrecursive: leave user configuration and unrelated packages. */
            RemoveDirectoryW(dir);
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    wchar_t message[256];
    _snwprintf(message, 255, blocked
        ? L"%s could not be fully removed - close OrcaSlicer and retry."
        : (removed ? L"%s removed; other plugin and user settings retained."
                   : L"%s is not installed - nothing to remove."), spec->name);
    message[255] = L'\0'; SetFooterStatus(message, blocked ? 2 : 0);
}

static BOOL HandleOrcaPluginCommand(HWND hwnd, int command) {
    for (int plugin = 0; plugin < 2; plugin++) {
        const HubOrcaPlugin *spec = &g_orcaPlugins[plugin];
        if (command == spec->updateId) { EnqueueTask(spec->task); return TRUE; }
        if (command == spec->folderId) {
            HubOrcaStatus status = ProbeOrcaPlugin(plugin, NULL);
            if (!status.folder[0]) OrcaPackageDir(spec, L"OrcaSlicer", status.folder);
            EnsureDirTree(status.folder);
            ShellExecuteW(hwnd, L"open", status.folder, NULL, NULL, SW_SHOWNORMAL);
            return TRUE;
        }
        if (command == spec->removeId) {
            RemoveOrcaPlugin(plugin); RefreshProjectStatuses(); return TRUE;
        }
        if (command == spec->disableId) {
            if (plugin == 0) g_orcaDisabled = !g_orcaDisabled;
            else g_infillDisabled = !g_infillDisabled;
            SaveAppsCfg();
            SetFooterStatus(L"Only this plugin's Hub install/update preference changed. Host settings are unchanged.", 0);
            RefreshProjectStatuses(); return TRUE;
        }
    }
    return FALSE;
}
