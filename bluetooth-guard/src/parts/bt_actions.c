/* Native Windows KSPROPERTY_ONESHOT_RECONNECT / DISCONNECT — exact API used by Windows 11 Settings! */
static BOOL TriggerBtAudioKsProperty(ULONGLONG address, const wchar_t *devName, ULONG propId) {
    wchar_t hexAddr[20];
    FormatCompactHex(address, hexAddr, 20);

    IMMDeviceEnumerator *pEnum = NULL;
    HRESULT hr = CoCreateInstance(&kClsidMMDevEnum, NULL, CLSCTX_INPROC_SERVER,
                                  &kIidIMMDevEnum, (void **)&pEnum);
    if (FAILED(hr) || !pEnum) return FALSE;

    IMMDeviceCollection *pCol = NULL;
    BOOL triggered = FALSE;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(
            pEnum, eAll, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &pCol)) && pCol) {
        UINT count = 0;
        IMMDeviceCollection_GetCount(pCol, &count);
        for (UINT i = 0; i < count; i++) {
            IMMDevice *pDev = NULL;
            if (FAILED(IMMDeviceCollection_Item(pCol, i, &pDev)) || !pDev) continue;

            IDeviceTopology *pTopo = NULL;
            if (SUCCEEDED(IMMDevice_Activate(pDev, &kIidIDeviceTopo, CLSCTX_INPROC_SERVER,
                                             NULL, (void **)&pTopo)) && pTopo) {
                IConnector *pConnFrom = NULL;
                if (SUCCEEDED(IDeviceTopology_GetConnector(pTopo, 0, &pConnFrom)) && pConnFrom) {
                    IConnector *pConnTo = NULL;
                    if (SUCCEEDED(IConnector_GetConnectedTo(pConnFrom, &pConnTo)) && pConnTo) {
                        static const GUID kIidIPart = {
                            0xae2de0e4, 0x5bca, 0x4f2d, { 0xaa, 0x46, 0x5d, 0x13, 0xf8, 0xfd, 0xb3, 0xa9 }
                        };
                        IPart *pPart = NULL;
                        if (SUCCEEDED(IConnector_QueryInterface(pConnTo, &kIidIPart, (void **)&pPart)) && pPart) {
                            IDeviceTopology *pPeerTopo = NULL;
                            if (SUCCEEDED(IPart_GetTopologyObjects(pPart, &pPeerTopo)) && pPeerTopo) {
                                LPWSTR devId = NULL;
                                if (SUCCEEDED(IDeviceTopology_GetDeviceId(pPeerTopo, &devId)) && devId) {
                                    if (ContainsCI(devId, hexAddr) || EndpointMatchesName(pDev, devName)) {
                                        IKsControl *pKs = NULL;
                                        if (SUCCEEDED(IDeviceTopology_QueryInterface(
                                                pPeerTopo, &kIidIKsControl, (void **)&pKs)) && pKs) {
                                            KSPROPERTY prop;
                                            ZeroMemory(&prop, sizeof(prop));
                                            prop.Set  = kKsPropSetBtAudio;
                                            prop.Id   = propId;
                                            prop.Flags = KSPROPERTY_TYPE_GET;
                                            ULONG bytesRet = 0;
                                            if (SUCCEEDED(IKsControl_KsProperty(
                                                    pKs, &prop, sizeof(prop), NULL, 0, &bytesRet))) {
                                                triggered = TRUE;
                                            }
                                            IKsControl_Release(pKs);
                                        }
                                    }
                                    CoTaskMemFree(devId);
                                }
                                IDeviceTopology_Release(pPeerTopo);
                            }
                            IPart_Release(pPart);
                        }
                        IConnector_Release(pConnTo);
                    }
                    IConnector_Release(pConnFrom);
                }
                IDeviceTopology_Release(pTopo);
            }
            IMMDevice_Release(pDev);
        }
        IMMDeviceCollection_Release(pCol);
    }
    IMMDeviceEnumerator_Release(pEnum);
    return triggered;
}

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

/* Actively connect device right now! */
static void DoConnectNow(ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    EnsureProfilesEnabled(address, kind);
    TriggerBtAudioKsProperty(address, name, KSPROPERTY_ONESHOT_RECONNECT);
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
}

static void WriteHeartbeat(void) {
    int blocked = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount; i++) {
        if (!g_devices[i].allowAuto) blocked++;
    }
    LeaveCriticalSection(&g_lock);

    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%lu|%d|%d|%d\r\n",
                       (unsigned long)GetCurrentProcessId(),
                       g_guardOn ? 1 : 0, blocked, g_devCount);
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

