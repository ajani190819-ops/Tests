/* ============================================================================
 *  --diag : report what the Core Audio APIs actually returned.
 *
 *  This app cannot be tested on the build machine (it needs a real Windows
 *  audio stack), so the COM plumbing is unverifiable by inspection alone --
 *  which is exactly how four misaligned vtables shipped. --diag prints every
 *  HRESULT, device id, PID and volume it sees, so a single run on a real
 *  machine shows whether the interfaces are genuinely working rather than
 *  merely compiling.
 *
 *    AudioSwitcher.exe --diag              report only, changes nothing
 *    AudioSwitcher.exe --diag-switch=2     also switch default to endpoint 2
 *                                          and re-read what Windows reports
 *
 *  Output goes to the console AND to %TEMP%\AudioSwitcher-diag.txt.
 *  --diag-switch is deliberately a separate, explicit flag: it really does
 *  change the system default device, so it must never happen by accident.
 * ============================================================================ */

static FILE   *g_diag_fp = NULL;
static wchar_t g_diag_path[MAX_PATH] = L"";

static void w2u8(const wchar_t *w, char *out, size_t cap) {
    if (cap == 0) return;
    if (!w || !w[0]) { out[0] = '\0'; return; }
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
    if (n <= 0) { out[0] = '?'; out[1] = '\0'; }
}

static void diag(const char *fmt, ...) {
    char buf[3072];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    if (g_diag_fp) { fputs(buf, g_diag_fp); fputc('\n', g_diag_fp); fflush(g_diag_fp); }
    printf("%s\n", buf);
}

static void diag_dump_role_defaults(const char *when) {
    static const char *rname[3] = { "eConsole", "eMultimedia", "eCommunications" };
    for (int r = 0; r < 3; r++) {
        IMMDevice *d = NULL;
        HRESULT h = CALL_VTBL3(g_enum, GetDefaultAudioEndpoint, eRender,
                               (enum ERole)r, (void**)&d);
        if (SUCCEEDED(h) && d) {
            LPWSTR wid = NULL;
            CALL_VTBL1(d, GetId, &wid);
            char idu[1024];
            w2u8(wid ? wid : L"(null)", idu, sizeof(idu));
            diag("  default[%s] %s = %s", rname[r], when, idu);
            if (wid) CoTaskMemFree(wid);
            CALL_VTBL0(d, Release);
        } else {
            diag("  default[%s] %s = none (0x%08lX)", rname[r], when, (unsigned long)h);
        }
    }
}

static int run_diagnostics(int switch_idx) {
    wchar_t temp[MAX_PATH] = L"";
    GetEnvironmentVariableW(L"TEMP", temp, MAX_PATH);
    if (!temp[0]) wcscpy(temp, L".");
    _snwprintf(g_diag_path, MAX_PATH, L"%s\\AudioSwitcher-diag.txt", temp);

    /* A GUI-subsystem process has no console; make one so the report is
     * visible even when launched from Explorer. */
    AllocConsole();
    if (!freopen("CONOUT$", "w", stdout)) printf("(console unavailable)\n");

    g_diag_fp = _wfopen(g_diag_path, L"wb");

    diag("Audio Switcher -- diagnostics");
    diag("=============================");
    char dp[2048];
    w2u8(g_diag_path, dp, sizeof(dp));
    diag("report file: %s", g_diag_fp ? dp : "(could not open; console only)");
    diag("");

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    diag("CoInitializeEx                        : 0x%08lX", (unsigned long)hr);

    hr = com_create_enumerator();
    diag("IMMDeviceEnumerator                   : 0x%08lX %s",
         (unsigned long)hr, SUCCEEDED(hr) ? "OK" : "FAILED");
    if (FAILED(hr) || !g_enum) {
        diag("");
        diag("FATAL: no device enumerator, nothing else can be probed.");
        goto done;
    }

    /* Which IPolicyConfig IID this Windows build accepts. */
    {
        const IID *cands[2] = { &IID_IPolicyConfig, &IID_IPolicyConfigVista };
        const char *cname[2] = { "IID_IPolicyConfig (Win7..Win11)",
                                 "IID_IPolicyConfigVista" };
        diag("");
        diag("IPolicyConfig IID probe (device switching):");
        for (int i = 0; i < 2; i++) {
            void *p = NULL;
            HRESULT h = CoCreateInstance(&CLSID_PolicyConfig, NULL, CLSCTX_ALL,
                                         cands[i], &p);
            diag("  %-34s: 0x%08lX %s", cname[i], (unsigned long)h,
                 SUCCEEDED(h) ? "OK" : "failed");
            if (p) CALL_VTBL0((IUnknown*)p, Release);
        }
        hr = policy_activate();
        diag("  policy_activate() -> 0x%08lX, variant=%d %s",
             (unsigned long)hr, g_policy_variant,
             g_policy_variant ? "(switching available)"
                              : "(switching NOT available)");
    }

    levels_path_init();
    levels_load();

    diag("");
    diag("Output endpoints:");
    hr = endpoints_refresh();
    diag("  endpoints_refresh() -> 0x%08lX, count=%d, default_idx=%d",
         (unsigned long)hr, g_endpoint_count, g_default_endpoint_idx);

    for (int i = 0; i < g_endpoint_count; i++) {
        char nm[512], idu[1024];
        w2u8(g_endpoints[i].name, nm, sizeof(nm));
        w2u8(g_endpoints[i].id, idu, sizeof(idu));
        diag("  [%d] name  = %s", i, nm);
        diag("      id    = %s", idu[0] ? idu : "(EMPTY)");
        diag("      volume_iface=%s volume=%d%% mute=%d is_default=%d",
             g_endpoints[i].vol ? "present" : "NULL",
             (int)(endpoint_get_volume(i) * 100.0f + 0.5f),
             (int)endpoint_get_mute(i), (int)g_endpoints[i].is_default);
        if (g_endpoints[i].device) {
            DWORD st = 0;
            CALL_VTBL1(g_endpoints[i].device, GetState, &st);
            diag("      device_state=%lu (1=ACTIVE)", (unsigned long)st);
        }
        if (!g_endpoints[i].id[0])
            diag("      WARNING: empty id -- SetDefaultEndpoint cannot name this device");
        if (!g_endpoints[i].vol)
            diag("      WARNING: no IAudioEndpointVolume -- its slider would be dead");
    }
    if (g_endpoint_count == 0)
        diag("  WARNING: no endpoints found at all");

    diag("");
    diag("Windows' own view of the default device:");
    diag_dump_role_defaults("(before)");

    diag("");
    diag("Audio sessions (per-app volume):");
    hr = sessions_refresh();
    diag("  sessions_refresh() -> 0x%08lX, count=%d",
         (unsigned long)hr, g_session_count);
    if (g_session_sweep_used)
        diag("  NOTE: the default output listed nothing, so these came from the "
             "fallback sweep of the other active outputs.");

    for (int i = 0; i < g_session_count; i++) {
        char nm[512], key[512], exep[2048];
        w2u8(g_sessions[i].name, nm, sizeof(nm));
        w2u8(g_sessions[i].key, key, sizeof(key));
        w2u8(g_sessions[i].exe_path, exep, sizeof(exep));
        diag("  [%d] name=%s", i, nm);
        diag("      key=%s pid=%lu volume=%d%% mute=%d label_from_session=%d "
             "on_other_output=%d",
             key[0] ? key : "(none)", (unsigned long)g_sessions[i].process_id,
             (int)(g_sessions[i].volume * 100.0f + 0.5f),
             (int)g_sessions[i].mute, (int)g_sessions[i].named_from_session,
             (int)g_sessions[i].on_other_device);
        diag("      exe=%s", exep[0] ? exep : "(unavailable)");
        if (!key[0])
            diag("      NOTE: no key -- this app's level cannot be remembered");
    }
    if (g_session_count == 0)
        diag("  WARNING: no sessions on ANY output (the default endpoint and the "
             "fallback sweep both came up empty). Either nothing is playing audio,");
    if (g_session_count == 0)
        diag("           or GetSessionEnumerator/GetProcessId is still failing.");

    diag("");
    char lp[2048];
    w2u8(g_levels_path, lp, sizeof(lp));
    diag("Remembered levels: enabled=%d count=%d file=%s",
         (int)g_levels_enabled, g_remembered_count, lp);
    for (int i = 0; i < g_remembered_count; i++) {
        char k[512];
        w2u8(g_remembered[i].key, k, sizeof(k));
        diag("  %s = %d%%, mute=%d", k, g_remembered[i].percent,
             (int)g_remembered[i].mute);
    }

    if (switch_idx >= 0) {
        diag("");
        diag("=== LIVE SWITCH TEST: endpoint_make_default(%d) ===", switch_idx);
        if (switch_idx >= g_endpoint_count) {
            diag("  index %d out of range (have %d endpoints)",
                 switch_idx, g_endpoint_count);
        } else {
            char nm[512];
            w2u8(g_endpoints[switch_idx].name, nm, sizeof(nm));
            diag("  target: [%d] %s", switch_idx, nm);
            HRESULT h = endpoint_make_default(switch_idx);
            diag("  endpoint_make_default -> 0x%08lX (%s)", (unsigned long)h,
                 SUCCEEDED(h) ? "S_OK: all three roles set" : "FAILED");
            endpoints_refresh();
            diag("  after switch: default_idx=%d", g_default_endpoint_idx);
            diag_dump_role_defaults("(after)");
            diag("  Compare the before/after ids above: if they are unchanged,");
            diag("  SetDefaultEndpoint is not taking effect on this Windows build.");
        }
    } else {
        diag("");
        diag("No switch performed. To test switching for real, run:");
        diag("  AudioSwitcher.exe --diag --diag-switch=<index>");
    }

done:
    diag("");
    diag("=== end of report ===");
    if (g_diag_fp) { fclose(g_diag_fp); g_diag_fp = NULL; }

    sessions_cleanup();
    endpoints_cleanup();
    com_release_enumerator();
    CoUninitialize();

    printf("\nReport written to: %ls\n", g_diag_path);
    printf("Press Enter to close...");
    fflush(stdout);
    (void)getchar();
    return 0;
}


/* Waits for a second launch to ask for the window, then marshals the request
   onto the UI thread. Polls on a short timeout so it notices shutdown without
   a second wake object; the event is auto-reset, so each request produces
   exactly one show. */
static DWORD WINAPI ShowWatcherThread(LPVOID param) {
    (void)param;
    if (!g_showEvent) return 0;
    while (g_showThreadRun) {
        DWORD wr = WaitForSingleObject(g_showEvent, 200);
        if (wr == WAIT_OBJECT_0) {
            if (g_hwnd) PostMessageW(g_hwnd, WM_ASW_SHOW, 0, 0);
        } else if (wr != WAIT_TIMEOUT) {
            break;              /* handle went invalid; stop, do not spin */
        }
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;

    g_silent = (cmdLine && (wcsstr(cmdLine, L"--silent") != NULL || wcsstr(cmdLine, L"-Silent") != NULL));

    /* --diag is handled BEFORE the single-instance mutex on purpose: the app is
     * usually already running in the tray, and a diagnostics run must not
     * silently bounce off the existing instance and report nothing. */
    if (cmdLine && wcsstr(cmdLine, L"--diag") != NULL) {
        int switch_idx = -1;
        const wchar_t *sw = wcsstr(cmdLine, L"--diag-switch=");
        if (sw) switch_idx = _wtoi(sw + 14);   /* len(L"--diag-switch=") == 14 */
        return run_diagnostics(switch_idx);
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!g_silent) {
            /* Ask the running instance to show itself. The named event is the
               reliable path across integrity levels (the tray copy is usually
               elevated); the direct window message is a best-effort fallback for
               an instance that predates the event, which UIPI may drop. */
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
            HWND existing = FindWindowW(L"TestsAudioSwitcherWnd", NULL);
            if (existing) {
                ShowWindowAsync(existing, SW_SHOWNORMAL);
                PostMessageW(existing, WM_ASW_SHOW, 0, 0);
            }
        }
        CloseHandle(hMutex);
        return 0;
    }

    /* First instance: create the show event before the window so a second launch
       that races our startup can already signal it. */
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);

    g_hInst = hInst;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TestsAudioSwitcherWnd";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(RES_ICON_APP));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    RegisterClassExW(&wc);

    g_dpi = GetWinDpi(NULL);

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, S(720), S(620),
        NULL, NULL, hInst, NULL);

    /* An explicit launch (the Hub's Open button, a desktop shortcut) must
       actually show the panel; only the silent logon/tray start hides. Starting
       hidden unconditionally was why clicking Open in the Hub appeared to do
       nothing - the window existed but was never shown. */
    if (g_silent) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
    }

    /* Surface the window when a later launch asks us to (this is what makes the
       Hub's Open button work once we are the hidden tray instance). Started
       after the window exists so g_hwnd is valid for the intra-process post. */
    g_showThreadRun = TRUE;
    g_showThread = CreateThread(NULL, 0, ShowWatcherThread, NULL, 0, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_showThreadRun = FALSE;
    if (g_showThread) {
        WaitForSingleObject(g_showThread, 1000);
        CloseHandle(g_showThread);
        g_showThread = NULL;
    }
    if (g_showEvent) { CloseHandle(g_showEvent); g_showEvent = NULL; }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
