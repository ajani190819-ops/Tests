/* ---------------- waiting watcher + native notification area ----------------

   This is a tray-resident watcher only while Agent Notes itself is running.
   It intentionally creates no Run key, task, service, or sign-in behavior.
   Windows may defer/suppress notification-area visuals under Focus Assist or
   notification policy, so the UI calls the state "alert shown", never an
   impossible guarantee that a disconnected/sleeping device displayed it. */

typedef struct {
    int linkFound;
    int statusOk;
    int waiting;
    wchar_t waitId[80];
    wchar_t readReceipts[HIST_MAX][80];
    int readCount;
} StatusPollResult;

/* The unified Project Hub icon is opt-in by presence, not a new startup
   facility. If Hub is not running (or it crashes), this app restores its own
   normal tray fallback on the next watcher tick. */
static void AgentNotesTraySync(void) {
    /* Hub's shared tray menu writes this existing preference in the same
       config file; refresh it here so a fallback alert honors the choice too. */
    g_waitSound = GetPrivateProfileIntW(L"AgentNotes", L"WaitSound", g_waitSound ? 1 : 0,
                                        g_cfgPath) ? 1 : 0;
    int wantOwnIcon = !SuiteHubOwnsTray();
    if (wantOwnIcon && !g_trayVisible) {
        if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
            g_trayVisible = 1;
            g_nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
        }
    } else if (!wantOwnIcon && g_trayVisible) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_trayVisible = 0;
    }
}

static void UpdateAgentNotesTrayTip(void) {
    if (!g_nid.cbSize || !g_trayVisible) return;
    g_nid.uFlags = NIF_TIP;
    wcsncpy(g_nid.szTip, g_activeWait[0]
                         ? L"Agent Notes - agent is waiting for input"
                         : L"Agent Notes - direct notes and waiting alerts", 127);
    g_nid.szTip[127] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void AgentNotesTrayInit(void) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
    wcsncpy(g_nid.szTip, L"Agent Notes - direct notes and waiting alerts", 127);
    g_nid.szTip[127] = L'\0';
    AgentNotesTraySync();
}

static void AgentNotesTrayRemove(void) {
    if (g_trayVisible) Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_trayVisible = 0;
    ZeroMemory(&g_nid, sizeof(g_nid));
}

/* The Hub reads only this opaque wait id and creates the visible notification
   when it owns the tray. No question text or note content crosses this local
   handoff. The legacy Agent Notes icon still alerts whenever Hub is absent. */
static void PublishHubWaitRelay(const wchar_t *waitId) {
    WritePrivateProfileStringW(L"HubRelay", L"WaitId", waitId ? waitId : L"", g_cfgPath);
}

static void ShowAgentWaitingAlert(void) {
    if (!g_nid.cbSize || !g_trayVisible) return;
    g_nid.uFlags = NIF_INFO;
    /* NIIF_NOSOUND asks Windows to keep the standard visual alert quiet. When
       the owner enables sound we deliberately omit that flag: Windows uses
       its normal notification sound path; Agent Notes has no custom mixer or
       audio engine to bypass the owner's Windows/EarTrumpet controls. */
    g_nid.dwInfoFlags = NIIF_INFO | (g_waitSound ? 0 : NIIF_NOSOUND);
    wcsncpy(g_nid.szInfoTitle, L"Agent is waiting for input", 63);
    wcsncpy(g_nid.szInfo,
             L"A question needs your reply in Arena. Open the Arena chat when you are ready.",
             255);
    g_nid.szInfoTitle[63] = L'\0';
    g_nid.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void ShowAgentNotesTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open Agent Notes");
    AppendMenuW(menu, MF_STRING | (g_waitSound ? MF_CHECKED : MF_UNCHECKED),
                ID_TRAY_SOUND, L"Play a sound for waiting alerts");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit (stop waiting alerts)");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

static int JsonBoolTrue(const char *json, const char *field) {
    char needle[64];
    _snprintf(needle, sizeof(needle) - 1, "\"%s\":true", field);
    needle[sizeof(needle) - 1] = '\0';
    return json && strstr(json, needle) != NULL;
}

static DWORD WINAPI StatusPollThread(LPVOID unused) {
    (void)unused;
    StatusPollResult *result = (StatusPollResult *)calloc(1, sizeof(*result));
    if (!result) { InterlockedExchange(&g_statusPollActive, 0); return 0; }

    /* A persisted link proves only that a previous session existed. Poll the
       committed branch address too, so a fresh Arena sandbox can replace a
       dead saved endpoint without asking the owner to copy anything. */
    if (LinkDiscoveryTick()) result->linkFound = 1;
    wchar_t link[256];
    wcsncpy(link, g_link, 255);
    link[255] = L'\0';
    if (link[0]) {
        wchar_t url[384];
        char body[256];
        _snwprintf(url, 383, L"%s/status", link);
        url[383] = L'\0';
        if (HttpGetText(url, body, sizeof(body)) > 0) {
            result->statusOk = 1;
            result->waiting = JsonBoolTrue(body, "waiting");
            char id8[80];
            if (result->waiting && JsonStringField(body, "wait_id", id8, sizeof(id8))) {
                MultiByteToWideChar(CP_UTF8, 0, id8, -1, result->waitId, 80);
                result->waitId[79] = L'\0';
            } else {
                result->waiting = 0;  /* never alert on a malformed response */
            }

            /* Receipt polling is intentionally bounded by local history. No
               note contents travel here; only opaque receiver tokens do. */
            for (int i = 0; i < g_histN && result->readCount < HIST_MAX; i++) {
                if (!g_hist[i].receipt[0] || g_hist[i].read) continue;
                char receipt8[80], receiptBody[192];
                if (WideCharToMultiByte(CP_UTF8, 0, g_hist[i].receipt, -1,
                                        receipt8, sizeof(receipt8), NULL, NULL) <= 0)
                    continue;
                _snwprintf(url, 383, L"%s/receipt/%hs", link, receipt8);
                url[383] = L'\0';
                if (HttpGetText(url, receiptBody, sizeof(receiptBody)) > 0 &&
                    JsonFieldEquals(receiptBody, "state", "read")) {
                    wcsncpy(result->readReceipts[result->readCount], g_hist[i].receipt, 79);
                    result->readReceipts[result->readCount][79] = L'\0';
                    result->readCount++;
                }
            }
        }
    }
    InterlockedExchange(&g_statusPollActive, 0);
    if (g_quitting || !g_hwnd || !PostMessageW(g_hwnd, WM_NOTES_STATUS, (WPARAM)result, 0))
        free(result);
    return 0;
}

static void StartStatusPoll(void) {
    if (InterlockedCompareExchange(&g_statusPollActive, 1, 0) != 0) return;
    HANDLE h = CreateThread(NULL, 0, StatusPollThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else InterlockedExchange(&g_statusPollActive, 0);
}

static void HandleStatusPoll(StatusPollResult *result) {
    if (!result) return;
    if (result->linkFound) {
        wcsncpy(g_status, L"Agent session found - queued notes will deliver automatically.", 255);
        g_status[255] = L'\0';
        if (OutboxCount() > 0 && !g_sending) StartFlush();
    }
    if (result->statusOk) {
        if (result->waiting) {
            wcsncpy(g_activeWait, result->waitId, 79);
            g_activeWait[79] = L'\0';
            PublishHubWaitRelay(result->waitId);
            if (wcscmp(result->waitId, g_seenWait) != 0) {
                /* When the Hub owns the tray it relays this same generic
                   wait through its one icon. If Hub is not available, our
                   restored fallback icon preserves the existing alert. */
                ShowAgentWaitingAlert();
                wcsncpy(g_seenWait, result->waitId, 79);
                g_seenWait[79] = L'\0';
                SaveConfig();
            }
            if (!g_sending) {
                wcsncpy(g_status, L"Agent is waiting for your input in Arena.", 255);
                g_status[255] = L'\0';
            }
        } else if (g_activeWait[0]) {
            g_activeWait[0] = L'\0';
            PublishHubWaitRelay(L"");
        }
        UpdateAgentNotesTrayTip();

        int changed = 0;
        for (int i = 0; i < result->readCount; i++)
            if (HistMarkReceiptRead(result->readReceipts[i])) changed = 1;
        if (changed) {
            HistSave();
            if (!g_sending) {
                wcsncpy(g_status, L"Agent read your note at its inbox checkpoint.", 255);
                g_status[255] = L'\0';
            }
        }
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
    free(result);
}
