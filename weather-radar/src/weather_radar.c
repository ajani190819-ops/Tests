/* WeatherRadar - pick a place, read an honest chance of rain, watch the radar.
 *
 * The point of this app is one number most weather apps get wrong on purpose.
 * The "chance of rain" everybody shows is PoP = confidence x areal coverage:
 * a forecaster who is certain half the metro gets wet reports 50%, and an app
 * that blends 40 km grid cells into your street reports the same 50% whether
 * the rain lands on you or 20 miles away. It is a statement about an AREA.
 *
 * This app asks the question you actually meant - "will it rain HERE?" - and
 * answers it the only honest way available without a forecaster: run the
 * ensemble. Open-Meteo serves every member of the GFS ensemble separately
 * (precipitation_member01..memberNN), so the fraction of members that put
 * >= 0.1 mm at these exact coordinates in the window IS the probability, by
 * definition, with no coverage term smuggled in. The denominator is the
 * members that actually reported that hour, because members stop producing
 * values past their own forecast horizon and counting a missing member as
 * "dry" would quietly deflate every long-range number.
 *
 * The coverage-blended figure is still fetched and drawn - as a hollow tick
 * beside each honest bar - so the difference is visible instead of claimed.
 *
 * Radar: RainViewer frames (past + nowcast) composited over OpenStreetMap
 * tiles, both decoded through WIC, which is reached with CoCreateInstance so
 * windowscodecs.dll never enters this exe's import table (the suite's PE
 * contract allows stock system DLLs only).
 *
 * Modularized like every other suite app: this file is ONE translation unit
 * that textually includes the part files in dependency order. Edit the parts.
 */
/* COBJMACROS before the headers: the WIC calls below use the C method
   macros, so the COM vtable layout never appears by hand. */
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>
#include "../../shared/win32/hub_tray_host.h"

#define APP_TITLE_W   L"Weather Radar"
#define APP_DIRNAME_W L"WeatherRadar"
#define MUTEX_NAME    L"Local\\Tests_WeatherRadar_SingleInstance_v1"
#define SHOW_EVENT    L"Local\\Tests_WeatherRadar_Show_v1"
#define HTTP_UA       L"TestsSuite-WeatherRadar/1.0 (personal desktop app; contact: owner)"

#define WM_APP_DATA   (WM_APP + 7)     /* worker -> UI: staging result is ready */
#define WM_APP_STATUS (WM_APP + 8)     /* worker -> UI: status line text */
#define WM_APP_SEARCH (WM_APP + 9)     /* edit Return -> UI search submit */
#define WM_TRAYICON    (WM_APP + 10)    /* notification-area interaction */

enum { BTN_SEARCH = 0, BTN_REFRESH, BTN_PLAY, BTN_PREV, BTN_NEXT,
       BTN_KEEP, BTN_DROP, BTN_ZOOM, BTN_COUNT };
enum { TIMER_EMBLEM = 1, TIMER_ANIM = 2, TIMER_REFRESH = 3, TIMER_TRAY_SYNC = 4 };
enum { ID_TRAY_OPEN = 201, ID_TRAY_EXIT = 202 };
#define ID_EDIT_SEARCH 101
#define HINT_SEARCH    L"city or postcode - press Search"

/* Owner-set rain alert policy: exact-location ensemble probability only.
   A qualifying episode emits once, then stays latched through the rain (and
   across a restart) until both the current weather and near-term forecast are
   dry. The cooldown is a second, independent anti-spam guard. */
#define RAIN_ALERT_PROB          0.66
#define RAIN_ALERT_HOURS         3
#define RAIN_ALERT_COOLDOWN_SEC  (3ULL * 60ULL * 60ULL)
#define RAIN_ALERT_TRACE_IN      0.004
#define RAIN_ETA_LIKELY_PROB     0.50

/* What "rain" means for the honest probability. 0.1 mm/h is the WMO trace
   threshold: below it a gauge reports nothing, so a member under it did not
   forecast rain at this point, however much it rained next door. */
#define RAIN_MM        0.1

#define MAX_PLACES     8
#define MAX_SEARCH     8
#define MAX_FRAMES     64
#define MAX_PAST_FRAMES 24
#define FC_DAYS        5
#define FC_HOURS       (FC_DAYS * 24 + 2) /* leave room for a local DST day */
#define HOURLY_SHOWN   12
#define MAX_MEMBERS    64
#define GRID_TILES     3               /* 3x3 tiles per radar view */
#define TILE_PX        256
#define GRID_PX        (GRID_TILES * TILE_PX)
/* RainViewer imagery is available through host level 7 in this app's route.
   Higher owner-selected levels are honest local detail crops from that source,
   not unsupported network requests masquerading as more precise radar data. */
#define RADAR_TILE_ZOOM_MIN     7
#define RADAR_TILE_ZOOM_MAX     7
#define RADAR_DISPLAY_ZOOM_MAX  10

/* Worker jobs. One thread, one queued job at a time: the UI never blocks and
   a job always writes to staging, never to what is on screen. */
enum { JOB_NONE = 0, JOB_GEOCODE, JOB_FORECAST, JOB_ENSEMBLE,
       JOB_RADAR_CATALOG, JOB_TILES };

typedef struct {
    wchar_t name[96];
    wchar_t region[96];       /* "Florida, United States" */
    double  lat, lon;
} Place;

typedef struct {
    long long time;           /* unix seconds, UTC */
    char      path[96];       /* RainViewer frame path */
    int       kind;           /* 0 = past (observed), 1 = nowcast (modelled) */
} RadarFrame;

typedef struct {
    wchar_t label[16];        /* local weekday/date, e.g. "Tue 11/17" */
    double  maxF, minF, regionalPoP, rainSumIn;
    double  cloudMean, windMaxMph;
    int     firstHour, hourCount; /* indices into the forecast's now-relative series */
} ForecastDay;

/* One forecast API call: current conditions, five daily rows, hourly rows. */
typedef struct {
    int     have;             /* 0 nothing, 1 partial, 2 complete */
    wchar_t fetched[24];      /* local HH:MM of the fetch */
    wchar_t tz[48];
    double  tempF, feelsF, rh, windMph, windGustMph, cloudCover, precipIn;
    int     wcode;
    double  dMaxF, dMinF, dPoP, dSumIn;   /* today, straight from the API */
    ForecastDay days[FC_DAYS];
    int     dayN;
    double  hT[FC_HOURS], hPoP[FC_HOURS], hP[FC_HOURS];
    double  hCloud[FC_HOURS], hWindMph[FC_HOURS];
    int     hCode[FC_HOURS], hDay[FC_HOURS];
    wchar_t hLabel[FC_HOURS][8];
    int     hN;
    int     nowIdx;           /* source-hour offset of the first displayed hour */
} Forecast;

/* The ensemble member matrix, and the probabilities derived from it. */
typedef struct {
    int     have;
    int     members;          /* distinct member series found */
    int     hours;
    double  mm[MAX_MEMBERS][FC_HOURS];   /* NAN where the member reported nothing */
    double  prob[FC_HOURS];   /* honest per-hour probability, 0..1 */
    int     reported[FC_HOURS];          /* denominator actually used per hour */
    wchar_t model[40];
} Ensemble;

static HINSTANCE g_hInst;
static HWND      g_hwnd, g_hSearch;
static HFONT     g_fontUI, g_fontTitle, g_fontBig, g_fontBtn, g_fontSmall;
static HBRUSH    g_bgBrush, g_inputBrush;
static UINT      g_dpi = 96;
static int       g_fit = 100;
static int       g_hoverBtn = -1;
static CRITICAL_SECTION g_lock;

/* theme-aware palette (filled by InitPalette before the first window exists) */
static COLORREF COL_BG, COL_CARD, COL_INPUT, COL_TXT, COL_MUTED, COL_ACCENT;
static COLORREF COL_WARN, COL_GOOD, COL_DIV, COL_HOT, COL_CARD_BD, COL_RAIN;

static wchar_t g_cfgDir[MAX_PATH];
static wchar_t g_cfgPath[MAX_PATH];
static wchar_t g_cacheDir[MAX_PATH];
static wchar_t g_status[320] = L"Pick a place to begin.";
static int     g_statusKind = 0;        /* 0 neutral, 1 good, 2 warn */
static NOTIFYICONDATAW g_nid;
static int       g_trayVisible = 0;
static int     g_quitting = 0;
static int     g_trayHinted = 0;

/* Persisted one-place episode state prevents a restart or a 10-minute refresh
   from re-announcing rain that has already begun. */
static int       g_rainAlertEpisode = 0;
static ULONGLONG g_rainAlertLastEpoch = 0;
static double    g_rainAlertLat = NAN, g_rainAlertLon = NAN;

static Place   g_places[MAX_PLACES];
static int     g_placeN = 0;
static int     g_placeSel = -1;
static Place   g_search[MAX_SEARCH];
static Place   g_searchStage[MAX_SEARCH];
static int     g_searchN = 0;
static int     g_searchStageN = 0;
static int     g_searchSel = -1;
static DWORD   g_searchTicket = 0;
static DWORD   g_searchStageTicket = 0;
static DWORD   g_placeTicket = 0;       /* rejects late forecast results for an old place */
static wchar_t g_query[96] = L"";
static int     g_searchHint = 1;

static Forecast  g_fc;
static Ensemble  g_ens;
static Forecast  g_fcStage;
static Ensemble  g_ensStage;

static RadarFrame g_frames[MAX_FRAMES];
static int        g_frameN = 0;
static int        g_frameSel = 0;
static int        g_playing = 0;
static int        g_zoom = 8;           /* selected display detail: 7 through 10 */
static wchar_t    g_radarHost[160] = L"";
/* 2 = local display crop beyond the validated host level 7 */
static int        g_radarNote = 0;

/* decoded imagery: 768x768 each, owned by whoever the UI currently points at */
static uint32_t *g_view = NULL;         /* composited radar over basemap */
static uint32_t *g_viewStage = NULL;
static uint32_t *g_base = NULL;         /* cached basemap grid */
static int       g_baseKey[3] = { -1, -1, -1 };   /* cached base: zoom, tx, ty */
static int       g_viewKey[3] = { -1, -1, -1 };   /* published view: zoom, tx, ty */
static DWORD     g_viewTicket = 0;      /* place generation used to build g_view */
static int       g_viewReady = 0;


static int S(int px) {
    return MulDiv(MulDiv(px, (int)g_dpi, 96), g_fit, 100);
}

/* Keep network requests on the validated source level. Every display level
   above 7 is a local crop of level-7 imagery, preserving the selected-place
   focus without requesting a provider's unsupported placeholder tiles. */
static int RadarTileZoomForDisplay(int displayZoom) {
    if (displayZoom > RADAR_TILE_ZOOM_MAX) return RADAR_TILE_ZOOM_MAX;
    if (displayZoom < RADAR_TILE_ZOOM_MIN) return RADAR_TILE_ZOOM_MIN;
    return displayZoom;
}

/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN DEPENDENCY ORDER, so
      weather_radar.c is still ONE translation unit. ---- */
#include "parts/util.c"
#include "parts/json.c"
#include "parts/net.c"
#include "parts/places.c"
#include "parts/radar.c"
#include "parts/forecast.c"
#include "parts/alerts.c"
#include "parts/workers.c"
#include "../../shared/win32/suite_emblem.c"
#include "parts/emblem.c"
#include "parts/ui.c"
#include "parts/entry.c"
