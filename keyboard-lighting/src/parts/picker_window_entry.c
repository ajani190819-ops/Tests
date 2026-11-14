/* ---- live-preview colour picker -------------------------------------
 * The colour dialog now pushes every intermediate choice straight to the
 * lights while it is still open (timer hook reading the dialog's live
 * rgbResult). Choosing a colour is real-time: the keyboard follows the
 * dialog as you browse. Cancel restores whatever was showing before. */
static COLORREF g_pickOrig = 0;
static COLORREF g_pickLast = 0;
static int      g_pickIdx  = -1;
static BOOL     g_pickLive = FALSE;

static UINT_PTR CALLBACK ColorPickerHook(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)lp); /* CHOOSECOLORW */
        SetTimer(hDlg, 0x51C0, 70, NULL);
        return 0;
    }
    if (msg == WM_TIMER && wp == 0x51C0) {
        CHOOSECOLORW *cc = (CHOOSECOLORW *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        GroupCfg *cg = CurGroup();
        if (cc && g_pickIdx >= 0 && g_pickIdx < cg->swatchCount) {
            COLORREF c = cc->rgbResult & 0x00FFFFFF;
            if (c != g_pickLast) {
                g_pickLast = c;
                cg->swatches[g_pickIdx] = c;
                g_pickLive = TRUE;
                SyncLinkedZone();
                ScheduleLiveApply();
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    return 0;
}

static void PickSwatchColor(HWND hwnd, int idx) {
    GroupCfg *cg = CurGroup();
    if (idx < 0 || idx >= cg->swatchCount) return;

    CHOOSECOLORW cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = hwnd;
    cc.rgbResult    = cg->swatches[idx];
    cc.lpCustColors = g_cfg.customColors;
    cc.lpfnHook     = ColorPickerHook;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
    g_pickOrig = cg->swatches[idx];
    g_pickLast = cg->swatches[idx];
    g_pickIdx  = idx;
    g_pickLive = FALSE;
    if (ChooseColorW(&cc)) {
        cg->swatches[idx] = cc.rgbResult;
        SyncLinkedZone();
        FlushLiveApply();
        InvalidateRect(hwnd, NULL, FALSE);
    } else {
        if (g_pickLive) {
            cg->swatches[idx] = g_pickOrig;
            SyncLinkedZone();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            SavePanelCfg();
        }
    }
    g_pickIdx = -1;
}

static void RelaunchElevated(HWND hwnd) {
    StopEngine(TRUE);
    DeleteFileW(g_lockPath);
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize       = sizeof(sei);
    sei.hwnd         = hwnd;
    sei.lpVerb       = L"runas";
    sei.lpFile       = g_exePath;
    sei.lpParameters = L"-NoElevate";
    sei.lpDirectory  = g_exeDir;
    sei.nShow        = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) {
        g_quitting = TRUE;
        DestroyWindow(hwnd);
    } else {
        WriteHeartbeat();
        StartEngine();
    }
}

static void EnsureLogonTask(void) {
    /* One UAC prompt ever: once we run elevated, register a logon task with
       highest privileges so every future start of Keyboard Lighting is
       silent (tasks skip UAC) and fully functional. Re-registering is
       idempotent, so updates simply refresh the task.
       Mirrors DnsPilot's EnsureAutostart: called on every launch, and the
       task command line carries no elevation-suppressing flag. Such a flag
       used to be passed here, which meant a start from a stale
       limited-privilege task arrived unelevated AND forbidden from
       relaunching itself elevated - the app sat in preview mode for the
       whole session with no way to heal. The manifest already requires
       administrator, so /RL HIGHEST is what makes the logon start silent
       instead of a UAC prompt. */
    if (AutostartMarkedOff()) return;
    if (!g_isAdmin) return;   /* /RL HIGHEST can only be registered elevated */

    /* Migrate away from a legacy Run-key entry, the way DnsPilot does: a
       requireAdministrator exe on a Run key trips a UAC prompt at every boot. */
    HKEY k = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegDeleteValueW(k, L"TestsSuite-KeyboardLighting");
        RegDeleteValueW(k, L"KeyboardLighting");
        RegCloseKey(k);
    }

    wchar_t cmd[640];
    _snwprintf(cmd, 639,
               L"schtasks /Create /F /SC ONLOGON /TN \"TestsSuite-KeyboardLighting\" "
               L"/TR \"\\\"%s\\\" -Silent\" /RL HIGHEST", g_exePath);
    cmd[639] = L'\0';
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 8000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void HandleHitAction(HWND hwnd, int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return;
    int kind = g_hits[hitIdx].kind;
    int idx  = g_hits[hitIdx].index;
    RECT rc  = g_hits[hitIdx].rect;
    GroupCfg *cg = CurGroup();

    if (kind == HIT_HDR_LIGHTS) {
        if (g_wantOff) StartEngine();
        else TurnAllLightsOff();
    } else if (kind == HIT_HDR_STARTUP) {
        SetKblAutostart(!g_autostart);
    } else if (kind == HIT_HDR_RESTART) {
        StartEngine();
    } else if (kind == HIT_TAB_KBD) {
        g_tab = 0;
    } else if (kind == HIT_TAB_BAR) {
        g_tab = 1;
    } else if (kind == HIT_TOG_ZONE_ON) {
        cg->on = !cg->on;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_LINK) {
        g_cfg.link = !g_cfg.link;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_PICK_EFFECT) {
        OpenEffectPopup(hwnd, rc);
        return;
    } else if (kind >= HIT_SWATCH_0 && kind < HIT_SWATCH_0 + 8) {
        PickSwatchColor(hwnd, idx);
        return;
    } else if (kind == HIT_SWATCH_ADD && cg->swatchCount < 8) {
        cg->swatches[cg->swatchCount] = RGB(255, 255, 255);
        cg->widths[cg->swatchCount]   = 1.0;
        cg->swatchCount++;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_SWATCH_REM && cg->swatchCount > 2) {
        cg->swatchCount--;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_LOOP) {
        cg->loop = !cg->loop;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_EQ) {
        cg->equalise = !cg->equalise;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_MIRROR) {
        cg->mirror = !cg->mirror;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_REVERSE) {
        cg->reverse = !cg->reverse;
        SyncLinkedZone();
        FlushLiveApply();
    } else if (kind == HIT_TOG_SMOOTH) {
        g_cfg.smooth = !g_cfg.smooth;
        FlushLiveApply();
    } else if (kind == HIT_TOG_OVERLAY) {
        g_cfg.overlay = !g_cfg.overlay;
        SavePanelCfg();
        if (!g_wantOff) StartEngine();
    } else if (kind == HIT_FOOT_SAVEP) {
        int slot = g_presetIdx;
        if (slot < 0) {
            slot = 0;
            for (int i = 0; i < PRESET_MAX; i++) {
                if (!g_presetUsed[i]) { slot = i; break; }
            }
        }
        SavePresetSlot(slot);
        _snwprintf(g_toastText, 95, L"Saved \x00B7 %s", g_presetName[slot]);
        g_toastAt = GetTickCount64() + 1400ULL;
    } else if (kind == HIT_FOOT_PATTERN) {
        OpenPatternPopup(hwnd, rc);
        return;
    } else if (kind == HIT_FOOT_ZONES) {
        ShellExecuteW(hwnd, L"open", g_zonesBat, NULL, g_exeDir, SW_SHOWNORMAL);
    } else if (kind == HIT_FOOT_ELEVATE) {
        RelaunchElevated(hwnd);
        return;
    } else if (kind == HIT_FOOT_QUIT) {
        g_quitting = TRUE;
        DestroyWindow(hwnd);
        return;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ShowTrayContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Keyboard Lighting");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TOGGLE,
                g_wantOff ? L"Turn Lighting On" : L"Turn Lighting Off");
    AppendMenuW(hMenu, MF_STRING | (g_cfg.kbd.on ? MF_CHECKED : 0), IDM_TRAY_KBD, L"Keyboard Zone Enabled");
    AppendMenuW(hMenu, MF_STRING | (g_cfg.bar.on ? MF_CHECKED : 0), IDM_TRAY_BAR, L"Light Bar Enabled");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_RESTART, L"Restart Lighting Engine");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* M4/M5 brightness behaviour. Read from hold_mode.txt on every menu open
       rather than cached, so it is right even when -HoldMode set it from
       another process. The engine re-reads the same file each tick, so the
       choice takes effect with no restart. DestroyMenu below takes the submenu
       with it. */
    {
        HMENU hHold = CreatePopupMenu();
        int hm = HoldModeRead();
        static const UINT holdIds[3] = { IDM_TRAY_HOLD_TAP, IDM_TRAY_HOLD_LATCH,
                                         IDM_TRAY_HOLD_BURST };
        int i;
        for (i = 0; i < 3; i++)
            AppendMenuW(hHold, MF_STRING | (hm == i ? MF_CHECKED : 0),
                        holdIds[i], kHoldLabels[i]);
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHold, L"M4/M5 Brightness Keys");
    }
    /* Read-only, and the reason it is here: it is the only visible answer to
       "why does holding the key do nothing", which otherwise has none. */
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, IDM_TRAY_HOLD_STATE, HoldReadyLabel());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_autostart ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_FOLDER, L"Open Program Folder");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Keyboard Lighting");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK KblWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        SetTimer(hwnd, 1, 45, NULL);  /* 22 FPS live preview & slider debounce */
        SetTimer(hwnd, 2, 600, NULL); /* Heartbeat, show.flag & engine watchdog */
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_GETMINMAXINFO: {
        int dpi = GetWinDpi(hwnd);
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = S(700, dpi);
        mmi->ptMinTrackSize.y = S(480, dpi);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RenderKblWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        g_scrollY -= (delta / WHEEL_DELTA) * S(44, GetWinDpi(hwnd));
        if (g_scrollY < 0) g_scrollY = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            int k = g_hits[i].kind;
            if (PtInRect(&g_hits[i].rect, pt) &&
                (k == HIT_SLIDER_MASTER || k == HIT_SLIDER_SPEED ||
                 k == HIT_SLIDER_BRIGHT || (k >= HIT_WIDTH_0 && k < HIT_WIDTH_0 + 8))) {
                g_dragKind = k;
                g_dragIdx  = g_hits[i].index;
                g_dragRect = g_hits[i].rect;
                SetCapture(hwnd);
                ApplySliderDrag(k, g_dragIdx, g_dragRect, pt.x);
                return 0;
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g_dragKind != HIT_NONE) {
            ApplySliderDrag(g_dragKind, g_dragIdx, g_dragRect, pt.x);
            return 0;
        }
        int found = -1;
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) { found = i; break; }
        }
        SetCursor(LoadCursorW(NULL, found >= 0 ? IDC_HAND : IDC_ARROW));
        if (found != g_hoverIdx) {
            g_hoverIdx = found;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_dragKind != HIT_NONE) {
            g_dragKind = HIT_NONE;
            ReleaseCapture();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) {
                HandleHitAction(hwnd, i);
                break;
            }
        }
        return 0;
    }

    case WM_HOTKEY:
        if (wp == 1) StepMasterBrightness(1);
        else if (wp == 2) StepMasterBrightness(-1);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= IDM_EFF_BASE && id < IDM_EFF_BASE + EFFECT_COUNT) {
            CurGroup()->effectIdx = id - IDM_EFF_BASE;
            SyncLinkedZone();
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id >= IDM_PAT_BASE && id < IDM_PAT_BASE + PRESET_MAX) {
            if (g_presetUsed[id - IDM_PAT_BASE])
                ApplyPresetSlot(id - IDM_PAT_BASE);
        } else if (id == IDM_PAT_NEW) {
            int slot = -1;
            for (int i = 0; i < PRESET_MAX; i++) {
                if (!g_presetUsed[i]) { slot = i; break; }
            }
            if (slot < 0) {
                _snwprintf(g_toastText, 95, L"All 8 pattern slots are full - delete one first");
                g_toastAt = GetTickCount64() + 1400ULL;
            } else {
                SavePresetSlot(slot);
                _snwprintf(g_toastText, 95, L"Saved \x00B7 %s", g_presetName[slot]);
                g_toastAt = GetTickCount64() + 1400ULL;
            }
        } else if (id == IDM_PAT_DEL) {
            if (g_presetIdx >= 0 && g_presetUsed[g_presetIdx]) {
                DeletePresetSlot(g_presetIdx);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (id == IDM_TRAY_TOGGLE) {
            if (g_wantOff) StartEngine(); else TurnAllLightsOff();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_KBD) {
            g_cfg.kbd.on = !g_cfg.kbd.on;
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_BAR) {
            g_cfg.bar.on = !g_cfg.bar.on;
            FlushLiveApply();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_RESTART) {
            StartEngine();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_HOLD_TAP || id == IDM_TRAY_HOLD_LATCH ||
                   id == IDM_TRAY_HOLD_BURST) {
            int m = (id == IDM_TRAY_HOLD_LATCH) ? 1
                  : (id == IDM_TRAY_HOLD_BURST) ? 2 : 0;
            HoldModeWrite(m);
            _snwprintf(g_statusText, 255, L"M4/M5: %s", kHoldLabels[m]);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_AUTOSTART) {
            SetKblAutostart(!g_autostart);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_FOLDER) {
            ShellExecuteW(hwnd, L"open", g_exeDir, NULL, NULL, SW_SHOWNORMAL);
        } else if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_ENGINE_BOOTED:
        if (wp == 1 && !g_wantOff) StartEngine();
        else wcsncpy(g_statusText, L"Could not download Aura-Background.ps1 - check internet.", 255);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_POWERBROADCAST:
        if ((wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) && !g_wantOff) {
            if (IsEngineAlive()) { WriteThemeJson(); WriteLiveBrightness(); }
            else StartEngine();
        }
        return TRUE;

    case WM_INPUT:
        OnRawInputMsg(hwnd, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_TIMER:
        if (wp == 1) {
            if (g_applyPending && GetTickCount64() >= g_applyAtTick) FlushLiveApply();
            PollFnBrightness();
            HoldPump();
            if (IsWindowVisible(hwnd)) {
                PollLivePreviewFrame();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (wp == 2) {
            WriteHeartbeat();
            WatchBrightTokens();
            PollFnBrightness();
            if (FileExistsW(g_showPath)) {
                DeleteFileW(g_showPath);
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
            }
            if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
            }
            if (!g_wantOff && !IsEngineAlive() && InterlockedCompareExchange(&g_downloading, 0, 0) == 0) {
                StartEngine();
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowTrayContextMenu(hwnd);
        }
        return 0;

    case WM_CLOSE:
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            if (!g_trayHinted) {
                g_trayHinted = TRUE;
                g_nid.uFlags = NIF_INFO;
                wcsncpy(g_nid.szInfoTitle, L"Keyboard Lighting is active in tray", 63);
                wcsncpy(g_nid.szInfo, L"Right-click the tray icon any time to adjust zones or quit.", 255);
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        StopEngine(TRUE);
        DeleteFileW(g_lockPath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void InitKblPaths(void) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    wcsncpy(g_exeDir, g_exePath, MAX_PATH - 1);
    wchar_t *sl = wcsrchr(g_exeDir, L'\\');
    if (sl) *sl = L'\0';

    wchar_t localApp[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);

    _snwprintf(g_cfgDir,     MAX_PATH, L"%s\\KeyboardLighting", localApp);
    EnsureDirTree(g_cfgDir);
    _snwprintf(g_panelPath,  MAX_PATH, L"%s\\panel.json", g_cfgDir);
    _snwprintf(g_themePath,  MAX_PATH, L"%s\\theme.json", g_cfgDir);
    _snwprintf(g_livePath,   MAX_PATH, L"%s\\live.txt", g_cfgDir);
    _snwprintf(g_framePath,  MAX_PATH, L"%s\\frame.txt", g_cfgDir);
    _snwprintf(g_layoutPath, MAX_PATH, L"%s\\layout.txt", g_cfgDir);
    _snwprintf(g_lockPath,   MAX_PATH, L"%s\\running.lock", g_cfgDir);
    _snwprintf(g_showPath,   MAX_PATH, L"%s\\show.flag", g_cfgDir);
    _snwprintf(g_engPidPath, MAX_PATH, L"%s\\engine.pid", g_cfgDir);
    _snwprintf(g_stopPath,   MAX_PATH, L"%s\\stop.flag", g_cfgDir);
    _snwprintf(g_brightUpPath,   MAX_PATH, L"%s\\bright_up.flag", g_cfgDir);
    _snwprintf(g_brightDownPath, MAX_PATH, L"%s\\bright_down.flag", g_cfgDir);
    _snwprintf(g_holdModePath,   MAX_PATH, L"%s\\hold_mode.txt", g_cfgDir);
    {
        wchar_t p[MAX_PATH];
        _snwprintf(p, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
        g_scanBound[0] = (int)GetPrivateProfileIntW(L"Keys", L"M4Scan", 0, p);
        g_scanBound[1] = (int)GetPrivateProfileIntW(L"Keys", L"M5Scan", 0, p);
        g_hidCode[0] = (DWORD)GetPrivateProfileIntW(L"Keys", L"M4Hid", 0, p);
        g_hidCode[1] = (DWORD)GetPrivateProfileIntW(L"Keys", L"M5Hid", 0, p);
    }
    _snwprintf(g_engineScript, MAX_PATH, L"%s\\Aura-Background.ps1", g_exeDir);
    _snwprintf(g_zonesBat,   MAX_PATH, L"%s\\Zones.bat", g_exeDir);
    if (!FileExistsW(g_zonesBat))
        _snwprintf(g_zonesBat, MAX_PATH, L"%s\\Zones.bat", g_cfgDir);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;
    InitKblPaths();
    OsdCreate(hInst);
    g_isAdmin = CheckIsAdmin();

    BOOL silent    = (cmdLine && (wcsstr(cmdLine, L"-Silent") != NULL || wcsstr(cmdLine, L"--silent") != NULL));
    BOOL noElevate = (cmdLine && (wcsstr(cmdLine, L"-NoElevate") != NULL || wcsstr(cmdLine, L"--no-elevate") != NULL));

    if (cmdLine && (wcsstr(cmdLine, L"--stop") != NULL || wcsstr(cmdLine, L"-Stop") != NULL)) {
        StopEngine(TRUE);
        KillPidFromFile(g_lockPath);
        return 0;
    }

    /* Brightness taps for the M4/M5 G-Helper 'Custom' actions: a second
       instance writes a one-shot token the engine turns into a paced master
       step and exits immediately - no window, no single-instance bounce.
       The token is a millisecond stamp so a held key's repeat taps are each
       seen; the engine paces them into a smooth sweep. */
    /* Hold-mode switch for the M4/M5 brightness taps: the engine reads
       hold_mode.txt every tick, so this changes behavior with no restart.
       Usage: -HoldMode latch (default) | burst | tap */
    if (cmdLine && wcsstr(cmdLine, L"-HoldMode") != NULL) {
        const wchar_t *m = wcsstr(cmdLine, L"-HoldMode") + 9;
        while (*m == L' ' || *m == L'=') m++;
        const char *mode = "latch";
        if (wcsncmp(m, L"tap", 3) == 0) mode = "tap";
        else if (wcsncmp(m, L"burst", 5) == 0) mode = "burst";
        WriteAtomicUtf8(g_holdModePath, mode);
        return 0;
    }
    if (cmdLine && (wcsstr(cmdLine, L"-BrightUp") != NULL || wcsstr(cmdLine, L"--bright-up") != NULL)) {
        char tok[32];
        _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
        tok[31] = '\0';
        WriteAtomicUtf8(g_brightUpPath, tok);
        return 0;
    }
    if (cmdLine && (wcsstr(cmdLine, L"-BrightDown") != NULL || wcsstr(cmdLine, L"--bright-down") != NULL)) {
        char tok[32];
        _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
        tok[31] = '\0';
        WriteAtomicUtf8(g_brightDownPath, tok);
        return 0;
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!silent) {
            WriteAtomicUtf8(g_showPath, "1");
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    /* Request elevation once for direct ROG Strix G16 USB HID RGB access unless declined */
    if (!g_isAdmin && !noElevate) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize       = sizeof(sei);
        sei.lpVerb       = L"runas";
        sei.lpFile       = g_exePath;
        sei.lpParameters = silent ? L"-Silent -NoElevate" : L"-NoElevate";
        sei.lpDirectory  = g_exeDir;
        sei.nShow        = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) return 0;
        /* User declined UAC - stay open gracefully in preview mode with 'Run as Admin' button! */
        hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    }

    /* Called on every launch, like DnsPilot's EnsureAutostart, so a task left
       behind by an older build is refreshed to /RL HIGHEST. It no-ops when
       this instance is not elevated. */
    EnsureLogonTask();

    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    /* Ensure any legacy Tray.ps1 process is cleanly closed before taking over */
    KillPidFromFile(g_lockPath);
    LoadPanelCfg();
    LoadPresetIndex();
    g_autostart = QueryAutostart();
    WriteHeartbeat();
    PollLivePreviewFrame();

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = KblWndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsKeyboardLightingSuiteWnd";
    RegisterClassExW(&wc);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(820, dpi), S(660, dpi),
                             NULL, NULL, hInst, NULL);
    RegisterBrightnessHotkeys(g_hwnd);
    /* Watch the M4/M5 keys through raw input (keyboard + HID), registered now
       that g_hwnd exists. WM_INPUT is queued to our own message loop, so unlike
       the WH_KEYBOARD_LL hook this used to rely on it never intercepts input
       system-wide and cannot make Windows drop keystrokes: a low-level hook lives
       on this thread, and this thread blocks for seconds in the engine
       (re)start waits below, which timed the hook out and silently killed
       PrintScreen (and other keys) machine-wide while the app ran. It was also
       previously called before CreateWindowExW, i.e. with a NULL hwnd, so raw
       input never arrived and the hook was doing all the work. */
    InitRawInput(g_hwnd);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_appIcon;
    wcsncpy(g_nid.szTip, L"Keyboard Lighting - ROG Strix G16", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    StartEngine();

    if (!silent) {
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_hwnd);
        SetForegroundWindow(g_hwnd);   /* an explicit Open must take focus */
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_showEvent) CloseHandle(g_showEvent);
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
