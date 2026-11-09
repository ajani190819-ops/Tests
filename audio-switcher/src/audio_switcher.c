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
#define WM_ASW_SHOW         (WM_APP + 2)   /* a second launch asks us to show */

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


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so audio_switcher.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/vtable_asserts.c"
#include "parts/audio_subsystem.c"
#include "parts/remembered_levels.c"
#include "parts/window_ui_tray.c"
#include "parts/control_theming.c"
#include "parts/ui_reconcile.c"
#include "parts/diag.c"
