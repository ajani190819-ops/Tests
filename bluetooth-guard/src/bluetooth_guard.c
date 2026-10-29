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
#include <dbt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <bluetoothapis.h>
#include <math.h>
#include <stdint.h>
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

#define RES_ICON_APP    1
#define RES_ICON_EMBLEM 2

#define IDM_TRAY_OPEN   200
#define IDM_TRAY_PAUSE  201
#define IDM_TRAY_QUIT   202
#define IDM_TRAY_DEV0   300

#define MAX_DEVICES     64
#define MAX_HITBOXES    192

/* Soft Slate Suite Palette (matches Keyboard Lighting, Project Hub & docs/UI_STYLE_GUIDE.md) */
#define COL_BG            RGB(24, 27, 36)    /* #181B24 */
#define COL_FOOTER_BG     RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER       RGB(48, 55, 72)    /* #303748 */
#define COL_CARD          RGB(34, 39, 52)    /* #222734 */
#define COL_BORDER        RGB(56, 64, 84)    /* #384054 */
#define COL_SEC_BTN       RGB(43, 49, 66)    /* #2B3142 */
#define COL_SEC_HOV       RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER    RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_RIM     RGB(96, 112, 144)  /* #607090 */
#define COL_TEXT          RGB(236, 239, 246) /* #ECEFF6 */
#define COL_MUTED         RGB(154, 163, 184) /* #9AA3B8 */
#define COL_TELEMETRY     RGB(116, 126, 148) /* #747E94 */
#define COL_GOOD          RGB(52, 211, 153)  /* #34D399 */
#define COL_GOOD_BG       RGB(24, 52, 44)
#define COL_WARN          RGB(251, 191, 36)  /* #FBBF24 */
#define COL_WARN_BG       RGB(56, 44, 24)
#define COL_ACCENT        RGB(56, 189, 248)  /* #38BDF8 */
#define COL_ACCENT_STRONG RGB(37, 99, 235)   /* #2563EB */
#define COL_ACCENT_HOV    RGB(59, 130, 246)  /* #3B82F6 */
#define COL_ACCENT_BG     RGB(24, 48, 68)
#define COL_DANGER        RGB(248, 113, 113) /* #F87171 */
#define COL_DANGER_BG     RGB(58, 30, 36)
#define COL_IDLE_SLATE    RGB(108, 120, 144)
#define COL_PILL_MUTED    RGB(42, 48, 64)

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
    BOOL      visible;
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
static HDEVNOTIFY       g_devNotifyIface = NULL;
static HDEVNOTIFY       g_devNotifyRadio = NULL;
static HANDLE           g_radioNotifyHandle = NULL;
static NOTIFYICONDATAW  g_nid;
static BOOL             g_quitting   = FALSE;
static BOOL             g_guardOn    = TRUE;
static BOOL             g_autostart  = TRUE;
static BOOL             g_autostartExplicit = FALSE;
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

static volatile LONGLONG g_fastBurstUntil = 0;
static ULONGLONG         g_manualGraceUntil = 0;

typedef BOOL (WINAPI *PFN_SetProcessInformation)(HANDLE, int, LPVOID, DWORD);

typedef struct {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} BtPowerThrottlingState;

static void ArmFastBurstWindow(ULONGLONG durationMs) {
    LONGLONG target = (LONGLONG)(GetTickCount64() + durationMs);
    LONGLONG cur = InterlockedCompareExchange64(&g_fastBurstUntil, 0, 0);
    if (target > cur) InterlockedExchange64(&g_fastBurstUntil, target);
}

/* Maximize CPU scheduling priority & disable Windows 11 EcoQoS throttling so boot/wake reaction is instantaneous */
static void BoostProcessAndShutdownPriority(void) {
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    }
    SetProcessShutdownParameters(0x3FF, 0);

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32) {
        PFN_SetProcessInformation pSetProcInfo =
            (PFN_SetProcessInformation)(void *)GetProcAddress(k32, "SetProcessInformation");
        if (pSetProcInfo) {
            BtPowerThrottlingState pts;
            pts.Version     = 1;
            pts.ControlMask = 0x7; /* EXECUTION_SPEED | IGNORE_TIMER_RESOLUTION | MIN_RESOLUTION */
            pts.StateMask   = 0;   /* Disable all background throttling even on battery */
            pSetProcInfo(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &pts, sizeof(pts));
        }
    }
}

/* Subscribe to live kernel Bluetooth HCI / radio interface notifications for <1ms connect detection */
static void RegisterBluetoothNotifications(HWND hwnd) {
    if (!hwnd) return;
    static const GUID kGuidBthPortIface =
        {0x0850302A, 0xB344, 0x4FDA, {0x9B,0xE9,0x90,0x57,0x6B,0x8D,0x46,0xF0}};

    if (!g_devNotifyIface) {
        DEV_BROADCAST_DEVICEINTERFACE_W dbi;
        ZeroMemory(&dbi, sizeof(dbi));
        dbi.dbcc_size       = sizeof(dbi);
        dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbi.dbcc_classguid  = kGuidBthPortIface;
        g_devNotifyIface = RegisterDeviceNotificationW(hwnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    if (!g_devNotifyRadio && pBtFindFirstRadio && pBtFindRadioClose) {
        BLUETOOTH_FIND_RADIO_PARAMS rp;
        rp.dwSize = sizeof(rp);
        HANDLE hRadio = NULL;
        HBLUETOOTH_RADIO_FIND hRF = pBtFindFirstRadio(&rp, &hRadio);
        if (hRF) {
            pBtFindRadioClose(hRF);
            if (hRadio) {
                DEV_BROADCAST_HANDLE dbh;
                ZeroMemory(&dbh, sizeof(dbh));
                dbh.dbch_size       = sizeof(dbh);
                dbh.dbch_devicetype = DBT_DEVTYP_HANDLE;
                dbh.dbch_handle     = hRadio;
                g_devNotifyRadio = RegisterDeviceNotificationW(hwnd, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
                if (g_devNotifyRadio) g_radioNotifyHandle = hRadio;
                else CloseHandle(hRadio);
            }
        }
    }
}

static void UnregisterBluetoothNotifications(void) {
    if (g_devNotifyRadio) { UnregisterDeviceNotification(g_devNotifyRadio); g_devNotifyRadio = NULL; }
    if (g_radioNotifyHandle) { CloseHandle(g_radioNotifyHandle); g_radioNotifyHandle = NULL; }
    if (g_devNotifyIface) { UnregisterDeviceNotification(g_devNotifyIface); g_devNotifyIface = NULL; }
}

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

/* Has this device ever carried a profile connection on this machine?
   BTHPORT keeps one subkey per service under the device key, and those
   appear the first time a profile actually connects. A paired-but-never-
   used entry (Swift Pair bait, stale cache rows) has the key but no
   services - exactly the devices that must never be listed here. When the
   registry cannot answer, err on showing the device. */
static BOOL HasConnectedBefore(ULONGLONG addr) {
    wchar_t folder[16], path[160];
    for (int i = 0; i < 6; i++) {
        unsigned v = (unsigned)((addr >> (8 * i)) & 0xFF);
        folder[i * 2]     = (wchar_t)(L"0123456789ABCDEF"[(v >> 4) & 15]);
        folder[i * 2 + 1] = (wchar_t)(L"0123456789ABCDEF"[v & 15]);
    }
    folder[12] = L'\0';
    _snwprintf(path, 160, L"SYSTEM\\CurrentControlSet\\Services\\BTHPORT\\Parameters\\Devices\\%s", folder);
    path[159] = L'\0';
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return TRUE;
    DWORD subKeys = 0;
    LONG rc = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) return TRUE;
    return subKeys > 0;
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
                            /* Manual connect via Windows Settings is always allowed.
                               If the user opened Settings/Quick Settings and connected,
                               grant a persistent session so the device stays connected
                               even after disconnect/reconnect cycles. */
                            if (!d->sessionAllowed) {
                                if (inWindowsManualUi) {
                                    /* User manually connected via Windows Settings - allow it */
                                    d->sessionAllowed = TRUE;
                                } else {
                                    /* Auto-connection outside manual UI - disconnect it */
                                    ULONGLONG addrCopy = d->address;
                                    wchar_t kindCopy[16], nameCopy[128];
                                    wcsncpy(kindCopy, d->kind, 15); kindCopy[15] = L'\0';
                                    wcsncpy(nameCopy, d->name, 127); nameCopy[127] = L'\0';
                                    LeaveCriticalSection(&g_lock);
                                    DoDisconnectOnly(addrCopy, kindCopy, nameCopy);
                                    EnterCriticalSection(&g_lock);
                                    d->connected = FALSE;
                                }
                            }
                            /* sessionAllowed is TRUE - leave connected (manual session active) */
                        }
                        /* Device not connected - do nothing. Wait for the next
                           connection attempt; manual connects via Windows Settings
                           will be caught by inWindowsManualUi above. */
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

/* =========================================================================
 *  Soft Slate Sub-Pixel Anti-Aliased Rounded UI Renderer (32-bit DIBSection)
 * ========================================================================= */
static inline uint32_t PackRgb(COLORREF c) {
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static inline uint32_t BlendRgb(uint32_t dst, uint32_t src, int a256) {
    if (a256 <= 0) return dst;
    if (a256 >= 256) return src;
    int inv = 256 - a256;
    uint32_t rb = (((dst & 0x00FF00FFu) * (uint32_t)inv) + ((src & 0x00FF00FFu) * (uint32_t)a256)) >> 8;
    uint32_t g  = (((dst & 0x0000FF00u) * (uint32_t)inv) + ((src & 0x0000FF00u) * (uint32_t)a256)) >> 8;
    return (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void FillCanvasRect(uint32_t *px, int W, int H, int x0, int y0, int x1, int y1, COLORREF col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t p = PackRgb(col);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; x++) row[x] = p;
    }
}

static void SmoothRoundRect(uint32_t *px, int W, int H, const RECT *rc, float radius,
                            COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    float x0 = (float)rc->left, y0 = (float)rc->top;
    float x1 = (float)rc->right, y1 = (float)rc->bottom;
    float rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0.5f || rh <= 0.5f) return;
    if (radius * 2.0f > rw) radius = rw * 0.5f;
    if (radius * 2.0f > rh) radius = rh * 0.5f;

    float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    float hx = 0.5f * rw - radius, hy = 0.5f * rh - radius;

    int iy0 = (int)floorf(y0 - 1.0f);
    int iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f);
    int ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP   = PackRgb(fillCol);
    uint32_t borderP = PackRgb(borderCol);
    BOOL sameBorder  = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd;
            if (qx > 0.0f && qy > 0.0f) {
                sd = sqrtf(qx * qx + qy * qy) - radius;
            } else {
                sd = (qx > qy ? qx : qy) - radius;
            }
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) {
                row[x] = c;
            } else {
                int cov = (int)((0.5f - sd) * 256.0f);
                row[x] = BlendRgb(row[x], c, cov);
            }
        }
    }
}

/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1
   from tools/generate_suite_icons.py (trispoke_sdf_alpha + header
   gradient). The mark is drawn straight at cache resolution, so the
   spinning emblem is razor sharp at any DPI with no bitmap upscaling. */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

static void EmbRasterize(uint32_t *dst, int n, COLORREF bg) {
    const double scale = EMB_SDF_SCALE, b = EMB_SDF_BOOST;
    const double pxSize = 2.0 / (double)n;
    const double aa = (pxSize * 1.35 > 1e-4) ? (pxSize * 1.35) : 1e-4;
    const double deg = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double nrm[3] = { 90.0, -150.0, -30.0 };
    int bgR = GetRValue(bg), bgG = GetGValue(bg), bgB = GetBValue(bg);
    for (int j = 0; j < n; j++) {
        double y0 = ((double)j + 0.5) / (double)n;
        double y = -1.0 + 2.0 * y0;
        for (int i = 0; i < n; i++) {
            double x0 = ((double)i + 0.5) / (double)n;
            double x = -1.0 + 2.0 * x0;
            double r = sqrt(x * x + y * y) / scale;
            double ang = atan2(y, x) / deg;
            double dOuter = fabs(r - 0.895) - (0.022 + b * 0.45);
            double dRad   = fabs(r - 0.742) - (0.056 + b * 0.5);
            double dGap = 180.0;
            for (int k = 0; k < 3; k++) {
                double m = fmod(ang - gaps[k] + 180.0, 360.0);
                if (m < 0) m += 360.0;
                double diff = fabs(m - 180.0);
                if (diff < dGap) dGap = diff;
            }
            double dCut  = (12.5 - dGap) * deg * 0.742;
            double dArcs = (dRad > dCut) ? dRad : dCut;
            double dSpokes = 10.0;
            double pMid = 0.052 + b * 0.12, pHw = 0.024 + b * 0.42;
            double sMid = 0.5 * (0.258 + 0.722), sHw = 0.5 * (0.722 - 0.258);
            for (int k = 0; k < 3; k++) {
                double rad = gaps[k] * deg;
                double ux = cos(rad), uy = sin(rad);
                double proj = (x * ux + y * uy) / scale;
                double perp = fabs(-x * uy + y * ux) / scale;
                double dAlong = fabs(proj - sMid) - sHw;
                double dBars  = fabs(perp - pMid) - pHw;
                double dThis  = (dAlong > dBars) ? dAlong : dBars;
                if (dThis < dSpokes) dSpokes = dThis;
            }
            double dHubOut = r - (0.224 + b * 0.25);
            double dTri = -10.0;
            for (int k = 0; k < 3; k++) {
                double rad = nrm[k] * deg;
                double dHalf = (x * cos(rad) + y * sin(rad)) / scale - 0.074;
                if (dHalf > dTri) dTri = dHalf;
            }
            double dHub = (dHubOut > -dTri) ? dHubOut : -dTri;
            double u1 = (dOuter < dArcs) ? dOuter : dArcs;
            double u2 = (dSpokes < dHub) ? dSpokes : dHub;
            double u  = (u1 < u2) ? u1 : u2;
            double alpha = 0.5 - (u * scale) / aa;
            if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
            double t = x0 * 0.3 + y0 * 0.7;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            double fr = 246.0 * (1 - t) + 196.0 * t;
            double fg = 249.0 * (1 - t) + 212.0 * t;
            double fb = 254.0 * (1 - t) + 234.0 * t;
            int rr = (int)(fr * alpha + bgR * (1 - alpha) + 0.5);
            int gg = (int)(fg * alpha + bgG * (1 - alpha) + 0.5);
            int bb = (int)(fb * alpha + bgB * (1 - alpha) + 0.5);
            dst[j * n + i] = RGB(rr, gg, bb);
        }
    }
}

/* Emblem cache: rasterized once at 4x by the SDF renderer; every paint
   only rotates it with bilinear sampling - smooth at any angle, and
   nothing at all is allocated per frame. */
typedef struct EmbCache {
    int      sz;        /* on-screen size */
    int      srcSz;     /* 4x supersample */
    uint32_t *src;      /* srcSz * srcSz colour rows, pre-blended on bg */
} EmbCache;
static EmbCache g_embCache = { 0, 0, NULL };

static void EmbCacheEnsure(int sz, COLORREF bg) {
    if (g_embCache.src && g_embCache.sz == sz) return;
    free(g_embCache.src);
    g_embCache.src = NULL;
    g_embCache.sz = 0;
    if (sz < 8 || sz > 512) return;
    int srcSz = sz * 4;
    uint32_t *src = (uint32_t *)malloc((size_t)srcSz * srcSz * sizeof(uint32_t));
    if (!src) return;
    EmbRasterize(src, srcSz, bg);
    g_embCache.sz    = sz;
    g_embCache.srcSz = srcSz;
    g_embCache.src   = src;
}

static void EmbSpinDraw(uint32_t *px, int W, int H, int x, int y, int sz, COLORREF bg) {
    EmbCacheEnsure(sz, bg);
    const uint32_t *src = g_embCache.src;
    if (!src) return;
    int srcSz = g_embCache.srcSz;
    /* One full turn every 30 seconds: slow enough to read as alive,
       slow enough to never pull the eye off the controls. */
    double ang = ((double)(GetTickCount64() % 30000ULL) / 30000.0) * 6.283185307179586;
    double ca = cos(ang), sa = sin(ang);
    double c = (sz - 1) * 0.5;
    double sc = (double)(srcSz - 1) / (double)(sz > 1 ? sz : 1);
    int maxs = srcSz - 1;
    for (int dy = 0; dy < sz; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= H) continue;
        double v = dy - c;
        for (int dx = 0; dx < sz; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= W) continue;
            double u = dx - c;
            double fx = (ca * u + sa * v + c) * sc;
            double fy = (-sa * u + ca * v + c) * sc;
            if (fx < 0) fx = 0; else if (fx > maxs) fx = maxs;
            if (fy < 0) fy = 0; else if (fy > maxs) fy = maxs;
            int x0 = (int)fx, y0 = (int)fy;
            double ax = fx - x0, ay = fy - y0;
            double w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay);
            double w01 = (1 - ax) * ay,       w11 = ax * ay;
            const uint32_t *p00 = &src[y0 * srcSz + x0];
            uint32_t c00 = p00[0];
            uint32_t c10 = (x0 < maxs) ? p00[1] : c00;
            const uint32_t *p01 = (y0 < maxs) ? p00 + srcSz : p00;
            uint32_t c01 = p01[0];
            uint32_t c11 = (x0 < maxs) ? p01[1] : c01;
            double r = (c00 & 255) * w00 + (c10 & 255) * w10 + (c01 & 255) * w01 + (c11 & 255) * w11;
            double g = ((c00 >> 8) & 255) * w00 + ((c10 >> 8) & 255) * w10 +
                       ((c01 >> 8) & 255) * w01 + ((c11 >> 8) & 255) * w11;
            double b = ((c00 >> 16) & 255) * w00 + ((c10 >> 16) & 255) * w10 +
                       ((c01 >> 16) & 255) * w01 + ((c11 >> 16) & 255) * w11;
            px[ty * W + tx] = RGB((int)(r + 0.5), (int)(g + 0.5), (int)(b + 0.5));
        }
    }
}

/* Kept as symbol reference for suite emblem tests - draws the embedded 32-bit Tri-Spoke Core Emblem */

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

static void DrawStatusTag(HDC hdc, uint32_t *px, int W, int H, HFONT font,
                          int rightX, int y, const wchar_t *text,
                          COLORREF fg, COLORREF bg, int dpi, int clipTop, int clipBottom) {
    SelectObject(hdc, font);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    int h = S(22, dpi);
    int w = sz.cx + S(28, dpi);
    RECT r = { rightX - w, y, rightX, y + h };
    SmoothRoundRect(px, W, H, &r, (float)h * 0.5f, bg, bg, clipTop, clipBottom);

    int dotD = S(6, dpi);
    RECT dotRc = {
        r.left + S(9, dpi),
        r.top + (h - dotD) / 2,
        r.left + S(9, dpi) + dotD,
        r.top + (h - dotD) / 2 + dotD
    };
    SmoothRoundRect(px, W, H, &dotRc, (float)dotD * 0.5f, fg, fg, clipTop, clipBottom);

    RECT tr = { r.left + S(19, dpi), y, r.right - S(8, dpi), y + h };
    SetTextColor(hdc, fg);
    DrawTextW(hdc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* style: 0=soft secondary, 1=primary cobalt, 2=coral disconnect, 3=primary connect */
static void DrawActionBtn(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                          const wchar_t *label, int style, BOOL hovered, int dpi,
                          int clipTop, int clipBottom) {
    COLORREF fill = hovered ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border = hovered ? COL_HOVER_RIM : COL_BTN_BORDER;
    COLORREF fg = COL_TEXT;

    if (style == 1 || style == 3) {
        fill = hovered ? COL_ACCENT_HOV : COL_ACCENT_STRONG;
        border = fill;
    } else if (style == 2) {
        fill = hovered ? RGB(74, 36, 44) : COL_DANGER_BG;
        border = COL_DANGER;
        fg = COL_DANGER;
    }

    SmoothRoundRect(px, W, H, &r, (float)S(7, dpi), fill, border, clipTop, clipBottom);
    SelectObject(hdc, font);
    SetTextColor(hdc, fg);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* Smooth rounded pill switch control with label + sliding circular knob */
static void DrawPillToggle(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                           const wchar_t *label, BOOL isOn, BOOL hovered, int dpi,
                           int clipTop, int clipBottom) {
    COLORREF cardFill = hovered ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border   = hovered ? COL_HOVER_RIM : COL_BTN_BORDER;
    SmoothRoundRect(px, W, H, &r, (float)S(7, dpi), cardFill, border, clipTop, clipBottom);

    int trackW = S(34, dpi), trackH = S(18, dpi);
    int trackX = r.right - S(10, dpi) - trackW;
    int trackY = r.top + (r.bottom - r.top - trackH) / 2;
    RECT trk = { trackX, trackY, trackX + trackW, trackY + trackH };
    COLORREF trkFill = isOn ? COL_ACCENT_STRONG : RGB(24, 28, 38);
    COLORREF trkBorder = isOn ? COL_ACCENT : COL_BORDER;
    SmoothRoundRect(px, W, H, &trk, (float)trackH * 0.5f, trkFill, trkBorder, clipTop, clipBottom);

    int knobD = trackH - S(6, dpi);
    int knobX = isOn ? (trk.right - S(3, dpi) - knobD) : (trk.left + S(3, dpi));
    int knobY = trk.top + (trackH - knobD) / 2;
    RECT knb = { knobX, knobY, knobX + knobD, knobY + knobD };
    COLORREF knbCol = isOn ? RGB(255, 255, 255) : COL_MUTED;
    SmoothRoundRect(px, W, H, &knb, (float)knobD * 0.5f, knbCol, knbCol, clipTop, clipBottom);

    SelectObject(hdc, font);
    SetTextColor(hdc, isOn ? COL_TEXT : COL_MUTED);
    RECT lbl = { r.left + S(12, dpi), r.top, trackX - S(8, dpi), r.bottom };
    DrawTextW(hdc, label, -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H; /* top-down 32bpp */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC mem = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(mem); return; }
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    HFONT fTitle = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fSub   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fCard  = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBody  = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    int M = S(28, dpi);
    int CW = W - M * 2;
    int headerH = S(76, dpi);
    int footerH = S(44, dpi);
    int footerTop = H - footerH;

    /* Calm low-glare slate canvas */
    FillCanvasRect(px, W, H, 0, 0, W, footerTop, COL_BG);
    FillCanvasRect(px, W, H, 0, footerTop, W, H, COL_FOOTER_BG);

    g_hitCount = 0;

    /* Copy device snapshot thread-safely */
    DeviceEntry snap[MAX_DEVICES];
    int count = 0, blockedCount = 0;
    BOOL radioOn = TRUE;
    EnterCriticalSection(&g_lock);
    count = g_devCount;
    radioOn = g_radioOn;
    if (count > 0) memcpy(snap, g_devices, sizeof(DeviceEntry) * (size_t)count);
    LeaveCriticalSection(&g_lock);
    int visCount = 0;
    for (int i = 0; i < count; i++) {
        if (!snap[i].visible) continue;
        visCount++;
        if (!snap[i].allowAuto) blockedCount++;
    }

    /* Header Suite Tri-Spoke Core Emblem + natural title + calm subtitle */
    int embSz = S(36, dpi);
    DrawWayfinderEmblem(px, W, H, M, (headerH - embSz) / 2, embSz);

    int topBtnH = S(34, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    RECT btnRefR   = { W - M - S(86, dpi),  topBtnY, W - M,                    topBtnY + topBtnH };
    RECT togStartR = { btnRefR.left - S(132, dpi), topBtnY, btnRefR.left - S(8, dpi), topBtnY + topBtnH };
    RECT togGuardR = { togStartR.left - S(122, dpi), topBtnY, togStartR.left - S(8, dpi), topBtnY + topBtnH };

    int idxGuard = g_hitCount;
    AddHit(togGuardR, HIT_TOGGLE_GUARD, 0);
    DrawPillToggle(mem, px, W, H, fBtn, togGuardR, L"Guard", g_guardOn, g_hoverIdx == idxGuard, dpi, 0, headerH);

    int idxStart = g_hitCount;
    AddHit(togStartR, HIT_TOGGLE_STARTUP, 0);
    DrawPillToggle(mem, px, W, H, fBtn, togStartR, L"Startup", g_autostart, g_hoverIdx == idxStart, dpi, 0, headerH);

    int idxRef = g_hitCount;
    AddHit(btnRefR, HIT_BTN_REFRESH, 0);
    DrawActionBtn(mem, px, W, H, fBtn, btnRefR, L"Refresh", 0, g_hoverIdx == idxRef, dpi, 0, headerH);

    int hdrLeft = M + embSz + S(14, dpi);
    SelectObject(mem, fTitle);
    SetTextColor(mem, COL_TEXT);
    RECT titleR = { hdrLeft, S(13, dpi), togGuardR.left - S(12, dpi), S(40, dpi) };
    DrawTextW(mem, L"Bluetooth Guard", -1, &titleR,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    wchar_t subHdr[96];
    if (!g_guardOn)
        wcsncpy(subHdr, L"Audio shield paused", 95);
    else if (!radioOn)
        wcsncpy(subHdr, L"Bluetooth radio is off", 95);
    else
        _snwprintf(subHdr, 95, L"Audio protection  \x00B7  %d protected (%d paired)", blockedCount, visCount);

    SelectObject(mem, fSub);
    SetTextColor(mem, COL_TELEMETRY);
    RECT subR = { hdrLeft, S(40, dpi), togGuardR.left - S(12, dpi), S(60, dpi) };
    DrawTextW(mem, subHdr, -1, &subR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillCanvasRect(px, W, H, M, headerH, W - M, headerH + 1, COL_DIVIDER);

    /* Scrollable device cards region */
    int listTop = headerH + S(14, dpi);
    int listBottom = footerTop;
    int cardH = S(82, dpi);
    int gap = S(11, dpi);
    g_contentH = visCount * (cardH + gap);
    int maxScroll = g_contentH - (listBottom - listTop);
    if (maxScroll < 0) maxScroll = 0;
    if (g_scrollY > maxScroll) g_scrollY = maxScroll;
    if (g_scrollY < 0) g_scrollY = 0;

    HRGN clip = CreateRectRgn(0, listTop, W, listBottom);
    SelectClipRgn(mem, clip);

    int cardIcoSz = S(42, dpi);
    HICON hCardIco = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP),
                                       IMAGE_ICON, cardIcoSz, cardIcoSz, LR_DEFAULTCOLOR);

    if (visCount == 0) {
        RECT emptyR = { M, listTop, M + CW, listTop + S(82, dpi) };
        SmoothRoundRect(px, W, H, &emptyR, (float)S(10, dpi), COL_CARD, COL_BORDER, listTop, listBottom);
        SelectObject(mem, fBody);
        SetTextColor(mem, COL_MUTED);
        RECT er = { emptyR.left + S(24, dpi), emptyR.top, emptyR.right - S(18, dpi), emptyR.bottom };
        DrawTextW(mem, L"No paired Bluetooth devices found yet. Pair headphones in Windows Settings, then click Refresh.",
                  -1, &er, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        int y = listTop - g_scrollY;
        for (int i = 0; i < count; i++) {
            DeviceEntry *d = &snap[i];
            /* hidden entries take no row space - the hit index below stays
               the raw snapshot index so clicks still land correctly */
            if (!d->visible) continue;
            RECT cr = { M, y, M + CW, y + cardH };
            if (cr.bottom >= listTop && cr.top <= listBottom) {
                SmoothRoundRect(px, W, H, &cr, (float)S(10, dpi), COL_CARD, COL_BORDER, listTop, listBottom);

                COLORREF barCol = COL_IDLE_SLATE, tagFg = COL_MUTED, tagBg = COL_PILL_MUTED;
                const wchar_t *tagTxt = L"Auto On";
                if (d->connected) {
                    barCol = COL_GOOD; tagFg = COL_GOOD; tagBg = COL_GOOD_BG;
                    tagTxt = !d->allowAuto ? L"Manual Session" : L"Connected";
                } else if (!d->allowAuto) {
                    barCol = COL_ACCENT; tagFg = COL_ACCENT; tagBg = COL_ACCENT_BG;
                    tagTxt = L"Phone Safe";
                }

                /* Soft rounded left accent capsule + Suite Bluetooth icon */
                RECT barRc = { cr.left + S(1, dpi), cr.top + S(16, dpi), cr.left + S(5, dpi), cr.bottom - S(16, dpi) };
                SmoothRoundRect(px, W, H, &barRc, (float)S(2, dpi), barCol, barCol, listTop, listBottom);
                if (hCardIco) {
                    DrawIconEx(mem, cr.left + S(18, dpi), (cr.top + cr.bottom - cardIcoSz) / 2,
                               hCardIco, cardIcoSz, cardIcoSz, 0, NULL, DI_NORMAL);
                }

                /* Top-right smooth status pill */
                DrawStatusTag(mem, px, W, H, fBadge, cr.right - S(16, dpi), cr.top + S(11, dpi),
                              tagTxt, tagFg, tagBg, dpi, listTop, listBottom);

                /* Clean natural-case device name */
                SelectObject(mem, fCard);
                SetTextColor(mem, COL_TEXT);
                RECT nr = { cr.left + S(72, dpi), cr.top + S(13, dpi), cr.right - S(176, dpi), cr.top + S(37, dpi) };
                DrawTextW(mem, d->name, -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                /* Right bottom controls: smooth pill switch [Auto-Connect] + rounded [Connect / Disconnect] */
                int btnBottomY = cr.bottom - S(12, dpi);
                int btnTopY    = btnBottomY - S(30, dpi);
                RECT connBtnR  = { cr.right - S(118, dpi), btnTopY, cr.right - S(16, dpi), btnBottomY };
                RECT autoTogR  = { connBtnR.left - S(156, dpi), btnTopY, connBtnR.left - S(8, dpi), btnBottomY };

                const wchar_t *subTxt = NULL;
                if (d->connected && !d->allowAuto)
                    subTxt = L"Connected manually  \x00B7  Re-locks after disconnect";
                else if (d->connected)
                    subTxt = L"Connected audio session";
                else if (!d->allowAuto)
                    subTxt = L"Auto-connect off  \x00B7  Click Connect anytime";
                else
                    subTxt = L"Auto-connect on  \x00B7  Connects automatically in range";

                SelectObject(mem, fBody);
                SetTextColor(mem, COL_MUTED);
                RECT sr = { cr.left + S(72, dpi), cr.top + S(39, dpi), autoTogR.left - S(14, dpi), cr.bottom - S(12, dpi) };
                DrawTextW(mem, subTxt, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                int hitAuto = g_hitCount;
                AddHit(autoTogR, HIT_DEV_AUTO_TOGGLE, i);
                DrawPillToggle(mem, px, W, H, fBtn, autoTogR, L"Auto-Connect",
                               d->allowAuto, g_hoverIdx == hitAuto, dpi, listTop, listBottom);

                int hitConn = g_hitCount;
                AddHit(connBtnR, HIT_DEV_CONNECT_BTN, i);
                DrawActionBtn(mem, px, W, H, fBtn, connBtnR,
                              d->connected ? L"Disconnect" : L"Connect",
                              d->connected ? 2 : 3,
                              g_hoverIdx == hitConn, dpi, listTop, listBottom);
            }
            y += cardH + gap;
        }
    }

    if (hCardIco) DestroyIcon(hCardIco);
    SelectClipRgn(mem, NULL);
    DeleteObject(clip);

    /* Calm rounded footer bar */
    FillCanvasRect(px, W, H, 0, footerTop, W, footerTop + 1, COL_DIVIDER);

    COLORREF footDot = g_guardOn ? COL_GOOD : COL_WARN;
    int fdSz = S(8, dpi);
    RECT fdRc = { M, footerTop + (footerH - fdSz) / 2, M + fdSz, footerTop + (footerH - fdSz) / 2 + fdSz };
    SmoothRoundRect(px, W, H, &fdRc, (float)fdSz * 0.5f, footDot, footDot, footerTop, H);

    SelectObject(mem, fSub);
    SetTextColor(mem, COL_MUTED);
    RECT footTxtR = { M + S(16, dpi), footerTop, W - M - S(112, dpi), H };
    DrawTextW(mem, L"Running in system tray  \x00B7  Manual connect works here & in Windows Quick Settings",
              -1, &footTxtR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    RECT quitBtnR = { W - M - S(96, dpi), footerTop + S(7, dpi), W - M, H - S(8, dpi) };
    int idxQuit = g_hitCount;
    AddHit(quitBtnR, HIT_BTN_QUIT, 0);
    DrawActionBtn(mem, px, W, H, fBtn, quitBtnR, L"Quit", 0, g_hoverIdx == idxQuit, dpi, footerTop, H);

    BitBlt(targetDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

    DeleteObject(fTitle);
    DeleteObject(fSub);
    DeleteObject(fCard);
    DeleteObject(fBtn);
    DeleteObject(fBadge);
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
        SaveRules();
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
        SetTimer(hwnd, 3, 50, NULL);    /* emblem spin repaint */
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
        if (wp == 3) { InvalidateRect(hwnd, NULL, FALSE); return 0; }
        CheckForegroundManualIntent();
        if (g_showEvent && WaitForSingleObject(g_showEvent, 0) == WAIT_OBJECT_0) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        }
        WriteHeartbeat();
        return 0;

    case WM_DEVICECHANGE:
        /* Kernel radio / HCI connection state changed — wake high-priority worker in <1ms! */
        ArmFastBurstWindow(5000ULL);
        ReleaseSemaphore(g_qSem, 1, NULL);
        return TRUE;

    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
        /* Clear active sessions & disconnect guarded earbuds before Restart / Fast Startup hibernate */
        g_manualGraceUntil = 0;
        EnterCriticalSection(&g_lock);
        for (int i = 0; i < g_devCount; i++) {
            g_devices[i].sessionAllowed = FALSE;
            g_devices[i].seenConnected = FALSE;
        }
        LeaveCriticalSection(&g_lock);
        DisconnectGuardedDevicesNow();
        return TRUE;

    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) {
            g_manualGraceUntil = 0;
            EnterCriticalSection(&g_lock);
            for (int i = 0; i < g_devCount; i++) {
                g_devices[i].sessionAllowed = FALSE;
                g_devices[i].seenConnected = FALSE;
            }
            LeaveCriticalSection(&g_lock);
            DisconnectGuardedDevicesNow();
        } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            g_manualGraceUntil = 0;
            ArmFastBurstWindow(30000ULL);
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
        UnregisterBluetoothNotifications();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        DeleteFileW(g_lockPath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;
    BoostProcessAndShutdownPriority();
    InitPaths();

    BOOL silent = (cmdLine && (wcsstr(cmdLine, L"--silent") != NULL ||
                               wcsstr(cmdLine, L"--startup") != NULL));

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
        if (!silent) {
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT_NAME);
    InitializeCriticalSection(&g_lock);
    g_qSem = CreateSemaphoreW(NULL, 1, 64, NULL);

    InitBluetoothApis();
    BOOL regAuto = IsAutostartEnabled();
    LoadRules();
    if (!g_autostartExplicit) g_autostart = regAuto ? TRUE : TRUE;

    /* Immediate millisecond-0 Bluetooth sweep BEFORE creating window/fonts/icons! */
    ArmFastBurstWindow(60000ULL);
    PollPairedDevices();
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
    RegisterBluetoothNotifications(g_hwnd);

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_appIcon;
    wcsncpy(g_nid.szTip, L"Bluetooth Guard (High-Priority Active)", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

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
