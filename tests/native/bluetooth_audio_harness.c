/* Executes production CoreAudio traversal and publication with COM I/O mocked.
   This proves routing, activation and ownership, not a Windows driver response. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef int BOOL;
typedef uint32_t DWORD;
typedef uint32_t ULONG;
typedef unsigned UINT;
typedef int32_t HRESULT;
typedef unsigned long long ULONGLONG;
typedef wchar_t *LPWSTR;
typedef struct { uint32_t a; uint16_t b, c; unsigned char d[8]; } GUID;
typedef GUID IID;
typedef GUID CLSID;
typedef struct { GUID Set; ULONG Id, Flags; } KSPROPERTY;
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define E_FAIL ((HRESULT)0x80004005u)
#define E_NOINTERFACE ((HRESULT)0x80004002u)
#define ERROR_NOT_FOUND 1168
#define HRESULT_FROM_WIN32(e) ((HRESULT)(0x80070000u | (e)))
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define CLSCTX_INPROC_SERVER 1
#define DEVICE_STATE_ACTIVE 1
#define DEVICE_STATE_DISABLED 2
#define DEVICE_STATE_NOTPRESENT 4
#define DEVICE_STATE_UNPLUGGED 8
#define KSPROPERTY_TYPE_GET 1
#define KSPROPERTY_TYPE_SET 2
#define KSPROPERTY_ONESHOT_RECONNECT 0
#define KSPROPERTY_ONESHOT_DISCONNECT 1
#define eAll 2
#define MAX_DEVICES 64
#define ZeroMemory(p, n) memset(p, 0, n)
#define _snwprintf swprintf
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "C line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
#include "bluetooth_audio_types.h"

static DeviceEntry g_devices[MAX_DEVICES];
static int g_devCount, g_lock;
static ULONGLONG tick = 10000;
static int lockDepth, creates, ksActivations, commands, allocations, frees;
static int failure, failureIndex = -1, reorderDevices, includeAllStates;
static ULONG expectedProperty;
enum { NO_FAILURE, FAIL_CREATE, FAIL_ENUM, FAIL_COUNT, FAIL_ITEM, FAIL_STATE,
       FAIL_TOPOLOGY, FAIL_CONNECTOR, FAIL_ID, FAIL_GET_DEVICE, FAIL_KS, FAIL_PROPERTY };
enum { OBJ_ENUM, OBJ_COLLECTION, OBJ_ENDPOINT, OBJ_ADAPTER, OBJ_TOPOLOGY, OBJ_CONNECTOR, OBJ_KS };
typedef struct { int kind, index, refs; } FakeObject;
typedef FakeObject IMMDevice;
typedef FakeObject IMMDeviceEnumerator;
typedef FakeObject IMMDeviceCollection;
typedef FakeObject IDeviceTopology;
typedef FakeObject IConnector;
typedef FakeObject IKsControl;
static FakeObject enumerator, collection, endpoints[8], adapters[8], topologies[8], connectors[8], ks[8];
static const wchar_t *adapterIds[8];
static DWORD states[8];
static int endpointCount, indices[8], collectionCount;
static int _wcsnicmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        wint_t x = towlower(a[i]), y = towlower(b[i]);
        if (x != y) return x < y ? -1 : 1;
        if (!a[i] || !b[i]) return 0;
    }
    return 0;
}
static ULONGLONG GetTickCount64(void) { return tick; }
static void EnterCriticalSection(int *lock) { (void)lock; CHECK(lockDepth == 0); lockDepth++; }
static void LeaveCriticalSection(int *lock) { (void)lock; CHECK(lockDepth == 1); lockDepth--; }
static int FindDeviceIndexByAddress(ULONGLONG address) {
    for (int i = 0; i < g_devCount; i++) if (g_devices[i].address == address) return i;
    return -1;
}
static void *Acquire(FakeObject *object) { object->refs++; return object; }
static void Release(FakeObject *object) { CHECK(object && object->refs > 0); object->refs--; }
#define IMMDeviceEnumerator_Release Release
#define IMMDeviceCollection_Release Release
#define IMMDevice_Release Release
#define IDeviceTopology_Release Release
#define IConnector_Release Release
#define IKsControl_Release Release
static HRESULT CoCreateInstance(const CLSID *cls, void *outer, int context, const IID *iid, void **out) {
    CHECK(!lockDepth && !outer && context == CLSCTX_INPROC_SERVER);
    CHECK(memcmp(cls, &kClsidMMDevEnum, sizeof(*cls)) == 0);
    CHECK(memcmp(iid, &kIidIMMDevEnum, sizeof(*iid)) == 0);
    creates++; *out = NULL;
    if (failure == FAIL_CREATE) return E_FAIL;
    if (reorderDevices && g_devCount == 2) {
        DeviceEntry tmp = g_devices[0]; g_devices[0] = g_devices[1]; g_devices[1] = tmp;
        reorderDevices = FALSE;
    }
    *out = Acquire(&enumerator); return S_OK;
}
static HRESULT IMMDeviceEnumerator_EnumAudioEndpoints(IMMDeviceEnumerator *e, int flow, DWORD mask,
                                                       IMMDeviceCollection **out) {
    CHECK(e == &enumerator && flow == eAll);
    CHECK(mask == (DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED));
    *out = NULL;
    if (failure == FAIL_ENUM) return E_FAIL;
    collectionCount = 0;
    for (int i = 0; i < endpointCount; i++) {
        if (includeAllStates || (states[i] & mask)) indices[collectionCount++] = i;
    }
    *out = Acquire(&collection); return S_OK;
}
static HRESULT IMMDeviceCollection_GetCount(IMMDeviceCollection *c, UINT *count) {
    CHECK(c == &collection);
    if (failure == FAIL_COUNT) return E_FAIL;
    *count = (UINT)collectionCount; return S_OK;
}
static HRESULT IMMDeviceCollection_Item(IMMDeviceCollection *c, UINT index, IMMDevice **out) {
    CHECK(c == &collection && index < (UINT)collectionCount);
    *out = NULL;
    if (failure == FAIL_ITEM) return E_FAIL;
    *out = Acquire(&endpoints[indices[index]]); return S_OK;
}
static HRESULT IMMDevice_GetState(IMMDevice *e, DWORD *state) {
    CHECK(e->kind == OBJ_ENDPOINT);
    if (failure == FAIL_STATE && (failureIndex < 0 || failureIndex == e->index)) return E_FAIL;
    *state = states[e->index]; return S_OK;
}
static HRESULT IMMDevice_Activate(IMMDevice *device, const IID *iid, int context, void *params, void **out) {
    CHECK(!lockDepth && context == CLSCTX_INPROC_SERVER && !params);
    *out = NULL;
    if (device->kind == OBJ_ENDPOINT) {
        CHECK(memcmp(iid, &kIidIDeviceTopo, sizeof(*iid)) == 0);
        if (failure == FAIL_TOPOLOGY) return E_FAIL;
        *out = Acquire(&topologies[device->index]);
    } else {
        /* IKsControl must be ACTIVATED on the adapter IMMDevice, not obtained
           from topology QueryInterface or endpoint Activate. */
        CHECK(device->kind == OBJ_ADAPTER);
        CHECK(memcmp(iid, &kIidIKsControl, sizeof(*iid)) == 0);
        ksActivations++;
        if (failure == FAIL_KS) return E_NOINTERFACE;
        *out = Acquire(&ks[device->index]);
    }
    return S_OK;
}
static HRESULT IDeviceTopology_GetConnector(IDeviceTopology *t, UINT index, IConnector **out) {
    CHECK(t->kind == OBJ_TOPOLOGY && index == 0);
    *out = NULL;
    if (failure == FAIL_CONNECTOR) return E_FAIL;
    *out = Acquire(&connectors[t->index]); return S_OK;
}
static HRESULT IConnector_GetDeviceIdConnectedTo(IConnector *c, LPWSTR *out) {
    CHECK(c->kind == OBJ_CONNECTOR); *out = NULL;
    if (failure == FAIL_ID) return E_FAIL;
    size_t bytes = (wcslen(adapterIds[c->index]) + 1) * sizeof(wchar_t);
    *out = malloc(bytes); CHECK(*out); memcpy(*out, adapterIds[c->index], bytes);
    allocations++; return S_OK;
}
static void CoTaskMemFree(void *p) { CHECK(p); frees++; free(p); }
static HRESULT IMMDeviceEnumerator_GetDevice(IMMDeviceEnumerator *e, const wchar_t *id, IMMDevice **out) {
    CHECK(e == &enumerator); *out = NULL;
    if (failure == FAIL_GET_DEVICE) return E_FAIL;
    for (int i = 0; i < endpointCount; i++) {
        if (wcscmp(adapterIds[i], id) == 0) { *out = Acquire(&adapters[i]); return S_OK; }
    }
    CHECK(FALSE); return E_FAIL;
}
static HRESULT IKsControl_KsProperty(IKsControl *control, KSPROPERTY *property, ULONG size,
                                     void *data, ULONG dataSize, ULONG *returned) {
    CHECK(control->kind == OBJ_KS && size == sizeof(*property));
    CHECK(memcmp(&property->Set, &kKsPropSetBtAudio, sizeof(GUID)) == 0);
    CHECK(property->Id == expectedProperty && property->Flags == KSPROPERTY_TYPE_GET);
    CHECK(!data && !dataSize); *returned = 0; commands++;
    return failure == FAIL_PROPERTY ? E_FAIL : S_OK;
}
#include "bluetooth_audio_production.h"

static void NoLeaks(void) {
    CHECK(allocations == frees && !enumerator.refs && !collection.refs && !lockDepth);
    for (int i = 0; i < 8; i++) {
        CHECK(!endpoints[i].refs && !adapters[i].refs && !topologies[i].refs && !connectors[i].refs && !ks[i].refs);
    }
}
static void Reset(void) {
    NoLeaks();
    memset(g_devices, 0, sizeof(g_devices)); g_devCount = 0;
    creates = ksActivations = commands = allocations = frees = failure = includeAllStates = 0;
    failureIndex = -1; reorderDevices = FALSE;
    enumerator = (FakeObject){OBJ_ENUM, 0, 0}; collection = (FakeObject){OBJ_COLLECTION, 0, 0};
    for (int i = 0; i < 8; i++) {
        endpoints[i] = (FakeObject){OBJ_ENDPOINT, i, 0}; adapters[i] = (FakeObject){OBJ_ADAPTER, i, 0};
        topologies[i] = (FakeObject){OBJ_TOPOLOGY, i, 0}; connectors[i] = (FakeObject){OBJ_CONNECTOR, i, 0};
        ks[i] = (FakeObject){OBJ_KS, i, 0}; states[i] = DEVICE_STATE_UNPLUGGED;
        adapterIds[i] = L"\\\\?\\BTHENUM#DEV_AABBCCDDEEFF#adapter";
    }
    endpointCount = 1; expectedProperty = KSPROPERTY_ONESHOT_RECONNECT;
}
static void Routing(void) {
    Reset();
    BtAudioResult r = TriggerBtAudioKsProperty(0xAABBCCDDEEFFULL, L"Earbuds", KSPROPERTY_ONESHOT_RECONNECT);
    CHECK(r.matched && r.requested && r.commandHr == S_OK && commands == 1 && ksActivations == 1); NoLeaks();
    /* A display name, zero address, substring of a longer identifier, or a
       non-Bluetooth identifier can never select an adapter. */
    Reset(); r = TriggerBtAudioKsProperty(0x112233445566ULL, L"Earbuds", 0);
    CHECK(!r.matched && !r.requested && !commands); NoLeaks();
    CHECK(!AudioAdapterMatchesAddress(L"BTHENUM#DEV_0AABBCCDDEEFF#adapter", 0xAABBCCDDEEFFULL));
    CHECK(!AudioAdapterMatchesAddress(L"BTHENUM#DEV_AABBCCDDEEFF0#adapter", 0xAABBCCDDEEFFULL));
    CHECK(!AudioAdapterMatchesAddress(L"USB#DEV_AABBCCDDEEFF#adapter", 0xAABBCCDDEEFFULL));
    CHECK(!AudioAdapterMatchesAddress(adapterIds[0], 0));
    CHECK(!AudioAdapterMatchesAddress(adapterIds[0], 0xAABBCCDDEEFF0ULL));
    CHECK(AudioAdapterMatchesAddress(L"bthenum#dev_aabbccddeeff#adapter", 0xAABBCCDDEEFFULL));
    /* Both directions use precisely the same addressed adapter path. */
    Reset(); expectedProperty = KSPROPERTY_ONESHOT_DISCONNECT;
    r = TriggerBtAudioKsProperty(0xAABBCCDDEEFFULL, L"Same friendly name", KSPROPERTY_ONESHOT_DISCONNECT);
    CHECK(r.requested && commands == 1); NoLeaks();
}
static void State(void) {
    Reset(); endpointCount = 3;
    states[0] = DEVICE_STATE_UNPLUGGED; states[1] = DEVICE_STATE_ACTIVE;
    adapterIds[2] = L"BTHENUM#DEV_112233445566#adapter";
    BtAudioObservation o[2] = {{0xAABBCCDDEEFFULL, FALSE, FALSE}, {0x112233445566ULL, FALSE, FALSE}};
    BtAudioResult r = ProbeBtAudioEndpoints(o, 2, 0, 0);
    CHECK(SUCCEEDED(r.enumerationHr) && o[0].found && o[0].active && o[1].found && !o[1].active);
    CHECK(!commands && !ksActivations && !r.requested); NoLeaks();
    /* Disabled/not-present is not an observed connected endpoint, even if
       a provider returns it despite the requested mask. */
    Reset(); endpointCount = 2; includeAllStates = TRUE;
    states[0] = DEVICE_STATE_DISABLED; states[1] = DEVICE_STATE_NOTPRESENT;
    o[0] = (BtAudioObservation){0xAABBCCDDEEFFULL, FALSE, FALSE};
    r = ProbeBtAudioEndpoints(o, 1, 0, 0);
    CHECK(!o[0].found && !o[0].active && !commands); NoLeaks();
    /* Request accepted does not mutate an unplugged endpoint into ACTIVE. */
    Reset(); o[0] = (BtAudioObservation){0xAABBCCDDEEFFULL, FALSE, FALSE};
    r = ProbeBtAudioEndpoints(o, 1, o[0].address, 0);
    CHECK(r.requested && o[0].found && !o[0].active); NoLeaks();
}
static void Failures(void) {
    for (int stage = FAIL_CREATE; stage <= FAIL_PROPERTY; stage++) {
        Reset(); failure = stage;
        BtAudioResult r = TriggerBtAudioKsProperty(0xAABBCCDDEEFFULL, L"Earbuds", 0);
        CHECK(!r.requested && FAILED(r.commandHr)); NoLeaks();
        if (stage >= FAIL_GET_DEVICE) CHECK(r.matched);
        if (stage == FAIL_KS) CHECK(r.commandHr == E_NOINTERFACE);
    }
    /* One broken unrelated endpoint must not prevent a later addressed
       adapter from receiving Connect, or make partial status authoritative. */
    Reset(); failure = FAIL_STATE; failureIndex = 0; endpointCount = 2;
    BtAudioResult r = TriggerBtAudioKsProperty(0xAABBCCDDEEFFULL, L"Earbuds", 0);
    CHECK(r.requested && FAILED(r.enumerationHr) && commands == 1); NoLeaks();
}
static void Refresh(void) {
    Reset(); g_devCount = 2;
    g_devices[0].address = 0xAABBCCDDEEFFULL; g_devices[1].address = 0x112233445566ULL;
    /* Publication resolves captured addresses, not indices, and does no COM
       under the shared device lock. */
    states[0] = DEVICE_STATE_ACTIVE; reorderDevices = TRUE;
    RefreshAudioConnections();
    int a = FindDeviceIndexByAddress(0xAABBCCDDEEFFULL), b = FindDeviceIndexByAddress(0x112233445566ULL);
    CHECK(a == 1 && b == 0 && g_devices[a].audioConnected && g_devices[a].audioEndpointFound);
    CHECK(g_devices[a].audioStatusValid && g_devices[b].audioStatusValid && !g_devices[b].audioConnected);
    CHECK(!g_devices[a].visible && !g_devices[a].allowAuto && !g_devices[a].paired);
    int before = creates; tick += 999; RefreshAudioConnections(); CHECK(creates == before);
    tick++; states[0] = DEVICE_STATE_UNPLUGGED; RefreshAudioConnections();
    CHECK(!g_devices[a].audioConnected && g_devices[a].audioEndpointFound); NoLeaks();
    tick += 1000; failure = FAIL_ENUM; RefreshAudioConnections();
    CHECK(!g_devices[a].audioStatusValid && !g_devices[a].audioConnected); NoLeaks();
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "routing")) Routing();
    else if (!strcmp(argv[1], "state")) State();
    else if (!strcmp(argv[1], "failures")) Failures();
    else if (!strcmp(argv[1], "refresh")) Refresh();
    else CHECK(FALSE);
    return 0;
}
