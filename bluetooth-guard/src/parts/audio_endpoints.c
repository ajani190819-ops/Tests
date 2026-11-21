/* CoreAudio's adapter IMMDevice owns IKsControl. A topology object does not
   promise that interface. Never fall back to a friendly name: two pairs can
   share it, and a wrong-device Disconnect is worse than an unavailable action.
   See Microsoft: Using the IKsControl Interface to Access Audio Properties. */
static BOOL AudioIdHexDigit(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') ||
           (ch >= L'A' && ch <= L'F');
}

static BOOL AudioAdapterMatchesAddress(const wchar_t *id, ULONGLONG address) {
    if (!id || !address || address > 0xFFFFFFFFFFFFULL || !ContainsCI(id, L"BTH")) return FALSE;
    wchar_t hex[20];
    FormatCompactHex(address, hex, 20);
    size_t len = wcslen(id);
    for (size_t i = 0; i + 12 <= len; i++) {
        if (_wcsnicmp(id + i, hex, 12) == 0 &&
            (i == 0 || !AudioIdHexDigit(id[i - 1])) &&
            (i + 12 == len || !AudioIdHexDigit(id[i + 12]))) return TRUE;
    }
    return FALSE;
}

/* GetDeviceIdConnectedTo is the documented shortcut for GetConnectedTo ->
   IPart -> peer topology -> GetDeviceId, including unplugged audio endpoints.
   The returned adapter ID belongs to the caller (CoTaskMemFree). */
static HRESULT GetAudioAdapterId(IMMDevice *endpoint, LPWSTR *id) {
    *id = NULL;
    IDeviceTopology *topology = NULL;
    IConnector *connector = NULL;
    HRESULT hr = IMMDevice_Activate(endpoint, &kIidIDeviceTopo, CLSCTX_INPROC_SERVER,
                                    NULL, (void **)&topology);
    if (SUCCEEDED(hr) && topology) {
        /* An endpoint topology has one connector, to its adapter. */
        hr = IDeviceTopology_GetConnector(topology, 0, &connector);
        if (SUCCEEDED(hr) && connector)
            hr = IConnector_GetDeviceIdConnectedTo(connector, id);
        else if (SUCCEEDED(hr)) hr = E_NOINTERFACE;
    } else if (SUCCEEDED(hr)) hr = E_NOINTERFACE;
    if (connector) IConnector_Release(connector);
    if (topology) IDeviceTopology_Release(topology);
    return hr;
}

/* One enumeration serves all status observations. A nonzero commandAddress
   additionally sends a one-shot property to that address's audio adapter(s).
   A successful request is NOT a connected-state observation. */
static BtAudioResult ProbeBtAudioEndpoints(BtAudioObservation *observations, int observationCount,
                                           ULONGLONG commandAddress, ULONG propId) {
    BtAudioResult result = {S_OK, HRESULT_FROM_WIN32(ERROR_NOT_FOUND), FALSE, FALSE};
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *collection = NULL;
    result.enumerationHr = CoCreateInstance(&kClsidMMDevEnum, NULL, CLSCTX_INPROC_SERVER,
                                            &kIidIMMDevEnum, (void **)&enumerator);
    if (FAILED(result.enumerationHr) || !enumerator) {
        if (SUCCEEDED(result.enumerationHr)) result.enumerationHr = E_NOINTERFACE;
        result.commandHr = result.enumerationHr;
        return result;
    }
    result.enumerationHr = IMMDeviceEnumerator_EnumAudioEndpoints(
        enumerator, eAll, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection);
    if (SUCCEEDED(result.enumerationHr) && collection) {
        UINT count = 0;
        result.enumerationHr = IMMDeviceCollection_GetCount(collection, &count);
        if (FAILED(result.enumerationHr)) count = 0;
        for (UINT i = 0; i < count; i++) {
            IMMDevice *endpoint = NULL;
            HRESULT hr = IMMDeviceCollection_Item(collection, i, &endpoint);
            if (FAILED(hr) || !endpoint) {
                result.enumerationHr = FAILED(hr) ? hr : E_NOINTERFACE;
                if (endpoint) IMMDevice_Release(endpoint);
                continue; /* incomplete status, but another adapter may work */
            }
            DWORD state = 0;
            hr = IMMDevice_GetState(endpoint, &state);
            if (FAILED(hr)) result.enumerationHr = hr;
            LPWSTR adapterId = NULL;
            if (SUCCEEDED(hr) && (state & (DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED)) &&
                SUCCEEDED(GetAudioAdapterId(endpoint, &adapterId)) && adapterId) {
                for (int j = 0; j < observationCount; j++) {
                    if (!AudioAdapterMatchesAddress(adapterId, observations[j].address)) continue;
                    observations[j].found = TRUE;
                    if (state & DEVICE_STATE_ACTIVE) observations[j].active = TRUE;
                }
                if (commandAddress && AudioAdapterMatchesAddress(adapterId, commandAddress)) {
                    IMMDevice *adapter = NULL;
                    IKsControl *ks = NULL;
                    result.matched = TRUE;
                    hr = IMMDeviceEnumerator_GetDevice(enumerator, adapterId, &adapter);
                    if (SUCCEEDED(hr) && adapter)
                        hr = IMMDevice_Activate(adapter, &kIidIKsControl, CLSCTX_INPROC_SERVER,
                                                NULL, (void **)&ks);
                    else if (SUCCEEDED(hr)) hr = E_NOINTERFACE;
                    if (SUCCEEDED(hr) && ks) {
                        KSPROPERTY property;
                        ZeroMemory(&property, sizeof(property));
                        property.Set = kKsPropSetBtAudio;
                        property.Id = propId;
                        property.Flags = KSPROPERTY_TYPE_GET;
                        ULONG returned = 0;
                        hr = IKsControl_KsProperty(ks, &property, sizeof(property), NULL, 0, &returned);
                        if (SUCCEEDED(hr)) result.requested = TRUE;
                    } else if (SUCCEEDED(hr)) hr = E_NOINTERFACE;
                    if (!result.requested) result.commandHr = hr;
                    if (ks) IKsControl_Release(ks);
                    if (adapter) IMMDevice_Release(adapter);
                }
            }
            if (adapterId) CoTaskMemFree(adapterId);
            IMMDevice_Release(endpoint);
        }
    } else if (SUCCEEDED(result.enumerationHr)) result.enumerationHr = E_NOINTERFACE;
    if (collection) IMMDeviceCollection_Release(collection);
    IMMDeviceEnumerator_Release(enumerator);
    if (result.requested) result.commandHr = S_OK;
    else if (FAILED(result.enumerationHr)) result.commandHr = result.enumerationHr;
    return result;
}

static BtAudioResult TriggerBtAudioKsProperty(ULONGLONG address, const wchar_t *devName, ULONG propId) {
    (void)devName; /* display-only; routing is strictly by Bluetooth address */
    return ProbeBtAudioEndpoints(NULL, 0, address, propId);
}

/* Worker-only, at most once a second, never from paint or under g_lock during
   COM calls. Re-resolve addresses when publishing: discovery can compact rows. */
static void RefreshAudioConnections(void) {
    static ULONGLONG nextPoll = 0;
    ULONGLONG now = GetTickCount64();
    if (now < nextPoll) return;
    nextPoll = now + 1000ULL;
    BtAudioObservation observations[MAX_DEVICES];
    ZeroMemory(observations, sizeof(observations));
    EnterCriticalSection(&g_lock);
    int count = g_devCount;
    for (int i = 0; i < count; i++) observations[i].address = g_devices[i].address;
    LeaveCriticalSection(&g_lock);
    if (!count) return;
    BtAudioResult result = ProbeBtAudioEndpoints(observations, count, 0, 0);
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < count; i++) {
        int idx = FindDeviceIndexByAddress(observations[i].address);
        if (idx < 0) continue;
        DeviceEntry *d = &g_devices[idx];
        d->audioStatusValid = SUCCEEDED(result.enumerationHr);
        d->audioEndpointFound = d->audioStatusValid && observations[i].found;
        d->audioConnected = d->audioStatusValid && observations[i].active;
    }
    LeaveCriticalSection(&g_lock);
}
