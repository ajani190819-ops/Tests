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
    BOOL         on_other_device;    /* found on an output that is not current */
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
/* One manager per device the sweep touched, all kept alive: the sessions'
   ISimpleAudioVolume pointers belong to them. */
static IAudioSessionManager2 *g_session_mgrs[32];
static int                  g_session_mgr_count = 0;
static BOOL                 g_session_sweep_used = FALSE;
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

