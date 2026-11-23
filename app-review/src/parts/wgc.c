/* One-frame Windows.Graphics.Capture, Windows 10 1903+ / Windows 11.
   Compositor-owned window pixels, not desktop BitBlt or app WM_PRINT exports.
   These minimal ABI declarations follow the Microsoft SDK interfaces; see the
   source links in app-review/README.md. No border/policy suppression, monitor
   capture, fallback capture API, target activation, or persisted preferences. */
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <d3d11.h>
#include <dxgi.h>

#define REVIEW_UNKNOWN_SLOTS(T) \
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(T *, REFIID, void **); \
    ULONG (STDMETHODCALLTYPE *AddRef)(T *); \
    ULONG (STDMETHODCALLTYPE *Release)(T *)
#define REVIEW_INSPECTABLE_SLOTS(T) \
    REVIEW_UNKNOWN_SLOTS(T); \
    HRESULT (STDMETHODCALLTYPE *GetIids)(T *, ULONG *, IID **); \
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(T *, HSTRING *); \
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(T *, TrustLevel *)

typedef struct ReviewSize { INT32 width, height; } ReviewSize;
typedef struct ReviewCaptureItem ReviewCaptureItem;
typedef struct ReviewFrame ReviewFrame;
typedef struct ReviewPool ReviewPool;
typedef struct ReviewSession ReviewSession;

typedef struct ReviewItemInterop ReviewItemInterop;
typedef struct ReviewItemInteropVtbl {
    REVIEW_UNKNOWN_SLOTS(ReviewItemInterop);
    HRESULT (STDMETHODCALLTYPE *CreateForWindow)(ReviewItemInterop *, HWND, REFIID, void **);
    HRESULT (STDMETHODCALLTYPE *CreateForMonitor)(ReviewItemInterop *, HMONITOR, REFIID, void **);
} ReviewItemInteropVtbl;
struct ReviewItemInterop { const ReviewItemInteropVtbl *lpVtbl; };

typedef struct ReviewCaptureItemVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewCaptureItem);
    HRESULT (STDMETHODCALLTYPE *get_DisplayName)(ReviewCaptureItem *, HSTRING *);
    HRESULT (STDMETHODCALLTYPE *get_Size)(ReviewCaptureItem *, ReviewSize *);
    HRESULT (STDMETHODCALLTYPE *add_Closed)(ReviewCaptureItem *, void *, INT64 *);
    HRESULT (STDMETHODCALLTYPE *remove_Closed)(ReviewCaptureItem *, INT64);
} ReviewCaptureItemVtbl;
struct ReviewCaptureItem { const ReviewCaptureItemVtbl *lpVtbl; };

typedef struct ReviewFrameVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewFrame);
    HRESULT (STDMETHODCALLTYPE *get_Surface)(ReviewFrame *, IInspectable **);
    HRESULT (STDMETHODCALLTYPE *get_SystemRelativeTime)(ReviewFrame *, INT64 *);
    HRESULT (STDMETHODCALLTYPE *get_ContentSize)(ReviewFrame *, ReviewSize *);
} ReviewFrameVtbl;
struct ReviewFrame { const ReviewFrameVtbl *lpVtbl; };

typedef struct ReviewPoolVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewPool);
    HRESULT (STDMETHODCALLTYPE *Recreate)(ReviewPool *, IInspectable *, INT32, INT32, ReviewSize);
    HRESULT (STDMETHODCALLTYPE *TryGetNextFrame)(ReviewPool *, ReviewFrame **);
    HRESULT (STDMETHODCALLTYPE *add_FrameArrived)(ReviewPool *, void *, INT64 *);
    HRESULT (STDMETHODCALLTYPE *remove_FrameArrived)(ReviewPool *, INT64);
    HRESULT (STDMETHODCALLTYPE *CreateCaptureSession)(ReviewPool *, ReviewCaptureItem *, ReviewSession **);
    HRESULT (STDMETHODCALLTYPE *get_DispatcherQueue)(ReviewPool *, IInspectable **);
} ReviewPoolVtbl;
struct ReviewPool { const ReviewPoolVtbl *lpVtbl; };

typedef struct ReviewPoolStatics ReviewPoolStatics;
typedef struct ReviewPoolStaticsVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewPoolStatics);
    HRESULT (STDMETHODCALLTYPE *CreateFreeThreaded)(ReviewPoolStatics *, IInspectable *, INT32, INT32, ReviewSize, ReviewPool **);
} ReviewPoolStaticsVtbl;
struct ReviewPoolStatics { const ReviewPoolStaticsVtbl *lpVtbl; };

typedef struct ReviewSessionVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewSession);
    HRESULT (STDMETHODCALLTYPE *StartCapture)(ReviewSession *);
} ReviewSessionVtbl;
struct ReviewSession { const ReviewSessionVtbl *lpVtbl; };

typedef struct ReviewSessionStatics ReviewSessionStatics;
typedef struct ReviewSessionStaticsVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewSessionStatics);
    HRESULT (STDMETHODCALLTYPE *IsSupported)(ReviewSessionStatics *, BYTE *);
} ReviewSessionStaticsVtbl;
struct ReviewSessionStatics { const ReviewSessionStaticsVtbl *lpVtbl; };

typedef struct ReviewSessionCursor ReviewSessionCursor;
typedef struct ReviewSessionCursorVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewSessionCursor);
    HRESULT (STDMETHODCALLTYPE *get_IsCursorCaptureEnabled)(ReviewSessionCursor *, BYTE *);
    HRESULT (STDMETHODCALLTYPE *put_IsCursorCaptureEnabled)(ReviewSessionCursor *, BYTE);
} ReviewSessionCursorVtbl;
struct ReviewSessionCursor { const ReviewSessionCursorVtbl *lpVtbl; };

typedef struct ReviewClosable ReviewClosable;
typedef struct ReviewClosableVtbl {
    REVIEW_INSPECTABLE_SLOTS(ReviewClosable);
    HRESULT (STDMETHODCALLTYPE *Close)(ReviewClosable *);
} ReviewClosableVtbl;
struct ReviewClosable { const ReviewClosableVtbl *lpVtbl; };

typedef struct ReviewDxgiAccess ReviewDxgiAccess;
typedef struct ReviewDxgiAccessVtbl {
    REVIEW_UNKNOWN_SLOTS(ReviewDxgiAccess);
    HRESULT (STDMETHODCALLTYPE *GetInterface)(ReviewDxgiAccess *, REFIID, void **);
} ReviewDxgiAccessVtbl;
struct ReviewDxgiAccess { const ReviewDxgiAccessVtbl *lpVtbl; };

_Static_assert(offsetof(ReviewItemInteropVtbl, CreateForWindow) == 3 * sizeof(void *), "Capture interop ABI");
_Static_assert(offsetof(ReviewCaptureItemVtbl, get_Size) == 7 * sizeof(void *), "Capture item ABI");
_Static_assert(offsetof(ReviewFrameVtbl, get_ContentSize) == 8 * sizeof(void *), "Capture frame ABI");
_Static_assert(offsetof(ReviewPoolVtbl, CreateCaptureSession) == 10 * sizeof(void *), "Capture pool ABI");
_Static_assert(offsetof(ReviewPoolStaticsVtbl, CreateFreeThreaded) == 6 * sizeof(void *), "Capture factory ABI");

static const GUID ReviewIID_ItemInterop = {0x3628e81b,0x3cac,0x4c60,{0xb7,0xf4,0x23,0xce,0x0e,0x0c,0x33,0x56}};
static const GUID ReviewIID_Item = {0x79c3f95b,0x31f7,0x4ec2,{0xa4,0x64,0x63,0x2e,0xf5,0xd3,0x07,0x60}};
static const GUID ReviewIID_PoolStatics2 = {0x589b103f,0x6bbc,0x5df5,{0xa9,0x91,0x02,0xe2,0x8b,0x3b,0x66,0xd5}};
static const GUID ReviewIID_SessionStatics = {0x2224a540,0x5974,0x49aa,{0xb2,0x32,0x08,0x82,0x53,0x6f,0x4c,0xb5}};
static const GUID ReviewIID_SessionCursor = {0x2c39ae40,0x7d2e,0x5044,{0x80,0x4e,0x8b,0x67,0x99,0xd4,0xcf,0x9e}};
static const GUID ReviewIID_Closable = {0x30d5a829,0x7fa4,0x4026,{0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e}};
static const GUID ReviewIID_Direct3DDevice = {0xa37624ab,0x8d5f,0x4650,{0x9d,0x3e,0x9e,0xae,0x3d,0x9b,0xc6,0x70}};
static const GUID ReviewIID_DxgiAccess = {0xa9b3d012,0x3df2,0x4ee3,{0xb8,0xd1,0x86,0x95,0xf4,0x57,0xd3,0xc1}};

typedef struct ReviewRuntimeApi {
    HMODULE library;
    HRESULT (WINAPI *initialize)(RO_INIT_TYPE);
    void (WINAPI *uninitialize)(void);
    HRESULT (WINAPI *create_string)(PCNZWCH, UINT32, HSTRING *);
    HRESULT (WINAPI *delete_string)(HSTRING);
    HRESULT (WINAPI *factory)(HSTRING, REFIID, void **);
} ReviewRuntimeApi;
static ReviewRuntimeApi g_runtime;

static HRESULT ReviewRuntimeBegin(void) {
    ZeroMemory(&g_runtime, sizeof(g_runtime));
    g_runtime.library = LoadLibraryExW(L"combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_runtime.library) return E_NOINTERFACE;
    g_runtime.initialize = (HRESULT (WINAPI *)(RO_INIT_TYPE))(void *)GetProcAddress(g_runtime.library, "RoInitialize");
    g_runtime.uninitialize = (void (WINAPI *)(void))(void *)GetProcAddress(g_runtime.library, "RoUninitialize");
    g_runtime.create_string = (HRESULT (WINAPI *)(PCNZWCH, UINT32, HSTRING *))(void *)GetProcAddress(g_runtime.library, "WindowsCreateString");
    g_runtime.delete_string = (HRESULT (WINAPI *)(HSTRING))(void *)GetProcAddress(g_runtime.library, "WindowsDeleteString");
    g_runtime.factory = (HRESULT (WINAPI *)(HSTRING, REFIID, void **))(void *)GetProcAddress(g_runtime.library, "RoGetActivationFactory");
    HRESULT hr = E_NOINTERFACE;
    if (g_runtime.initialize && g_runtime.uninitialize && g_runtime.create_string && g_runtime.delete_string && g_runtime.factory)
        hr = g_runtime.initialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr)) { FreeLibrary(g_runtime.library); ZeroMemory(&g_runtime, sizeof(g_runtime)); }
    return hr;
}

static void ReviewRuntimeEnd(void) {
    if (g_runtime.library) { g_runtime.uninitialize(); FreeLibrary(g_runtime.library); }
    ZeroMemory(&g_runtime, sizeof(g_runtime));
}

static HRESULT ReviewFactory(const wchar_t *name, const GUID *iid, void **out) {
    HSTRING string = NULL;
    HRESULT hr = g_runtime.create_string(name, (UINT32)wcslen(name), &string);
    if (SUCCEEDED(hr)) hr = g_runtime.factory(string, iid, out);
    if (string) g_runtime.delete_string(string);
    return hr;
}

static void ReviewCloseObject(void *object) {
    if (!object) return;
    ReviewClosable *closable = NULL;
    if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)object, &ReviewIID_Closable, (void **)&closable))) {
        closable->lpVtbl->Close(closable);
        closable->lpVtbl->Release(closable);
    }
}

/* All COM/D3D state belongs to the disposable capture worker. The OS enforces
   capture permission/protection; failure is NEVER retried via another API. */
static HRESULT ReviewWindowPixels(HWND window, uint32_t **pixels, ReviewSize *size) {
    ReviewSessionStatics *supported = NULL;
    ReviewItemInterop *interop = NULL;
    ReviewCaptureItem *item = NULL;
    ReviewPoolStatics *pool_statics = NULL;
    ReviewPool *pool = NULL;
    ReviewSession *session = NULL;
    ReviewSessionCursor *cursor = NULL;
    ReviewFrame *frame = NULL;
    IInspectable *bridge_object = NULL, *winrt_device = NULL, *surface = NULL;
    IDXGIDevice *dxgi = NULL;
    ReviewDxgiAccess *access = NULL;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    ID3D11Texture2D *texture = NULL, *staging = NULL;
    HMODULE d3d_library = NULL;
    BYTE is_supported = FALSE;
    ReviewSize item_size = {0};
    *pixels = NULL;
    HRESULT hr = ReviewFactory(L"Windows.Graphics.Capture.GraphicsCaptureSession", &ReviewIID_SessionStatics, (void **)&supported);
    if (SUCCEEDED(hr)) hr = supported->lpVtbl->IsSupported(supported, &is_supported);
    if (SUCCEEDED(hr) && !is_supported) hr = E_NOTIMPL;
    if (SUCCEEDED(hr)) hr = ReviewFactory(L"Windows.Graphics.Capture.GraphicsCaptureItem", &ReviewIID_ItemInterop, (void **)&interop);
    if (SUCCEEDED(hr)) hr = interop->lpVtbl->CreateForWindow(interop, window, &ReviewIID_Item, (void **)&item);
    if (SUCCEEDED(hr)) hr = item->lpVtbl->get_Size(item, &item_size);
    if (SUCCEEDED(hr) && !ReviewDimensions(item_size.width, item_size.height)) hr = E_INVALIDARG;
    if (SUCCEEDED(hr)) {
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               NULL, 0, D3D11_SDK_VERSION, &device, NULL, &context);
        if (FAILED(hr)) {
            if (context) { ID3D11DeviceContext_Release(context); context = NULL; }
            if (device) { ID3D11Device_Release(device); device = NULL; }
            hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   NULL, 0, D3D11_SDK_VERSION, &device, NULL, &context);
        }
    }
    if (SUCCEEDED(hr)) hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi);
    typedef HRESULT (WINAPI *BridgeFn)(IDXGIDevice *, IInspectable **);
    BridgeFn bridge = NULL;
    if (SUCCEEDED(hr)) {
        d3d_library = LoadLibraryExW(L"d3d11.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (d3d_library) bridge = (BridgeFn)(void *)GetProcAddress(d3d_library, "CreateDirect3D11DeviceFromDXGIDevice");
        if (!bridge) hr = E_NOINTERFACE;
    }
    if (SUCCEEDED(hr)) hr = bridge(dxgi, &bridge_object);
    if (SUCCEEDED(hr)) hr = IInspectable_QueryInterface(bridge_object, &ReviewIID_Direct3DDevice, (void **)&winrt_device);
    if (SUCCEEDED(hr)) hr = ReviewFactory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool", &ReviewIID_PoolStatics2, (void **)&pool_statics);
    if (SUCCEEDED(hr)) hr = pool_statics->lpVtbl->CreateFreeThreaded(pool_statics, winrt_device,
        (INT32)DXGI_FORMAT_B8G8R8A8_UNORM, 1, item_size, &pool);
    if (SUCCEEDED(hr)) hr = pool->lpVtbl->CreateCaptureSession(pool, item, &session);
    if (SUCCEEDED(hr) && SUCCEEDED(session->lpVtbl->QueryInterface(session, &ReviewIID_SessionCursor, (void **)&cursor)))
        cursor->lpVtbl->put_IsCursorCaptureEnabled(cursor, FALSE); /* This one capture, never an app preference. */
    if (SUCCEEDED(hr)) hr = session->lpVtbl->StartCapture(session);
    ULONGLONG started = GetTickCount64();
    while (SUCCEEDED(hr) && !frame && GetTickCount64() - started < 3000) {
        hr = pool->lpVtbl->TryGetNextFrame(pool, &frame);
        if (SUCCEEDED(hr) && !frame) Sleep(16);
    }
    if (SUCCEEDED(hr) && !frame) hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    if (SUCCEEDED(hr)) hr = frame->lpVtbl->get_ContentSize(frame, size);
    if (SUCCEEDED(hr) && (!ReviewDimensions(size->width, size->height) ||
        size->width != item_size.width || size->height != item_size.height)) hr = E_INVALIDARG;
    if (SUCCEEDED(hr)) hr = frame->lpVtbl->get_Surface(frame, &surface);
    if (SUCCEEDED(hr)) hr = IInspectable_QueryInterface(surface, &ReviewIID_DxgiAccess, (void **)&access);
    if (SUCCEEDED(hr)) hr = access->lpVtbl->GetInterface(access, &IID_ID3D11Texture2D, (void **)&texture);
    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D_GetDesc(texture, &desc);
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || desc.Width < (UINT)size->width || desc.Height < (UINT)size->height)
            hr = E_INVALIDARG;
        else {
            desc.Width = (UINT)size->width; desc.Height = (UINT)size->height;
            desc.MipLevels = 1; desc.ArraySize = 1; desc.SampleDesc.Count = 1; desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
            hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);
        }
    }
    if (SUCCEEDED(hr)) {
        D3D11_BOX box = {0, 0, 0, (UINT)size->width, (UINT)size->height, 1};
        ID3D11DeviceContext_CopySubresourceRegion(context, (ID3D11Resource *)staging, 0, 0, 0, 0,
                                                  (ID3D11Resource *)texture, 0, &box);
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            if (!mapped.pData || mapped.RowPitch < (UINT)size->width * 4) hr = E_FAIL;
            else {
                *pixels = (uint32_t *)malloc((size_t)size->width * size->height * 4);
                if (!*pixels) hr = E_OUTOFMEMORY;
                else for (int y = 0; y < size->height; ++y)
                    memcpy(*pixels + (size_t)y * size->width, (BYTE *)mapped.pData + (size_t)y * mapped.RowPitch, (size_t)size->width * 4);
            }
            ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
        }
    }
    ReviewCloseObject(frame); ReviewCloseObject(session); ReviewCloseObject(pool);
    if (staging) ID3D11Texture2D_Release(staging);
    if (texture) ID3D11Texture2D_Release(texture);
    if (access) access->lpVtbl->Release(access);
    if (surface) IInspectable_Release(surface);
    if (frame) frame->lpVtbl->Release(frame);
    if (cursor) cursor->lpVtbl->Release(cursor);
    if (session) session->lpVtbl->Release(session);
    if (pool) pool->lpVtbl->Release(pool);
    if (pool_statics) pool_statics->lpVtbl->Release(pool_statics);
    if (item) item->lpVtbl->Release(item);
    if (interop) interop->lpVtbl->Release(interop);
    if (supported) supported->lpVtbl->Release(supported);
    if (winrt_device) IInspectable_Release(winrt_device);
    if (bridge_object) IInspectable_Release(bridge_object);
    if (dxgi) IDXGIDevice_Release(dxgi);
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    if (d3d_library) FreeLibrary(d3d_library);
    return hr;
}
#undef REVIEW_UNKNOWN_SLOTS
#undef REVIEW_INSPECTABLE_SLOTS
