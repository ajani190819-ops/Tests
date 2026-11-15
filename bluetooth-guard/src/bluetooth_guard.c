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
    HIT_DEV_CONNECT_BTN,
    HIT_DEV_FORGET_BTN
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
typedef DWORD                  (WINAPI *PFN_BluetoothRemoveDevice)(ULONGLONG *);
typedef UINT                   (WINAPI *PFN_GetDpiForWindow)(HWND);
typedef HRESULT                (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static PFN_BtFindFirstRadio   pBtFindFirstRadio   = NULL;
static PFN_BtFindNextRadio    pBtFindNextRadio    = NULL;
static PFN_BtFindRadioClose   pBtFindRadioClose   = NULL;
static PFN_BtFindFirstDevice  pBtFindFirstDevice  = NULL;
static PFN_BtFindNextDevice   pBtFindNextDevice   = NULL;
static PFN_BtFindDeviceClose  pBtFindDeviceClose  = NULL;
static PFN_BtSetServiceState  pBtSetServiceState  = NULL;
static PFN_BluetoothRemoveDevice pBluetoothRemoveDevice = NULL;

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


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so bluetooth_guard.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/system_init.c"
#include "parts/device_history.c"
#include "parts/device_classify.c"
#include "parts/bt_actions.c"
#include "parts/autostart_rules.c"
#include "parts/ui_renderer.c"
#include "parts/emblem.c"
#include "parts/ui_controls.c"
#include "parts/window_entry.c"
