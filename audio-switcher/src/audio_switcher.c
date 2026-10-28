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

typedef struct IMMDeviceVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetId)(void* This, LPWSTR* ppstrId);
    HRESULT (__stdcall *Activate)(void* This, const IID* iid, DWORD dwClsCtx, void* pActivationParams, void** ppInterface);
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

typedef struct IAudioEndpointVolumeVtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *SetMasterVolumeLevelScalar)(void* This, float fLevel, const GUID* pguidEventContext);
    HRESULT (__stdcall *GetMasterVolumeLevelScalar)(void* This, float* pfLevel);
    HRESULT (__stdcall *SetMute)(void* This, BOOL bMute, const GUID* pguidEventContext);
    HRESULT (__stdcall *GetMute)(void* This, BOOL* pbMute);
} IAudioEndpointVolumeVtbl;
typedef struct IAudioEndpointVolume { const struct IAudioEndpointVolumeVtbl* lpVtbl; } IAudioEndpointVolume;

typedef struct IAudioSessionManager2Vtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetSessionEnumerator)(void* This, void** SessionEnumerator);
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

typedef struct IAudioSessionControl2Vtbl {
    HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
    ULONG   (__stdcall *AddRef)(void* This);
    ULONG   (__stdcall *Release)(void* This);
    HRESULT (__stdcall *GetProcessId)(void* This, DWORD* ProcessId);
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
typedef struct ComBase { const void* lpVtbl; } ComBase;

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
    wchar_t      name[256];
    DWORD        process_id;
    float        volume;           /* 0.0 .. 1.0 */
    BOOL         mute;
    ISimpleAudioVolume *svol;
} AudioSession;

static AudioEndpoint   g_endpoints[32];
static int             g_endpoint_count = 0;
static int             g_default_endpoint_idx = -1;
static AudioSession    g_sessions[128];
static int             g_session_count = 0;

static IMMDeviceEnumerator *g_enum = NULL;
static IMMDevice           *g_default_device = NULL;
static IAudioSessionManager2 *g_session_mgr = NULL;

static HRESULT com_create_enumerator(void) {
    HRESULT hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        &IID_IMMDeviceEnumerator, (void**)&g_enum);
    return hr;
}

static void com_release_enumerator(void) {
    if (g_enum) { CALL_VTBL0(g_enum, Release); g_enum = NULL; }
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

    g_endpoint_count = 0;
    g_default_endpoint_idx = -1;

    GUID fmtid = { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } };

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

        /* Friendly name via IPropertyStore */
        IPropertyStore *ps = NULL;
        hr = CALL_VTBL4(dev, Activate, &IID_IPropertyStore, 0, NULL, (void**)&ps);
        if (SUCCEEDED(hr) && ps) {
            typedef HRESULT (__stdcall *GetValueFn)(void*, const GUID*, PROPVARIANT*);
            GetValueFn getval = (GetValueFn)((void**)ps->lpVtbl)[5];
            PROPVARIANT pv = {0};
            getval(ps, &fmtid, &pv);
            if (pv.vt == VT_LPWSTR && pv.pwszVal) {
                wcsncpy(g_endpoints[g_endpoint_count].name, pv.pwszVal, 255);
                g_endpoints[g_endpoint_count].name[255] = L'\0';
            }
            PropVariantClear(&pv);
            CALL_VTBL0(ps, Release);
        } else {
            wcscpy(g_endpoints[g_endpoint_count].name, L"Audio Output");
        }

        g_endpoints[g_endpoint_count].device = dev;
        g_endpoints[g_endpoint_count].vol = NULL;
        g_endpoints[g_endpoint_count].is_default = FALSE;
        g_endpoint_count++;

        IAudioEndpointVolume *ev = NULL;
        hr = CALL_VTBL4(dev, Activate, &IID_IAudioEndpointVolume, 0, NULL, (void**)&ev);
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

static HRESULT sessions_refresh(void) {
    IMMDeviceEnumerator *e = g_enum;
    if (!e) return E_FAIL;

    IMMDevice *dev = NULL;
    HRESULT hr = CALL_VTBL3(e, GetDefaultAudioEndpoint, eRender, eConsole, (void**)&dev);
    if (FAILED(hr)) return hr;

    IAudioSessionManager2 *mgr = NULL;
    hr = CALL_VTBL4(dev, Activate, &IID_IAudioSessionManager2, 0, NULL, (void**)&mgr);
    CALL_VTBL0(dev, Release);
    if (FAILED(hr)) return hr;

    if (g_session_mgr) { CALL_VTBL0(g_session_mgr, Release); }
    g_session_mgr = mgr;

    IAudioSessionEnumerator *enum_sessions = NULL;
    hr = CALL_VTBL1(mgr, GetSessionEnumerator, (void**)&enum_sessions);
    if (FAILED(hr)) {
        CALL_VTBL0(mgr, Release);
        g_session_mgr = NULL;
        return hr;
    }

    int count = 0;
    typedef struct IAudioSessionEnumeratorVtbl2 {
        HRESULT (__stdcall *QueryInterface)(void* This, const IID* riid, void** ppvObject);
        ULONG   (__stdcall *AddRef)(void* This);
        ULONG   (__stdcall *Release)(void* This);
        HRESULT (__stdcall *GetCount)(void* This, int* SessionCount);
        HRESULT (__stdcall *GetSession)(void* This, int SessionIndex, void** Session);
    } IAudioSessionEnumeratorVtbl2;
    IAudioSessionEnumeratorVtbl2 *ev2 = (IAudioSessionEnumeratorVtbl2*)enum_sessions->lpVtbl;
    ev2->GetCount(enum_sessions, &count);

    g_session_count = 0;
    for (int i = 0; i < count && g_session_count < 128; i++) {
        void *session = NULL;
        ev2->GetSession(enum_sessions, i, &session);
        if (!session) continue;

        void *iface2 = NULL;
        typedef HRESULT (__stdcall *QIFn)(void*, REFIID, void**);
        QIFn qifn = (QIFn)((void**)((ComBase*)session)->lpVtbl)[0];
        hr = qifn(session, &IID_IAudioSessionControl2, &iface2);
        if (FAILED(hr)) {
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }
        IAudioSessionControl2 *ctl2 = (IAudioSessionControl2*)iface2;

        DWORD pid = 0;
        typedef HRESULT (__stdcall *GetPidFn)(void*, DWORD*);
        typedef HRESULT (__stdcall *GetPidFn)(void*, DWORD*);
        GetPidFn getpid = (GetPidFn)ctl2->lpVtbl->GetProcessId;
        getpid(ctl2, &pid);

        if (pid == 0) {
            CALL_VTBL0(ctl2, Release);
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }

        wchar_t proc_name[256] = L"";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            DWORD size = 256;
            QueryFullProcessImageNameW(hProc, 0, proc_name, &size);
            const wchar_t *slash = wcsrchr(proc_name, L'\\');
            if (slash) wcscpy(proc_name, slash + 1);
            CloseHandle(hProc);
        }

        ISimpleAudioVolume *svol = NULL;
        void *svol_iface = NULL;
        qifn(ctl2, &IID_ISimpleAudioVolume, &svol_iface);
        svol = (ISimpleAudioVolume*)svol_iface;

        float vol = 0.0f;
        BOOL mute = FALSE;
        if (svol) {
            typedef HRESULT (__stdcall *GetVolFn)(void*, float*);
            GetVolFn getvol = (GetVolFn)((void**)svol->lpVtbl)[4];
            getvol(svol, &vol);
            typedef HRESULT (__stdcall *GetMuteFn)(void*, BOOL*);
            GetMuteFn getmute = (GetMuteFn)((void**)svol->lpVtbl)[6];
            getmute(svol, &mute);
        }

        wcsncpy(g_sessions[g_session_count].name, proc_name, 255);
        g_sessions[g_session_count].name[255] = L'\0';
        g_sessions[g_session_count].process_id = pid;
        g_sessions[g_session_count].volume = vol;
        g_sessions[g_session_count].mute = mute;
        g_sessions[g_session_count].svol = svol;
        g_session_count++;

        CALL_VTBL0(ctl2, Release);
        CALL_VTBL0((IUnknown*)session, Release);
    }

    ReleaseFn rel2 = (ReleaseFn)((void**)enum_sessions->lpVtbl)[2];
    rel2(enum_sessions);

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

static void draw_suite_emblem(HDC hdc, int x, int y, int size) {
    HPEN hPen = CreatePen(PS_SOLID, 2, COL_ACCENT);
    HBRUSH hBrush = CreateSolidBrush(COL_BG);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    SetBkMode(hdc, TRANSPARENT);

    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2;

    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    for (int d = 0; d < 3; d++) {
        double rad = (-90.0 + d * 120.0) * 3.14159265 / 180.0;
        int x2 = cx + (int)(cos(rad) * r * 0.85);
        int y2 = cy + (int)(sin(rad) * r * 0.85);
        MoveToEx(hdc, cx, cy, NULL);
        LineTo(hdc, x2, y2);
    }

    Ellipse(hdc, cx - r/4, cy - r/4, cx + r/4, cy + r/4);

    POINT tri[3] = {
        { cx, cy - r/5 },
        { cx + r/6, cy + r/8 },
        { cx - r/6, cy + r/8 }
    };
    SelectObject(hdc, GetStockBrush(DC_BRUSH));
    SetDCBrushColor(hdc, COL_BG);
    Polygon(hdc, tri, 3);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
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

static void refresh_ui(void) {
    if (!g_hwnd) return;

    for (int i = 0; i < 32; i++) {
        if (g_endpoint_ctrls[i].hwnd_btn) DestroyWindow(g_endpoint_ctrls[i].hwnd_btn);
        if (g_endpoint_ctrls[i].hwnd_vol) DestroyWindow(g_endpoint_ctrls[i].hwnd_vol);
        if (g_endpoint_ctrls[i].hwnd_mute) DestroyWindow(g_endpoint_ctrls[i].hwnd_mute);
        if (g_endpoint_ctrls[i].hwnd_label) DestroyWindow(g_endpoint_ctrls[i].hwnd_label);
        if (g_endpoint_ctrls[i].hwnd_voltxt) DestroyWindow(g_endpoint_ctrls[i].hwnd_voltxt);
        memset(&g_endpoint_ctrls[i], 0, sizeof(g_endpoint_ctrls[i]));
    }
    for (int i = 0; i < 128; i++) {
        if (g_session_ctrls[i].hwnd_label) DestroyWindow(g_session_ctrls[i].hwnd_label);
        if (g_session_ctrls[i].hwnd_vol) DestroyWindow(g_session_ctrls[i].hwnd_vol);
        if (g_session_ctrls[i].hwnd_mute) DestroyWindow(g_session_ctrls[i].hwnd_mute);
        if (g_session_ctrls[i].hwnd_voltxt) DestroyWindow(g_session_ctrls[i].hwnd_voltxt);
        memset(&g_session_ctrls[i], 0, sizeof(g_session_ctrls[i]));
    }

    int y = PANEL_Y;

    /* Endpoints section */
    {
        HDC hdc = GetDC(g_hwnd);
        SetBkColor(hdc, COL_CARD);
        SetTextColor(hdc, COL_TEXT);
        DrawTextW(hdc, L"Audio Outputs", -1, &(RECT){PANEL_X, y, PANEL_X + PANEL_W, y + S(28)}, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ReleaseDC(g_hwnd, hdc);
    }
    y += S(34);

    for (int i = 0; i < g_endpoint_count; i++) {
        int row_h = S(52);
        g_endpoint_ctrls[i].hwnd_label = CreateWindowExW(
            0, L"STATIC", g_endpoints[i].name,
            WS_CHILD | WS_VISIBLE,
            PANEL_X, y, PANEL_W - S(160), S(24),
            g_hwnd, NULL, g_hInst, NULL);

        g_endpoint_ctrls[i].hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
            PANEL_X, y + S(28), PANEL_W - S(260), S(20),
            g_hwnd, (HMENU)(INT_PTR)(1000 + i), g_hInst, NULL);
        SendMessageW(g_endpoint_ctrls[i].hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        int vol_pct = (int)(endpoint_get_volume(i) * 100.0f + 0.5f);
        SendMessageW(g_endpoint_ctrls[i].hwnd_vol, TBM_SETPOS, TRUE, vol_pct);

        g_endpoint_ctrls[i].hwnd_mute = CreateWindowExW(
            0, L"BUTTON", endpoint_get_mute(i) ? L"Unmute" : L"Mute",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            PANEL_W - S(200), y + S(26), S(80), S(24),
            g_hwnd, (HMENU)(INT_PTR)(2000 + i), g_hInst, NULL);

        g_endpoint_ctrls[i].hwnd_btn = CreateWindowExW(
            0, L"BUTTON", (i == g_default_endpoint_idx) ? L"Default" : L"Set Default",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            PANEL_W - S(110), y + S(26), S(96), S(24),
            g_hwnd, (HMENU)(INT_PTR)(3000 + i), g_hInst, NULL);

        wchar_t vtext[32];
        swprintf(vtext, 32, L"%d%%", vol_pct);
        g_endpoint_ctrls[i].hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", vtext,
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            PANEL_W - S(170), y + S(2), S(50), S(20),
            g_hwnd, NULL, g_hInst, NULL);

        g_endpoint_ctrls[i].endpoint_idx = i;
        y += row_h;
    }

    y += S(16);

    /* Sessions section */
    {
        HDC hdc = GetDC(g_hwnd);
        SetBkColor(hdc, COL_CARD);
        SetTextColor(hdc, COL_TEXT);
        DrawTextW(hdc, L"App Volume", -1, &(RECT){PANEL_X, y, PANEL_X + PANEL_W, y + S(28)}, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ReleaseDC(g_hwnd, hdc);
    }
    y += S(34);

    for (int i = 0; i < g_session_count; i++) {
        int row_h = S(44);
        g_session_ctrls[i].hwnd_label = CreateWindowExW(
            0, L"STATIC", g_sessions[i].name,
            WS_CHILD | WS_VISIBLE,
            PANEL_X, y, PANEL_W - S(260), S(20),
            g_hwnd, NULL, g_hInst, NULL);

        g_session_ctrls[i].hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
            PANEL_X, y + S(22), PANEL_W - S(320), S(20),
            g_hwnd, (HMENU)(INT_PTR)(4000 + i), g_hInst, NULL);
        SendMessageW(g_session_ctrls[i].hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        int vol_pct = (int)(g_sessions[i].volume * 100.0f + 0.5f);
        SendMessageW(g_session_ctrls[i].hwnd_vol, TBM_SETPOS, TRUE, vol_pct);

        g_session_ctrls[i].hwnd_mute = CreateWindowExW(
            0, L"BUTTON", g_sessions[i].mute ? L"Unmute" : L"Mute",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            PANEL_W - S(200), y + S(20), S(80), S(22),
            g_hwnd, (HMENU)(INT_PTR)(5000 + i), g_hInst, NULL);

        wchar_t vtext[32];
        swprintf(vtext, 32, L"%d%%", vol_pct);
        g_session_ctrls[i].hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", vtext,
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            PANEL_W - S(170), y + S(2), S(50), S(18),
            g_hwnd, NULL, g_hInst, NULL);

        g_session_ctrls[i].session_idx = i;
        y += row_h;
    }

    InvalidateRect(g_hwnd, NULL, FALSE);
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

        g_btn_refresh = CreateWindowExW(
            0, L"BUTTON", L"\u21BB Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            PANEL_X, PANEL_Y - S(36), S(96), S(28),
            hwnd, (HMENU)IDM_REFRESH, g_hInst, NULL);

        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        com_create_enumerator();
        endpoints_refresh();
        sessions_refresh();

        SetTimer(hwnd, 1, 2000, NULL);
        return 0;
    }

    case WM_DESTROY:
        g_quitting = TRUE;
        KillTimer(hwnd, 1);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        sessions_cleanup();
        endpoints_cleanup();
        com_release_enumerator();
        CoUninitialize();
        PostQuitMessage(0);
        return 0;

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
            g_default_endpoint_idx = idx;
            for (int i = 0; i < g_endpoint_count; i++)
                g_endpoints[i].is_default = (i == idx);
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
            session_set_mute(idx, !g_sessions[idx].mute);
            refresh_ui();
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
            sessions_refresh();
            refresh_ui();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, CreateSolidBrush(COL_BG));

        RECT hdr = { 0, 0, rc.right, S(60) };
        FillRect(hdc, &hdr, CreateSolidBrush(COL_FOOTER_BG));
        SetBkColor(hdc, COL_FOOTER_BG);
        SetTextColor(hdc, COL_TEXT);
        DrawTextW(hdc, APP_TITLE, -1, &(RECT){ S(20), S(8), rc.right, S(32) }, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_suite_emblem(hdc, rc.right - S(56), S(6), S(48));

        RECT div = { 0, S(60), rc.right, S(62) };
        FillRect(hdc, &div, CreateSolidBrush(COL_DIVIDER));

        wchar_t status[256];
        swprintf(status, 256, L"%d output(s)  \u00B7  %d app(s)",
                 g_endpoint_count, g_session_count);
        SetTextColor(hdc, COL_MUTED);
        DrawTextW(hdc, status, -1, &(RECT){ PANEL_X, S(68), PANEL_W, S(88) }, DT_LEFT | DT_TOP | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)nShow;

    g_silent = (cmdLine && (wcsstr(cmdLine, L"--silent") != NULL || wcsstr(cmdLine, L"-Silent") != NULL));

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

    if (!g_silent) ShowWindow(hwnd, SW_SHOWNORMAL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
