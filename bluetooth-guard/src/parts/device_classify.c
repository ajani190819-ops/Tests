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

