/* Disconnect active link cleanly WITHOUT disabling Bluetooth profiles! */
static void DoDisconnectOnly(ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    (void)kind;
    TriggerBtAudioKsProperty(address, name, KSPROPERTY_ONESHOT_DISCONNECT);

    if (!pBtFindFirstRadio) return;
    BLUETOOTH_FIND_RADIO_PARAMS rp;
    rp.dwSize = sizeof(rp);
    HANDLE hRadio = NULL;
    HBLUETOOTH_RADIO_FIND hFind = pBtFindFirstRadio(&rp, &hRadio);
    if (hFind) {
        do {
            if (hRadio) {
                ULONGLONG addr = address;
                DWORD ret = 0;
                DeviceIoControl(hRadio, IOCTL_BTH_DISCONNECT_DEVICE, &addr, sizeof(addr),
                                NULL, 0, &ret, NULL);
                CloseHandle(hRadio);
                hRadio = NULL;
            }
        } while (pBtFindNextRadio(hFind, &hRadio));
        pBtFindRadioClose(hFind);
    }
}

/* Actively connect only a device with durable trusted membership. This second
   check closes stale tray-command/queue races with Forget. */
static void DoConnectNow(ULONGLONG address, const wchar_t *kind, const wchar_t *name, BOOL automatic) {
    BOOL trusted = FALSE;
    ULONGLONG now = GetTickCount64();
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount; i++) {
        if (g_devices[i].address == address && g_devices[i].visible && !g_devices[i].forgotten && !g_devices[i].denied) {
            DeviceEntry *d = &g_devices[i];
            if (g_suspendAutoConnect || d->autoPaused || (!d->allowAuto && !d->sessionAllowed)) break;
            if (automatic && !DeviceAutoConnectDue(d, now, g_radioOn && !g_suspendAutoConnect)) break;
            trusted = TRUE;
            d->connectState = BT_CONNECT_REQUESTED;
            d->connectRequestTick = now;
            if (d->connectAttempts < 5) d->connectAttempts++;
            d->nextConnectTick = now + ConnectRetryDelayMs(d->connectAttempts);
            break;
        }
    }
    LeaveCriticalSection(&g_lock);
    if (!trusted) return;
    EnsureProfilesEnabled(address, kind);
    BtAudioResult result = TriggerBtAudioKsProperty(address, name, KSPROPERTY_ONESHOT_RECONNECT);
    EnterCriticalSection(&g_lock);
    int idx = FindDeviceIndexByAddress(address);
    if (idx >= 0) {
        DeviceEntry *d = &g_devices[idx];
        if (d->visible && !d->forgotten && !d->denied && !d->autoPaused &&
            (d->allowAuto || d->sessionAllowed)) {
            d->connectError = result.commandHr;
            d->connectState = result.requested ? BT_CONNECT_REQUESTED :
                              ((result.matched || FAILED(result.enumerationHr)) ?
                               BT_CONNECT_FAILED : BT_CONNECT_NO_ENDPOINT);
            if (!DeviceIsAudio(d) && !result.matched) d->connectState = BT_CONNECT_WINDOWS;
        }
    }
    LeaveCriticalSection(&g_lock);
}

/* Only the worker runs automatic requests. No queue flood, no Guard gate, and
   every request rechecks current approval/Auto/pause/radio/readiness by address. */
static void RunAutoConnect(void) {
    ULONGLONG now = GetTickCount64(), address = 0;
    wchar_t kind[16] = L"", name[248] = L"";
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount; i++) {
        DeviceEntry *d = &g_devices[i];
        if (!DeviceAutoConnectDue(d, now, g_radioOn && !g_suspendAutoConnect)) continue;
        address = d->address;
        wcsncpy(kind, d->kind, 15);
        wcsncpy(name, d->name, 247);
        break; /* at most one device per worker pass; retain policy responsiveness */
    }
    LeaveCriticalSection(&g_lock);
    if (address) DoConnectNow(address, kind, name, TRUE);
}

/* A Connect/Auto change can supersede a queued explicit Disconnect too. */
static void DisconnectIfStillRequested(ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    EnterCriticalSection(&g_lock);
    int idx = FindDeviceIndexByAddress(address);
    BOOL requested = idx >= 0 && g_devices[idx].autoPaused && !g_devices[idx].sessionAllowed;
    LeaveCriticalSection(&g_lock);
    if (requested) DoDisconnectOnly(address, kind, name);
}

/* A denial/guard drop can be queued before the owner approves a row. Check
   the current decision again rather than disconnecting a newly accepted link. */
static void DisconnectIfStillBlocked(ULONGLONG address, const wchar_t *kind,
                                      const wchar_t *name) {
    if (!address) return;
    EnterCriticalSection(&g_lock);
    int idx = FindDeviceIndexByAddress(address);
    BOOL blocked = (idx < 0) || DeviceShouldBlock(&g_devices[idx], g_guardOn);
    LeaveCriticalSection(&g_lock);
    if (blocked) DoDisconnectOnly(address, kind, name);
}

/* True unpair: remove the device from Windows entirely (the Forget button).
   BluetoothRemoveDevice deletes the pairing itself - NOT merely a profile -
   so the device must be paired again to come back. This is deliberately not
   BluetoothSetServiceState(DISABLE): profiles stay enabled per suite policy. */
static void DoForgetDevice(ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    (void)kind; (void)name;
    if (!pBluetoothRemoveDevice) return;   /* not available on this SKU */
    ULONGLONG addr = address;
    pBluetoothRemoveDevice(&addr);
}

static void InitPaths(void) {
    wchar_t localApp[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp)))
        GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
    _snwprintf(g_cfgDir, MAX_PATH, L"%s\\BluetoothGuard", localApp);
    CreateDirectoryW(g_cfgDir, NULL);
    _snwprintf(g_rulesPath, MAX_PATH, L"%s\\rules.txt", g_cfgDir);
    _snwprintf(g_lockPath, MAX_PATH, L"%s\\running.lock", g_cfgDir);
    _snwprintf(g_hubTogglePath, MAX_PATH, L"%s\\hub-toggle-guard.flag", g_cfgDir);
    _snwprintf(g_hubExitPath, MAX_PATH, L"%s\\hub-exit.flag", g_cfgDir);
}

static void WriteHeartbeat(void) {
    int blocked = 0, trusted = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount; i++) {
        DeviceEntry *d = &g_devices[i];
        if (!DeviceIsListed(d)) continue;
        if (d->visible) trusted++;
        if (d->denied || (d->visible && !d->allowAuto)) blocked++;
    }
    LeaveCriticalSection(&g_lock);

    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%lu|%d|%d|%d\r\n",
                       (unsigned long)GetCurrentProcessId(),
                       g_guardOn ? 1 : 0, blocked, trusted);
    HANDLE hf = CreateFileW(g_lockPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(hf, buf, (DWORD)len, &wr, NULL);
        CloseHandle(hf);
    }
}

static BOOL IsAutostartEnabled(void) {
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hk) != ERROR_SUCCESS) return FALSE;
    wchar_t val[512];
    DWORD sz = sizeof(val), type = 0;
    LONG rc = RegQueryValueExW(hk, L"BluetoothGuard", NULL, &type, (LPBYTE)val, &sz);
    RegCloseKey(hk);
    return (rc == ERROR_SUCCESS && sz > 4);
}

#define FASTBOOT_TASK_NAME L"Tests_BluetoothGuard_FastBoot"

static void RunHiddenCmd(const wchar_t *app, wchar_t *args, DWORD waitMs) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(app, args, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, waitMs);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

