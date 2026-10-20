#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bluetoothapis.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define APP_TITLE           L"Bluetooth Guard"
#define SINGLE_MUTEX_NAME   L"Local\\Tests_BluetoothGuard_SingleInstance"
#define SHOW_EVENT_NAME     L"Local\\Tests_BluetoothGuard_ShowWindow"

#define IOCTL_BTH_DISCONNECT_DEVICE 0x0041000Cu

#define WM_TRAYICON     (WM_APP + 1)
#define WM_WORKER_DONE  (WM_APP + 2)

#define ID_CHK_GUARD    101
#define ID_CHK_STARTUP  102
#define ID_LIST_DEVICES 103
#define ID_BTN_ALLOW    104
#define ID_BTN_BLOCK    105
#define ID_BTN_CONNECT  106
#define ID_BTN_DISCONN  107
#define ID_BTN_REFRESH  108
#define ID_BTN_QUIT     109

#define IDM_TRAY_OPEN   200
#define IDM_TRAY_PAUSE  201
#define IDM_TRAY_QUIT   202
#define IDM_TRAY_DEV0   300

#define MAX_DEVICES     64

typedef HBLUETOOTH_RADIO_FIND  (WINAPI *PFN_BtFindFirstRadio)(const BLUETOOTH_FIND_RADIO_PARAMS *, HANDLE *);
typedef BOOL                   (WINAPI *PFN_BtFindNextRadio)(HBLUETOOTH_RADIO_FIND, HANDLE *);
typedef BOOL                   (WINAPI *PFN_BtFindRadioClose)(HBLUETOOTH_RADIO_FIND);
typedef HBLUETOOTH_DEVICE_FIND (WINAPI *PFN_BtFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS *, BLUETOOTH_DEVICE_INFO *);
typedef BOOL                   (WINAPI *PFN_BtFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO *);
typedef BOOL                   (WINAPI *PFN_BtFindDeviceClose)(HBLUETOOTH_DEVICE_FIND);
typedef DWORD                  (WINAPI *PFN_BtEnumServices)(HANDLE, const BLUETOOTH_DEVICE_INFO *, DWORD *, GUID *);
typedef DWORD                  (WINAPI *PFN_BtSetServiceState)(HANDLE, const BLUETOOTH_DEVICE_INFO *, const GUID *, DWORD);

static PFN_BtFindFirstRadio   pBtFindFirstRadio   = NULL;
static PFN_BtFindNextRadio    pBtFindNextRadio    = NULL;
static PFN_BtFindRadioClose   pBtFindRadioClose   = NULL;
static PFN_BtFindFirstDevice  pBtFindFirstDevice  = NULL;
static PFN_BtFindNextDevice   pBtFindNextDevice   = NULL;
static PFN_BtFindDeviceClose  pBtFindDeviceClose  = NULL;
static PFN_BtEnumServices     pBtEnumServices     = NULL;
static PFN_BtSetServiceState  pBtSetServiceState  = NULL;

static BOOL InitBluetoothApis(void) {
    HMODULE m = LoadLibraryW(L"BluetoothAPIs.dll");
    if (!m) m = LoadLibraryW(L"bthprops.cpl");
    if (!m) return FALSE;
    pBtFindFirstRadio  = (PFN_BtFindFirstRadio)(void *)GetProcAddress(m, "BluetoothFindFirstRadio");
    pBtFindNextRadio   = (PFN_BtFindNextRadio)(void *)GetProcAddress(m, "BluetoothFindNextRadio");
    pBtFindRadioClose  = (PFN_BtFindRadioClose)(void *)GetProcAddress(m, "BluetoothFindRadioClose");
    pBtFindFirstDevice = (PFN_BtFindFirstDevice)(void *)GetProcAddress(m, "BluetoothFindFirstDevice");
    pBtFindNextDevice  = (PFN_BtFindNextDevice)(void *)GetProcAddress(m, "BluetoothFindNextDevice");
    pBtFindDeviceClose = (PFN_BtFindDeviceClose)(void *)GetProcAddress(m, "BluetoothFindDeviceClose");
    pBtEnumServices    = (PFN_BtEnumServices)(void *)GetProcAddress(m, "BluetoothEnumerateInstalledServices");
    pBtSetServiceState = (PFN_BtSetServiceState)(void *)GetProcAddress(m, "BluetoothSetServiceState");
    return (pBtFindFirstRadio && pBtFindNextRadio && pBtFindRadioClose &&
            pBtFindFirstDevice && pBtFindNextDevice && pBtFindDeviceClose &&
            pBtEnumServices && pBtSetServiceState);
}

typedef struct {
    ULONGLONG address;
    wchar_t   mac[24];
    wchar_t   name[248];
    wchar_t   kind[16];
    BOOL      connected;
    BOOL      allowAuto;
    BOOL      sessionAllowed;
    BOOL      seenConnected;
    BOOL      enforcedIdle;
    ULONGLONG sessionStartTick;
} DeviceEntry;

typedef struct {
    int       op; /* 1 = block+disconnect, 2 = enable+connect */
    ULONGLONG address;
    wchar_t   kind[16];
    wchar_t   name[128];
} WorkItem;

static const GUID kGuidAudioSink      = {0x0000110B, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidAvRemoteTarget = {0x0000110C, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidAvRemote       = {0x0000110E, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHeadset        = {0x00001108, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHandsfree      = {0x0000111E, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidGenericAudio   = {0x00001203, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHid            = {0x00001124, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};

static HWND             g_hwnd      = NULL;
static HWND             g_lblStatus = NULL;
static HWND             g_lblHint   = NULL;
static HWND             g_chkGuard  = NULL;
static HWND             g_chkStart  = NULL;
static HWND             g_list      = NULL;
static HWND             g_btnAllow  = NULL;
static HWND             g_btnBlock  = NULL;
static HWND             g_btnConn   = NULL;
static HWND             g_btnDisc   = NULL;
static HWND             g_btnRef    = NULL;
static HWND             g_btnQuit   = NULL;
static HFONT            g_fontTitle = NULL;
static HFONT            g_fontBody  = NULL;
static HFONT            g_fontBold  = NULL;
static HICON            g_appIcon   = NULL;
static NOTIFYICONDATAW  g_nid;
static BOOL             g_quitting  = FALSE;
static BOOL             g_guardOn   = TRUE;
static BOOL             g_radioOn   = TRUE;
static BOOL             g_trayHinted= FALSE;
static HANDLE           g_showEvent = NULL;
static CRITICAL_SECTION g_lock;

static DeviceEntry      g_devices[MAX_DEVICES];
static int              g_devCount  = 0;

static WorkItem         g_queue[64];
static int              g_qHead     = 0;
static int              g_qTail     = 0;
static HANDLE           g_qSem      = NULL;
static volatile LONG    g_workerRun = 1;
static wchar_t          g_cfgDir[MAX_PATH];
static wchar_t          g_rulesPath[MAX_PATH];
static wchar_t          g_lockPath[MAX_PATH];

typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

static int GetWindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        PFN_GetDpiForWindow fn = (PFN_GetDpiForWindow)(void *)GetProcAddress(user32, "GetDpiForWindow");
        if (fn && hwnd) {
            UINT d = fn(hwnd);
            if (d >= 96) return (int)d;
        }
    }
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi >= 96 ? dpi : 96;
}

static int S(int px, int dpi) { return MulDiv(px, dpi, 96); }

static void FormatMac(ULONGLONG addr, wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%02X:%02X:%02X:%02X:%02X:%02X",
        (unsigned)((addr >> 40) & 0xFF), (unsigned)((addr >> 32) & 0xFF),
        (unsigned)((addr >> 24) & 0xFF), (unsigned)((addr >> 16) & 0xFF),
        (unsigned)((addr >> 8)  & 0xFF), (unsigned)(addr & 0xFF));
    out[cap - 1] = L'\0';
}

static BOOL ContainsCaseInsensitive(const wchar_t *haystack, const wchar_t *needle) {
    size_t nh = wcslen(haystack), nn = wcslen(needle);
    if (nn == 0 || nn > nh) return FALSE;
    for (size_t i = 0; i + nn <= nh; i++) {
        if (_wcsnicmp(haystack + i, needle, nn) == 0) return TRUE;
    }
    return FALSE;
}

static void ClassifyDevice(ULONG cod, const wchar_t *name, wchar_t *out, size_t cap) {
    ULONG major = (cod >> 8) & 0x1F;
    BOOL audioSvc = ((cod & (1u << 21)) != 0) || (major == 0x04);
    if (audioSvc ||
        ContainsCaseInsensitive(name, L"airpod") ||
        ContainsCaseInsensitive(name, L"buds") ||
        ContainsCaseInsensitive(name, L"earbud") ||
        ContainsCaseInsensitive(name, L"headphone") ||
        ContainsCaseInsensitive(name, L"headset") ||
        ContainsCaseInsensitive(name, L"sony") ||
        ContainsCaseInsensitive(name, L"bose") ||
        ContainsCaseInsensitive(name, L"beats") ||
        ContainsCaseInsensitive(name, L"jbl") ||
        ContainsCaseInsensitive(name, L"speaker") ||
        ContainsCaseInsensitive(name, L"xm4") ||
        ContainsCaseInsensitive(name, L"xm5") ||
        ContainsCaseInsensitive(name, L"soundcore") ||
        ContainsCaseInsensitive(name, L"sennheiser")) {
        wcsncpy(out, L"Audio", cap - 1);
    } else if (major == 0x05 ||
               ContainsCaseInsensitive(name, L"mouse") ||
               ContainsCaseInsensitive(name, L"keyboard") ||
               ContainsCaseInsensitive(name, L"controller") ||
               ContainsCaseInsensitive(name, L"xbox") ||
               ContainsCaseInsensitive(name, L"dualsense")) {
        wcsncpy(out, L"Input", cap - 1);
    } else if (major == 0x02) {
        wcsncpy(out, L"Phone", cap - 1);
    } else {
        wcsncpy(out, L"Device", cap - 1);
    }
    out[cap - 1] = L'\0';
}

static void InitPaths(void) {
    wchar_t localApp[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp))) {
        GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
    }
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

static void SaveRules(void) {
    FILE *fp = _wfopen(g_rulesPath, L"w, ccs=UTF-8");
    if (!fp) return;
    EnterCriticalSection(&g_lock);
    fwprintf(fp, L"guard=%d\n", g_guardOn ? 1 : 0);
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
        wchar_t *p1 = wcschr(line, L'|');
        if (!p1) continue;
        *p1++ = L'\0';
        wchar_t *p2 = wcschr(p1, L'|');
        if (!p2) continue;
        *p2++ = L'\0';
        wchar_t *p3 = wcschr(p2, L'|');
        if (!p3) continue;
        *p3++ = L'\0';
        wchar_t *p4 = wcschr(p3, L'|');
        if (!p4) continue;
        *p4++ = L'\0';

        if (g_devCount < MAX_DEVICES) {
            DeviceEntry *d = &g_devices[g_devCount++];
            ZeroMemory(d, sizeof(*d));
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

static void SetAutostart(BOOL enable) {
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hk) != ERROR_SUCCESS) return;
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

static void DoBlockAndDisconnect(ULONGLONG address, const wchar_t *kind) {
    if (!pBtSetServiceState) return;
    BLUETOOTH_DEVICE_INFO bdi;
    ZeroMemory(&bdi, sizeof(bdi));
    bdi.dwSize = sizeof(bdi);
    bdi.Address.ullLong = address;

    GUID installed[16];
    DWORD count = 16;
    if (pBtEnumServices(NULL, &bdi, &count, installed) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < count && i < 16; i++) {
            pBtSetServiceState(NULL, &bdi, &installed[i], BLUETOOTH_SERVICE_DISABLE);
        }
    }

    const GUID *stdGuids[7] = {
        &kGuidAudioSink, &kGuidHandsfree, &kGuidHeadset,
        &kGuidAvRemoteTarget, &kGuidAvRemote, &kGuidGenericAudio, &kGuidHid
    };
    int limit = (_wcsicmp(kind, L"Input") == 0) ? 7 : 6;
    for (int i = 0; i < limit; i++) {
        pBtSetServiceState(NULL, &bdi, stdGuids[i], BLUETOOTH_SERVICE_DISABLE);
    }

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

static void DoEnableAndConnect(ULONGLONG address, const wchar_t *kind) {
    if (!pBtSetServiceState) return;
    BLUETOOTH_DEVICE_INFO bdi;
    ZeroMemory(&bdi, sizeof(bdi));
    bdi.dwSize = sizeof(bdi);
    bdi.Address.ullLong = address;

    if (_wcsicmp(kind, L"Input") == 0) {
        pBtSetServiceState(NULL, &bdi, &kGuidHid, BLUETOOTH_SERVICE_ENABLE);
    } else {
        const GUID *audio[5] = {
            &kGuidAudioSink, &kGuidHandsfree, &kGuidHeadset, &kGuidAvRemoteTarget, &kGuidAvRemote
        };
        for (int i = 0; i < 5; i++) {
            pBtSetServiceState(NULL, &bdi, audio[i], BLUETOOTH_SERVICE_ENABLE);
        }
    }
}

static void EnqueueWork(int op, ULONGLONG address, const wchar_t *kind, const wchar_t *name) {
    EnterCriticalSection(&g_lock);
    int next = (g_qTail + 1) % 64;
    if (next != g_qHead) {
        g_queue[g_qTail].op = op;
        g_queue[g_qTail].address = address;
        wcsncpy(g_queue[g_qTail].kind, kind ? kind : L"Audio", 15);
        wcsncpy(g_queue[g_qTail].name, name ? name : L"", 127);
        g_qTail = next;
        ReleaseSemaphore(g_qSem, 1, NULL);
    }
    LeaveCriticalSection(&g_lock);
}

static void PollPairedDevices(BOOL forceRelock) {
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
    sp.fReturnAuthenticated = TRUE;
    sp.fReturnRemembered = TRUE;
    sp.fReturnConnected = TRUE;
    sp.fReturnUnknown = FALSE;
    sp.fIssueInquiry = FALSE;

    BLUETOOTH_DEVICE_INFO info;
    ZeroMemory(&info, sizeof(info));
    info.dwSize = sizeof(info);

    EnterCriticalSection(&g_lock);
    g_radioOn = radioPresent;

    HBLUETOOTH_DEVICE_FIND hFind = pBtFindFirstDevice(&sp, &info);
    if (hFind) {
        do {
            if (info.Address.ullLong != 0) {
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

                    if (g_guardOn && !d->allowAuto) {
                        if (d->connected) {
                            if (d->sessionAllowed) {
                                d->seenConnected = TRUE;
                            } else {
                                d->enforcedIdle = TRUE;
                                LeaveCriticalSection(&g_lock);
                                DoBlockAndDisconnect(d->address, d->kind);
                                EnterCriticalSection(&g_lock);
                                d->connected = FALSE;
                            }
                        } else {
                            if (d->sessionAllowed) {
                                ULONGLONG elapsed = GetTickCount64() - d->sessionStartTick;
                                if (d->seenConnected || elapsed > 45000ULL) {
                                    d->sessionAllowed = FALSE;
                                    d->seenConnected = FALSE;
                                    d->enforcedIdle = FALSE;
                                }
                            }
                            if (!d->sessionAllowed && (forceRelock || !d->enforcedIdle)) {
                                d->enforcedIdle = TRUE;
                                LeaveCriticalSection(&g_lock);
                                DoBlockAndDisconnect(d->address, d->kind);
                                EnterCriticalSection(&g_lock);
                            }
                        }
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

static DWORD WINAPI WorkerThread(LPVOID unused) {
    (void)unused;
    while (InterlockedCompareExchange(&g_workerRun, 1, 1) == 1) {
        DWORD w = WaitForSingleObject(g_qSem, 2200);
        if (InterlockedCompareExchange(&g_workerRun, 1, 1) == 0) break;

        if (w == WAIT_OBJECT_0) {
            while (1) {
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
                if (item.op == 1) DoBlockAndDisconnect(item.address, item.kind);
                else if (item.op == 2) DoEnableAndConnect(item.address, item.kind);
            }
        }
        PollPairedDevices(FALSE);
        WriteHeartbeat();
        if (g_hwnd) PostMessageW(g_hwnd, WM_WORKER_DONE, 0, 0);
    }
    return 0;
}

static int GetSelectedDeviceIndex(void) {
    return (int)SendMessageW(g_list, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
}

static void RefreshUiList(void) {
    int sel = GetSelectedDeviceIndex();
    SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g_list, LVM_DELETEALLITEMS, 0, 0);

    int blockedCount = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount; i++) {
        DeviceEntry *d = &g_devices[i];
        if (!d->allowAuto) blockedCount++;

        LVITEMW lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = d->name;
        SendMessageW(g_list, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

        LVITEMW sub;
        ZeroMemory(&sub, sizeof(sub));
        sub.mask = LVIF_TEXT;
        sub.iItem = i;

        sub.iSubItem = 1;
        sub.pszText = d->kind;
        SendMessageW(g_list, LVM_SETITEMW, 0, (LPARAM)&sub);

        wchar_t connBuf[64];
        if (d->connected && !d->allowAuto && d->sessionAllowed)
            wcsncpy(connBuf, L"Connected (This Session)", 63);
        else if (d->connected)
            wcsncpy(connBuf, L"Connected", 63);
        else
            wcsncpy(connBuf, L"Disconnected", 63);
        sub.iSubItem = 2;
        sub.pszText = connBuf;
        SendMessageW(g_list, LVM_SETITEMW, 0, (LPARAM)&sub);

        wchar_t ruleBuf[64];
        wcsncpy(ruleBuf, d->allowAuto ? L"Allowed (Auto-Connect)" : L"BLOCKED (Phone Safe)", 63);
        sub.iSubItem = 3;
        sub.pszText = ruleBuf;
        SendMessageW(g_list, LVM_SETITEMW, 0, (LPARAM)&sub);
    }
    BOOL radioOn = g_radioOn;
    int total = g_devCount;
    LeaveCriticalSection(&g_lock);

    if (sel >= 0 && sel < total) {
        LVITEMW st;
        ZeroMemory(&st, sizeof(st));
        st.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        st.state = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW(g_list, LVM_SETITEMSTATE, sel, (LPARAM)&st);
    }
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_list, NULL, TRUE);

    wchar_t banner[256];
    if (!g_guardOn) {
        wcsncpy(banner, L"Status: Guard Paused — Windows is allowed to auto-connect all paired devices.", 255);
    } else if (!radioOn) {
        wcsncpy(banner, L"Status: Bluetooth is off — Guard is watching and ready when Bluetooth turns on.", 255);
    } else {
        _snwprintf(banner, 255, L"Status: Guard Active — %d of %d paired device(s) set to BLOCKED (Phone Safe).",
                   blockedCount, total);
    }
    SetWindowTextW(g_lblStatus, banner);
}

static void ApplySelectedAction(int action) {
    int idx = GetSelectedDeviceIndex();
    if (idx < 0) {
        MessageBoxW(g_hwnd, L"Select a Bluetooth device in the list first.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    ULONGLONG addr = 0;
    wchar_t kind[16] = L"Audio", name[128] = L"";

    EnterCriticalSection(&g_lock);
    if (idx < g_devCount) {
        DeviceEntry *d = &g_devices[idx];
        addr = d->address;
        wcsncpy(kind, d->kind, 15);
        wcsncpy(name, d->name, 127);
        if (action == ID_BTN_ALLOW) {
            d->allowAuto = TRUE;
            d->sessionAllowed = FALSE;
            d->enforcedIdle = FALSE;
        } else if (action == ID_BTN_BLOCK) {
            d->allowAuto = FALSE;
            d->sessionAllowed = FALSE;
            d->enforcedIdle = TRUE;
        } else if (action == ID_BTN_CONNECT) {
            d->sessionAllowed = TRUE;
            d->seenConnected = FALSE;
            d->enforcedIdle = FALSE;
            d->sessionStartTick = GetTickCount64();
        } else if (action == ID_BTN_DISCONN) {
            d->sessionAllowed = FALSE;
            d->enforcedIdle = TRUE;
        }
    }
    LeaveCriticalSection(&g_lock);

    SaveRules();
    if (action == ID_BTN_ALLOW || action == ID_BTN_CONNECT)
        EnqueueWork(2, addr, kind, name);
    else if (action == ID_BTN_BLOCK || action == ID_BTN_DISCONN)
        EnqueueWork(1, addr, kind, name);

    RefreshUiList();
}

static void ShowTrayMenu(void) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Bluetooth Guard");
    AppendMenuW(hMenu, MF_STRING | (g_guardOn ? MF_CHECKED : 0), IDM_TRAY_PAUSE, L"Guard Active");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount && i < 20; i++) {
        wchar_t label[300];
        _snwprintf(label, 299, L"%s %s",
                   g_devices[i].connected ? L"Disconnect:" : L"Connect Now:",
                   g_devices[i].name);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_DEV0 + i, label);
    }
    LeaveCriticalSection(&g_lock);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Bluetooth Guard");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(hMenu);
}

static void LayoutControls(HWND hwnd) {
    int dpi = GetWindowDpi(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int m = S(16, dpi);
    int cw = w - m * 2;

    MoveWindow(g_lblStatus, m, S(14, dpi), cw, S(24, dpi), TRUE);
    MoveWindow(g_chkGuard,  m, S(42, dpi), cw / 2 - S(8, dpi), S(24, dpi), TRUE);
    MoveWindow(g_chkStart,  m + cw / 2, S(42, dpi), cw / 2, S(24, dpi), TRUE);
    MoveWindow(g_lblHint,   m, S(70, dpi), cw, S(20, dpi), TRUE);

    int listTop = S(96, dpi);
    int bottomArea = S(96, dpi);
    int listH = h - listTop - bottomArea;
    if (listH < S(120, dpi)) listH = S(120, dpi);
    MoveWindow(g_list, m, listTop, cw, listH, TRUE);

    int row1Y = listTop + listH + S(12, dpi);
    int bw4 = (cw - S(30, dpi)) / 4;
    MoveWindow(g_btnAllow, m,                      row1Y, bw4, S(34, dpi), TRUE);
    MoveWindow(g_btnBlock, m + (bw4 + S(10, dpi)), row1Y, bw4, S(34, dpi), TRUE);
    MoveWindow(g_btnConn,  m + (bw4 + S(10, dpi)) * 2, row1Y, bw4, S(34, dpi), TRUE);
    MoveWindow(g_btnDisc,  m + (bw4 + S(10, dpi)) * 3, row1Y, bw4, S(34, dpi), TRUE);

    int row2Y = row1Y + S(42, dpi);
    MoveWindow(g_btnRef,  m, row2Y, S(140, dpi), S(32, dpi), TRUE);
    MoveWindow(g_btnQuit, w - m - S(160, dpi), row2Y, S(160, dpi), S(32, dpi), TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int dpi = GetWindowDpi(hwnd);
        g_fontTitle = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBold  = CreateFontW(-S(10, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBody  = CreateFontW(-S(10, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        g_lblStatus = CreateWindowExW(0, L"STATIC", L"Scanning paired Bluetooth devices...",
                                      WS_CHILD | WS_VISIBLE, 0, 0, 100, 24, hwnd, NULL, NULL, NULL);
        g_chkGuard  = CreateWindowExW(0, L"BUTTON", L"Actively block auto-connect for Blocked devices",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 100, 24,
                                      hwnd, (HMENU)ID_CHK_GUARD, NULL, NULL);
        g_chkStart  = CreateWindowExW(0, L"BUTTON", L"Start quietly when Windows logs in",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 100, 24,
                                      hwnd, (HMENU)ID_CHK_STARTUP, NULL, NULL);
        g_lblHint   = CreateWindowExW(0, L"STATIC",
                                      L"Select your phone earbuds below and click \"Block Auto-Connect\". Use \"Connect Now\" whenever you want them on this PC.",
                                      WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hwnd, NULL, NULL, NULL);

        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                 0, 0, 100, 200, hwnd, (HMENU)ID_LIST_DEVICES, NULL, NULL);
        SendMessageW(g_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                     LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = S(220, dpi); col.pszText = L"Bluetooth Device";
        SendMessageW(g_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
        col.cx = S(80, dpi);  col.pszText = L"Type";
        SendMessageW(g_list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
        col.cx = S(165, dpi); col.pszText = L"Status";
        SendMessageW(g_list, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);
        col.cx = S(185, dpi); col.pszText = L"Auto-Connect Rule";
        SendMessageW(g_list, LVM_INSERTCOLUMNW, 3, (LPARAM)&col);

        g_btnAllow = CreateWindowExW(0, L"BUTTON", L"Allow Auto-Connect",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_ALLOW, NULL, NULL);
        g_btnBlock = CreateWindowExW(0, L"BUTTON", L"Block Auto-Connect",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_BLOCK, NULL, NULL);
        g_btnConn  = CreateWindowExW(0, L"BUTTON", L"Connect Now",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_CONNECT, NULL, NULL);
        g_btnDisc  = CreateWindowExW(0, L"BUTTON", L"Disconnect",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_DISCONN, NULL, NULL);
        g_btnRef   = CreateWindowExW(0, L"BUTTON", L"Refresh Devices",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
        g_btnQuit  = CreateWindowExW(0, L"BUTTON", L"Quit Completely",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 32,
                                     hwnd, (HMENU)ID_BTN_QUIT, NULL, NULL);

        HWND ctrls[] = {g_chkGuard, g_chkStart, g_lblHint, g_list,
                        g_btnAllow, g_btnBlock, g_btnConn, g_btnDisc, g_btnRef, g_btnQuit};
        SendMessageW(g_lblStatus, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
        for (size_t i = 0; i < sizeof(ctrls)/sizeof(ctrls[0]); i++)
            SendMessageW(ctrls[i], WM_SETFONT, (WPARAM)g_fontBody, TRUE);

        SendMessageW(g_chkGuard, BM_SETCHECK, g_guardOn ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_chkStart, BM_SETCHECK, IsAutostartEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

        LayoutControls(hwnd);
        SetTimer(hwnd, 1, 800, NULL);
        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_CHK_GUARD) {
            g_guardOn = (SendMessageW(g_chkGuard, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveRules();
            ReleaseSemaphore(g_qSem, 1, NULL);
            RefreshUiList();
        } else if (id == ID_CHK_STARTUP) {
            SetAutostart(SendMessageW(g_chkStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
        } else if (id == ID_BTN_ALLOW || id == ID_BTN_BLOCK || id == ID_BTN_CONNECT || id == ID_BTN_DISCONN) {
            ApplySelectedAction(id);
        } else if (id == ID_BTN_REFRESH) {
            ReleaseSemaphore(g_qSem, 1, NULL);
        } else if (id == ID_BTN_QUIT || id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
        } else if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (id == IDM_TRAY_PAUSE) {
            g_guardOn = !g_guardOn;
            SendMessageW(g_chkGuard, BM_SETCHECK, g_guardOn ? BST_CHECKED : BST_UNCHECKED, 0);
            SaveRules();
            RefreshUiList();
        } else if (id >= IDM_TRAY_DEV0 && id < IDM_TRAY_DEV0 + MAX_DEVICES) {
            int di = id - IDM_TRAY_DEV0;
            EnterCriticalSection(&g_lock);
            if (di < g_devCount) {
                DeviceEntry *d = &g_devices[di];
                ULONGLONG addr = d->address;
                wchar_t kind[16], name[128];
                wcsncpy(kind, d->kind, 15); kind[15] = L'\0';
                wcsncpy(name, d->name, 127); name[127] = L'\0';
                BOOL wasConn = d->connected;
                if (wasConn) {
                    d->sessionAllowed = FALSE;
                    d->enforcedIdle = TRUE;
                } else {
                    d->sessionAllowed = TRUE;
                    d->seenConnected = FALSE;
                    d->enforcedIdle = FALSE;
                    d->sessionStartTick = GetTickCount64();
                }
                LeaveCriticalSection(&g_lock);
                EnqueueWork(wasConn ? 1 : 2, addr, kind, name);
            } else {
                LeaveCriticalSection(&g_lock);
            }
        }
        return 0;
    }

    case WM_WORKER_DONE:
        RefreshUiList();
        return 0;

    case WM_TIMER:
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        WriteHeartbeat();
        return 0;

    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) {
            EnterCriticalSection(&g_lock);
            for (int i = 0; i < g_devCount; i++) {
                g_devices[i].sessionAllowed = FALSE;
                g_devices[i].seenConnected = FALSE;
                g_devices[i].enforcedIdle = FALSE;
            }
            LeaveCriticalSection(&g_lock);
        } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            ReleaseSemaphore(g_qSem, 1, NULL);
        }
        return TRUE;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;

    case WM_CLOSE:
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            if (!g_trayHinted) {
                g_trayHinted = TRUE;
                g_nid.uFlags = NIF_INFO;
                wcsncpy(g_nid.szInfoTitle, L"Bluetooth Guard is still running", 63);
                wcsncpy(g_nid.szInfo, L"Guarding your earbuds in the background. Right-click the tray icon any time.", 255);
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        DeleteFileW(g_lockPath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;
    InitPaths();

    if (cmdLine && wcsstr(cmdLine, L"--stop")) {
        FILE *fp = _wfopen(g_lockPath, L"r");
        if (fp) {
            unsigned long pid = 0;
            if (fscanf(fp, "%lu", &pid) == 1 && pid > 0 && pid != GetCurrentProcessId()) {
                HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
                if (hp) { TerminateProcess(hp, 0); CloseHandle(hp); }
            }
            fclose(fp);
        }
        DeleteFileW(g_lockPath);
        return 0;
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
        if (ev) { SetEvent(ev); CloseHandle(ev); }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    InitializeCriticalSection(&g_lock);
    g_qSem = CreateSemaphoreW(NULL, 1, 64, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    InitBluetoothApis();
    LoadRules();
    WriteHeartbeat();

    g_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = g_appIcon;
    wc.hIconSm       = g_appIcon;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TestsBluetoothGuardWnd";
    RegisterClassExW(&wc);

    int dpi = GetWindowDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(700, dpi), S(480, dpi),
                             NULL, NULL, hInst, NULL);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_appIcon;
    wcsncpy(g_nid.szTip, L"Bluetooth Guard", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

    BOOL silent = (cmdLine && wcsstr(cmdLine, L"--silent") != NULL);
    if (!silent) {
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_workerRun, 0);
    ReleaseSemaphore(g_qSem, 1, NULL);
    if (hThread) { WaitForSingleObject(hThread, 1500); CloseHandle(hThread); }
    CloseHandle(g_qSem);
    if (g_showEvent) CloseHandle(g_showEvent);
    if (hMutex) CloseHandle(hMutex);
    DeleteCriticalSection(&g_lock);
    return 0;
}
