/* ---- one canonical install folder, every copy kept in sync --------------
 * A user browses %LOCALAPPDATA%\Programs\TestsSuite to see the suite, so an
 * update that lands anywhere else looks like "it did not update". The hub
 * itself is usually launched from wherever it was first downloaded (often
 * Downloads), and the old resolution order made THAT folder the install
 * target: the suite copy kept the build from nine days ago forever while the
 * stray copy silently got every update.
 *
 * Rule now: the suite folder is the install target, and every other copy that
 * already exists receives the identical bytes in the same click. Copies are
 * never created where the user did not already have one.
 */
enum { MIRROR_BTG = 0, MIRROR_KBL, MIRROR_DNS, MIRROR_NOTES,
       MIRROR_AUDIO, MIRROR_GENERIC, MIRROR_HUB };

static void SuiteAppPath(wchar_t *out, size_t cap, const wchar_t *sub) {
    _snwprintf(out, cap - 1, L"%s\\%s", g_suiteDir, sub);
    out[cap - 1] = L'\0';
}

/* Left-shorten a folder for the card detail line. PathCompactPathW would do
   this, but shlwapi is deliberately not in the hub's import table. */
static void ShortDirForUi(const wchar_t *dir, wchar_t *out, size_t cap) {
    size_t n = wcslen(dir);
    if (n + 1 <= cap) { wcsncpy(out, dir, cap - 1); out[cap - 1] = L'\0'; return; }
    size_t keep = cap - 4;                 /* room for the leading ellipsis */
    wcsncpy(out, L"...", 3);
    wcsncpy(out + 3, dir + (n - keep), keep);
    out[cap - 1] = L'\0';
}

/* Say where the app actually lives, so a folder that is not being updated is
   visible on the card instead of only in Explorer. */
static void AppendInstallFolder(wchar_t *detail, const wchar_t *dir) {
    wchar_t shortDir[128];
    ShortDirForUi(dir, shortDir, 128);
    wcsncat(detail, L"  \x00B7  ", 255 - wcslen(detail));
    wcsncat(detail, shortDir, 255 - wcslen(detail));
    detail[255] = L'\0';
}

static void AddMirrorDir(int slot, const wchar_t *dir) {
    if (slot < 0 || slot > MIRROR_HUB || !dir || !dir[0]) return;
    for (int i = 0; i < g_mirrorCount[slot]; i++)
        if (_wcsicmp(g_mirrorDirs[slot][i], dir) == 0) return;
    if (g_mirrorCount[slot] >= MAX_MIRRORS) return;
    wcsncpy(g_mirrorDirs[slot][g_mirrorCount[slot]], dir, MAX_PATH - 1);
    g_mirrorDirs[slot][g_mirrorCount[slot]][MAX_PATH - 1] = L'\0';
    g_mirrorCount[slot]++;
}

/* Overwrite every registered copy that is not the one just written. A running
   image cannot be replaced, so a failure here is reported rather than hidden:
   the suite copy - the one the card measures - is already correct. */
static int MirrorFileEverywhere(int slot, const wchar_t *primaryPath) {
    int mirrored = 0;
    for (int i = 0; i < g_mirrorCount[slot]; i++) {
        wchar_t alt[MAX_PATH];
        const wchar_t *name = wcsrchr(primaryPath, L'\\');
        if (!name) return mirrored;
        _snwprintf(alt, MAX_PATH - 1, L"%s%s", g_mirrorDirs[slot][i], name);
        alt[MAX_PATH - 1] = L'\0';
        if (_wcsicmp(alt, primaryPath) == 0) continue;
        /* The suite folder is the managed home, so its copy is created when it
           is missing. Anywhere else, only a copy the user already had is
           touched - the hub must not scatter new folders around the disk. */
        BOOL inSuite = (_wcsicmp(g_mirrorDirs[slot][i], g_suiteDir) == 0) ||
                       (_wcsnicmp(g_mirrorDirs[slot][i], g_suiteDir, wcslen(g_suiteDir)) == 0 &&
                        g_mirrorDirs[slot][i][wcslen(g_suiteDir)] == L'\\');
        if (!FileExistsW(alt) && !inSuite) continue;
        EnsureDirTree(g_mirrorDirs[slot][i]);
        if (CopyFileW(primaryPath, alt, FALSE)) {
            wchar_t ads[MAX_PATH + 24];
            _snwprintf(ads, MAX_PATH + 23, L"%s:Zone.Identifier", alt);
            DeleteFileW(ads);
            mirrored++;
        }
    }
    return mirrored;
}

/* Downloads honour the released marker as well: an update clicked while the
   agent is mid-task still installs the last finished state, never a
   half-written one. Before the first probe of a session there is no marker
   yet, and the branch tip is the best ref available. */
static void EnsureProbeRef(wchar_t *outBranch, size_t cap) {
    if (g_probeRef[0]) {
        _snwprintf(outBranch, cap - 1, L"%s", g_probeRef);
        outBranch[cap - 1] = L'\0';
        return;
    }
    EnsureActiveBranch(outBranch, cap);
}

/* One download, every copy: the suite folder plus any stray copy on disk. */
static BOOL InstallRepoFileEverywhere(int slot, const wchar_t *branch,
                                      const wchar_t *relUrl, const wchar_t *primaryDir,
                                      const wchar_t *fileName, int *outMirrors) {
    wchar_t dest[MAX_PATH];
    DWORD len = 0;
    BYTE *data = DownloadRepoBytes(branch, relUrl, &len);
    if (!data) return FALSE;
    EnsureDirTree(primaryDir);
    _snwprintf(dest, MAX_PATH - 1, L"%s\\%s", primaryDir, fileName);
    dest[MAX_PATH - 1] = L'\0';
    BOOL ok = WriteBinaryFileAtomic(dest, data, len);
    free(data);
    if (!ok) return FALSE;
    int n = MirrorFileEverywhere(slot, dest);
    if (outMirrors) *outMirrors = n;
    return TRUE;
}

/* A tray app started from a stray copy keeps running that stray build, and its
 * single-instance mutex makes the fresh suite copy exit immediately - the user
 * would keep using the old build after a successful update. Close the foreign
 * instance so the copy from the suite folder is the one that comes back up.
 * Only ever called for a copy that is about to be replaced anyway. */
static BOOL StopForeignInstance(const wchar_t *wndClass, const wchar_t *primaryDir) {
    HWND hw = FindWindowW(wndClass, NULL);
    if (!hw) return FALSE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hw, &pid);
    if (!pid) return FALSE;
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
    if (!hp) return FALSE;
    wchar_t img[MAX_PATH] = L"";
    DWORD n = MAX_PATH;
    BOOL foreign = FALSE;
    if (QueryFullProcessImageNameW(hp, 0, img, &n)) {
        wchar_t dir[MAX_PATH];
        wcsncpy(dir, img, MAX_PATH - 1);
        dir[MAX_PATH - 1] = L'\0';
        wchar_t *sl = wcsrchr(dir, L'\\');
        if (sl) *sl = L'\0';
        foreign = (_wcsicmp(dir, primaryDir) != 0);
    }
    if (foreign) {
        PostMessageW(hw, WM_CLOSE, 0, 0);
        if (WaitForSingleObject(hp, 1200) == WAIT_TIMEOUT) TerminateProcess(hp, 0);
        WaitForSingleObject(hp, 1200);
    }
    CloseHandle(hp);
    return foreign;
}

static void InitAllPaths(void) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    wcsncpy(g_exeDir, g_exePath, MAX_PATH - 1);
    wchar_t *sl = wcsrchr(g_exeDir, L'\\');
    if (sl) *sl = L'\0';

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, g_localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", g_localApp, MAX_PATH);
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, g_roamApp)))
        GetEnvironmentVariableW(L"APPDATA", g_roamApp, MAX_PATH);

    _snwprintf(g_hubCfgDir, MAX_PATH, L"%s\\ProjectHub", g_localApp);
    _snwprintf(g_suiteDir,  MAX_PATH, L"%s\\Programs\\TestsSuite", g_localApp);
    EnsureDirTree(g_hubCfgDir);
    EnsureDirTree(g_suiteDir);

    /* The hub keeps a copy of itself in the suite folder too, so the folder
       the user browses always holds the hub build they last updated to - even
       when the hub they launched lives somewhere else, like Downloads. */
    _snwprintf(g_suiteHubExe, MAX_PATH, L"%s\\ProjectHub.exe", g_suiteDir);
    AddMirrorDir(MIRROR_HUB, g_suiteDir);

    wchar_t desk[MAX_PATH] = L"";
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk);

    wchar_t parent[MAX_PATH];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    wchar_t *psl = wcsrchr(parent, L'\\');
    if (psl) *psl = L'\0';

    wchar_t cand[MAX_PATH], lnk[MAX_PATH];

    /* 1. Bluetooth Guard: the suite folder is the install target; every other
       copy already on disk (hub folder, repo checkout, shortcut target, the
       build a running guard was launched from) is kept in sync with it. */
    g_btgDir[0] = L'\0';
    g_btgExtraExe[0] = L'\0';
    wchar_t btgLock[MAX_PATH];
    _snwprintf(btgLock, MAX_PATH, L"%s\\BluetoothGuard\\running.lock", g_localApp);
    char *lockTxt = ReadTextFileUtf8(btgLock, NULL);
    if (lockTxt) {
        unsigned long pid = strtoul(lockTxt, NULL, 10);
        free(lockTxt);
        if (pid > 0) {
            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            if (hp) {
                wchar_t procPath[MAX_PATH] = L"";
                DWORD plen = MAX_PATH;
                if (QueryFullProcessImageNameW(hp, 0, procPath, &plen) && FileExistsW(procPath)) {
                    wcsncpy(g_btgExtraExe, procPath, MAX_PATH - 1);
                }
                CloseHandle(hp);
            }
        }
    }

    wchar_t btgSuite[MAX_PATH];
    SuiteAppPath(btgSuite, MAX_PATH, L"BluetoothGuard");
    _snwprintf(cand, MAX_PATH, L"%s\\BluetoothGuard.exe", g_exeDir);
    BOOL btgInHubDir = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\bluetooth-guard\\BluetoothGuard.exe", parent);
    BOOL btgInSibling = FileExistsW(cand);
    wchar_t btgLnkDir[MAX_PATH] = L"";
    if (desk[0]) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Bluetooth Guard.lnk", desk);
        ResolveShortcutTargetDir(lnk, btgLnkDir);
    }
    _snwprintf(cand, MAX_PATH, L"%s\\BluetoothGuard.exe", btgSuite);
    BOOL btgInSuite = FileExistsW(cand);
    if (btgInSuite) wcsncpy(g_btgDir, btgSuite, MAX_PATH - 1);
    else if (g_btgExtraExe[0]) {   /* the build that is actually running */
        wcsncpy(g_btgDir, g_btgExtraExe, MAX_PATH - 1);
        wchar_t *sl = wcsrchr(g_btgDir, L'\\');
        if (sl) *sl = L'\0';
    }
    else if (btgInHubDir) wcsncpy(g_btgDir, g_exeDir, MAX_PATH - 1);
    else if (btgInSibling) _snwprintf(g_btgDir, MAX_PATH, L"%s\\bluetooth-guard", parent);
    else if (btgLnkDir[0]) wcsncpy(g_btgDir, btgLnkDir, MAX_PATH - 1);
    if (!g_btgDir[0]) wcsncpy(g_btgDir, btgSuite, MAX_PATH - 1);
    AddMirrorDir(MIRROR_BTG, btgSuite);
    if (btgInHubDir) AddMirrorDir(MIRROR_BTG, g_exeDir);
    if (btgInSibling) {
        _snwprintf(cand, MAX_PATH, L"%s\\bluetooth-guard", parent);
        AddMirrorDir(MIRROR_BTG, cand);
    }
    if (btgLnkDir[0]) AddMirrorDir(MIRROR_BTG, btgLnkDir);
    if (g_btgExtraExe[0]) {
        wchar_t extraDir[MAX_PATH];
        wcsncpy(extraDir, g_btgExtraExe, MAX_PATH - 1);
        extraDir[MAX_PATH - 1] = L'\0';
        wchar_t *sl = wcsrchr(extraDir, L'\\');
        if (sl) *sl = L'\0';
        AddMirrorDir(MIRROR_BTG, extraDir);
    }

    /* 2. Keyboard Lighting: same rule - suite folder first, every existing
       copy (repo checkout, shortcut target) registered for the mirror. */
    g_kblDir[0] = L'\0';
    wchar_t kblSuite[MAX_PATH], kblSibling[MAX_PATH];
    SuiteAppPath(kblSuite, MAX_PATH, L"KeyboardLighting");
    _snwprintf(kblSibling, MAX_PATH, L"%s\\keyboard-lighting", parent);
    _snwprintf(cand, MAX_PATH, L"%s\\Tray.ps1", kblSibling);
    BOOL kblInSibling = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\KeyboardLighting.exe", kblSuite);
    BOOL kblInSuite = FileExistsW(cand);
    wchar_t kblLnkDir[MAX_PATH] = L"";
    if (desk[0]) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Keyboard Lighting.lnk", desk);
        ResolveShortcutTargetDir(lnk, kblLnkDir);
    }
    if (!kblLnkDir[0]) {
        _snwprintf(lnk, MAX_PATH, L"%s\\Microsoft\\Windows\\Start Menu\\Programs\\Keyboard Lighting.lnk", g_roamApp);
        ResolveShortcutTargetDir(lnk, kblLnkDir);
    }
    if (kblInSuite) wcsncpy(g_kblDir, kblSuite, MAX_PATH - 1);
    else if (kblInSibling) wcsncpy(g_kblDir, kblSibling, MAX_PATH - 1);
    else if (kblLnkDir[0]) wcsncpy(g_kblDir, kblLnkDir, MAX_PATH - 1);
    else wcsncpy(g_kblDir, kblSuite, MAX_PATH - 1);
    AddMirrorDir(MIRROR_KBL, kblSuite);
    if (kblInSibling) AddMirrorDir(MIRROR_KBL, kblSibling);
    if (kblLnkDir[0]) AddMirrorDir(MIRROR_KBL, kblLnkDir);

    /* 2b. DNS Pilot: suite folder first, existing strays mirrored. */
    g_dnsDir[0] = L'\0';
    wchar_t dnsSuite[MAX_PATH];
    SuiteAppPath(dnsSuite, MAX_PATH, L"DnsPilot");
    _snwprintf(cand, MAX_PATH, L"%s\\DnsPilot.exe", g_exeDir);
    BOOL dnsInHubDir = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\dns-pilot\\DnsPilot.exe", parent);
    BOOL dnsInSibling = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\DnsPilot.exe", dnsSuite);
    if (FileExistsW(cand)) wcsncpy(g_dnsDir, dnsSuite, MAX_PATH - 1);
    else if (dnsInHubDir) wcsncpy(g_dnsDir, g_exeDir, MAX_PATH - 1);
    else if (dnsInSibling) _snwprintf(g_dnsDir, MAX_PATH, L"%s\\dns-pilot", parent);
    else wcsncpy(g_dnsDir, dnsSuite, MAX_PATH - 1);
    AddMirrorDir(MIRROR_DNS, dnsSuite);
    if (dnsInHubDir) AddMirrorDir(MIRROR_DNS, g_exeDir);
    if (dnsInSibling) {
        _snwprintf(cand, MAX_PATH, L"%s\\dns-pilot", parent);
        AddMirrorDir(MIRROR_DNS, cand);
    }

    /* 2c. Agent Notes: suite folder first, existing strays mirrored. */
    g_notesDir[0] = L'\0';
    wchar_t notesSuite[MAX_PATH];
    SuiteAppPath(notesSuite, MAX_PATH, L"AgentNotes");
    _snwprintf(cand, MAX_PATH, L"%s\\AgentNotes.exe", g_exeDir);
    BOOL notesInHubDir = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\agent-notes\\AgentNotes.exe", parent);
    BOOL notesInSibling = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\AgentNotes.exe", notesSuite);
    if (FileExistsW(cand)) wcsncpy(g_notesDir, notesSuite, MAX_PATH - 1);
    else if (notesInHubDir) wcsncpy(g_notesDir, g_exeDir, MAX_PATH - 1);
    else if (notesInSibling) _snwprintf(g_notesDir, MAX_PATH, L"%s\\agent-notes", parent);
    else wcsncpy(g_notesDir, notesSuite, MAX_PATH - 1);
    AddMirrorDir(MIRROR_NOTES, notesSuite);
    if (notesInHubDir) AddMirrorDir(MIRROR_NOTES, g_exeDir);
    if (notesInSibling) {
        _snwprintf(cand, MAX_PATH, L"%s\\agent-notes", parent);
        AddMirrorDir(MIRROR_NOTES, cand);
    }

    /* 2d. Audio Switcher: the suite folder is the install target and the copy
       the card measures. A hub launched from Downloads used to make Downloads
       the target, so the suite folder kept an old build forever. */
    g_audioSwitcherDir[0] = L'\0';
    wchar_t asSuite[MAX_PATH];
    SuiteAppPath(asSuite, MAX_PATH, L"AudioSwitcher");
    _snwprintf(cand, MAX_PATH, L"%s\\AudioSwitcher.exe", g_exeDir);
    BOOL asInHubDir = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\audio-switcher\\AudioSwitcher.exe", parent);
    BOOL asInSibling = FileExistsW(cand);
    _snwprintf(cand, MAX_PATH, L"%s\\AudioSwitcher.exe", asSuite);
    if (FileExistsW(cand)) wcsncpy(g_audioSwitcherDir, asSuite, MAX_PATH - 1);
    else if (asInHubDir) wcsncpy(g_audioSwitcherDir, g_exeDir, MAX_PATH - 1);
    else if (asInSibling) wcsncpy(g_audioSwitcherDir, parent, MAX_PATH - 1);
    else wcsncpy(g_audioSwitcherDir, asSuite, MAX_PATH - 1);
    AddMirrorDir(MIRROR_AUDIO, asSuite);
    if (asInHubDir) AddMirrorDir(MIRROR_AUDIO, g_exeDir);
    if (asInSibling) AddMirrorDir(MIRROR_AUDIO, parent);

    /* 3. OrcaSlicer plugins */
    _snwprintf(cand, MAX_PATH, L"%s\\Orca-Plugins.bat", parent);
    if (FileExistsW(cand)) wcsncpy(g_orcaBatDir, parent, MAX_PATH - 1);
    else _snwprintf(g_orcaBatDir, MAX_PATH, L"%s\\OrcaPlugins", g_suiteDir);

    /* 4. Spatial HUD mods folder */
    wchar_t hudTargetTxt[MAX_PATH];
    _snwprintf(hudTargetTxt, MAX_PATH, L"%s\\SpatialHudUpdater\\target-directory.txt", g_localApp);
    char *savedMods = ReadTextFileUtf8(hudTargetTxt, NULL);
    if (savedMods) {
        unsigned char *s = (unsigned char *)savedMods;
        if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) s += 3;
        size_t n = strlen((char *)s);
        while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = '\0';
        if (n > 2) MultiByteToWideChar(CP_UTF8, 0, (char *)s, -1, g_modsDir, MAX_PATH);
        free(savedMods);
    }
    if (!g_modsDir[0]) {
        _snwprintf(g_modsDir, MAX_PATH, L"%s\\ModrinthApp\\profiles\\F5W\\mods", g_roamApp);
    }
}

static int CheckPidLockStatus(const wchar_t *lockFile, double maxAgeSec, int *outField2, int *outField3) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(lockFile, GetFileExInfoStandard, &fad)) return 0;

    char *txt = ReadTextFileUtf8(lockFile, NULL);
    if (!txt) return 0;
    unsigned long pid = 0;
    int f1 = 0, f2 = 0, f3 = 0;
    sscanf(txt, "%lu|%d|%d|%d", &pid, &f1, &f2, &f3);
    free(txt);
    if (outField2) *outField2 = f2;
    if (outField3) *outField3 = f3;
    if (pid == 0) return 0;

    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hp) return 0;
    DWORD exitCode = 0;
    BOOL alive = GetExitCodeProcess(hp, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hp);
    if (!alive) return 0;

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER uNow, uMod;
    uNow.LowPart = nowFt.dwLowDateTime; uNow.HighPart = nowFt.dwHighDateTime;
    uMod.LowPart = fad.ftLastWriteTime.dwLowDateTime; uMod.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    double ageSec = (uNow.QuadPart > uMod.QuadPart)
                  ? (double)(uNow.QuadPart - uMod.QuadPart) / 10000000.0
                  : 0.0;
    return (ageSec > maxAgeSec) ? 2 : 1;
}

static BOOL CreateShortcutLink(const wchar_t *lnkPath, const wchar_t *targetExe,
                               const wchar_t *workDir, const wchar_t *desc) {
    IShellLinkW *psl = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IShellLinkW, (void **)&psl);
    if (FAILED(hr) || !psl) return FALSE;
    IShellLinkW_SetPath(psl, targetExe);
    IShellLinkW_SetWorkingDirectory(psl, workDir);
    IShellLinkW_SetIconLocation(psl, targetExe, 0);
    IShellLinkW_SetDescription(psl, desc);

    IPersistFile *ppf = NULL;
    BOOL ok = FALSE;
    if (SUCCEEDED(IShellLinkW_QueryInterface(psl, &IID_IPersistFile, (void **)&ppf)) && ppf) {
        ok = SUCCEEDED(IPersistFile_Save(ppf, lnkPath, TRUE));
        IPersistFile_Release(ppf);
    }
    IShellLinkW_Release(psl);
    return ok;
}

/* The Desktop is not ours to fill in. Installs and updates used to drop a
   shortcut there for every app on every run, which is how they piled up;
   the owner asked for that to stop. A shortcut the owner pinned is still
   repointed so it never dangles at a replaced copy after an update, but a
   Desktop shortcut is CREATED only by the explicit pin item in the emblem
   menu. Everything on the install and update paths goes through here. */
/* The suite used to keep a Desktop shortcut per app and re-create them on
   every update until the owner asked for it to stop. New builds create
   none, but the old icons were still sitting there. Delete the five known
   names exactly once: the flag file is what guarantees a shortcut the
   owner pins later, through the emblem menu, is never touched by any
   future run. */
static void RemoveLegacyDesktopShortcuts(void) {
    wchar_t flag[MAX_PATH], desk[MAX_PATH], p[MAX_PATH];
    _snwprintf(flag, MAX_PATH - 1, L"%s\\desktop_cleanup_done", g_hubCfgDir);
    flag[MAX_PATH - 1] = L'\0';
    if (GetFileAttributesW(flag) != INVALID_FILE_ATTRIBUTES) return;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) {
        static const wchar_t *const names[5] = {
            L"Audio Switcher.lnk", L"Bluetooth Guard.lnk", L"DNS Pilot.lnk",
            L"Keyboard Lighting.lnk", L"Project Hub.lnk"
        };
        for (int i = 0; i < 5; i++) {
            _snwprintf(p, MAX_PATH - 1, L"%s\\%s", desk, names[i]);
            p[MAX_PATH - 1] = L'\0';
            DeleteFileW(p);
        }
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    CreateDirectoryW(g_hubCfgDir, NULL);
    WriteBinaryFileAtomic(flag, (const BYTE *)"1", 1);
}

static BOOL RepointDesktopShortcutIfPresent(const wchar_t *lnkPath, const wchar_t *targetExe,
                                            const wchar_t *workDir, const wchar_t *desc) {
    if (GetFileAttributesW(lnkPath) == INVALID_FILE_ATTRIBUTES) return FALSE;
    return CreateShortcutLink(lnkPath, targetExe, workDir, desc);
}


