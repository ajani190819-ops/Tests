/* Modern Windows.Devices.Geolocation, asynchronous on the foreground UI
   thread as RequestAccessAsync requires. No Win32 legacy Location API,
   tracking subscriptions, consentless fallback, subprocess, or IP lookup.
   WinRT entry points are loaded from System32 to preserve the suite PE contract.
   All objects and polling belong to the UI thread; no COM pointer crosses it. */
#include "location_abi.h"

static HMODULE g_locationDll;
static int g_locationRoInitialized;
static HRESULT (WINAPI *LocationRoInitialize)(int);
static void (WINAPI *LocationRoUninitialize)(void);
static HRESULT (WINAPI *LocationRoGetActivationFactory)(HSTRING, REFIID, void **);
static HRESULT (WINAPI *LocationRoActivateInstance)(HSTRING, IInspectable **);
static HRESULT (WINAPI *LocationWindowsCreateString)(PCNZWCH, UINT32, HSTRING *);
static HRESULT (WINAPI *LocationWindowsDeleteString)(HSTRING);
static LocAccessOp *g_locationAccess;
static LocPositionOp *g_locationPosition;
static IAsyncInfo *g_locationInfo;
static LocGeolocator *g_locationLocator;

static int LocationProviderError(HRESULT hr) {
    return hr == E_ACCESSDENIED ? LOCATION_PROVIDER_DENIED : LOCATION_PROVIDER_UNAVAILABLE;
}

static void LocationProviderClose(int cancel) {
    if (g_locationInfo) {
        if (cancel) (void)IAsyncInfo_Cancel(g_locationInfo);
        (void)IAsyncInfo_Close(g_locationInfo);
        IAsyncInfo_Release(g_locationInfo);
        g_locationInfo = NULL;
    }
    if (g_locationAccess) { g_locationAccess->lpVtbl->Release(g_locationAccess); g_locationAccess = NULL; }
    if (g_locationPosition) { g_locationPosition->lpVtbl->Release(g_locationPosition); g_locationPosition = NULL; }
    if (g_locationLocator) { g_locationLocator->lpVtbl->Release(g_locationLocator); g_locationLocator = NULL; }
}

static void LocationProviderShutdown(void) {
    LocationProviderClose(1);
    if (g_locationRoInitialized) { LocationRoUninitialize(); g_locationRoInitialized = 0; }
    /* Keep the system DLL loaded through process exit: a canceled Windows
       operation can still be retiring inside the runtime. No callbacks exist. */
}

static int LocationProviderInit(void) {
    if (g_locationRoInitialized) return 1;
    if (!g_locationDll) g_locationDll = LoadLibraryExW(L"combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_locationDll) return 0;
#define LOC_PROC(name) Location##name = (void *)GetProcAddress(g_locationDll, #name); if (!Location##name) return 0
    LOC_PROC(RoInitialize);
    LOC_PROC(RoUninitialize);
    LOC_PROC(RoGetActivationFactory);
    LOC_PROC(RoActivateInstance);
    LOC_PROC(WindowsCreateString);
    LOC_PROC(WindowsDeleteString);
#undef LOC_PROC
    HRESULT hr = LocationRoInitialize(0); /* RO_INIT_SINGLETHREADED, same UI thread */
    if (FAILED(hr)) return 0;
    g_locationRoInitialized = 1; /* balance S_OK and S_FALSE with RoUninitialize */
    return 1;
}

static HRESULT LocationClassName(HSTRING *name) {
    const wchar_t *text = L"Windows.Devices.Geolocation.Geolocator";
    return LocationWindowsCreateString(text, (UINT32)wcslen(text), name);
}

static int LocationProviderBeginAccess(void) {
    if (!LocationProviderInit()) return LOCATION_PROVIDER_UNAVAILABLE;
    HSTRING name = NULL;
    LocStatics *factory = NULL;
    HRESULT hr = LocationClassName(&name);
    if (SUCCEEDED(hr)) hr = LocationRoGetActivationFactory(name, &LOC_IID_STATICS, (void **)&factory);
    if (name) LocationWindowsDeleteString(name);
    if (SUCCEEDED(hr) && factory) hr = factory->lpVtbl->RequestAccessAsync(factory, &g_locationAccess);
    if (factory) factory->lpVtbl->Release(factory);
    if (SUCCEEDED(hr) && g_locationAccess)
        hr = g_locationAccess->lpVtbl->QueryInterface(g_locationAccess, &LOC_IID_ASYNCINFO, (void **)&g_locationInfo);
    return SUCCEEDED(hr) && g_locationInfo ? LOCATION_PROVIDER_PENDING : LocationProviderError(hr);
}

static int LocationProviderBeginFix(void) {
    HSTRING name = NULL;
    IInspectable *instance = NULL;
    HRESULT hr = LocationClassName(&name);
    if (SUCCEEDED(hr)) hr = LocationRoActivateInstance(name, &instance);
    if (name) LocationWindowsDeleteString(name);
    if (SUCCEEDED(hr) && instance)
        hr = IInspectable_QueryInterface(instance, &LOC_IID_GEOLOCATOR, (void **)&g_locationLocator);
    if (instance) IInspectable_Release(instance);
    if (SUCCEEDED(hr) && g_locationLocator) {
        hr = g_locationLocator->lpVtbl->put_DesiredAccuracy(g_locationLocator, 1); /* PositionAccuracy.High */
        LocTimeSpan maxAge = {60LL * 10000000LL}, timeout = {30LL * 10000000LL};
        if (SUCCEEDED(hr)) hr = g_locationLocator->lpVtbl->GetGeopositionAsyncWithAgeAndTimeout(
            g_locationLocator, maxAge, timeout, &g_locationPosition);
    }
    if (SUCCEEDED(hr) && g_locationPosition)
        hr = g_locationPosition->lpVtbl->QueryInterface(g_locationPosition, &LOC_IID_ASYNCINFO, (void **)&g_locationInfo);
    return SUCCEEDED(hr) && g_locationInfo ? LOCATION_PROVIDER_PENDING : LocationProviderError(hr);
}

static HRESULT LocationProviderRead(LocPosition *position, LocationSample *sample) {
    LocCoordinate *coordinate = NULL;
    LocCoordinatePoint *withPoint = NULL;
    LocPoint *point = NULL;
    LocBasicPosition basic = {NAN, NAN, NAN};
    LocDateTime stamp = {0};
    sample->lat = sample->lon = sample->accuracyM = NAN; sample->fixEpoch = 0;
    HRESULT hr = position->lpVtbl->get_Coordinate(position, &coordinate);
    if (SUCCEEDED(hr) && !coordinate) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = coordinate->lpVtbl->get_Accuracy(coordinate, &sample->accuracyM);
    if (SUCCEEDED(hr)) hr = coordinate->lpVtbl->get_Timestamp(coordinate, &stamp);
    if (SUCCEEDED(hr)) hr = coordinate->lpVtbl->QueryInterface(coordinate, &LOC_IID_COORDINATE_POINT, (void **)&withPoint);
    if (SUCCEEDED(hr) && !withPoint) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = withPoint->lpVtbl->get_Point(withPoint, &point);
    if (SUCCEEDED(hr) && !point) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = point->lpVtbl->get_Position(point, &basic);
    if (SUCCEEDED(hr)) {
        sample->lat = basic.Latitude; sample->lon = basic.Longitude;
        if (stamp.UniversalTime >= 116444736000000000LL)
            sample->fixEpoch = (uint64_t)(stamp.UniversalTime - 116444736000000000LL) / 10000000ULL;
    }
    if (point) point->lpVtbl->Release(point);
    if (withPoint) withPoint->lpVtbl->Release(withPoint);
    if (coordinate) coordinate->lpVtbl->Release(coordinate);
    return hr;
}

static int LocationProviderPoll(int access, LocationSample *sample) {
    if (!g_locationInfo) return LOCATION_PROVIDER_UNAVAILABLE;
    AsyncStatus status = Started;
    HRESULT hr = IAsyncInfo_get_Status(g_locationInfo, &status);
    if (FAILED(hr)) return LocationProviderError(hr);
    if (status == Started) return LOCATION_PROVIDER_PENDING;
    if (status == Canceled) return LOCATION_PROVIDER_CANCELED;
    if (status != Completed) {
        hr = E_FAIL;
        (void)IAsyncInfo_get_ErrorCode(g_locationInfo, &hr);
        return LocationProviderError(hr);
    }
    if (access) {
        int allowed = 0; /* GeolocationAccessStatus.Unspecified */
        hr = g_locationAccess->lpVtbl->GetResults(g_locationAccess, &allowed);
        if (FAILED(hr)) return LocationProviderError(hr);
        return allowed == 1 ? LOCATION_PROVIDER_READY : /* Allowed, not Unspecified/Denied */
               allowed == 2 ? LOCATION_PROVIDER_DENIED : LOCATION_PROVIDER_UNAVAILABLE;
    }
    LocPosition *position = NULL;
    hr = g_locationPosition->lpVtbl->GetResults(g_locationPosition, &position);
    if (SUCCEEDED(hr) && position) hr = LocationProviderRead(position, sample);
    else if (SUCCEEDED(hr)) hr = E_FAIL;
    if (position) position->lpVtbl->Release(position);
    return SUCCEEDED(hr) ? LOCATION_PROVIDER_READY : LocationProviderError(hr);
}
