#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <bluetoothapis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define APP_TITLE           L"Bluetooth Guard"
#define SINGLE_MUTEX_NAME   L"Local\\Tests_BluetoothGuard_SingleInstance"
#define SHOW_EVENT_NAME     L"Local\\Tests_BluetoothGuard_ShowWindow"

#define IOCTL_BTH_DISCONNECT_DEVICE 0x0041000Cu
#define KSPROPERTY_ONESHOT_RECONNECT  0
#define KSPROPERTY_ONESHOT_DISCONNECT 1

#define WM_TRAYICON     (WM_APP + 1)
#define WM_WORKER_DONE  (WM_APP + 2)

#define IDM_TRAY_OPEN   200
#define IDM_TRAY_PAUSE  201
#define IDM_TRAY_QUIT   202
#define IDM_TRAY_DEV0   300

#define MAX_DEVICES     64
#define MAX_HITBOXES    192

/* Theme palette (matches Project Hub & Keyboard Lighting) */
#define COL_BG          RGB(18, 20, 28)
#define COL_CARD        RGB(27, 31, 44)
#define COL_CARD_HOVER  RGB(32, 37, 52)
#define COL_BORDER      RGB(44, 51, 70)
#define COL_TEXT        RGB(240, 243, 250)
#define COL_MUTED       RGB(142, 152, 178)
#define COL_PRIMARY     RGB(37, 99, 235)
#define COL_PRIMARY_HOV RGB(59, 130, 246)
#define COL_SEC_BTN     RGB(38, 44, 62)
#define COL_SEC_HOV     RGB(50, 58, 82)
#define COL_DANGER      RGB(185, 28, 28)
#define COL_DANGER_HOV  RGB(220, 38, 38)
#define COL_GOOD        RGB(52, 211, 153)
#define COL_GOOD_BG     RGB(22, 58, 48)
#define COL_WARN        RGB(251, 191, 36)
#define COL_WARN_BG     RGB(62, 50, 24)
#define COL_ACCENT      RGB(96, 165, 250)
#define COL_ACCENT_BG   RGB(26, 48, 78)
#define COL_PILL_MUTED  RGB(36, 41, 56)

enum HitKind {
    HIT_NONE = 0,
    HIT_TOGGLE_GUARD,
    HIT_TOGGLE_STARTUP,
    HIT_BTN_REFRESH,
    HIT_BTN_QUIT,
    HIT_DEV_AUTO_TOGGLE,
    HIT_DEV_CONNECT_BTN
};

typedef struct {
    RECT rect;
    int  kind;
    int  index;
} HitBox;

typedef struct {
    ULONGLONG address;
    wchar_t   mac[24];
    wchar_t   name[248];
    wchar_t   kind[16];
    BOOL      connected;
    BOOL      allowAuto;
    BOOL      sessionAllowed;
    BOOL      seenConnected;
    BOOL      profilesRepaired;
    ULONGLONG sessionStartTick;
} DeviceEntry;

typedef struct {
    int       op; /* 1 = disconnect now, 2 = connect now */
    ULONGLONG address;
    wchar_t   mac[24];
    wchar_t   kind[16];
    wchar_t   name[128];
} WorkItem;

typedef HBLUETOOTH_RADIO_FIND  (WINAPI *PFN_BtFindFirstRadio)(const BLUETOOTH_FIND_RADIO_PARAMS *, HANDLE *);
typedef BOOL                   (WINAPI *PFN_BtFindNextRadio)(HBLUETOOTH_RADIO_FIND, HANDLE *);
typedef BOOL                   (WINAPI *PFN_BtFindRadioClose)(HBLUETOOTH_RADIO_FIND);
typedef HBLUETOOTH_DEVICE_FIND (WINAPI *PFN_BtFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS *, BLUETOOTH_DEVICE_INFO *);
typedef BOOL                   (WINAPI *PFN_BtFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO *);
typedef BOOL                   (WINAPI *PFN_BtFindDeviceClose)(HBLUETOOTH_DEVICE_FIND);
typedef DWORD                  (WINAPI *PFN_BtSetServiceState)(HANDLE, const BLUETOOTH_DEVICE_INFO *, const GUID *, DWORD);
typedef UINT                   (WINAPI *PFN_GetDpiForWindow)(HWND);
typedef HRESULT                (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static PFN_BtFindFirstRadio   pBtFindFirstRadio   = NULL;
static PFN_BtFindNextRadio    pBtFindNextRadio    = NULL;
static PFN_BtFindRadioClose   pBtFindRadioClose   = NULL;
static PFN_BtFindFirstDevice  pBtFindFirstDevice  = NULL;
static PFN_BtFindNextDevice   pBtFindNextDevice   = NULL;
static PFN_BtFindDeviceClose  pBtFindDeviceClose  = NULL;
static PFN_BtSetServiceState  pBtSetServiceState  = NULL;

static const GUID kGuidAudioSink      = {0x0000110B, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidAvRemoteTarget = {0x0000110C, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidAvRemote       = {0x0000110E, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHeadset        = {0x00001108, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHandsfree      = {0x0000111E, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};
static const GUID kGuidHid            = {0x00001124, 0x0000, 0x1000, {0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};

/* Windows KSPROPSETID_BtAudio = {7FA06C40-B8F6-4C7E-8556-E8C33A12E54D} */
static const GUID kKsPropSetBtAudio   = {0x7FA06C40, 0xB8F6, 0x4C7E, {0x85,0x56,0xE8,0xC3,0x3A,0x12,0xE5,0x4D}};
static const CLSID kClsidMMDevEnum    = {0xBCDE0395, 0xE52F, 0x467C, {0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const IID   kIidIMMDevEnum     = {0xA95664D2, 0x9614, 0x4F35, {0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static const IID   kIidIDeviceTopo    = {0x2A07407E, 0x6497, 0x4A18, {0x97,0x87,0x32,0xF7,0x9B,0xD0,0xD9,0x8F}};
static const IID   kIidIKsControl     = {0x28F54685, 0x06FD, 0x11D2, {0xB2,0x7A,0x00,0xA0,0xC9,0x22,0x31,0x96}};

static HWND             g_hwnd       = NULL;
static HICON            g_appIcon    = NULL;
static NOTIFYICONDATAW  g_nid;
static BOOL             g_quitting   = FALSE;
static BOOL             g_guardOn    = TRUE;
static BOOL             g_autostart  = FALSE;
static BOOL             g_radioOn    = TRUE;
static BOOL             g_trayHinted = FALSE;
static HANDLE           g_showEvent  = NULL;
static CRITICAL_SECTION g_lock;

static DeviceEntry      g_devices[MAX_DEVICES];
static int              g_devCount   = 0;

static HitBox           g_hits[MAX_HITBOXES];
static int              g_hitCount   = 0;
static int              g_hoverIdx   = -1;
static int              g_scrollY    = 0;
static int              g_contentH   = 0;

static ULONGLONG        g_manualGraceUntil = 0;

static WorkItem         g_queue[64];
static int              g_qHead      = 0;
static int              g_qTail      = 0;
static HANDLE           g_qSem       = NULL;
static volatile LONG    g_workerRun  = 1;
static wchar_t          g_cfgDir[MAX_PATH];
static wchar_t          g_rulesPath[MAX_PATH];
static wchar_t          g_lockPath[MAX_PATH];

static int GetWinDpi(HWND hwnd) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        PFN_GetDpiForWindow fn = (PFN_GetDpiForWindow)(void *)GetProcAddress(u32, "GetDpiForWindow");
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

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD roundPref = 2; /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 33, &roundPref, sizeof(roundPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
    }
    FreeLibrary(dwm);
}

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
    pBtSetServiceState = (PFN_BtSetServiceState)(void *)GetProcAddress(m, "BluetoothSetServiceState");
    return (pBtFindFirstRadio && pBtFindNextRadio && pBtFindRadioClose &&
            pBtFindFirstDevice && pBtFindNextDevice && pBtFindDeviceClose && pBtSetServiceState);
}

static void FormatMac(ULONGLONG addr, wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%02X:%02X:%02X:%02X:%02X:%02X",
        (unsigned)((addr >> 40) & 0xFF), (unsigned)((addr >> 32) & 0xFF),
        (unsigned)((addr >> 24) & 0xFF), (unsigned)((addr >> 16) & 0xFF),
        (unsigned)((addr >> 8)  & 0xFF), (unsigned)(addr & 0xFF));
    out[cap - 1] = L'\0';
}

static void FormatCompactHex(ULONGLONG addr, wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%012llX", addr);
    out[cap - 1] = L'\0';
}

static BOOL ContainsCI(const wchar_t *hay, const wchar_t *needle) {
    size_t nh = wcslen(hay), nn = wcslen(needle);
    if (nn == 0 || nn > nh) return FALSE;
    for (size_t i = 0; i + nn <= nh; i++) {
        if (_wcsnicmp(hay + i, needle, nn) == 0) return TRUE;
    }
    return FALSE;
}

/* Detect when the user is actively clicking inside Windows Settings or Windows 11 Quick Settings flyout */
static void CheckForegroundManualIntent(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return;
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hp) return;
    wchar_t img[MAX_PATH] = L"";
    DWORD sz = MAX_PATH;
    if (QueryFullProcessImageNameW(hp, 0, img, &sz)) {
        if (ContainsCI(img, L"SystemSettings.exe") ||
            ContainsCI(img, L"ShellExperienceHost.exe")) {
            /* User opened Windows Settings or Quick Settings panel — allow manual connects! */
            g_manualGraceUntil = GetTickCount64() + 25000ULL;
        }
    }
    CloseHandle(hp);
}

static void ClassifyDevice(ULONG cod, const wchar_t *name, wchar_t *out, size_t cap) {
    ULONG major = (cod >> 8) & 0x1F;
    BOOL audioSvc = ((cod & (1u << 21)) != 0) || (major == 0x04);
    if (audioSvc || ContainsCI(name, L"airpod") || ContainsCI(name, L"buds") ||
        ContainsCI(name, L"earbud") || ContainsCI(name, L"headphone") ||
        ContainsCI(name, L"headset") || ContainsCI(name, L"sony") ||
        ContainsCI(name, L"bose") || ContainsCI(name, L"beats") ||
        ContainsCI(name, L"jbl") || ContainsCI(name, L"speaker") ||
        ContainsCI(name, L"xm4") || ContainsCI(name, L"xm5") ||
        ContainsCI(name, L"soundcore") || ContainsCI(name, L"sennheiser")) {
        wcsncpy(out, L"Audio", cap - 1);
    } else if (major == 0x05 || ContainsCI(name, L"mouse") || ContainsCI(name, L"keyboard") ||
               ContainsCI(name, L"controller") || ContainsCI(name, L"xbox") || ContainsCI(name, L"dualsense")) {
        wcsncpy(out, L"Input", cap - 1);
    } else if (major == 0x02) {
        wcsncpy(out, L"Phone", cap - 1);
    } else {
        wcsncpy(out, L"Device", cap - 1);
    }
    out[cap - 1] = L'\0';
}

/* Keep Windows Bluetooth service profiles ENABLED so manual Connect in Windows Settings ALWAYS works! */
static void EnsureProfilesEnabled(ULONGLONG address, const wchar_t *kind) {
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

/* Match endpoint adapter friendly name (covers HFP / LE Audio when PnP ID lacks classic hex MAC) */
static BOOL EndpointMatchesName(IMMDevice *pDev, const wchar_t *devName) {
    if (!pDev || !devName || !devName[0]) return FALSE;
    static const PROPERTYKEY kPkeyDevIfaceFriendly = {
        { 0x026e516e, 0xb814, 0x414b, { 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22 } }, 2
    };
    IPropertyStore *pProps = NULL;
    BOOL match = FALSE;
    if (SUCCEEDED(pDev->lpVtbl->OpenPropertyStore(pDev, STGM_READ, &pProps)) && pProps) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(pProps->lpVtbl->GetValue(pProps, &kPkeyDevIfaceFriendly, &pv))) {
            if (pv.vt == VT_LPWSTR && pv.pwszVal && _wcsicmp(pv.pwszVal, devName) == 0) {
                match = TRUE;
            }
            PropVariantClear(&pv);
        }
        pProps->lpVtbl->Release(pProps);
    }
    return match;
}

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
    g_autostart = enable;
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
        wchar_t *p1 = wcschr(line, L'|'); if (!p1) continue; *p1++ = L'\0';
        wchar_t *p2 = wcschr(p1, L'|');   if (!p2) continue; *p2++ = L'\0';
        wchar_t *p3 = wcschr(p2, L'|');   if (!p3) continue; *p3++ = L'\0';
        wchar_t *p4 = wcschr(p3, L'|');   if (!p4) continue; *p4++ = L'\0';

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

    CheckForegroundManualIntent();
    ULONGLONG nowTick = GetTickCount64();
    BOOL inWindowsManualUi = (nowTick < g_manualGraceUntil);

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
                            /* Approve manual session if user clicked Connect either in our UI OR in Windows Quick Settings / Settings! */
                            if (inWindowsManualUi && !d->sessionAllowed) {
                                d->sessionAllowed = TRUE;
                                d->sessionStartTick = nowTick;
                            }
                            if (d->sessionAllowed) {
                                d->seenConnected = TRUE;
                            } else {
                                ULONGLONG addrCopy = d->address;
                                wchar_t kindCopy[16], nameCopy[128];
                                wcsncpy(kindCopy, d->kind, 15); kindCopy[15] = L'\0';
                                wcsncpy(nameCopy, d->name, 127); nameCopy[127] = L'\0';
                                LeaveCriticalSection(&g_lock);
                                DoDisconnectOnly(addrCopy, kindCopy, nameCopy);
                                EnterCriticalSection(&g_lock);
                                d->connected = FALSE;
                            }
                        } else {
                            if (d->sessionAllowed) {
                                ULONGLONG elapsed = nowTick - d->sessionStartTick;
                                if (d->seenConnected || elapsed > 35000ULL) {
                                    d->sessionAllowed = FALSE;
                                    d->seenConnected = FALSE;
                                }
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
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    while (InterlockedCompareExchange(&g_workerRun, 1, 1) == 1) {
        DWORD w = WaitForSingleObject(g_qSem, 1500);
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
        WriteHeartbeat();
        if (g_hwnd) PostMessageW(g_hwnd, WM_WORKER_DONE, 0, 0);
    }
    CoUninitialize();
    return 0;
}

/* =========================================================================
 *  Clean Modern GDI Drawing Helpers
 * ========================================================================= */
static void FillRoundBox(HDC hdc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN   pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldB = SelectObject(hdc, br);
    HGDIOBJ oldP = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
}

static void DrawPillBadge(HDC hdc, HFONT font, int rightX, int y, const wchar_t *text,
                          COLORREF fg, COLORREF bg, int dpi) {
    SelectObject(hdc, font);
    SIZE sz;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    int padX = S(12, dpi);
    int h = S(24, dpi);
    int w = sz.cx + padX * 2 + S(12, dpi);
    RECT r = { rightX - w, y, rightX, y + h };
    FillRoundBox(hdc, r, h, bg, bg);

    /* Status dot */
    HBRUSH dotBr = CreateSolidBrush(fg);
    HPEN   dotPen = CreatePen(PS_SOLID, 1, fg);
    HGDIOBJ ob = SelectObject(hdc, dotBr);
    HGDIOBJ op = SelectObject(hdc, dotPen);
    int dotSz = S(6, dpi);
    int dy = y + (h - dotSz) / 2;
    Ellipse(hdc, r.left + S(10, dpi), dy, r.left + S(10, dpi) + dotSz, dy + dotSz);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(dotBr);
    DeleteObject(dotPen);

    RECT tr = { r.left + S(21, dpi), y, r.right - S(8, dpi), y + h };
    SetTextColor(hdc, fg);
    DrawTextW(hdc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void DrawActionBtn(HDC hdc, HFONT font, RECT r, const wchar_t *label,
                          int style /* 0=sec, 1=primary, 2=danger */, BOOL hovered, int dpi) {
    COLORREF fill = COL_SEC_BTN, border = COL_BORDER;
    if (style == 1) {
        fill = hovered ? COL_PRIMARY_HOV : COL_PRIMARY;
        border = fill;
    } else if (style == 2) {
        fill = hovered ? COL_DANGER_HOV : COL_DANGER;
        border = fill;
    } else if (hovered) {
        fill = COL_SEC_HOV;
    }
    FillRoundBox(hdc, r, S(8, dpi), fill, border);
    SelectObject(hdc, font);
    SetTextColor(hdc, COL_TEXT);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawToggleSwitch(HDC hdc, HFONT font, RECT r, BOOL on, const wchar_t *label,
                             BOOL hovered, int dpi) {
    int tw = S(40, dpi), th = S(22, dpi);
    int ty = r.top + (r.bottom - r.top - th) / 2;
    RECT track = { r.left, ty, r.left + tw, ty + th };
    COLORREF fill = on ? COL_PRIMARY : (hovered ? COL_SEC_HOV : COL_SEC_BTN);
    COLORREF rim  = on ? COL_PRIMARY_HOV : COL_BORDER;
    FillRoundBox(hdc, track, th, fill, rim);

    int knob = th - S(6, dpi);
    int kx = on ? (track.right - knob - S(3, dpi)) : (track.left + S(3, dpi));
    int ky = ty + S(3, dpi);
    HBRUSH kb = CreateSolidBrush(RGB(255, 255, 255));
    HPEN   kp = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ ob = SelectObject(hdc, kb);
    HGDIOBJ op = SelectObject(hdc, kp);
    Ellipse(hdc, kx, ky, kx + knob, ky + knob);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(kb);
    DeleteObject(kp);

    if (label && label[0]) {
        SelectObject(hdc, font);
        SetTextColor(hdc, hovered ? COL_TEXT : COL_MUTED);
        RECT lr = { track.right + S(9, dpi), r.top, r.right, r.bottom };
        DrawTextW(hdc, label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void AddHit(RECT r, int kind, int index) {
    if (g_hitCount < MAX_HITBOXES) {
        g_hits[g_hitCount].rect  = r;
        g_hits[g_hitCount].kind  = kind;
        g_hits[g_hitCount].index = index;
        g_hitCount++;
    }
}

static void RenderWindow(HWND hwnd, HDC targetDc) {
    RECT client;
    GetClientRect(hwnd, &client);
    int W = client.right - client.left;
    int H = client.bottom - client.top;
    if (W <= 0 || H <= 0) return;

    int dpi = GetWinDpi(hwnd);
    HDC mem = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateCompatibleBitmap(targetDc, W, H);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    HFONT fTitle = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fCard  = CreateFontW(-S(13, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBtn   = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBody  = CreateFontW(-S(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    HBRUSH bgBrush = CreateSolidBrush(COL_BG);
    FillRect(mem, &client, bgBrush);
    DeleteObject(bgBrush);

    g_hitCount = 0;
    int M = S(20, dpi);
    int CW = W - M * 2;

    /* Copy device snapshot thread-safely */
    DeviceEntry snap[MAX_DEVICES];
    int count = 0, blockedCount = 0;
    BOOL radioOn = TRUE;
    EnterCriticalSection(&g_lock);
    count = g_devCount;
    radioOn = g_radioOn;
    if (count > 0) memcpy(snap, g_devices, sizeof(DeviceEntry) * (size_t)count);
    LeaveCriticalSection(&g_lock);
    for (int i = 0; i < count; i++) if (!snap[i].allowAuto) blockedCount++;

    /* Header hero card */
    int heroY = S(16, dpi);
    int heroH = S(104, dpi);
    RECT heroR = { M, heroY, M + CW, heroY + heroH };
    FillRoundBox(mem, heroR, S(12, dpi), COL_CARD, COL_BORDER);

    SelectObject(mem, fTitle);
    SetTextColor(mem, COL_TEXT);
    RECT titleR = { heroR.left + S(18, dpi), heroR.top + S(12, dpi), heroR.right - S(220, dpi), heroR.top + S(40, dpi) };
    DrawTextW(mem, L"Bluetooth Guard", -1, &titleR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (!g_guardOn) {
        DrawPillBadge(mem, fBtn, heroR.right - S(18, dpi), heroR.top + S(15, dpi),
                      L"Guard Paused", COL_WARN, COL_WARN_BG, dpi);
    } else if (!radioOn) {
        DrawPillBadge(mem, fBtn, heroR.right - S(18, dpi), heroR.top + S(15, dpi),
                      L"Bluetooth Off", COL_MUTED, COL_PILL_MUTED, dpi);
    } else {
        wchar_t pillTxt[64];
        _snwprintf(pillTxt, 63, L"Active  ·  %d Protected", blockedCount);
        DrawPillBadge(mem, fBtn, heroR.right - S(18, dpi), heroR.top + S(15, dpi),
                      pillTxt, COL_GOOD, COL_GOOD_BG, dpi);
    }

    SelectObject(mem, fBody);
    SetTextColor(mem, COL_MUTED);
    RECT subR = { heroR.left + S(18, dpi), heroR.top + S(40, dpi), heroR.right - S(18, dpi), heroR.top + S(60, dpi) };
    DrawTextW(mem, L"Turn off Auto-Connect for phone earbuds. Manual connect still works here or in Windows Quick Settings.",
              -1, &subR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT togGuardR = { heroR.left + S(18, dpi), heroR.top + S(66, dpi), heroR.left + S(230, dpi), heroR.top + S(94, dpi) };
    int idxGuard = g_hitCount;
    AddHit(togGuardR, HIT_TOGGLE_GUARD, 0);
    DrawToggleSwitch(mem, fBody, togGuardR, g_guardOn, L"Guard Enabled", g_hoverIdx == idxGuard, dpi);

    RECT togStartR = { heroR.left + S(240, dpi), heroR.top + S(66, dpi), heroR.left + S(470, dpi), heroR.top + S(94, dpi) };
    int idxStart = g_hitCount;
    AddHit(togStartR, HIT_TOGGLE_STARTUP, 0);
    DrawToggleSwitch(mem, fBody, togStartR, g_autostart, L"Start with Windows", g_hoverIdx == idxStart, dpi);

    RECT btnRefR = { heroR.right - S(108, dpi), heroR.top + S(64, dpi), heroR.right - S(18, dpi), heroR.top + S(94, dpi) };
    int idxRef = g_hitCount;
    AddHit(btnRefR, HIT_BTN_REFRESH, 0);
    DrawActionBtn(mem, fBtn, btnRefR, L"Refresh", 0, g_hoverIdx == idxRef, dpi);

    /* Scrollable device cards region */
    int listTop = heroR.bottom + S(14, dpi);
    int footerH = S(50, dpi);
    int listBottom = H - footerH;
    int cardH = S(78, dpi);
    int gap = S(10, dpi);
    g_contentH = count * (cardH + gap);
    int maxScroll = g_contentH - (listBottom - listTop);
    if (maxScroll < 0) maxScroll = 0;
    if (g_scrollY > maxScroll) g_scrollY = maxScroll;
    if (g_scrollY < 0) g_scrollY = 0;

    HRGN clip = CreateRectRgn(0, listTop, W, listBottom);
    SelectClipRgn(mem, clip);

    if (count == 0) {
        RECT emptyR = { M, listTop, M + CW, listTop + S(80, dpi) };
        FillRoundBox(mem, emptyR, S(12, dpi), COL_CARD, COL_BORDER);
        SelectObject(mem, fBody);
        SetTextColor(mem, COL_MUTED);
        RECT er = { emptyR.left + S(18, dpi), emptyR.top, emptyR.right - S(18, dpi), emptyR.bottom };
        DrawTextW(mem, L"No paired Bluetooth devices found yet. Pair headphones/earbuds in Windows Settings, then click Refresh.",
                  -1, &er, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        int y = listTop - g_scrollY;
        for (int i = 0; i < count; i++) {
            DeviceEntry *d = &snap[i];
            RECT cr = { M, y, M + CW, y + cardH };
            if (cr.bottom >= listTop && cr.top <= listBottom) {
                FillRoundBox(mem, cr, S(12, dpi), COL_CARD, COL_BORDER);

                /* Device name + kind */
                wchar_t titleBuf[280];
                _snwprintf(titleBuf, 279, L"%s", d->name);
                SelectObject(mem, fCard);
                SetTextColor(mem, COL_TEXT);
                RECT nr = { cr.left + S(18, dpi), cr.top + S(13, dpi), cr.right - S(340, dpi), cr.top + S(38, dpi) };
                DrawTextW(mem, titleBuf, -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                /* Subtitle status line */
                const wchar_t *subTxt = NULL;
                if (d->connected && !d->allowAuto)
                    subTxt = L"Connected manually for this session  ·  Re-locks automatically after disconnect";
                else if (d->connected)
                    subTxt = L"Connected";
                else if (!d->allowAuto)
                    subTxt = L"Auto-connect OFF (phone safe)  ·  Click Connect any time to use on this PC";
                else
                    subTxt = L"Auto-connect ON  ·  Connects automatically when in range";

                SelectObject(mem, fBody);
                SetTextColor(mem, d->connected ? COL_GOOD : (!d->allowAuto ? COL_ACCENT : COL_MUTED));
                RECT sr = { cr.left + S(18, dpi), cr.top + S(41, dpi), cr.right - S(300, dpi), cr.top + S(64, dpi) };
                DrawTextW(mem, subTxt, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                /* Right controls: Auto-Connect switch + Connect/Disconnect button */
                RECT autoTogR = { cr.right - S(290, dpi), cr.top + S(24, dpi), cr.right - S(138, dpi), cr.top + S(54, dpi) };
                int hitAuto = g_hitCount;
                AddHit(autoTogR, HIT_DEV_AUTO_TOGGLE, i);
                DrawToggleSwitch(mem, fBody, autoTogR, d->allowAuto, L"Auto-Connect", g_hoverIdx == hitAuto, dpi);

                RECT connBtnR = { cr.right - S(126, dpi), cr.top + S(22, dpi), cr.right - S(18, dpi), cr.top + S(56, dpi) };
                int hitConn = g_hitCount;
                AddHit(connBtnR, HIT_DEV_CONNECT_BTN, i);
                DrawActionBtn(mem, fBtn, connBtnR,
                              d->connected ? L"Disconnect" : L"Connect",
                              d->connected ? 2 : 1,
                              g_hoverIdx == hitConn, dpi);
            }
            y += cardH + gap;
        }
    }

    SelectClipRgn(mem, NULL);
    DeleteObject(clip);

    /* Clean bottom footer */
    SelectObject(mem, fBody);
    SetTextColor(mem, COL_MUTED);
    RECT footTxtR = { M, listBottom + S(8, dpi), W - M - S(150, dpi), H - S(8, dpi) };
    DrawTextW(mem, L"Closing this window keeps Bluetooth Guard active in your system tray.",
              -1, &footTxtR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT quitBtnR = { W - M - S(132, dpi), listBottom + S(9, dpi), W - M, H - S(11, dpi) };
    int idxQuit = g_hitCount;
    AddHit(quitBtnR, HIT_BTN_QUIT, 0);
    DrawActionBtn(mem, fBtn, quitBtnR, L"Quit App", 0, g_hoverIdx == idxQuit, dpi);

    BitBlt(targetDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

    DeleteObject(fTitle);
    DeleteObject(fCard);
    DeleteObject(fBtn);
    DeleteObject(fBody);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void HandleHitClick(int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return;
    int kind = g_hits[hitIdx].kind;
    int di   = g_hits[hitIdx].index;

    if (kind == HIT_TOGGLE_GUARD) {
        g_guardOn = !g_guardOn;
        SaveRules();
        ReleaseSemaphore(g_qSem, 1, NULL);
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_TOGGLE_STARTUP) {
        SetAutostart(!g_autostart);
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_BTN_REFRESH) {
        ReleaseSemaphore(g_qSem, 1, NULL);
    } else if (kind == HIT_BTN_QUIT) {
        g_quitting = TRUE;
        DestroyWindow(g_hwnd);
    } else if (kind == HIT_DEV_AUTO_TOGGLE) {
        ULONGLONG addr = 0;
        wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
        BOOL nowAllow = TRUE, wasConn = FALSE;
        EnterCriticalSection(&g_lock);
        if (di >= 0 && di < g_devCount) {
            DeviceEntry *d = &g_devices[di];
            d->allowAuto = !d->allowAuto;
            d->sessionAllowed = FALSE;
            d->seenConnected = FALSE;
            nowAllow = d->allowAuto;
            wasConn = d->connected;
            addr = d->address;
            wcsncpy(dkind, d->kind, 15);
            wcsncpy(dname, d->name, 127);
            wcsncpy(dmac, d->mac, 23);
        }
        LeaveCriticalSection(&g_lock);
        SaveRules();
        if (!nowAllow && wasConn && g_guardOn) {
            EnqueueWork(1, addr, dmac, dkind, dname);
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (kind == HIT_DEV_CONNECT_BTN) {
        ULONGLONG addr = 0;
        wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
        BOOL wasConn = FALSE;
        EnterCriticalSection(&g_lock);
        if (di >= 0 && di < g_devCount) {
            DeviceEntry *d = &g_devices[di];
            wasConn = d->connected;
            addr = d->address;
            wcsncpy(dkind, d->kind, 15);
            wcsncpy(dname, d->name, 127);
            wcsncpy(dmac, d->mac, 23);
            if (wasConn) {
                d->sessionAllowed = FALSE;
                d->seenConnected = FALSE;
                d->connected = FALSE;
            } else {
                d->sessionAllowed = TRUE;
                d->seenConnected = FALSE;
                d->sessionStartTick = GetTickCount64();
            }
        }
        LeaveCriticalSection(&g_lock);
        EnqueueWork(wasConn ? 1 : 2, addr, dmac, dkind, dname);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void ShowTrayMenu(void) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Bluetooth Guard");
    AppendMenuW(hMenu, MF_STRING | (g_guardOn ? MF_CHECKED : 0), IDM_TRAY_PAUSE, L"Guard Enabled");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_devCount && i < 20; i++) {
        wchar_t label[300];
        _snwprintf(label, 299, L"%s  %s",
                   g_devices[i].connected ? L"Disconnect" : L"Connect",
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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        ApplyModernWindowFrame(hwnd);
        SetTimer(hwnd, 1, 700, NULL);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RenderWindow(hwnd, hdc);
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

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
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
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < g_hitCount; i++) {
            if (PtInRect(&g_hits[i].rect, pt)) {
                HandleHitClick(i);
                break;
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (id == IDM_TRAY_PAUSE) {
            g_guardOn = !g_guardOn;
            SaveRules();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
        } else if (id >= IDM_TRAY_DEV0 && id < IDM_TRAY_DEV0 + MAX_DEVICES) {
            int di = id - IDM_TRAY_DEV0;
            ULONGLONG addr = 0;
            wchar_t dkind[16] = L"Audio", dname[128] = L"", dmac[24] = L"";
            BOOL wasConn = FALSE;
            EnterCriticalSection(&g_lock);
            if (di < g_devCount) {
                DeviceEntry *d = &g_devices[di];
                wasConn = d->connected;
                addr = d->address;
                wcsncpy(dkind, d->kind, 15);
                wcsncpy(dname, d->name, 127);
                wcsncpy(dmac, d->mac, 23);
                if (wasConn) {
                    d->sessionAllowed = FALSE;
                    d->seenConnected = FALSE;
                } else {
                    d->sessionAllowed = TRUE;
                    d->seenConnected = FALSE;
                    d->sessionStartTick = GetTickCount64();
                }
            }
            LeaveCriticalSection(&g_lock);
            EnqueueWork(wasConn ? 1 : 2, addr, dmac, dkind, dname);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_WORKER_DONE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        CheckForegroundManualIntent();
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        WriteHeartbeat();
        return 0;

    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) {
            g_manualGraceUntil = 0;
            EnterCriticalSection(&g_lock);
            for (int i = 0; i < g_devCount; i++) {
                g_devices[i].sessionAllowed = FALSE;
                g_devices[i].seenConnected = FALSE;
            }
            LeaveCriticalSection(&g_lock);
        } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            g_manualGraceUntil = 0;
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
                wcsncpy(g_nid.szInfoTitle, L"Bluetooth Guard is active in tray", 63);
                wcsncpy(g_nid.szInfo, L"Right-click the tray icon any time to quick-connect or disconnect.", 255);
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

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    InitializeCriticalSection(&g_lock);
    g_qSem = CreateSemaphoreW(NULL, 1, 64, NULL);

    InitBluetoothApis();
    g_autostart = IsAutostartEnabled();
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
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    wc.lpszClassName = L"TestsBluetoothGuardModernWnd";
    RegisterClassExW(&wc);

    int dpi = GetWinDpi(NULL);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(740, dpi), S(560, dpi),
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
    CoUninitialize();
    return 0;
}
