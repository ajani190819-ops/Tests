#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

/* ============================================================================
 *  Audio Switcher - Native Win32 Suite App
 *
 *  Per-app volume control and audio output switching for the Soft Slate suite.
 *  Uses the Windows Core Audio API (COM) to enumerate audio endpoints, list
 *  active audio sessions, and adjust per-app master volume and mute state.
 *
 *  Matches the unified Soft Slate Suite UI shared with ProjectHub.exe,
 *  BluetoothGuard.exe and KeyboardLighting.exe.
 * ============================================================================ */

/* --- Soft Slate palette (docs/UI_STYLE_GUIDE.md) --- */
#define COL_BG              RGB(24, 27, 36)    /* #181B24 canvas */
#define COL_FOOTER_BG       RGB(20, 23, 31)    /* #14171F */
#define COL_DIVIDER         RGB(48, 55, 72)    /* #303748 */
#define COL_CARD            RGB(34, 39, 52)    /* #222734 card */
#define COL_BORDER          RGB(56, 64, 84)    /* #384054 */
#define COL_SEC_BTN         RGB(43, 49, 66)    /* #2B3142 */
#define COL_SEC_HOV         RGB(54, 62, 84)    /* #363E54 */
#define COL_BTN_BORDER      RGB(64, 74, 96)    /* #404A60 */
#define COL_HOVER_RIM       RGB(96, 112, 144)  /* #607090 */
#define COL_TEXT            RGB(236, 239, 246) /* #ECEFF6 */
#define COL_MUTED           RGB(154, 163, 184) /* #9AA3B8 */
#define COL_TELEMETRY       RGB(116, 126, 148) /* #747E94 */
#define COL_GOOD            RGB(52, 211, 153)  /* #34D399 */
#define COL_WARN            RGB(251, 191, 36)  /* #FBBF24 */
#define COL_ACCENT          RGB(56, 189, 248)  /* #38BDF8 */
#define COL_ACCENT_STRONG   RGB(37, 99, 235)   /* #2563EB */
#define COL_DANGER          RGB(248, 113, 113) /* #F87171 */

#define APP_TITLE           L"Audio Switcher"
#define SINGLE_MUTEX_NAME   L"Local\\Tests_AudioSwitcher_SingleInstance_v1"
#define SHOW_EVENT_NAME     L"Local\\Tests_AudioSwitcher_ShowWindow_v1"
#define REPO_OWNER_W        L"ajani190819-ops"
#define REPO_NAME_W         L"Tests"
#define FALLBACK_BRANCH_W   L"arena/e895b94f-tests"

#define RES_ICON_APP        1
#define RES_ICON_EMBLEM     2

#define WM_TRAYICON         (WM_APP + 1)

#define IDM_TRAY_OPEN       200
#define IDM_TRAY_QUIT       201
#define IDM_REFRESH         300

/* Tri-Spoke Core Emblem - same SDF renderer as Project Hub */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

/* --- COM / Core Audio GUIDs --- */
typedef const GUID *LPCGEVENTCONTEXT;

static const CLSID CLSID_MMDeviceEnumerator =
    { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const IID   IID_IMMDeviceEnumerator =
    { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const IID   IID_IAudioEndpointVolume =
    { 0x5CDF2C82, 0x841E, 0x4546, { 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A } };
static const IID   IID_IAudioSessionManager2 =
    { 0x77AA99A0, 0x1BD6, 0x484F, { 0x8B, 0xC7, 0x2C, 0x65, 0x4C, 0x9A, 0x9B, 0x6F } };
static const IID   IID_IAudioSessionControl2 =
    { 0xBFB7FF88, 0x7239, 0x4FC9, { 0x8F, 0xA2, 0x07, 0xC9, 0x50, 0xBE, 0x9C, 0x6D } };
static const IID   IID_ISimpleAudioVolume =
    { 0x87CE5498, 0x68D6, 0x44E5, { 0x92, 0x15, 0x6D, 0x47, 0x40, 0x82, 0x9F, 0x43 } };

enum EDataFlow { eRender = 0, eCapture = 1, eAll = 2 };
enum ERole   { eConsole = 0, eMultimedia = 1, eCommunications = 2 };

/* --- Custom COM interfaces (SDK defines IUnknown, IUnknownVtbl already) --- */

typedef struct IMMDeviceEnumeratorVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *EnumAudioEndpoints)(void* This, enum EDataFlow dataFlow, DWORD dwStateMask, void** ppEnum);
    HRESULT (__stdcall *GetDefaultAudioEndpoint)(void* This, enum EDataFlow dataFlow, enum ERole role, void** ppDevice);
} IMMDeviceEnumeratorVtbl;
typedef struct IMMDeviceEnumerator { const struct IMMDeviceEnumeratorVtbl* lpVtbl; } IMMDeviceEnumerator;

/* IMMDevice -- the vtable MUST list every method in Windows' real order.
 * COM dispatches by slot position, not by name, so declaring only the methods
 * we call silently shifts them onto the wrong functions. Activate is slot 3
 * here; an earlier version put GetId at 3 and Activate at 4, so every Activate
 * call actually invoked OpenPropertyStore and every GetId call invoked
 * Activate. That made device ids, friendly names, endpoint volumes and the
 * whole per-app session list fail at once. */
typedef struct IMMDeviceVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *Activate)(void* This, const IID* iid, DWORD dwClsCtx, void* pActivationParams, void** ppInterface);
    HRESULT (__stdcall *OpenPropertyStore)(void* This, DWORD stgmAccess, void** ppProperties);
    HRESULT (__stdcall *GetId)(void* This, LPWSTR* ppstrId);
    HRESULT (__stdcall *GetState)(void* This, DWORD* pdwState);
} IMMDeviceVtbl;
typedef struct IMMDevice { const struct IMMDeviceVtbl* lpVtbl; } IMMDevice;

typedef struct IMMDeviceCollectionVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetCount)(void* This, UINT* pcDevices);
    HRESULT (__stdcall *Item)(void* This, UINT nDevice, void** ppDevice);
} IMMDeviceCollectionVtbl;
typedef struct IMMDeviceCollection { const struct IMMDeviceCollectionVtbl* lpVtbl; } IMMDeviceCollection;

/* IAudioEndpointVolume -- full vtable in real slot order. Only
 * SetMasterVolumeLevelScalar (7), GetMasterVolumeLevelScalar (9), SetMute (14)
 * and GetMute (15) are called; the rest are declared so those four land on the
 * right slots. An earlier version declared SetMasterVolumeLevelScalar at slot
 * 3, which is really RegisterControlChangeNotify -- dragging a master volume
 * slider would have passed a float where a callback pointer is expected. */
typedef struct IAudioEndpointVolumeVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *RegisterControlChangeNotify)(void* This, void* pNotify);
    HRESULT (__stdcall *UnregisterControlChangeNotify)(void* This, void* pNotify);
    HRESULT (__stdcall *GetChannelCount)(void* This, UINT* pnChannelCount);
    HRESULT (__stdcall *SetMasterVolumeLevel)(void* This, float fLevelDB, const GUID* pguidEventContext);
    HRESULT (__stdcall *SetMasterVolumeLevelScalar)(void* This, float fLevel, const GUID* pguidEventContext);
    HRESULT (__stdcall *GetMasterVolumeLevel)(void* This, float* pfLevelDB);
    HRESULT (__stdcall *GetMasterVolumeLevelScalar)(void* This, float* pfLevel);
    HRESULT (__stdcall *SetChannelVolumeLevel)(void* This, UINT nChannel, float fLevelDB, const GUID* pguidEventContext);
    HRESULT (__stdcall *SetChannelVolumeLevelScalar)(void* This, UINT nChannel, float fLevel, const GUID* pguidEventContext);
    HRESULT (__stdcall *GetChannelVolumeLevel)(void* This, UINT nChannel, float* pfLevelDB);
    HRESULT (__stdcall *GetChannelVolumeLevelScalar)(void* This, UINT nChannel, float* pfLevel);
    HRESULT (__stdcall *SetMute)(void* This, BOOL bMute, const GUID* pguidEventContext);
    HRESULT (__stdcall *GetMute)(void* This, BOOL* pbMute);
} IAudioEndpointVolumeVtbl;
typedef struct IAudioEndpointVolume { const struct IAudioEndpointVolumeVtbl* lpVtbl; } IAudioEndpointVolume;

/* IAudioSessionManager2 inherits IAudioSessionManager, whose two methods
 * occupy slots 3 and 4; GetSessionEnumerator is therefore slot 5, not 3. */
typedef struct IAudioSessionManager2Vtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetAudioSessionControl)(void* This, const GUID* AudioSessionGuid, DWORD StreamFlags, void** SessionControl);
    HRESULT (__stdcall *GetSimpleAudioVolume)(void* This, const GUID* AudioSessionGuid, DWORD StreamFlags, void** AudioVolume);
    HRESULT (__stdcall *GetSessionEnumerator)(void* This, void** SessionEnumerator);
    HRESULT (__stdcall *RegisterSessionNotification)(void* This, void* SessionNotification);
    HRESULT (__stdcall *UnregisterSessionNotification)(void* This, void* SessionNotification);
    HRESULT (__stdcall *RegisterDuckNotification)(void* This, void* DuckNotification);
    HRESULT (__stdcall *UnregisterDuckNotification)(void* This, void* DuckNotification);
} IAudioSessionManager2Vtbl;
typedef struct IAudioSessionManager2 { const struct IAudioSessionManager2Vtbl* lpVtbl; } IAudioSessionManager2;

typedef struct IAudioSessionEnumeratorVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetCount)(void* This, int* SessionCount);
    HRESULT (__stdcall *GetSession)(void* This, int SessionIndex, void** Session);
} IAudioSessionEnumeratorVtbl;
typedef struct IAudioSessionEnumerator { const struct IAudioSessionEnumeratorVtbl* lpVtbl; } IAudioSessionEnumerator;

/* IAudioSessionControl -- the base interface. Declared so its slots are
 * accounted for, and because GetDisplayName (slot 4) is the useful fallback
 * for labelling a session whose process we cannot open. */
typedef struct IAudioSessionControlVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetState)(void* This, int* pRetVal);
    HRESULT (__stdcall *GetDisplayName)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *SetDisplayName)(void* This, LPCWSTR Value, const GUID* EventContext);
    HRESULT (__stdcall *GetIconPath)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *SetIconPath)(void* This, LPCWSTR Value, const GUID* EventContext);
    HRESULT (__stdcall *GetGroupingParam)(void* This, GUID* pRetVal);
    HRESULT (__stdcall *SetGroupingParam)(void* This, const GUID* Override, const GUID* EventContext);
    HRESULT (__stdcall *RegisterAudioSessionNotification)(void* This, void* NewNotifications);
    HRESULT (__stdcall *UnregisterAudioSessionNotification)(void* This, void* NewNotifications);
} IAudioSessionControlVtbl;
typedef struct IAudioSessionControl { const struct IAudioSessionControlVtbl* lpVtbl; } IAudioSessionControl;

/* IAudioSessionControl2 inherits all of IAudioSessionControl (slots 3-11), so
 * GetProcessId is slot 14. Declaring it at slot 3 put it on GetState: the value
 * read back was the session state (0 inactive, 1 active, 2 expired), which the
 * caller treated as a PID. OpenProcess then failed, every app row came up
 * blank, and inactive sessions were skipped by the `if (pid == 0)` guard. */
typedef struct IAudioSessionControl2Vtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetState)(void* This, int* pRetVal);
    HRESULT (__stdcall *GetDisplayName)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *SetDisplayName)(void* This, LPCWSTR Value, const GUID* EventContext);
    HRESULT (__stdcall *GetIconPath)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *SetIconPath)(void* This, LPCWSTR Value, const GUID* EventContext);
    HRESULT (__stdcall *GetGroupingParam)(void* This, GUID* pRetVal);
    HRESULT (__stdcall *SetGroupingParam)(void* This, const GUID* Override, const GUID* EventContext);
    HRESULT (__stdcall *RegisterAudioSessionNotification)(void* This, void* NewNotifications);
    HRESULT (__stdcall *UnregisterAudioSessionNotification)(void* This, void* NewNotifications);
    HRESULT (__stdcall *GetSessionIdentifier)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *GetSessionInstanceIdentifier)(void* This, LPWSTR* pRetVal);
    HRESULT (__stdcall *GetProcessId)(void* This, DWORD* pRetVal);
    HRESULT (__stdcall *IsSystemSoundsSession)(void* This);
    HRESULT (__stdcall *SetDuckingPreference)(void* This, BOOL optOut);
} IAudioSessionControl2Vtbl;
typedef struct IAudioSessionControl2 { const struct IAudioSessionControl2Vtbl* lpVtbl; } IAudioSessionControl2;

typedef struct ISimpleAudioVolumeVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *SetMasterVolume)(void* This, float fLevel, const GUID* EventContext);
    HRESULT (__stdcall *GetMasterVolume)(void* This, float* pfLevel);
    HRESULT (__stdcall *SetMute)(void* This, BOOL bMute, const GUID* EventContext);
    HRESULT (__stdcall *GetMute)(void* This, BOOL* pbMute);
} ISimpleAudioVolumeVtbl;
typedef struct ISimpleAudioVolume { const struct ISimpleAudioVolumeVtbl* lpVtbl; } ISimpleAudioVolume;

/* --- IPolicyConfig: the only way to set the default audio endpoint ---
 *
 * There is NO documented public API for this. IMMDeviceEnumerator can read the
 * default endpoint but not set it, which is why the old "Set Default" button
 * only repainted its own label. Every audio switcher on Windows goes through
 * this undocumented-but-stable COM interface, present since Vista and still
 * shipping in Windows 11.
 *
 * Only SetDefaultEndpoint (slot 13) is called. The surrounding methods are
 * declared with placeholder signatures so that slot 13 lands correctly; they
 * must never be invoked through this struct.
 *
 * The interface IID differs between Windows versions, so policy_activate()
 * tries each known one in turn and --diag reports which succeeded. */
static const CLSID CLSID_PolicyConfig =
    { 0x870AF99C, 0x171D, 0x4F9E, { 0xAF, 0x0D, 0xE6, 0x3D, 0xF4, 0x0C, 0x2B, 0xC9 } };
static const IID   IID_IPolicyConfig =
    { 0xF8679F50, 0x850A, 0x41CF, { 0x9C, 0x72, 0x43, 0x0F, 0x29, 0x02, 0x90, 0xC8 } };
static const IID   IID_IPolicyConfigVista =
    { 0x568B9108, 0x44BF, 0x40B4, { 0x90, 0x06, 0x86, 0xAF, 0xE5, 0xB5, 0xA6, 0x20 } };

typedef struct IPolicyConfigVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetMixFormat)(void* This, void* a, void* b);
    HRESULT (__stdcall *GetDeviceFormat)(void* This, void* a, void* b, void* c);
    HRESULT (__stdcall *ResetDeviceFormat)(void* This, void* a);
    HRESULT (__stdcall *SetDeviceFormat)(void* This, void* a, void* b, void* c);
    HRESULT (__stdcall *GetProcessingPeriod)(void* This, void* a, void* b, void* c, void* d);
    HRESULT (__stdcall *SetProcessingPeriod)(void* This, void* a, void* b);
    HRESULT (__stdcall *GetShareMode)(void* This, void* a, void* b);
    HRESULT (__stdcall *SetShareMode)(void* This, void* a, void* b);
    HRESULT (__stdcall *GetPropertyValue)(void* This, void* a, void* b, void* c);
    HRESULT (__stdcall *SetPropertyValue)(void* This, void* a, void* b, void* c);
    HRESULT (__stdcall *SetDefaultEndpoint)(void* This, LPCWSTR pszDeviceName, enum ERole role);
    HRESULT (__stdcall *SetEndpointVisibility)(void* This, void* a, void* b);
} IPolicyConfigVtbl;
typedef struct IPolicyConfig { const struct IPolicyConfigVtbl* lpVtbl; } IPolicyConfig;

typedef struct ComBase { const void* lpVtbl; } ComBase;

/* ============================================================================
 *  Compile-time vtable slot assertions
 *
 *  COM dispatches by slot position, so a hand-written vtable that drops or
 *  reorders a method silently calls a different function with the wrong
 *  arguments -- it compiles, links, and then corrupts state or fails quietly.
 *  That is exactly how this app shipped with four broken interfaces. These
 *  assertions turn the whole class of bug into a build error.
 * ============================================================================ */
#define VTBL_SLOT(VtblType, field) \
    ((int)(offsetof(VtblType, field) / sizeof(void*)))

_Static_assert(VTBL_SLOT(IMMDeviceEnumeratorVtbl, EnumAudioEndpoints) == 3,      "IMMDeviceEnumerator::EnumAudioEndpoints must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceEnumeratorVtbl, GetDefaultAudioEndpoint) == 4, "IMMDeviceEnumerator::GetDefaultAudioEndpoint must be slot 4");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, Activate) == 3,                          "IMMDevice::Activate must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, OpenPropertyStore) == 4,                 "IMMDevice::OpenPropertyStore must be slot 4");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, GetId) == 5,                             "IMMDevice::GetId must be slot 5");
_Static_assert(VTBL_SLOT(IMMDeviceVtbl, GetState) == 6,                          "IMMDevice::GetState must be slot 6");
_Static_assert(VTBL_SLOT(IMMDeviceCollectionVtbl, GetCount) == 3,                "IMMDeviceCollection::GetCount must be slot 3");
_Static_assert(VTBL_SLOT(IMMDeviceCollectionVtbl, Item) == 4,                    "IMMDeviceCollection::Item must be slot 4");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, SetMasterVolumeLevelScalar) == 7,  "IAudioEndpointVolume::SetMasterVolumeLevelScalar must be slot 7");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, GetMasterVolumeLevelScalar) == 9,  "IAudioEndpointVolume::GetMasterVolumeLevelScalar must be slot 9");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, SetMute) == 14,               "IAudioEndpointVolume::SetMute must be slot 14");
_Static_assert(VTBL_SLOT(IAudioEndpointVolumeVtbl, GetMute) == 15,               "IAudioEndpointVolume::GetMute must be slot 15");
_Static_assert(VTBL_SLOT(IAudioSessionManager2Vtbl, GetSessionEnumerator) == 5,  "IAudioSessionManager2::GetSessionEnumerator must be slot 5");
_Static_assert(VTBL_SLOT(IAudioSessionEnumeratorVtbl, GetCount) == 3,            "IAudioSessionEnumerator::GetCount must be slot 3");
_Static_assert(VTBL_SLOT(IAudioSessionEnumeratorVtbl, GetSession) == 4,          "IAudioSessionEnumerator::GetSession must be slot 4");
_Static_assert(VTBL_SLOT(IAudioSessionControlVtbl, GetState) == 3,               "IAudioSessionControl::GetState must be slot 3");
_Static_assert(VTBL_SLOT(IAudioSessionControlVtbl, GetDisplayName) == 4,         "IAudioSessionControl::GetDisplayName must be slot 4");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, GetSessionIdentifier) == 12, "IAudioSessionControl2::GetSessionIdentifier must be slot 12");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, GetProcessId) == 14,         "IAudioSessionControl2::GetProcessId must be slot 14");
_Static_assert(VTBL_SLOT(IAudioSessionControl2Vtbl, IsSystemSoundsSession) == 15,"IAudioSessionControl2::IsSystemSoundsSession must be slot 15");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, SetMasterVolume) == 3,          "ISimpleAudioVolume::SetMasterVolume must be slot 3");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, GetMasterVolume) == 4,          "ISimpleAudioVolume::GetMasterVolume must be slot 4");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, SetMute) == 5,                  "ISimpleAudioVolume::SetMute must be slot 5");
_Static_assert(VTBL_SLOT(ISimpleAudioVolumeVtbl, GetMute) == 6,                  "ISimpleAudioVolume::GetMute must be slot 6");
_Static_assert(VTBL_SLOT(IPolicyConfigVtbl, SetDefaultEndpoint) == 13,           "IPolicyConfig::SetDefaultEndpoint must be slot 13");

typedef void (__stdcall *ReleaseFn)(void*);

/* --- Helper macro for COM calls --- *//* --- Helper macro for COM calls --- */
#define CALL_VTBL0(iface, idx) ((iface)->lpVtbl->idx(iface))
#define CALL_VTBL1(iface, idx, a1) ((iface)->lpVtbl->idx(iface, a1))
#define CALL_VTBL2(iface, idx, a1, a2) ((iface)->lpVtbl->idx(iface, a1, a2))
#define CALL_VTBL3(iface, idx, a1, a2, a3) ((iface)->lpVtbl->idx(iface, a1, a2, a3))
#define CALL_VTBL4(iface, idx, a1, a2, a3, a4) ((iface)->lpVtbl->idx(iface, a1, a2, a3, a4))
#define CALL_VTBL5(iface, idx, a1, a2, a3, a4, a5) ((iface)->lpVtbl->idx(iface, a1, a2, a3, a4, a5))

/* ============================================================================
 *  Audio subsystem
 * ============================================================================ */

typedef struct {
    wchar_t              name[256];
    wchar_t              id[512];
    BOOL                 is_default;
    IMMDevice            *device;
    IAudioEndpointVolume *vol;
} AudioEndpoint;

typedef struct {
    wchar_t      name[256];        /* label shown in the UI */
    wchar_t      key[256];         /* lowercase exe basename: the persistence key */
    wchar_t      exe_path[MAX_PATH]; /* full image path, surfaced by --diag */
    DWORD        process_id;
    float        volume;           /* 0.0 .. 1.0 */
    BOOL         mute;
    BOOL         named_from_session; /* TRUE when the label came from GetDisplayName */
    ISimpleAudioVolume *svol;
} AudioSession;

static AudioEndpoint   g_endpoints[32];
static int             g_endpoint_count = 0;
static int             g_default_endpoint_idx = -1;
static AudioSession    g_sessions[128];
static int             g_session_count = 0;

/* PIDs we have already restored a remembered level for. Without this, the
 * 2-second poll would fight the user: they drag a slider, the next tick snaps
 * it back to the stored value. A remembered level is applied once per process
 * lifetime, when that process is first seen. */
#define MAX_APPLIED_PIDS 512
static DWORD           g_applied_pids[MAX_APPLIED_PIDS];
static int             g_applied_pid_count = 0;

static IMMDeviceEnumerator *g_enum = NULL;
static IMMDevice           *g_default_device = NULL;
static IAudioSessionManager2 *g_session_mgr = NULL;
static IPolicyConfig       *g_policy = NULL;

/* Which IPolicyConfig IID CoCreateInstance accepted: 0 = none (switching
 * unavailable), 1 = the Win7..Win11 IID, 2 = the Vista IID. Reported by --diag
 * and by the status line, because a failed switch must never look like a
 * successful one. */
static int                  g_policy_variant = 0;

/* The HRESULT of the most recent attempt to obtain IPolicyConfig. The status
 * line must show this: claiming "could not be created" without ever having
 * tried is exactly the kind of false alarm that makes a tool untrustworthy. */
static HRESULT              g_policy_hr = S_OK;

/* Outcome of the last session enumeration, so an empty APP VOLUME list can say
 * whether that is "nothing is playing" or "this is broken, here is the code". */
static HRESULT              g_sessions_hr = S_OK;

/* Last switching outcome, surfaced in the UI status line. */
static HRESULT              g_last_switch_hr = S_OK;
static int                  g_last_switch_idx = -1;

static HRESULT com_create_enumerator(void) {
    HRESULT hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        &IID_IMMDeviceEnumerator, (void**)&g_enum);
    return hr;
}

static void com_release_enumerator(void) {
    if (g_enum) { CALL_VTBL0(g_enum, Release); g_enum = NULL; }
    if (g_policy) { CALL_VTBL0(g_policy, Release); g_policy = NULL; }
    g_policy_variant = 0;
}

/* Obtain IPolicyConfig. The IID is not stable across Windows versions, so try
 * each known one and remember which worked. Returns S_OK only if we actually
 * hold a usable interface -- callers must check, never assume. */
static HRESULT policy_activate(void) {
    if (g_policy) return S_OK;

    HRESULT last = E_NOINTERFACE;
    const IID *candidates[2] = { &IID_IPolicyConfig, &IID_IPolicyConfigVista };
    for (int i = 0; i < 2; i++) {
        void *p = NULL;
        HRESULT hr = CoCreateInstance(&CLSID_PolicyConfig, NULL, CLSCTX_ALL,
                                      candidates[i], &p);
        if (SUCCEEDED(hr) && p) {
            g_policy = (IPolicyConfig*)p;
            g_policy_variant = i + 1;
            g_policy_hr = S_OK;
            return S_OK;
        }
        last = hr;
    }
    g_policy = NULL;
    g_policy_variant = 0;
    g_policy_hr = last;      /* the real reason, for the status line and --diag */
    return last;
}

/* Make endpoint `idx` the system default for ALL THREE roles.
 *
 * Setting only eConsole leaves apps pinned to Multimedia or Communications on
 * the old device, which reads to the user as "the switch half-worked". The
 * returned HRESULT is the first failure, or S_OK if all three were set;
 * g_last_switch_hr records it so the UI can report honestly instead of
 * repainting a "Default" badge over a device Windows never switched to. */
static HRESULT endpoint_make_default(int idx) {
    g_last_switch_idx = idx;
    g_last_switch_hr = S_OK;

    if (idx < 0 || idx >= g_endpoint_count) {
        g_last_switch_hr = E_INVALIDARG;
        return g_last_switch_hr;
    }
    if (!g_endpoints[idx].id[0]) {
        /* No device id means SetDefaultEndpoint has nothing to name. This is
         * what the broken GetId slot used to produce for every device. */
        g_last_switch_hr = E_FAIL;
        return g_last_switch_hr;
    }

    HRESULT hr = policy_activate();
    if (FAILED(hr)) {
        g_last_switch_hr = hr;
        return hr;
    }

    static const enum ERole roles[3] = { eConsole, eMultimedia, eCommunications };
    HRESULT first_fail = S_OK;
    for (int r = 0; r < 3; r++) {
        HRESULT h = CALL_VTBL2(g_policy, SetDefaultEndpoint,
                               g_endpoints[idx].id, roles[r]);
        if (FAILED(h) && SUCCEEDED(first_fail)) first_fail = h;
    }
    g_last_switch_hr = first_fail;
    return first_fail;
}

static HRESULT endpoints_refresh(void) {
    IMMDeviceEnumerator *e = g_enum;
    if (!e) return E_FAIL;

    IMMDeviceCollection *coll = NULL;
    HRESULT hr = CALL_VTBL3(e, EnumAudioEndpoints, eRender, 0x1, (void**)&coll);
    if (FAILED(hr)) return hr;

    IMMDeviceCollectionVtbl *cv = (IMMDeviceCollectionVtbl*)coll->lpVtbl;
    UINT count = 0;
    cv->GetCount(coll, &count);

    /* Release the previous refresh's COM references before overwriting them.
     * Without this, every refresh leaked one IMMDevice and one
     * IAudioEndpointVolume per endpoint, plus the default-device reference. */
    for (int i = 0; i < g_endpoint_count; i++) {
        if (g_endpoints[i].vol)    { CALL_VTBL0(g_endpoints[i].vol, Release);    g_endpoints[i].vol = NULL; }
        if (g_endpoints[i].device) { CALL_VTBL0(g_endpoints[i].device, Release); g_endpoints[i].device = NULL; }
    }
    if (g_default_device) { CALL_VTBL0(g_default_device, Release); g_default_device = NULL; }

    g_endpoint_count = 0;
    g_default_endpoint_idx = -1;

    /* PKEY_Device_FriendlyName. A PROPERTYKEY is a GUID *plus* a property id;
     * passing only the GUID made GetValue read `pid` from whatever followed it
     * on the stack, so the friendly-name lookup could never succeed. */
    PROPERTYKEY pkeyName = {
        { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } },
        14
    };

    for (UINT i = 0; i < count && g_endpoint_count < 32; i++) {
        IMMDevice *dev = NULL;
        cv->Item(coll, i, (void**)&dev);
        if (!dev) continue;

        LPWSTR dev_id = NULL;
        hr = CALL_VTBL1(dev, GetId, &dev_id);
        if (SUCCEEDED(hr) && dev_id) {
            wcsncpy(g_endpoints[g_endpoint_count].id, dev_id, 511);
            g_endpoints[g_endpoint_count].id[511] = L'\0';
            CoTaskMemFree(dev_id);
        } else {
            g_endpoints[g_endpoint_count].id[0] = L'\0';
        }

        /* Friendly name via IPropertyStore. This is OpenPropertyStore (slot 4),
         * NOT Activate -- the two were transposed in the old IMMDevice vtable. */
        IPropertyStore *ps = NULL;
        hr = CALL_VTBL2(dev, OpenPropertyStore, 0x00000000u /* STGM_READ */, (void**)&ps);
        if (SUCCEEDED(hr) && ps) {
            typedef HRESULT (__stdcall *GetValueFn)(void*, const PROPERTYKEY*, PROPVARIANT*);
            GetValueFn getval = (GetValueFn)((void**)ps->lpVtbl)[VTBL_SLOT(IPropertyStoreVtbl, GetValue)];
            PROPVARIANT pv = {0};
            getval(ps, &pkeyName, &pv);
            if (pv.vt == VT_LPWSTR && pv.pwszVal) {
                wcsncpy(g_endpoints[g_endpoint_count].name, pv.pwszVal, 255);
                g_endpoints[g_endpoint_count].name[255] = L'\0';
            } else {
                _snwprintf(g_endpoints[g_endpoint_count].name, 256, L"Audio Output %u", i + 1);
            }
            PropVariantClear(&pv);
            CALL_VTBL0(ps, Release);
        } else {
            _snwprintf(g_endpoints[g_endpoint_count].name, 256, L"Audio Output %u", i + 1);
        }

        g_endpoints[g_endpoint_count].device = dev;
        g_endpoints[g_endpoint_count].vol = NULL;
        g_endpoints[g_endpoint_count].is_default = FALSE;
        g_endpoint_count++;

        IAudioEndpointVolume *ev = NULL;
        hr = CALL_VTBL4(dev, Activate, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&ev);
        if (SUCCEEDED(hr) && ev) {
            g_endpoints[g_endpoint_count - 1].vol = ev;
        }
    }

    typedef void (__stdcall *ReleaseFn)(void*);
    ReleaseFn rel = (ReleaseFn)((void**)coll->lpVtbl)[2];
    rel(coll);

    /* Get default endpoint */
    IMMDevice *def = NULL;
    hr = CALL_VTBL3(e, GetDefaultAudioEndpoint, eRender, eConsole, (void**)&def);
    if (SUCCEEDED(hr) && def) {
        LPWSTR def_id = NULL;
        if (SUCCEEDED(CALL_VTBL1(def, GetId, &def_id))) {
            for (int i = 0; i < g_endpoint_count; i++) {
                if (wcscmp(g_endpoints[i].id, def_id) == 0) {
                    g_default_endpoint_idx = i;
                    g_endpoints[i].is_default = TRUE;
                    g_default_device = def;
                    break;
                }
            }
            CoTaskMemFree(def_id);
        }
        if (g_default_endpoint_idx < 0) {
            CALL_VTBL0(def, Release);
        }
    }

    return S_OK;
}

static void endpoints_cleanup(void) {
    for (int i = 0; i < g_endpoint_count; i++) {
        if (g_endpoints[i].vol) { CALL_VTBL0(g_endpoints[i].vol, Release); g_endpoints[i].vol = NULL; }
        if (g_endpoints[i].device) { CALL_VTBL0(g_endpoints[i].device, Release); g_endpoints[i].device = NULL; }
    }
    g_endpoint_count = 0;
    g_default_endpoint_idx = -1;
    if (g_default_device) { CALL_VTBL0(g_default_device, Release); g_default_device = NULL; }
}

/* ============================================================================
 *  Remembered per-app levels
 *
 *  Stored as UTF-8 in %LOCALAPPDATA%\AudioSwitcher\levels.txt, matching the
 *  suite convention (ProjectHub uses %LOCALAPPDATA%\ProjectHub). One line per
 *  app, keyed by lowercase exe basename so the level survives restarts and PID
 *  changes:
 *
 *      spotify.exe=40,0        volume 40%, not muted
 *
 *  A line-based format is deliberate: it is human-readable, hand-editable, and
 *  needs no JSON parser or escaping rules in C.
 * ============================================================================ */

#define MAX_REMEMBERED 256

typedef struct {
    wchar_t key[256];
    int     percent;
    BOOL    mute;
} RememberedLevel;

static RememberedLevel g_remembered[MAX_REMEMBERED];
static int             g_remembered_count = 0;
static wchar_t         g_levels_path[MAX_PATH] = L"";
static BOOL            g_levels_enabled = TRUE;

/* Lowercased exe basename: the identity an app keeps across launches. */
static void key_from_path(const wchar_t *full, wchar_t *key, size_t cap) {
    const wchar_t *base = wcsrchr(full, L'\\');
    base = base ? base + 1 : full;
    if (cap == 0) return;
    wcsncpy(key, base, cap - 1);
    key[cap - 1] = L'\0';
    for (wchar_t *p = key; *p; p++)
        if (*p >= L'A' && *p <= L'Z') *p = (wchar_t)(*p - L'A' + L'a');
}

static void levels_path_init(void) {
    wchar_t local[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local)) || !local[0])
        GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (!local[0]) {
        g_levels_enabled = FALSE;   /* nowhere to write; run without persistence */
        return;
    }
    wchar_t dir[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%s\\AudioSwitcher", local);
    CreateDirectoryW(dir, NULL);    /* fails harmlessly if it already exists */
    _snwprintf(g_levels_path, MAX_PATH, L"%s\\levels.txt", dir);
}

static RememberedLevel *levels_find(const wchar_t *key) {
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < g_remembered_count; i++)
        if (wcscmp(g_remembered[i].key, key) == 0) return &g_remembered[i];
    return NULL;
}

static void levels_load(void) {
    g_remembered_count = 0;
    if (!g_levels_enabled || !g_levels_path[0]) return;

    FILE *f = _wfopen(g_levels_path, L"rb");
    if (!f) return;                 /* first run: no file yet is normal */

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Strip a UTF-8 BOM if a text editor added one. */
    char *p = buf;
    if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB
               && (unsigned char)p[2] == 0xBF) p += 3;

    wchar_t wide[65536];
    int wn = MultiByteToWideChar(CP_UTF8, 0, p, -1, wide, 65536);
    if (wn <= 0) return;

    wchar_t *line = wide;
    while (line && *line && g_remembered_count < MAX_REMEMBERED) {
        wchar_t *nl = wcschr(line, L'\n');
        if (nl) *nl = L'\0';
        wchar_t *cr = wcschr(line, L'\r');
        if (cr) *cr = L'\0';

        if (line[0] && line[0] != L'#') {
            wchar_t *eq = wcschr(line, L'=');
            if (eq) {
                *eq = L'\0';
                int percent = 0, mute = 0;
                if (swscanf(eq + 1, L"%d,%d", &percent, &mute) >= 1) {
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                    wcsncpy(g_remembered[g_remembered_count].key, line, 255);
                    g_remembered[g_remembered_count].key[255] = L'\0';
                    g_remembered[g_remembered_count].percent = percent;
                    g_remembered[g_remembered_count].mute = mute ? TRUE : FALSE;
                    g_remembered_count++;
                }
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

static void levels_save(void) {
    if (!g_levels_enabled || !g_levels_path[0]) return;

    /* Build the whole file in memory, then write it in one go: a half-written
     * levels file would be worse than none, since it is read at every start. */
    static char out[MAX_REMEMBERED * 320];
    int off = 0;
    off += _snprintf(out + off, sizeof(out) - off,
                     "# Audio Switcher remembered per-app levels\n"
                     "# <exe-name>=<volume percent 0-100>,<muted 0|1>\n"
                     "# Hand-editable. Delete a line to forget that app.\n");
    for (int i = 0; i < g_remembered_count && off < (int)sizeof(out) - 320; i++) {
        char keyu8[512];
        int kn = WideCharToMultiByte(CP_UTF8, 0, g_remembered[i].key, -1,
                                     keyu8, sizeof(keyu8), NULL, NULL);
        if (kn <= 0) continue;
        off += _snprintf(out + off, sizeof(out) - off, "%s=%d,%d\n",
                         keyu8, g_remembered[i].percent,
                         g_remembered[i].mute ? 1 : 0);
    }

    FILE *f = _wfopen(g_levels_path, L"wb");
    if (!f) return;
    fwrite(out, 1, (size_t)off, f);
    fclose(f);
}

/* Record a level the user set, and persist it. */
static void levels_remember(const wchar_t *key, float volume, BOOL mute) {
    if (!g_levels_enabled || !key || !key[0]) return;
    int percent = (int)(volume * 100.0f + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    RememberedLevel *slot = levels_find(key);
    if (!slot) {
        if (g_remembered_count >= MAX_REMEMBERED) return;
        slot = &g_remembered[g_remembered_count++];
        wcsncpy(slot->key, key, 255);
        slot->key[255] = L'\0';
    }
    slot->percent = percent;
    slot->mute = mute;
    levels_save();
}

static BOOL pid_already_applied(DWORD pid) {
    for (int i = 0; i < g_applied_pid_count; i++)
        if (g_applied_pids[i] == pid) return TRUE;
    return FALSE;
}

static void mark_pid_applied(DWORD pid) {
    if (g_applied_pid_count < MAX_APPLIED_PIDS)
        g_applied_pids[g_applied_pid_count++] = pid;
    else
        g_applied_pids[0] = pid;    /* ring over the oldest; bounds, not correctness */
}

static HRESULT sessions_refresh(void) {
    IMMDeviceEnumerator *e = g_enum;
    if (!e) return E_FAIL;

    IMMDevice *dev = NULL;
    HRESULT hr = CALL_VTBL3(e, GetDefaultAudioEndpoint, eRender, eConsole, (void**)&dev);
    if (FAILED(hr)) { g_sessions_hr = hr; return hr; }

    IAudioSessionManager2 *mgr = NULL;
    hr = CALL_VTBL4(dev, Activate, &IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (void**)&mgr);
    CALL_VTBL0(dev, Release);
    if (FAILED(hr)) { g_sessions_hr = hr; return hr; }

    if (g_session_mgr) { CALL_VTBL0(g_session_mgr, Release); }
    g_session_mgr = mgr;

    IAudioSessionEnumerator *enum_sessions = NULL;
    hr = CALL_VTBL1(mgr, GetSessionEnumerator, (void**)&enum_sessions);
    if (FAILED(hr)) {
        CALL_VTBL0(mgr, Release);
        g_session_mgr = NULL;
        g_sessions_hr = hr;
        return hr;
    }

    /* The enumerator vtable is declared correctly now, so call it by name
     * instead of re-declaring a duplicate struct at the call site. */
    int count = 0;
    CALL_VTBL1(enum_sessions, GetCount, &count);

    /* Release the previous poll's ISimpleAudioVolume references. This runs on
     * a two-second timer, so the old code leaked one COM reference per app per
     * poll for as long as the window was open. */
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].svol) { CALL_VTBL0(g_sessions[i].svol, Release); g_sessions[i].svol = NULL; }
    }

    g_session_count = 0;
    for (int i = 0; i < count && g_session_count < 128; i++) {
        void *session = NULL;
        CALL_VTBL2(enum_sessions, GetSession, i, &session);
        if (!session) continue;

        IAudioSessionControl2 *ctl2 = NULL;
        hr = CALL_VTBL2((IUnknown*)session, QueryInterface,
                        &IID_IAudioSessionControl2, (void**)&ctl2);
        if (FAILED(hr) || !ctl2) {
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }

        /* Skip System Sounds: not an app, and nothing useful to control.
         * IsSystemSoundsSession returns S_OK when it IS system sounds and
         * S_FALSE otherwise, so compare exactly -- SUCCEEDED() would match
         * both and drop every session. */
        if (CALL_VTBL0(ctl2, IsSystemSoundsSession) == S_OK) {
            CALL_VTBL0(ctl2, Release);
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }

        AudioSession *s = &g_sessions[g_session_count];
        memset(s, 0, sizeof(*s));

        /* GetProcessId is slot 14 of IAudioSessionControl2. It used to be
         * declared at slot 3, which is really GetState, so this "PID" was the
         * session state (0/1/2): OpenProcess then failed and every app row
         * came up blank, while `if (pid == 0)` silently skipped inactive
         * sessions for the wrong reason. */
        CALL_VTBL1(ctl2, GetProcessId, &s->process_id);

        if (s->process_id != 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                       FALSE, s->process_id);
            if (hProc) {
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, s->exe_path, &size)) {
                    const wchar_t *slash = wcsrchr(s->exe_path, L'\\');
                    wcsncpy(s->name, slash ? slash + 1 : s->exe_path, 255);
                    s->name[255] = L'\0';
                    key_from_path(s->exe_path, s->key, 256);
                }
                CloseHandle(hProc);
            }
        }

        /* Fall back to the session's own display name when the process could
         * not be opened (elevated app, or a session with no owning process). */
        if (!s->name[0]) {
            LPWSTR disp = NULL;
            if (SUCCEEDED(CALL_VTBL1(ctl2, GetDisplayName, &disp)) && disp && disp[0]) {
                wcsncpy(s->name, disp, 255);
                s->name[255] = L'\0';
                s->named_from_session = TRUE;
            }
            if (disp) CoTaskMemFree(disp);
        }
        if (!s->name[0])
            _snwprintf(s->name, 256, L"Unknown app (PID %lu)",
                       (unsigned long)s->process_id);

        ISimpleAudioVolume *svol = NULL;
        hr = CALL_VTBL2(ctl2, QueryInterface, &IID_ISimpleAudioVolume, (void**)&svol);
        if (FAILED(hr) || !svol) {
            /* No volume interface means the row would have a dead slider;
             * better to not show it at all. */
            CALL_VTBL0(ctl2, Release);
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }
        s->svol = svol;

        CALL_VTBL1(svol, GetMasterVolume, &s->volume);
        CALL_VTBL1(svol, GetMute, &s->mute);

        /* Restore a remembered level the first time we see this process, and
         * only then: re-applying on every 2-second poll would snap the slider
         * back out from under the user while they dragged it. */
        if (s->key[0] && s->process_id != 0 && !pid_already_applied(s->process_id)) {
            RememberedLevel *mem = levels_find(s->key);
            if (mem) {
                float want = mem->percent / 100.0f;
                CALL_VTBL2(svol, SetMasterVolume, want, NULL);
                CALL_VTBL2(svol, SetMute, mem->mute, NULL);
                s->volume = want;
                s->mute = mem->mute;
            }
            mark_pid_applied(s->process_id);
        }

        CALL_VTBL0(ctl2, Release);
        CALL_VTBL0((IUnknown*)session, Release);
        g_session_count++;
    }

    CALL_VTBL0(enum_sessions, Release);

    g_sessions_hr = S_OK;
    return S_OK;
}

static void sessions_cleanup(void) {
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].svol) { CALL_VTBL0(g_sessions[i].svol, Release); g_sessions[i].svol = NULL; }
    }
    g_session_count = 0;
    if (g_session_mgr) { CALL_VTBL0(g_session_mgr, Release); g_session_mgr = NULL; }
}

static BOOL endpoint_set_volume(int idx, float level) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_endpoints[idx].vol, SetMasterVolumeLevelScalar, level, NULL);
    return SUCCEEDED(hr);
}

static float endpoint_get_volume(int idx) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return 0.0f;
    float v = 0.0f;
    CALL_VTBL1(g_endpoints[idx].vol, GetMasterVolumeLevelScalar, &v);
    return v;
}

static BOOL endpoint_get_mute(int idx) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    BOOL m = FALSE;
    CALL_VTBL1(g_endpoints[idx].vol, GetMute, &m);
    return m;
}

static BOOL endpoint_set_mute(int idx, BOOL mute) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_endpoints[idx].vol, SetMute, mute, NULL);
    return SUCCEEDED(hr);
}

static BOOL session_set_volume(int idx, float level) {
    if (idx < 0 || idx >= g_session_count || !g_sessions[idx].svol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_sessions[idx].svol, SetMasterVolume, level, NULL);
    if (SUCCEEDED(hr)) g_sessions[idx].volume = level;
    return SUCCEEDED(hr);
}

static BOOL session_set_mute(int idx, BOOL mute) {
    if (idx < 0 || idx >= g_session_count || !g_sessions[idx].svol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_sessions[idx].svol, SetMute, mute, NULL);
    if (SUCCEEDED(hr)) g_sessions[idx].mute = mute;
    return SUCCEEDED(hr);
}

/* ============================================================================
 *  Win32 window + UI + tray
 * ============================================================================ */

static HINSTANCE g_hInst = NULL;
static HWND   g_hwnd = NULL;
static HICON  g_app_icon = NULL;
static NOTIFYICONDATAW g_nid = {0};
static BOOL   g_quitting = FALSE;
static BOOL   g_silent = FALSE;

#define PANEL_X      24
#define PANEL_Y      80
#define PANEL_W      640

static HWND g_btn_refresh = NULL;

typedef struct {
    HWND hwnd_btn;
    HWND hwnd_vol;
    HWND hwnd_mute;
    HWND hwnd_label;
    HWND hwnd_voltxt;
    int  endpoint_idx;
} EndpointCtrl;

static EndpointCtrl g_endpoint_ctrls[32];

typedef struct {
    HWND hwnd_label;
    HWND hwnd_vol;
    HWND hwnd_mute;
    HWND hwnd_voltxt;
    int  session_idx;
} SessionCtrl;

static SessionCtrl g_session_ctrls[128];

static int g_dpi = 96;
static int S(int x) { return MulDiv(x, g_dpi, 96); }

/* Spinning emblem */
static RECT   g_rcEmblem   = {0, 0, 0, 0};

/* ============================================================================
 *  Control theming
 *
 *  Stock Win32 buttons and trackbars render with the light OS chrome, which on
 *  the Soft Slate dark canvas looked like unfinished controls dropped onto a
 *  finished background. Everything user-facing here is drawn from the palette:
 *  buttons are owner-drawn, sliders are custom-drawn, and text uses Segoe UI at
 *  consistent sizes instead of the default 8px system font.
 * ============================================================================ */

static HFONT  g_font_ui    = NULL;   /* Segoe UI 9pt            */
static HFONT  g_font_head  = NULL;   /* Segoe UI 9pt semibold   */
static HFONT  g_font_small = NULL;   /* Segoe UI 8pt            */
static HWND   g_hover_btn  = NULL;   /* button currently under the cursor */

/* Small brush cache: WM_DRAWITEM and WM_PAINT fire constantly, and creating a
 * brush per paint would leak GDI objects until the desktop ran out. */
static struct { COLORREF c; HBRUSH b; } g_brush_cache[24];

static HBRUSH ui_brush(COLORREF c) {
    for (int i = 0; i < 24; i++) {
        if (g_brush_cache[i].b && g_brush_cache[i].c == c) return g_brush_cache[i].b;
    }
    for (int i = 0; i < 24; i++) {
        if (!g_brush_cache[i].b) {
            g_brush_cache[i].c = c;
            g_brush_cache[i].b = CreateSolidBrush(c);
            return g_brush_cache[i].b;
        }
    }
    return (HBRUSH)GetStockObject(WHITE_BRUSH);
}

static void ui_fonts_create(void) {
    LOGFONTW lf = {0};
    wcscpy(lf.lfFaceName, L"Segoe UI");
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfHeight = -MulDiv(9, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    g_font_ui = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    g_font_head = CreateFontIndirectW(&lf);
    lf.lfHeight = -MulDiv(8, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    g_font_small = CreateFontIndirectW(&lf);
}

static void ui_fonts_destroy(void) {
    if (g_font_ui)    { DeleteObject(g_font_ui);    g_font_ui = NULL; }
    if (g_font_head)  { DeleteObject(g_font_head);  g_font_head = NULL; }
    if (g_font_small) { DeleteObject(g_font_small); g_font_small = NULL; }
    for (int i = 0; i < 24; i++) {
        if (g_brush_cache[i].b) { DeleteObject(g_brush_cache[i].b); g_brush_cache[i].b = NULL; }
    }
}

/* Hover tracking for owner-drawn buttons: ODS_HOTLIGHT is not reliably set for
 * BS_OWNERDRAW, so track the cursor ourselves and invalidate. */
static LRESULT CALLBACK BtnHoverProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                     UINT_PTR uid, DWORD_PTR ref) {
    (void)uid; (void)ref;
    if (msg == WM_MOUSEMOVE && g_hover_btn != hwnd) {
        g_hover_btn = hwnd;
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (msg == WM_MOUSELEAVE && g_hover_btn == hwnd) {
        g_hover_btn = NULL;
        InvalidateRect(hwnd, NULL, FALSE);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* A trackbar paints its own background in light chrome. Take the erase over so
 * the whole control sits on the card colour, and let NM_CUSTOMDRAW paint the
 * channel and thumb. */
static LRESULT CALLBACK TrackbarSkinProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR uid, DWORD_PTR ref) {
    (void)uid; (void)ref;
    if (msg == WM_ERASEBKGND) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, ui_brush((COLORREF)ref ? COL_CARD : COL_BG));
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1 from Project Hub */
static void EmbRasterize(uint32_t *dst, int n, COLORREF bg) {
    (void)bg;
    const double scale = EMB_SDF_SCALE, b = EMB_SDF_BOOST;
    const double pxSize = 2.0 / (double)n;
    const double aa = (pxSize * 1.35 > 1e-4) ? (pxSize * 1.35) : 1e-4;
    const double deg = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double nrm[3] = { 90.0, -150.0, -30.0 };
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
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

typedef struct EmbCache {
    int      sz;
    int      srcSz;
    uint32_t *src;
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
            double a = ((c00 >> 24) & 255) * w00 + ((c10 >> 24) & 255) * w10 +
                       ((c01 >> 24) & 255) * w01 + ((c11 >> 24) & 255) * w11;
            int ai = (int)(a + 0.5);
            uint32_t d = px[ty * W + tx];
            if (ai >= 255) {
                px[ty * W + tx] = RGB((int)(r + 0.5), (int)(g + 0.5), (int)(b + 0.5));
            } else if (ai > 0) {
                int dr = (int)(d & 255), dg = (int)((d >> 8) & 255), db = (int)((d >> 16) & 255);
                px[ty * W + tx] = RGB(((int)(r + 0.5) * ai + dr * (255 - ai) + 127) / 255,
                                      ((int)(g + 0.5) * ai + dg * (255 - ai) + 127) / 255,
                                      ((int)(b + 0.5) * ai + db * (255 - ai) + 127) / 255);
            }
        }
    }
}

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). */
static void PaintEmblemBlit(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    uint32_t bgP = ((uint32_t)GetRValue(COL_BG) << 16) |
                   ((uint32_t)GetGValue(COL_BG) << 8) |
                   (uint32_t)GetBValue(COL_BG);
    for (int i = 0; i < sz * sz; i++) px[i] = bgP;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

static int GetWinDpi(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi ? dpi : 96;
}

static void update_volume_text(HWND hwnd_txt, float vol) {
    wchar_t text[32];
    swprintf(text, 32, L"%d%%", (int)(vol * 100.0f + 0.5f));
    SetWindowTextW(hwnd_txt, text);
}

/* ---------------------------------------------------------------------------
 *  UI reconciliation
 *
 *  The old refresh_ui() destroyed and recreated all 32 endpoint and 128 session
 *  controls on every call -- and WM_TIMER called it every two seconds. Dragging
 *  a volume slider therefore had the slider destroyed underneath the mouse,
 *  focus was lost, and the panel flickered twice a second forever.
 *
 *  Now the control tree is rebuilt ONLY when the set of devices or apps
 *  actually changes (detected by a hash over their identities). Otherwise the
 *  existing controls are updated in place, and a slider the user is currently
 *  dragging is left alone: a trackbar holds the mouse capture while its thumb
 *  is down, so GetCapture() is a reliable "hands off" signal.
 * ------------------------------------------------------------------------- */

static uint64_t g_ui_endpoint_sig = 0;
static uint64_t g_ui_session_sig  = 0;

/* Layout captured while building, so WM_PAINT can draw section headers and the
 * footer status line at the same positions the controls actually occupy. */
static int g_y_outputs_header = 0;
static int g_y_apps_header    = 0;
static int g_y_footer         = 0;
static int g_truncated_endpoints = 0;
static int g_truncated_sessions  = 0;

/* Card rectangles, captured at build time so WM_PAINT can draw the card
 * background behind each row at exactly the geometry the controls use. */
static RECT g_ep_card[32];
static RECT g_ss_card[128];
static int  g_ep_rows = 0;
static int  g_ss_rows = 0;

static uint64_t ui_hash_wstr(uint64_t h, const wchar_t *s) {
    for (; s && *s; s++) { h ^= (uint64_t)*s; h *= 1099511628211ULL; }
    return h;
}

static uint64_t ui_mix(uint64_t h, uint64_t v) {
    return (h ^ v) * 1099511628211ULL;
}

/* Identity only -- deliberately excludes volume and mute, which change
 * constantly and must be updated in place rather than trigger a rebuild. */
static uint64_t endpoints_signature(void) {
    uint64_t h = 1469598103934665603ULL;
    h = ui_mix(h, (uint64_t)g_endpoint_count);
    for (int i = 0; i < g_endpoint_count; i++) {
        h = ui_hash_wstr(h, g_endpoints[i].id);
        h = ui_mix(h, (uint64_t)(g_endpoints[i].is_default ? 1 : 0));
        h = ui_mix(h, (uint64_t)(g_endpoints[i].vol ? 1 : 0));
    }
    return h;
}

static uint64_t sessions_signature(void) {
    uint64_t h = 1469598103934665603ULL;
    h = ui_mix(h, (uint64_t)g_session_count);
    for (int i = 0; i < g_session_count; i++) {
        h = ui_mix(h, (uint64_t)g_sessions[i].process_id);
        h = ui_hash_wstr(h, g_sessions[i].key);
        h = ui_hash_wstr(h, g_sessions[i].name);
        h = ui_mix(h, (uint64_t)(g_sessions[i].svol ? 1 : 0));
    }
    return h;
}

static void ui_destroy_all(void) {
    /* A hovered button that is about to be destroyed would otherwise leave
     * g_hover_btn pointing at a handle Windows may hand to the next button,
     * making an unrelated button draw itself as hovered. */
    g_hover_btn = NULL;

    for (int i = 0; i < 32; i++) {
        if (g_endpoint_ctrls[i].hwnd_btn)    DestroyWindow(g_endpoint_ctrls[i].hwnd_btn);
        if (g_endpoint_ctrls[i].hwnd_vol)    DestroyWindow(g_endpoint_ctrls[i].hwnd_vol);
        if (g_endpoint_ctrls[i].hwnd_mute)   DestroyWindow(g_endpoint_ctrls[i].hwnd_mute);
        if (g_endpoint_ctrls[i].hwnd_label)  DestroyWindow(g_endpoint_ctrls[i].hwnd_label);
        if (g_endpoint_ctrls[i].hwnd_voltxt) DestroyWindow(g_endpoint_ctrls[i].hwnd_voltxt);
        memset(&g_endpoint_ctrls[i], 0, sizeof(g_endpoint_ctrls[i]));
        g_endpoint_ctrls[i].endpoint_idx = -1;
    }
    for (int i = 0; i < 128; i++) {
        if (g_session_ctrls[i].hwnd_label)  DestroyWindow(g_session_ctrls[i].hwnd_label);
        if (g_session_ctrls[i].hwnd_vol)    DestroyWindow(g_session_ctrls[i].hwnd_vol);
        if (g_session_ctrls[i].hwnd_mute)   DestroyWindow(g_session_ctrls[i].hwnd_mute);
        if (g_session_ctrls[i].hwnd_voltxt) DestroyWindow(g_session_ctrls[i].hwnd_voltxt);
        memset(&g_session_ctrls[i], 0, sizeof(g_session_ctrls[i]));
        g_session_ctrls[i].session_idx = -1;
    }
}

/* Layout is computed against a fixed budget, NOT against the current client
 * height, and the window is then sized to the result. Deriving rows from the
 * live client height would feed the resize back into the next layout and
 * oscillate. */
#define UI_MAX_CLIENT 940

static void ui_build_controls(void) {
    int hdr_h  = S(34);
    int ep_row = S(72);
    int ss_row = S(56);
    int gap    = S(18);
    int footer = S(40);

    int budget = S(UI_MAX_CLIENT) - footer - (PANEL_Y + hdr_h);

    g_y_outputs_header = PANEL_Y;
    int y = PANEL_Y + hdr_h;

    /* Reserve room for three app rows so a long device list can never push the
     * per-app section out entirely. */
    int reserve = hdr_h + gap + 3 * ss_row;
    int ep_avail = budget - reserve;
    int ep_max = ep_avail > 0 ? ep_avail / ep_row : 0;
    if (ep_max > g_endpoint_count) ep_max = g_endpoint_count;
    if (ep_max > 32) ep_max = 32;
    g_truncated_endpoints = g_endpoint_count - ep_max;
    g_ep_rows = ep_max;

    for (int i = 0; i < ep_max; i++) {
        EndpointCtrl *c = &g_endpoint_ctrls[i];
        c->endpoint_idx = i;
        SetRect(&g_ep_card[i], PANEL_X, y, PANEL_X + PANEL_W, y + ep_row - S(8));

        /* Columns measured inward from the right edge so they can never
         * overlap, whatever the DPI. */
        int right  = PANEL_X + PANEL_W - S(14);
        int def_w = S(104), mute_w = S(72), pct_w = S(46), g10 = S(10);
        int def_x  = right - def_w;
        int mute_x = def_x - g10 - mute_w;
        int pct_x  = mute_x - g10 - pct_w;
        int sl_x   = PANEL_X + S(16);
        int sl_w   = pct_x - S(10) - sl_x;

        c->hwnd_label = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            PANEL_X + S(16), y + S(9), PANEL_W - S(32), S(20),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_label, WM_SETFONT, (WPARAM)g_font_head, TRUE);

        c->hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | (g_endpoints[i].vol ? 0 : WS_DISABLED),
            sl_x, y + S(36), sl_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(1000 + i), g_hInst, NULL);
        SendMessageW(c->hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        SetWindowSubclass(c->hwnd_vol, TrackbarSkinProc, 0, (DWORD_PTR)1);

        c->hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            pct_x, y + S(39), pct_w, S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_voltxt, WM_SETFONT, (WPARAM)g_font_small, TRUE);

        c->hwnd_mute = CreateWindowExW(
            0, L"BUTTON", L"Mute",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            mute_x, y + S(34), mute_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(2000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_mute, BtnHoverProc, 0, 0);

        c->hwnd_btn = CreateWindowExW(
            0, L"BUTTON", L"Set Default",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            def_x, y + S(34), def_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(3000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_btn, BtnHoverProc, 0, 0);

        y += ep_row;
    }

    y += gap;
    g_y_apps_header = y;
    y += hdr_h;

    int ss_avail = S(UI_MAX_CLIENT) - footer - y;
    int ss_max = ss_avail > 0 ? ss_avail / ss_row : 0;
    if (ss_max > g_session_count) ss_max = g_session_count;
    if (ss_max > 128) ss_max = 128;
    g_truncated_sessions = g_session_count - ss_max;
    g_ss_rows = ss_max;

    for (int i = 0; i < ss_max; i++) {
        SessionCtrl *c = &g_session_ctrls[i];
        c->session_idx = i;
        SetRect(&g_ss_card[i], PANEL_X, y, PANEL_X + PANEL_W, y + ss_row - S(6));

        int right  = PANEL_X + PANEL_W - S(14);
        int mute_w = S(72), pct_w = S(46), g10 = S(10);
        int mute_x = right - mute_w;
        int pct_x  = mute_x - g10 - pct_w;
        int sl_x   = PANEL_X + S(16);
        int sl_w   = pct_x - S(10) - sl_x;

        c->hwnd_label = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            PANEL_X + S(16), y + S(7), PANEL_W - S(32), S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_label, WM_SETFONT, (WPARAM)g_font_ui, TRUE);

        c->hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE,
            sl_x, y + S(27), sl_w, S(24),
            g_hwnd, (HMENU)(INT_PTR)(4000 + i), g_hInst, NULL);
        SendMessageW(c->hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        SetWindowSubclass(c->hwnd_vol, TrackbarSkinProc, 0, (DWORD_PTR)1);

        c->hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            pct_x, y + S(30), pct_w, S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_voltxt, WM_SETFONT, (WPARAM)g_font_small, TRUE);

        c->hwnd_mute = CreateWindowExW(
            0, L"BUTTON", L"Mute",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            mute_x, y + S(25), mute_w, S(24),
            g_hwnd, (HMENU)(INT_PTR)(5000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_mute, BtnHoverProc, 0, 0);

        y += ss_row;
    }

    g_y_footer = y + S(10);

    /* Size the window to the content so there is no dead void under a short
     * list, clamped so it always fits a laptop screen. */
    int content = g_y_footer + footer;
    if (content < S(460)) content = S(460);
    if (content > S(UI_MAX_CLIENT)) content = S(UI_MAX_CLIENT);

    RECT wr, cr;
    GetWindowRect(g_hwnd, &wr);
    GetClientRect(g_hwnd, &cr);
    int frame_h = (wr.bottom - wr.top) - (cr.bottom - cr.top);
    int frame_w = (wr.right - wr.left) - (cr.right - cr.left);
    int want_w = PANEL_X * 2 + PANEL_W + frame_w;
    SetWindowPos(g_hwnd, NULL, 0, 0, want_w, content + frame_h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void ui_update_values(void) {
    wchar_t label[320];

    for (int i = 0; i < 32; i++) {
        EndpointCtrl *c = &g_endpoint_ctrls[i];
        if (!c->hwnd_vol) continue;
        int idx = c->endpoint_idx;
        if (idx < 0 || idx >= g_endpoint_count) continue;

        BOOL is_def = (idx == g_default_endpoint_idx);
        _snwprintf(label, 320, L"%s%s", g_endpoints[idx].name,
                   is_def ? L"   \u25CF current output" : L"");
        label[319] = L'\0';
        if (c->hwnd_label) SetWindowTextW(c->hwnd_label, label);

        /* Leave a slider alone while the user is dragging it. */
        if (GetCapture() != c->hwnd_vol) {
            if (g_endpoints[idx].vol) {
                float v = endpoint_get_volume(idx);
                int pct = (int)(v * 100.0f + 0.5f);
                SendMessageW(c->hwnd_vol, TBM_SETPOS, TRUE, pct);
                if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, v);
            } else if (c->hwnd_voltxt) {
                /* No IAudioEndpointVolume means we genuinely cannot read this
                 * device's level. Showing 0% would be a lie the user can hear. */
                SetWindowTextW(c->hwnd_voltxt, L"\u2014");
            }
        }
        if (c->hwnd_mute)
            SetWindowTextW(c->hwnd_mute, endpoint_get_mute(idx) ? L"Unmute" : L"Mute");
        if (c->hwnd_btn)
            SetWindowTextW(c->hwnd_btn, is_def ? L"Default" : L"Set Default");
    }

    for (int i = 0; i < 128; i++) {
        SessionCtrl *c = &g_session_ctrls[i];
        if (!c->hwnd_vol) continue;
        int idx = c->session_idx;
        if (idx < 0 || idx >= g_session_count) continue;

        if (c->hwnd_label) SetWindowTextW(c->hwnd_label, g_sessions[idx].name);
        if (GetCapture() != c->hwnd_vol) {
            SendMessageW(c->hwnd_vol, TBM_SETPOS, TRUE,
                         (int)(g_sessions[idx].volume * 100.0f + 0.5f));
            if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, g_sessions[idx].volume);
        }
        if (c->hwnd_mute)
            SetWindowTextW(c->hwnd_mute, g_sessions[idx].mute ? L"Unmute" : L"Mute");
    }
}

static void refresh_ui(void) {
    if (!g_hwnd) return;

    uint64_t esig = endpoints_signature();
    uint64_t ssig = sessions_signature();

    if (esig != g_ui_endpoint_sig || ssig != g_ui_session_sig) {
        ui_destroy_all();
        ui_build_controls();
        g_ui_endpoint_sig = esig;
        g_ui_session_sig  = ssig;
    }

    ui_update_values();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* Footer status line. Reports what actually happened rather than implying
 * success: a switch that Windows refused must say so, because the old code
 * repainted a "Default" badge over a device it had never switched to. */
static void ui_status_text(wchar_t *out, size_t cap) {
    wchar_t detail[256];

    if (g_last_switch_idx >= 0 && FAILED(g_last_switch_hr)) {
        _snwprintf(detail, 256,
                   L"Switch FAILED (0x%08lX) \u2014 audio is still on the previous device",
                   (unsigned long)g_last_switch_hr);
    } else if (g_policy_variant == 0) {
        /* Probed at startup, so this is Windows' actual refusal code, not a
         * guess. The code is what makes it actionable. */
        _snwprintf(detail, 256,
                   L"Device switching unavailable: IPolicyConfig 0x%08lX (run --diag)",
                   (unsigned long)g_policy_hr);
    } else if (g_last_switch_idx >= 0 && g_last_switch_idx < g_endpoint_count) {
        _snwprintf(detail, 256, L"Switched to %s", g_endpoints[g_last_switch_idx].name);
    } else {
        _snwprintf(detail, 256, L"%d output(s)  \u00B7  %d app(s)  \u00B7  %d remembered level(s)",
                   g_endpoint_count, g_session_count, g_remembered_count);
    }
    detail[255] = L'\0';

    int n = _snwprintf(out, cap, L"%s", detail);
    if (n < 0) n = 0;
    if (g_truncated_endpoints > 0 || g_truncated_sessions > 0) {
        _snwprintf(out + n, cap - (size_t)n, L"   \u00B7  %d hidden (window too small)",
                   g_truncated_endpoints + g_truncated_sessions);
    }
    out[cap - 1] = L'\0';
}


static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_app_icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP));
        if (!g_app_icon) g_app_icon = LoadIconW(NULL, IDI_APPLICATION);

        ZeroMemory(&g_nid, sizeof(g_nid));
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_app_icon;
        wcscpy(g_nid.szTip, L"Audio Switcher - per-app volume & output switching");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        ui_fonts_create();

        g_btn_refresh = CreateWindowExW(
            0, L"BUTTON", L"\u21BB  Refresh",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            PANEL_X, PANEL_Y - S(38), S(104), S(28),
            hwnd, (HMENU)IDM_REFRESH, g_hInst, NULL);
        SetWindowSubclass(g_btn_refresh, BtnHoverProc, 0, 0);

        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        com_create_enumerator();
        levels_path_init();
        levels_load();
        endpoints_refresh();
        sessions_refresh();

        /* Probe IPolicyConfig once at startup, not lazily on first click. The
         * status line reports g_policy_variant, so leaving it 0 until someone
         * pressed a button made the footer claim "switching unavailable" on
         * machines where it had simply never been attempted -- a false alarm
         * that reads as a broken app. Attempting it here means the warning, if
         * it still appears, is a genuine HRESULT from Windows. */
        policy_activate();

        SetTimer(hwnd, 1, 2000, NULL);
        /* Emblem spin timer - 50ms for smooth rotation */
        SetTimer(hwnd, 2, 50, NULL);
        return 0;
    }

    case WM_DESTROY:
        g_quitting = TRUE;
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        sessions_cleanup();
        endpoints_cleanup();
        com_release_enumerator();
        ui_fonts_destroy();
        CoUninitialize();
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Audio Switcher");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Audio Switcher");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                           pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lp == WM_LBUTTONDBLCLK) {
            if (!IsWindowVisible(hwnd))
                ShowWindow(hwnd, SW_SHOWNORMAL);
            else
                ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    }

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDM_REFRESH) {
            sessions_refresh();
            refresh_ui();
            return 0;
        }
        if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id >= 3000 && id < 3100) {
            int idx = id - 3000;
            /* Actually switch the device, then re-read the truth from Windows
             * instead of trusting our own bookkeeping. The old handler only set
             * g_default_endpoint_idx and repainted, so the button said "Default"
             * while audio kept playing through the previous device. */
            HRESULT hr = endpoint_make_default(idx);
            if (SUCCEEDED(hr)) {
                endpoints_refresh();
                sessions_refresh();   /* sessions live on the new default device */
            }
            refresh_ui();
            return 0;
        }
        if (id >= 2000 && id < 2100) {
            int idx = id - 2000;
            endpoint_set_mute(idx, !endpoint_get_mute(idx));
            refresh_ui();
            return 0;
        }
        if (id >= 5000 && id < 5200) {
            int idx = id - 5000;
            /* Bounds-check before reading g_sessions[idx]: the 2-second poll
             * can shrink the session list while a stale button id is still in
             * flight, which made this an out-of-bounds read. */
            if (idx >= 0 && idx < g_session_count) {
                BOOL now_muted = !g_sessions[idx].mute;
                session_set_mute(idx, now_muted);
                levels_remember(g_sessions[idx].key, g_sessions[idx].volume, now_muted);
                refresh_ui();
            }
            return 0;
        }
        return 0;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lp;
        int id = (int)GetWindowLongPtrW(slider, GWLP_ID);
        int pos = SendMessageW(slider, TBM_GETPOS, 0, 0);
        float level = pos / 100.0f;

        if (id >= 1000 && id < 1100) {
            int idx = id - 1000;
            endpoint_set_volume(idx, level);
            for (int i = 0; i < 32; i++) {
                if (g_endpoint_ctrls[i].endpoint_idx == idx && g_endpoint_ctrls[i].hwnd_voltxt) {
                    update_volume_text(g_endpoint_ctrls[i].hwnd_voltxt, level);
                    break;
                }
            }
        } else if (id >= 4000 && id < 4200) {
            int idx = id - 4000;
            session_set_volume(idx, level);
            if (idx >= 0 && idx < g_session_count)
                levels_remember(g_sessions[idx].key, level, g_sessions[idx].mute);
            for (int i = 0; i < 128; i++) {
                if (g_session_ctrls[i].session_idx == idx && g_session_ctrls[i].hwnd_voltxt) {
                    update_volume_text(g_session_ctrls[i].hwnd_voltxt, level);
                    break;
                }
            }
        }
        return 0;
    }

    case WM_TIMER:
        if (wp == 1) {
            /* Re-read endpoints too, not just sessions: plugging in headphones
             * must appear without a manual Refresh, and if the user switches
             * the default device in Windows' own settings the "current output"
             * marker has to follow. Safe to poll now that both refresh paths
             * release their previous COM references instead of leaking them. */
            endpoints_refresh();
            sessions_refresh();
            refresh_ui();
        } else if (wp == 2) {
            /* Emblem spin only - invalidate just the emblem square */
            if (g_rcEmblem.right > g_rcEmblem.left)
                InvalidateRect(hwnd, &g_rcEmblem, FALSE);
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT);
        return (INT_PTR)ui_brush(COL_CARD);
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lp;
        if (!di || di->CtlType != ODT_BUTTON) return FALSE;

        BOOL disabled = !IsWindowEnabled(di->hwndItem);
        BOOL pressed  = (di->itemState & ODS_SELECTED) != 0;
        BOOL hovered  = (g_hover_btn == di->hwndItem) && !disabled;

        wchar_t cap[128] = L"";
        GetWindowTextW(di->hwndItem, cap, 128);
        BOOL is_current = (wcscmp(cap, L"Default") == 0);

        COLORREF fill = disabled          ? COL_SEC_BTN
                      : pressed           ? COL_ACCENT_STRONG
                      : (hovered || is_current) ? COL_SEC_HOV
                      : COL_SEC_BTN;
        COLORREF edge = (hovered || pressed) ? COL_HOVER_RIM : COL_BTN_BORDER;
        COLORREF txt  = disabled ? COL_TELEMETRY : COL_TEXT;

        FillRect(di->hDC, &di->rcItem, ui_brush(fill));
        FrameRect(di->hDC, &di->rcItem, ui_brush(edge));

        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, txt);
        HFONT old = (HFONT)SelectObject(di->hDC, g_font_ui);
        DrawTextW(di->hDC, cap, -1, &di->rcItem,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(di->hDC, old);

        if (di->itemState & ODS_FOCUS) {
            RECT ring = di->rcItem;
            InflateRect(&ring, -3, -3);
            DrawFocusRect(di->hDC, &ring);
        }
        return TRUE;
    }

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR*)lp;
        if (!nm || nm->code != NM_CUSTOMDRAW) return 0;
        NMCUSTOMDRAW *cd = (NMCUSTOMDRAW*)lp;
        HWND tb = (HWND)cd->hdr.hwndFrom;

        if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->dwDrawStage != CDDS_ITEMPREPAINT) return 0;

        COLORREF acc = IsWindowEnabled(tb) ? COL_ACCENT : COL_TELEMETRY;

        if (cd->dwItemSpec == TBCD_CHANNEL) {
            RECT r = cd->rc;
            int mid = r.top + (r.bottom - r.top) / 2;
            RECT groove = { r.left, mid - S(2), r.right, mid + S(2) };
            FillRect(cd->hdc, &groove, ui_brush(COL_DIVIDER));
            LONG pos = SendMessageW(tb, TBM_GETPOS, 0, 0);
            int fw = MulDiv(groove.right - groove.left, pos, 100);
            if (fw > 0) {
                RECT filled = { groove.left, groove.top, groove.left + fw, groove.bottom };
                FillRect(cd->hdc, &filled, ui_brush(acc));
            }
            return CDRF_SKIPDEFAULT;
        }
        if (cd->dwItemSpec == TBCD_THUMB) {
            RECT r = cd->rc;
            HPEN pen = CreatePen(PS_SOLID, 1, COL_BG);
            HPEN oldp = (HPEN)SelectObject(cd->hdc, pen);
            HBRUSH oldb = (HBRUSH)SelectObject(cd->hdc, ui_brush(acc));
            Ellipse(cd->hdc, r.left, r.top, r.right + 1, r.bottom + 1);
            SelectObject(cd->hdc, oldb);
            SelectObject(cd->hdc, oldp);
            DeleteObject(pen);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_SKIPDEFAULT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, ui_brush(COL_BG));

        /* Header band */
        RECT hdr = { 0, 0, rc.right, S(60) };
        FillRect(hdc, &hdr, ui_brush(COL_FOOTER_BG));

        int embSz = S(44);
        int embX = rc.right - embSz - S(12);
        int embY = S(8);
        SetRect(&g_rcEmblem, embX, embY, embX + embSz, embY + embSz);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT);
        HFONT saved = (HFONT)SelectObject(hdc, g_font_head);
        DrawTextW(hdc, APP_TITLE, -1, &(RECT){ S(20), S(7), embX - S(12), S(29) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, saved);

        SetTextColor(hdc, COL_MUTED);
        saved = (HFONT)SelectObject(hdc, g_font_small);
        DrawTextW(hdc, L"Output devices and per-app volume", -1,
                  &(RECT){ S(20), S(31), embX - S(12), S(49) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, saved);
        PaintEmblemBlit(hdc, embX, embY, embSz);

        RECT div = { 0, S(60), rc.right, S(61) };
        FillRect(hdc, &div, ui_brush(COL_DIVIDER));

        /* Card backgrounds behind each row. The current output gets an accent
         * border so the default device reads at a glance instead of relying on
         * a text suffix alone. */
        for (int i = 0; i < g_ep_rows; i++) {
            FillRect(hdc, &g_ep_card[i], ui_brush(COL_CARD));
            FrameRect(hdc, &g_ep_card[i],
                      ui_brush(i == g_default_endpoint_idx ? COL_ACCENT : COL_BORDER));
        }
        for (int i = 0; i < g_ss_rows; i++) {
            FillRect(hdc, &g_ss_card[i], ui_brush(COL_CARD));
            FrameRect(hdc, &g_ss_card[i], ui_brush(COL_BORDER));
        }

        /* Section headers at the positions the layout actually used. */
        SetBkMode(hdc, TRANSPARENT);
        if (g_y_outputs_header > 0) {
            SetTextColor(hdc, COL_MUTED);
            saved = (HFONT)SelectObject(hdc, g_font_head);
            DrawTextW(hdc, L"OUTPUT DEVICES", -1,
                      &(RECT){ PANEL_X, g_y_outputs_header, PANEL_X + PANEL_W,
                               g_y_outputs_header + S(26) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, saved);
        }
        if (g_y_apps_header > 0) {
            SetTextColor(hdc, COL_MUTED);
            saved = (HFONT)SelectObject(hdc, g_font_head);
            DrawTextW(hdc, L"APP VOLUME", -1,
                      &(RECT){ PANEL_X, g_y_apps_header, PANEL_X + PANEL_W,
                               g_y_apps_header + S(26) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, saved);

            /* An empty app list must say why. A blank panel reads as a broken
             * app; "nothing is playing" and "enumeration failed with
             * 0x80070005" are two very different empty panels. */
            if (g_session_count == 0) {
                wchar_t why[256];
                if (FAILED(g_sessions_hr))
                    _snwprintf(why, 256,
                               L"Could not list running apps (0x%08lX). Run "
                               L"AudioSwitcher.exe --diag and send the report.",
                               (unsigned long)g_sessions_hr);
                else
                    wcscpy(why, L"No apps are playing audio right now. Start "
                                L"something and it appears here.");
                why[255] = L'\0';
                SetTextColor(hdc, COL_TELEMETRY);
                saved = (HFONT)SelectObject(hdc, g_font_ui);
                DrawTextW(hdc, why, -1,
                          &(RECT){ PANEL_X, g_y_apps_header + S(28),
                                   PANEL_X + PANEL_W, g_y_apps_header + S(72) },
                          DT_LEFT | DT_TOP | DT_WORDBREAK);
                SelectObject(hdc, saved);
            }
        }

        /* Footer status line: the honest report of what the last action did. */
        if (g_y_footer > 0) {
            RECT fband = { 0, g_y_footer - S(8), rc.right, rc.bottom };
            FillRect(hdc, &fband, ui_brush(COL_FOOTER_BG));
            RECT fdiv = { 0, g_y_footer - S(8), rc.right, g_y_footer - S(7) };
            FillRect(hdc, &fdiv, ui_brush(COL_DIVIDER));

            wchar_t status[320];
            ui_status_text(status, 320);

            COLORREF col = COL_MUTED;
            if (g_last_switch_idx >= 0 && FAILED(g_last_switch_hr)) col = COL_DANGER;
            else if (g_policy_variant == 0)                         col = COL_WARN;
            else if (g_last_switch_idx >= 0)                        col = COL_GOOD;

            /* state dot */
            int dy = g_y_footer + S(10);
            RECT dot = { PANEL_X, dy - S(4), PANEL_X + S(8), dy + S(4) };
            FillRect(hdc, &dot, ui_brush(col));

            SetTextColor(hdc, col);
            saved = (HFONT)SelectObject(hdc, g_font_small);
            DrawTextW(hdc, status, -1,
                      &(RECT){ PANEL_X + S(16), g_y_footer, PANEL_X + PANEL_W,
                               g_y_footer + S(22) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, saved);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

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

    for (int i = 0; i < g_session_count; i++) {
        char nm[512], key[512], exep[2048];
        w2u8(g_sessions[i].name, nm, sizeof(nm));
        w2u8(g_sessions[i].key, key, sizeof(key));
        w2u8(g_sessions[i].exe_path, exep, sizeof(exep));
        diag("  [%d] name=%s", i, nm);
        diag("      key=%s pid=%lu volume=%d%% mute=%d label_from_session=%d",
             key[0] ? key : "(none)", (unsigned long)g_sessions[i].process_id,
             (int)(g_sessions[i].volume * 100.0f + 0.5f),
             (int)g_sessions[i].mute, (int)g_sessions[i].named_from_session);
        diag("      exe=%s", exep[0] ? exep : "(unavailable)");
        if (!key[0])
            diag("      NOTE: no key -- this app's level cannot be remembered");
    }
    if (g_session_count == 0)
        diag("  WARNING: no sessions. Either nothing is playing audio, or");
    if (g_session_count == 0)
        diag("           GetSessionEnumerator/GetProcessId is still failing.");

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
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT_NAME);
            if (ev) { SetEvent(ev); CloseHandle(ev); }
        }
        CloseHandle(hMutex);
        return 0;
    }

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
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, S(720), S(620),
        NULL, NULL, hInst, NULL);

    /* Start hidden in tray - user opens from tray icon or hotkey */
    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
