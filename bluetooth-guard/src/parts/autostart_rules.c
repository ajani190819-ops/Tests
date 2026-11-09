/* Install both:
 *   1. Zero-delay ONLOGON Scheduled Task (Priority 4, runs on battery, bypasses Explorer's 10s delay)
 *   2. Explorer\Serialize StartupDelayInMSec=0 + HKCU\...\Run fallback
 * Runs entirely in user context (LeastPrivilege) — NO administrator prompt needed! */
static void SetAutostart(BOOL enable) {
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
        if (!enable) {
            RegDeleteValueW(hk, L"BluetoothGuard");
        } else {
            wchar_t exe[MAX_PATH], cmd[MAX_PATH + 32];
            GetModuleFileNameW(NULL, exe, MAX_PATH);
            _snwprintf(cmd, MAX_PATH + 32, L"\"%s\" --silent", exe);
            RegSetValueExW(hk, L"BluetoothGuard", 0, REG_SZ,
                           (const BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hk);
    }

    if (enable) {
        /* Disable Windows Explorer's artificial ~10-second post-boot startup delay */
        HKEY hSer = NULL;
        if (RegCreateKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Serialize",
                            0, NULL, 0, KEY_SET_VALUE, NULL, &hSer, NULL) == ERROR_SUCCESS) {
            DWORD zero = 0;
            RegSetValueExW(hSer, L"StartupDelayInMSec", 0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));
            RegSetValueExW(hSer, L"WaitForIdleState",   0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));
            RegCloseKey(hSer);
        }

        /* Write Scheduled Task XML with PT0S logon trigger, Priority=4 (interactive/high), and battery allowed */
        wchar_t exe[MAX_PATH], xmlPath[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        _snwprintf(xmlPath, MAX_PATH, L"%s\\fastboot-task.xml", g_cfgDir);
        FILE *xf = _wfopen(xmlPath, L"w, ccs=UTF-16LE");
        if (xf) {
            fwprintf(xf,
                L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
                L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
                L"  <Triggers>\r\n"
                L"    <LogonTrigger>\r\n"
                L"      <Enabled>true</Enabled>\r\n"
                L"      <Delay>PT0S</Delay>\r\n"
                L"    </LogonTrigger>\r\n"
                L"  </Triggers>\r\n"
                L"  <Principals>\r\n"
                L"    <Principal id=\"Author\">\r\n"
                L"      <LogonType>InteractiveToken</LogonType>\r\n"
                L"      <RunLevel>HighestAvailable</RunLevel>\r\n"
                L"    </Principal>\r\n"
                L"  </Principals>\r\n"
                L"  <Settings>\r\n"
                L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\r\n"
                L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\r\n"
                L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\r\n"
                L"    <AllowHardTerminate>true</AllowHardTerminate>\r\n"
                L"    <StartWhenAvailable>true</StartWhenAvailable>\r\n"
                L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\r\n"
                L"    <Priority>4</Priority>\r\n"
                L"  </Settings>\r\n"
                L"  <Actions Context=\"Author\">\r\n"
                L"    <Exec>\r\n"
                L"      <Command>\"%s\"</Command>\r\n"
                L"      <Arguments>--silent</Arguments>\r\n"
                L"    </Exec>\r\n"
                L"  </Actions>\r\n"
                L"</Task>\r\n",
                exe);
            fclose(xf);
            wchar_t schArgs[MAX_PATH * 2];
            _snwprintf(schArgs, MAX_PATH * 2 - 1,
                       L"schtasks.exe /Create /TN \"%s\" /XML \"%s\" /F",
                       FASTBOOT_TASK_NAME, xmlPath);
            RunHiddenCmd(NULL, schArgs, 4000);
            DeleteFileW(xmlPath);
        }
    } else {
        wchar_t delArgs[256];
        _snwprintf(delArgs, 255, L"schtasks.exe /Delete /TN \"%s\" /F", FASTBOOT_TASK_NAME);
        RunHiddenCmd(NULL, delArgs, 3000);
    }

    g_autostart = enable;
    g_autostartExplicit = TRUE;
}

static void SaveRules(void) {
    FILE *fp = _wfopen(g_rulesPath, L"w, ccs=UTF-8");
    if (!fp) return;
    EnterCriticalSection(&g_lock);
    fwprintf(fp, L"guard=%d\n", g_guardOn ? 1 : 0);
    fwprintf(fp, L"autostart=%d\n", g_autostart ? 1 : 0);
    for (int i = 0; i < g_devCount; i++) {
        fwprintf(fp, L"%s|%d|%llu|%s|%s\n",
                 g_devices[i].mac,
                 g_devices[i].allowAuto ? 1 : 0,
                 g_devices[i].address,
                 g_devices[i].kind,
                 g_devices[i].name);
    }
    LeaveCriticalSection(&g_lock);
    fclose(fp);
    WriteHeartbeat();
}

static void LoadRules(void) {
    FILE *fp = _wfopen(g_rulesPath, L"r, ccs=UTF-8");
    if (!fp) return;
    wchar_t line[512];
    EnterCriticalSection(&g_lock);
    while (fgetws(line, 512, fp)) {
        size_t l = wcslen(line);
        while (l > 0 && (line[l - 1] == L'\n' || line[l - 1] == L'\r')) line[--l] = L'\0';
        if (wcsncmp(line, L"guard=", 6) == 0) {
            g_guardOn = (_wtoi(line + 6) != 0);
            continue;
        }
        if (wcsncmp(line, L"autostart=", 10) == 0) {
            g_autostart = (_wtoi(line + 10) != 0);
            g_autostartExplicit = TRUE;
            continue;
        }
        wchar_t *p1 = wcschr(line, L'|'); if (!p1) continue; *p1++ = L'\0';
        wchar_t *p2 = wcschr(p1, L'|');   if (!p2) continue; *p2++ = L'\0';
        wchar_t *p3 = wcschr(p2, L'|');   if (!p3) continue; *p3++ = L'\0';
        wchar_t *p4 = wcschr(p3, L'|');   if (!p4) continue; *p4++ = L'\0';

        if (g_devCount < MAX_DEVICES) {
            DeviceEntry *d = &g_devices[g_devCount++];
            ZeroMemory(d, sizeof(*d));
            /* saved rows only carry the allow/block policy; they are not
               proof the device is one of yours - the OS scan decides */
            d->visible = FALSE;
            wcsncpy(d->mac, line, 23);
            d->allowAuto = (_wtoi(p1) != 0);
            d->address = _wcstoui64(p2, NULL, 10);
            wcsncpy(d->kind, p3, 15);
            wcsncpy(d->name, p4, 247);
        }
    }
    LeaveCriticalSection(&g_lock);
    fclose(fp);
}

static void EnqueueWork(int op, ULONGLONG address, const wchar_t *mac,
                        const wchar_t *kind, const wchar_t *name) {
    EnterCriticalSection(&g_lock);
    int next = (g_qTail + 1) % 64;
    if (next != g_qHead) {
        g_queue[g_qTail].op = op;
        g_queue[g_qTail].address = address;
        wcsncpy(g_queue[g_qTail].mac, mac ? mac : L"", 23);
        wcsncpy(g_queue[g_qTail].kind, kind ? kind : L"Audio", 15);
        wcsncpy(g_queue[g_qTail].name, name ? name : L"", 127);
        g_qTail = next;
        ReleaseSemaphore(g_qSem, 1, NULL);
    }
    LeaveCriticalSection(&g_lock);
}

static void PollPairedDevices(void) {
    if (!pBtFindFirstRadio || !pBtFindFirstDevice) {
        EnterCriticalSection(&g_lock);
        g_radioOn = FALSE;
        LeaveCriticalSection(&g_lock);
        return;
    }

    BLUETOOTH_FIND_RADIO_PARAMS rp;
    rp.dwSize = sizeof(rp);
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hRF = pBtFindFirstRadio(&rp, &hRadio);
    BOOL radioPresent = (hRF != NULL);
    if (hRF) {
        if (hRadio) CloseHandle(hRadio);
        pBtFindRadioClose(hRF);
    }

    BLUETOOTH_DEVICE_SEARCH_PARAMS sp;
    ZeroMemory(&sp, sizeof(sp));
    sp.dwSize = sizeof(sp);
    /* Only this machine's own devices: paired or currently connected.
       fReturnRemembered stays off - remembered-but-never-used entries
       (Swift Pair bait, stale inquiry cache) are strangers and the list
       has no business showing them. */
    sp.fReturnAuthenticated = TRUE;
    sp.fReturnConnected = TRUE;

    BLUETOOTH_DEVICE_INFO info;
    ZeroMemory(&info, sizeof(info));
    info.dwSize = sizeof(info);

    EnterCriticalSection(&g_lock);
    g_radioOn = radioPresent;

    HBLUETOOTH_DEVICE_FIND hFind = pBtFindFirstDevice(&sp, &info);
    if (hFind) {
        do {
            if (info.Address.ullLong != 0) {
                if (!info.fAuthenticated && !info.fConnected) continue;
                wchar_t mac[24], kind[16];
                FormatMac(info.Address.ullLong, mac, 24);
                const wchar_t *nm = info.szName[0] ? info.szName : mac;
                ClassifyDevice(info.ulClassofDevice, nm, kind, 16);

                int idx = -1;
                for (int i = 0; i < g_devCount; i++) {
                    if (_wcsicmp(g_devices[i].mac, mac) == 0) { idx = i; break; }
                }
                if (idx < 0 && g_devCount < MAX_DEVICES) {
                    idx = g_devCount++;
                    ZeroMemory(&g_devices[idx], sizeof(DeviceEntry));
                    wcsncpy(g_devices[idx].mac, mac, 23);
                    g_devices[idx].allowAuto = TRUE;
                }
                if (idx >= 0) {
                    DeviceEntry *d = &g_devices[idx];
                    d->address = info.Address.ullLong;
                    wcsncpy(d->name, nm, 247);
                    wcsncpy(d->kind, kind, 15);
                    d->connected = info.fConnected ? TRUE : FALSE;
                    /* List only devices this machine has connected to
                       before (or is connected to right now). */
                    d->visible = info.fConnected || HasConnectedBefore(info.Address.ullLong);

                    /* Always ensure profiles stay enabled so manual connect always works! */
                    if (!d->profilesRepaired) {
                        d->profilesRepaired = TRUE;
                        ULONGLONG addrCopy = d->address;
                        wchar_t kindCopy[16];
                        wcsncpy(kindCopy, d->kind, 15); kindCopy[15] = L'\0';
                        LeaveCriticalSection(&g_lock);
                        EnsureProfilesEnabled(addrCopy, kindCopy);
                        EnterCriticalSection(&g_lock);
                    }

                    if (g_guardOn && !d->allowAuto) {
                        if (d->connected) {
                            /* Manual user choice is always allowed.
                               The guard prevents auto-connection, not manual
                               connects via taskbar flyout, Settings, etc. */
                            d->sessionAllowed = TRUE;
                            d->seenConnected = TRUE;
                        }
                        /* Don't disconnect - user's manual choice stands. */
                    }
                }
            }
            ZeroMemory(&info, sizeof(info));
            info.dwSize = sizeof(info);
        } while (pBtFindNextDevice(hFind, &info));
        pBtFindDeviceClose(hFind);
    }
    LeaveCriticalSection(&g_lock);
}

/* Disconnect all guarded non-auto devices right before shutdown / Fast Startup hibernate / sleep */
static void DisconnectGuardedDevicesNow(void) {
    if (!g_guardOn) return;
    PollPairedDevices();
}

static DWORD WINAPI WorkerThread(LPVOID unused) {
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    /* Ensure high-priority zero-delay logon task + Explorer StartupDelayInMSec=0 are installed */
    if (g_autostart) {
        SetAutostart(TRUE);
        SaveRules();
    }

    while (InterlockedCompareExchange(&g_workerRun, 1, 1) == 1) {
        LONGLONG burstUntil = InterlockedCompareExchange64(&g_fastBurstUntil, 0, 0);
        DWORD waitMs = ((LONGLONG)GetTickCount64() < burstUntil) ? 80 : 250;

        DWORD w = WaitForSingleObject(g_qSem, waitMs);
        if (InterlockedCompareExchange(&g_workerRun, 1, 1) == 0) break;

        if (w == WAIT_OBJECT_0) {
            for (;;) {
                WorkItem item;
                BOOL has = FALSE;
                EnterCriticalSection(&g_lock);
                if (g_qHead != g_qTail) {
                    item = g_queue[g_qHead];
                    g_qHead = (g_qHead + 1) % 64;
                    has = TRUE;
                }
                LeaveCriticalSection(&g_lock);
                if (!has) break;
                if (item.op == 1) DoDisconnectOnly(item.address, item.kind, item.name);
                else if (item.op == 2) DoConnectNow(item.address, item.kind, item.name);
            }
        }
        PollPairedDevices();
        if (g_hwnd && !g_devNotifyRadio) RegisterBluetoothNotifications(g_hwnd);
        WriteHeartbeat();
        if (g_hwnd && IsWindowVisible(g_hwnd)) PostMessageW(g_hwnd, WM_WORKER_DONE, 0, 0);
    }
    CoUninitialize();
    return 0;
}

