/* Narrow declarations for the documented Windows 10+ geolocation ABI.
   Zig's bundled MinGW SDK has IInspectable/IAsyncInfo, but not this WinRT
   namespace. Prefixes stop at the last used slot; unused slots are declared
   (not invoked). No private interfaces or guessed offsets are used.
   Reference: microsoft/win32metadata, generation/WinSDK/RecompiledIdlHeaders/
   winrt/windows.devices.geolocation.h (blob 326be3a4c4f7fdbc3f420709867fe21d79d1d880).
   Keep the slot assertions below and the ABI regression test in sync. */
#ifndef RADAR_LOCATION_ABI_H
#define RADAR_LOCATION_ABI_H
#include <inspectable.h>
#include <asyncinfo.h>
#include <stddef.h>

#define LOC_INSPECTABLE(T) \
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(T *, REFIID, void **); \
    ULONG (STDMETHODCALLTYPE *AddRef)(T *); \
    ULONG (STDMETHODCALLTYPE *Release)(T *); \
    HRESULT (STDMETHODCALLTYPE *GetIids)(T *, ULONG *, IID **); \
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(T *, HSTRING *); \
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(T *, TrustLevel *)
#define LOC_INTERFACE(T) typedef struct T T; struct T { const struct T##Vtbl *lpVtbl; }

/* WinRT structs are passed by value where specified, not as integers. */
typedef struct { INT64 Duration; } LocTimeSpan;
typedef struct { INT64 UniversalTime; } LocDateTime;
typedef struct { double Latitude, Longitude, Altitude; } LocBasicPosition;
LOC_INTERFACE(LocAccessOp);
LOC_INTERFACE(LocPositionOp);
LOC_INTERFACE(LocGeolocator);
LOC_INTERFACE(LocStatics);
LOC_INTERFACE(LocCoordinate);
LOC_INTERFACE(LocCoordinatePoint);
LOC_INTERFACE(LocPoint);
LOC_INTERFACE(LocPosition);

struct LocAccessOpVtbl {
    LOC_INSPECTABLE(LocAccessOp);
    HRESULT (STDMETHODCALLTYPE *put_Completed)(LocAccessOp *, void *);
    HRESULT (STDMETHODCALLTYPE *get_Completed)(LocAccessOp *, void **);
    HRESULT (STDMETHODCALLTYPE *GetResults)(LocAccessOp *, int *);
};
struct LocPositionOpVtbl {
    LOC_INSPECTABLE(LocPositionOp);
    HRESULT (STDMETHODCALLTYPE *put_Completed)(LocPositionOp *, void *);
    HRESULT (STDMETHODCALLTYPE *get_Completed)(LocPositionOp *, void **);
    HRESULT (STDMETHODCALLTYPE *GetResults)(LocPositionOp *, LocPosition **);
};
struct LocStaticsVtbl {
    LOC_INSPECTABLE(LocStatics);
    HRESULT (STDMETHODCALLTYPE *RequestAccessAsync)(LocStatics *, LocAccessOp **);
};
struct LocGeolocatorVtbl {
    LOC_INSPECTABLE(LocGeolocator);
    HRESULT (STDMETHODCALLTYPE *get_DesiredAccuracy)(LocGeolocator *, int *);
    HRESULT (STDMETHODCALLTYPE *put_DesiredAccuracy)(LocGeolocator *, int);
    HRESULT (STDMETHODCALLTYPE *get_MovementThreshold)(LocGeolocator *, double *);
    HRESULT (STDMETHODCALLTYPE *put_MovementThreshold)(LocGeolocator *, double);
    HRESULT (STDMETHODCALLTYPE *get_ReportInterval)(LocGeolocator *, UINT32 *);
    HRESULT (STDMETHODCALLTYPE *put_ReportInterval)(LocGeolocator *, UINT32);
    HRESULT (STDMETHODCALLTYPE *get_LocationStatus)(LocGeolocator *, int *);
    HRESULT (STDMETHODCALLTYPE *GetGeopositionAsync)(LocGeolocator *, LocPositionOp **);
    HRESULT (STDMETHODCALLTYPE *GetGeopositionAsyncWithAgeAndTimeout)(LocGeolocator *, LocTimeSpan, LocTimeSpan, LocPositionOp **);
};
struct LocPositionVtbl {
    LOC_INSPECTABLE(LocPosition);
    HRESULT (STDMETHODCALLTYPE *get_Coordinate)(LocPosition *, LocCoordinate **);
};
struct LocCoordinateVtbl {
    LOC_INSPECTABLE(LocCoordinate);
    HRESULT (STDMETHODCALLTYPE *get_Latitude)(LocCoordinate *, double *); /* deprecated: NOT called */
    HRESULT (STDMETHODCALLTYPE *get_Longitude)(LocCoordinate *, double *); /* NOT called */
    HRESULT (STDMETHODCALLTYPE *get_Altitude)(LocCoordinate *, void **);
    HRESULT (STDMETHODCALLTYPE *get_Accuracy)(LocCoordinate *, double *);
    HRESULT (STDMETHODCALLTYPE *get_AltitudeAccuracy)(LocCoordinate *, void **);
    HRESULT (STDMETHODCALLTYPE *get_Heading)(LocCoordinate *, void **);
    HRESULT (STDMETHODCALLTYPE *get_Speed)(LocCoordinate *, void **);
    HRESULT (STDMETHODCALLTYPE *get_Timestamp)(LocCoordinate *, LocDateTime *);
};
struct LocCoordinatePointVtbl {
    LOC_INSPECTABLE(LocCoordinatePoint);
    HRESULT (STDMETHODCALLTYPE *get_Point)(LocCoordinatePoint *, LocPoint **);
};
struct LocPointVtbl {
    LOC_INSPECTABLE(LocPoint);
    HRESULT (STDMETHODCALLTYPE *get_Position)(LocPoint *, LocBasicPosition *);
};

static const IID LOC_IID_ASYNCINFO = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID LOC_IID_STATICS = {0x9a8e7571,0x2df5,0x4591,{0x9f,0x87,0xeb,0x5f,0xd8,0x94,0xe9,0xb7}};
static const IID LOC_IID_GEOLOCATOR = {0xa9c3bf62,0x4524,0x4989,{0x8a,0xa9,0xde,0x01,0x9d,0x2e,0x55,0x1f}};
static const IID LOC_IID_COORDINATE_POINT = {0xfeea0525,0xd22c,0x4d46,{0xb5,0x27,0x0b,0x96,0x06,0x6f,0xc7,0xdb}};

#define LOC_SLOT(T, member, slot) _Static_assert(offsetof(struct T##Vtbl, member) == (slot) * sizeof(void *), #T "." #member " ABI slot")
LOC_SLOT(LocStatics, RequestAccessAsync, 6);
LOC_SLOT(LocGeolocator, put_DesiredAccuracy, 7);
LOC_SLOT(LocGeolocator, GetGeopositionAsyncWithAgeAndTimeout, 14);
LOC_SLOT(LocAccessOp, GetResults, 8);
LOC_SLOT(LocPositionOp, GetResults, 8);
LOC_SLOT(LocPosition, get_Coordinate, 6);
LOC_SLOT(LocCoordinate, get_Accuracy, 9);
LOC_SLOT(LocCoordinate, get_Timestamp, 13);
LOC_SLOT(LocCoordinatePoint, get_Point, 6);
LOC_SLOT(LocPoint, get_Position, 6);
_Static_assert(sizeof(LocTimeSpan) == 8 && sizeof(LocDateTime) == 8 && sizeof(LocBasicPosition) == 24, "WinRT value ABI");
#undef LOC_SLOT
#undef LOC_INTERFACE
#undef LOC_INSPECTABLE
#endif
