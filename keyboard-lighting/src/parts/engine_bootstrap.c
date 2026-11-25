/* ============================================================================
 *  Engine Lifecycle & Matched Standalone Engine
 * ============================================================================ */

static BOOL IsEngineAlive(void) {
    if (!g_engineProc) return FALSE;
    DWORD code = 0;
    if (GetExitCodeProcess(g_engineProc, &code) && code == STILL_ACTIVE) return TRUE;
    CloseHandle(g_engineProc);
    g_engineProc = NULL;
    g_enginePid = 0;
    return FALSE;
}

static void KillPidFromFile(const wchar_t *path) {
    char *txt = ReadFileUtf8(path, NULL);
    if (txt) {
        unsigned long pid = strtoul(txt, NULL, 10);
        free(txt);
        if (pid > 0 && pid != GetCurrentProcessId()) {
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
            if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
        }
    }
    DeleteFileW(path);
}

static void StopEngine(BOOL graceful) {
    SyncMasterBrightness();
    BrightnessStop(&g_brightness);
    FlushPendingBrightnessCfg();
    if (graceful && (IsEngineAlive() || FileExistsW(g_engPidPath))) {
        WriteAtomicUtf8(g_stopPath, "1");
        if (g_engineProc) WaitForSingleObject(g_engineProc, 450);
        else Sleep(200);
    }
    if (g_engineProc) {
        TerminateProcess(g_engineProc, 0);
        CloseHandle(g_engineProc);
        g_engineProc = NULL;
        g_enginePid = 0;
    }
    KillPidFromFile(g_engPidPath);
    DeleteFileW(g_stopPath);
}

/* The controller and engine protocol ship as one unit, even for an EXE-only
   install/update. Prefer an identical adjacent script (normal Hub layout),
   otherwise materialize this EXE's ASCII resource under a separate cache name.
   Never overwrite the legacy engine script or fetch an unrelated branch tip. */
static const wchar_t *PrepareBundledEngine(void) {
    HMODULE module = GetModuleHandleW(NULL);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(RES_ENGINE_SCRIPT), RT_RCDATA);
    if (!resource) return NULL;
    DWORD size = SizeofResource(module, resource);
    HGLOBAL loaded = LoadResource(module, resource);
    const char *data = loaded ? (const char *)LockResource(loaded) : NULL;
    if (!data || !size || size > 1024 * 1024) return NULL;
    DWORD length = 0;
    char *current = ReadFileUtf8(g_engineScript, &length);
    BOOL same = current && length == size && !memcmp(current, data, size);
    free(current);
    if (same) return g_engineScript;
    static wchar_t cached[MAX_PATH];
    int n = _snwprintf(cached, MAX_PATH, L"%s\\Aura-Native-Background.ps1", g_cfgDir);
    if (n < 0 || n >= MAX_PATH) return NULL;
    current = ReadFileUtf8(cached, &length);
    same = current && length == size && !memcmp(current, data, size);
    free(current);
    if (!same) {
        char *text = (char *)malloc((size_t)size + 1);
        if (!text) return NULL;
        memcpy(text, data, size); text[size] = 0;
        BOOL ok = WriteAtomicUtf8(cached, text);
        free(text);
        if (!ok) return NULL;
    }
    return cached;
}

static void StartEngine(void) {
    StopEngine(FALSE);
    g_wantOff = FALSE;

    const wchar_t *script = PrepareBundledEngine();
    if (!script) {
        wcsncpy(g_statusText, L"Could not prepare the matching lighting engine. Reinstall Prism Lighting.", 255);
        return;
    }

    WriteLiveBrightness();
    WriteThemeJson();

    char kCols[128] = "", kWds[128] = "", bCols[128] = "", bWds[128] = "", c0[8] = "#38BDF8", c1[8] = "#818CF8";
    size_t o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    for (int i = 0; i < g_cfg.kbd.swatchCount; i++) {
        char hx[8];
        FormatHexColor(g_cfg.kbd.swatches[i], hx);
        if (i == 0) strncpy(c0, hx, 7);
        if (i == 1) strncpy(c1, hx, 7);
        o1 += (size_t)snprintf(kCols + o1, sizeof(kCols) - o1, "%s%s", i ? "," : "", hx);
        o2 += (size_t)snprintf(kWds + o2, sizeof(kWds) - o2, "%s%.3f", i ? "," : "", g_cfg.kbd.widths[i]);
    }
    for (int i = 0; i < g_cfg.bar.swatchCount; i++) {
        char hx[8];
        FormatHexColor(g_cfg.bar.swatches[i], hx);
        o3 += (size_t)snprintf(bCols + o3, sizeof(bCols) - o3, "%s%s", i ? "," : "", hx);
        o4 += (size_t)snprintf(bWds + o4, sizeof(bWds) - o4, "%s%.3f", i ? "," : "", g_cfg.bar.widths[i]);
    }

    wchar_t cmd[2048];
    _snwprintf(cmd, 2047,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\""
        L" -Effect %S -Speed %.2f -Brightness %.3f -Master %.3f -Fps 60 -Quiet"
        L" -OwnerPid %lu -NativeBrightness -OnExit %S -Smooth %S"
        L" -Colors \"%S\" -Widths \"%S\" -Color \"%S\" -Color2 \"%S\""
        L"%s%s -Equalise %s -Layout %s%s"
        L" -BarEffect %S -BarSpeed %.2f -BarBrightness %.3f"
        L" -BarColors \"%S\" -BarWidths \"%S\""
        L"%s%s -BarEqualise %s -BarLayout %s%s%s",
        script,
        kEffects[g_cfg.kbd.effectIdx].token,
        (double)g_cfg.kbd.speed / 10.0,
        (double)g_cfg.kbd.brightness / 100.0,
        (double)g_brightness.level / 1000.0,
        (unsigned long)GetCurrentProcessId(),
        g_cfg.onExit[0] ? g_cfg.onExit : "off",
        g_cfg.smooth ? "on" : "off",
        kCols, kWds, c0, c1,
        g_cfg.kbd.mirror ? L" -Mirror" : L"",
        g_cfg.kbd.reverse ? L" -Reverse" : L"",
        g_cfg.kbd.equalise ? L"on" : L"off",
        g_cfg.kbd.loop ? L"loop" : L"across",
        g_cfg.kbd.on ? L"" : L" -KbdOff",
        kEffects[g_cfg.bar.effectIdx].token,
        (double)g_cfg.bar.speed / 10.0,
        (double)g_cfg.bar.brightness / 100.0,
        bCols, bWds,
        g_cfg.bar.mirror ? L" -BarMirror" : L"",
        g_cfg.bar.reverse ? L" -BarReverse" : L"",
        g_cfg.bar.equalise ? L"on" : L"off",
        g_cfg.bar.loop ? L"loop" : L"across",
        g_cfg.bar.on ? L"" : L" -BarOff",
        g_cfg.overlay ? L" -OverlayOn" : L"");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_exeDir, &si, &pi)) {
        CloseHandle(pi.hThread);
        g_engineProc = pi.hProcess;
        g_enginePid  = pi.dwProcessId;
        char pidBuf[32];
        snprintf(pidBuf, sizeof(pidBuf), "%lu\r\n", (unsigned long)g_enginePid);
        WriteAtomicUtf8(g_engPidPath, pidBuf);
        _snwprintf(g_statusText, 255, L"Active  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
                   g_cfg.kbd.on ? kEffects[g_cfg.kbd.effectIdx].label : "Off",
                   g_cfg.bar.on ? kEffects[g_cfg.bar.effectIdx].label : "Off");
    } else {
        wcsncpy(g_statusText, L"Could not launch lighting engine process.", 255);
    }
}

static void TurnAllLightsOff(void) {
    StopEngine(TRUE);
    g_wantOff = TRUE;
    const wchar_t *script = PrepareBundledEngine();
    if (script) {
        wchar_t cmd[768];
        _snwprintf(cmd, 767, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\" -Effect off -Quiet", script);
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_exeDir, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    wcsncpy(g_statusText, L"Lighting turned off  \x00B7  Click Lights toggle to resume", 255);
}

/* A user who turns the Startup toggle OFF gets a marker file so the logon
   task is not silently re-created on the next elevated start. */
static void MarkAutostartOff(BOOL off) {
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH - 1, L"%s\\autostart.off", g_cfgDir);
    p[MAX_PATH - 1] = L'\0';
    if (off) {
        HANDLE hf = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_HIDDEN, NULL);
        if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf);
    } else {
        DeleteFileW(p);
    }
}

static BOOL AutostartMarkedOff(void) {
    wchar_t p[MAX_PATH];
    _snwprintf(p, MAX_PATH - 1, L"%s\\autostart.off", g_cfgDir);
    p[MAX_PATH - 1] = L'\0';
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static BOOL TaskExists(const wchar_t *name) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    wchar_t cmd[MAX_PATH];
    _snwprintf(cmd, MAX_PATH - 1, L"schtasks.exe /Query /TN \"%s\"", name);
    cmd[MAX_PATH - 1] = L'\0';
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return FALSE;
    WaitForSingleObject(pi.hProcess, 2500);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (code == 0);
}

static BOOL QueryAutostart(void) {
    if (TaskExists(L"TestsSuite-KeyboardLighting")) return TRUE;
    return TaskExists(L"KeyboardLighting");   /* pre-unification task name */
}

static BOOL SetKblAutostart(BOOL enable) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[MAX_PATH * 2];
    DWORD code = 1;
    if (enable) {
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        /* One task for everything: the same name EnsureLogonTask keeps
           fresh, started quietly in the tray at logon (no UAC, no window).
           Always /RL HIGHEST and never an elevation-suppressing flag: a
           limited-privilege task would start a requireAdministrator exe
           unelevated (the scheduler does not prompt) and the old suppressing
           flag then blocked the app's own relaunch, pinning it in preview
           mode for the session. Registering a HIGHEST task needs admin, so a
           non-elevated session reports the toggle as failed instead of
           creating that trap. */
        _snwprintf(cmd, MAX_PATH * 2 - 1,
                   L"schtasks.exe /Create /TN \"TestsSuite-KeyboardLighting\" /TR \"\\\"%s\\\" -Silent\" /SC ONLOGON /RL HIGHEST /F",
                   g_exePath);
        cmd[MAX_PATH * 2 - 1] = L'\0';
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            return FALSE;
        WaitForSingleObject(pi.hProcess, 3500);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (code != 0) return FALSE;
    } else {
        /* disable: remove the unified task AND the legacy name */
        const wchar_t *names[2] = { L"TestsSuite-KeyboardLighting", L"KeyboardLighting" };
        for (int i = 0; i < 2; i++) {
            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            _snwprintf(cmd, MAX_PATH * 2 - 1, L"schtasks.exe /Delete /TN \"%s\" /F", names[i]);
            cmd[MAX_PATH * 2 - 1] = L'\0';
            if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 3500);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
    }
    g_autostart = enable;
    MarkAutostartOff(!enable);
    return TRUE;
}

static void ScheduleLiveApply(void) {
    /* Coalesce bursts but never hold a change back: once a change is queued
     * it goes out within ~40ms even if more edits keep arriving (each flush
     * re-queues the next one). The old code reset the deadline on every
     * mouse move, so drags applied only after you STOPPED - which is what
     * made the app feel laggy instead of real-time. */
    if (!g_applyPending) {
        g_applyPending = TRUE;
        g_applyAtTick  = GetTickCount64() + 40ULL;
    }
}

static void FlushLiveApply(void) {
    g_applyPending = FALSE;
    SavePanelCfg();
    if (g_wantOff) return;
    if (IsEngineAlive()) {
        WriteThemeJson(); /* preferences never republish an old master */
        _snwprintf(g_statusText, 255, L"Active  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
                   g_cfg.kbd.on ? kEffects[g_cfg.kbd.effectIdx].label : "Off",
                   g_cfg.bar.on ? kEffects[g_cfg.bar.effectIdx].label : "Off");
    } else {
        StartEngine();
    }
}

/* Light Bar physical order from the engine's layout.txt: the lamp ids as
   they sit around the real closed loop (the order established in Zones
   Setup), NOT the raw lamp-index order that frame.txt delivers. The ring
   preview must follow this order or a scrolling gradient jumps around the
   circle instead of travelling round it once. */
static int          g_barOrder[64];
static double       g_barQ[64];        /* 0..1 ring fraction, seam gap intact */
static int          g_barLayoutN = 0;
static ULONGLONG    g_layoutReadAt = 0;

static void ParseLayoutTxt(char *txt) {
    int order[64], n = 0, havePos = 0, ring = 0;
    double pos[64];
    for (char *q2 = txt; q2 && *q2; ) {
        char *nl = strchr(q2, '\n');
        if (nl) *nl = 0;
        size_t len = strlen(q2);
        while (len > 0 && (q2[len - 1] == '\r' || q2[len - 1] == ' ')) q2[--len] = 0;
        if (strncmp(q2, "bar=", 4) == 0) {
            const char *v = q2 + 4;
            n = 0;
            while (n < 64 && *v) {
                order[n++] = atoi(v);
                while (*v && *v != ',') v++;
                if (*v == ',') v++;
            }
        } else if (strncmp(q2, "barpos=", 7) == 0) {
            const char *v = q2 + 7;
            havePos = 0;
            while (havePos < 64 && *v) {
                pos[havePos++] = atof(v);
                while (*v && *v != ',') v++;
                if (*v == ',') v++;
            }
        } else if (strncmp(q2, "barring=", 8) == 0) {
            ring = atoi(q2 + 8) != 0;
        }
        q2 = nl ? nl + 1 : NULL;
    }
    if (n < 2) { g_barLayoutN = 0; return; }
    for (int k = 0; k < n; k++) g_barOrder[k] = order[k];
    if (havePos == n && ring) {
        /* barpos is re-normalised 0..1 across the endpoints, which would
           drop the last lamp ONTO the first one's angle. The engine's ring
           map leaves exactly one step of seam room: q = p * (n-1)/n. */
        for (int k = 0; k < n; k++)
            g_barQ[k] = pos[k] * (double)(n - 1) / (double)n;
    } else {
        for (int k = 0; k < n; k++) g_barQ[k] = (double)k / (double)n;
    }
    g_barLayoutN = n;
}

