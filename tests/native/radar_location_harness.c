/* Executes production location orchestration, saved-place storage and UI
   selection/completion handlers. Only Windows/provider I/O is mocked. This
   does NOT verify Windows consent, sensor accuracy, native GDI, or live tiles.
   The Python test supplies radar_location_under_test.h from current sources. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef uint32_t DWORD;
typedef uint64_t ULONGLONG;
typedef intptr_t LPARAM;
#define FALSE 0
#define WM_APP_DATA 1031
#define JOB_LOCATION 6
#define TIMER_LOCATION 5
#define ZeroMemory(p,n) memset(p,0,n)
static int g_hwnd = 1, foreground = 1;
static uint64_t clockMs = 1000, epoch = 1800000000;
static int timerOK = 1, postOK = 1, timerActive, posts;
static DWORD postedTicket;
static int beginAccess, beginFix, closeCalls, cancelCalls;
static int beginAccessResult, beginFixResult, accessResult, fixResult;
static int weatherRequests, g_searchSel, g_searchN;
static DWORD g_placeTicket;
static wchar_t statusText[320];
static wchar_t g_cfgPath[32] = L"fixture.ini";

typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
static int MulDiv(int a, int b, int c) { return (int)(((int64_t)a * b + c / 2) / c); }
static void SetRect(RECT *r, int left, int top, int right, int bottom) { *r = (RECT){left, top, right, bottom}; }
static int PtInRect(const RECT *r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }
static int Overlap(RECT a, RECT b) { return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom; }

static int GetForegroundWindow(void) { return foreground; }
static uint64_t GetTickCount64(void) { return clockMs; }
static uint64_t UnixNowSeconds(void) { return epoch; }
static int InvalidateRect(int hwnd, const void *r, int erase) { (void)hwnd; (void)r; (void)erase; return 1; }
static int SetTimer(int hwnd, int id, int ms, void *fn) {
    (void)fn; assert(hwnd == g_hwnd && id == TIMER_LOCATION && ms == 100);
    timerActive = timerOK; return timerOK;
}
static int KillTimer(int hwnd, int id) { (void)hwnd; assert(id == TIMER_LOCATION); timerActive = 0; return 1; }
static int PostMessageW(int hwnd, int msg, int kind, LPARAM ticket) {
    assert(hwnd == g_hwnd && msg == WM_APP_DATA && kind == JOB_LOCATION);
    posts++; postedTicket = (DWORD)ticket; return postOK;
}
static int _snwprintf(wchar_t *out, size_t cap, const wchar_t *fmt, ...) {
    /* Windows wide printf treats %s as wide; POSIX needs %ls. */
    wchar_t converted[512]; size_t n = 0;
    for (size_t i = 0; fmt[i]; i++) {
        converted[n++] = fmt[i];
        if (fmt[i] == L'%' && fmt[i + 1] == L's') converted[n++] = L'l';
    }
    converted[n] = 0;
    va_list args; va_start(args, fmt);
    int ret = vswprintf(out, cap, converted, args); va_end(args); return ret;
}

/* Small isolated in-memory profile store. No real owner configuration is read. */
static struct { wchar_t key[48], value[160]; int used; } ini[128];
static int FindKey(const wchar_t *key) {
    for (int i = 0; i < 128; i++) if (ini[i].used && !wcscmp(key, ini[i].key)) return i;
    return -1;
}
static int WritePrivateProfileStringW(const wchar_t *section, const wchar_t *key,
                                      const wchar_t *value, const wchar_t *file) {
    assert(!wcscmp(section, L"Places") && file == g_cfgPath);
    int slot = FindKey(key);
    if (!value) { if (slot >= 0) ini[slot].used = 0; return 1; }
    if (slot < 0) for (int i = 0; i < 128; i++) if (!ini[i].used) { slot = i; break; }
    assert(slot >= 0 && wcslen(key) < 48 && wcslen(value) < 160);
    wcscpy(ini[slot].key, key); wcscpy(ini[slot].value, value); ini[slot].used = 1; return 1;
}
static int GetPrivateProfileStringW(const wchar_t *section, const wchar_t *key,
                                    const wchar_t *fallback, wchar_t *out, int cap, const wchar_t *file) {
    assert(!wcscmp(section, L"Places") && file == g_cfgPath);
    int slot = FindKey(key); const wchar_t *value = slot < 0 ? fallback : ini[slot].value;
    wcsncpy(out, value, (size_t)cap - 1); out[cap - 1] = 0; return (int)wcslen(out);
}
static int GetPrivateProfileIntW(const wchar_t *section, const wchar_t *key, int fallback, const wchar_t *file) {
    assert(!wcscmp(section, L"Places") && file == g_cfgPath);
    int slot = FindKey(key); return slot < 0 ? fallback : (int)wcstol(ini[slot].value, NULL, 10);
}

#include "../../weather-radar/src/parts/location_policy.c"
static LocationSample mockSample;
static int LocationProviderBeginAccess(void) { beginAccess++; return beginAccessResult; }
static int LocationProviderBeginFix(void) { beginFix++; return beginFixResult; }
static int LocationProviderPoll(int access, LocationSample *sample) {
    if (!access) *sample = mockSample;
    return access ? accessResult : fixResult;
}
static void LocationProviderClose(int cancel) { closeCalls++; cancelCalls += cancel; }

/* Actual model/storage/core/UI functions are generated into this include. */
#include "radar_location_under_test.h"

static void Reset(void) {
    CancelWindowsLocation();
    memset(ini, 0, sizeof(ini)); memset(g_places, 0, sizeof(g_places));
    g_placeN = 0; g_placeSel = -1; g_searchN = 0; g_searchSel = -1;
    g_locationTicket = 0; g_locationStageTicket = 0;
    foreground = g_hwnd; clockMs = 1000; epoch = 1800000000;
    timerOK = postOK = 1; posts = beginAccess = beginFix = closeCalls = cancelCalls = weatherRequests = 0;
    g_placeTicket = 20;
    beginAccessResult = beginFixResult = accessResult = fixResult = LOCATION_PROVIDER_PENDING;
    mockSample = (LocationSample){30.445, -84.285, 42.5, epoch}; /* synthetic, not owner's position */
}
static Place SearchPoint(void) {
    Place p; memset(&p, 0, sizeof(p)); wcscpy(p.name, L"Search fixture"); wcscpy(p.region, L"Test region");
    p.lat = 47.6; p.lon = -122.33; return p;
}
static void SelectInitial(void) { Place p = SearchPoint(); RememberPlace(&p); assert(g_placeSel == 0); }
static void ReadyFix(void) {
    RequestWindowsLocation(); assert(beginAccess == 1 && beginFix == 0 && timerActive);
    PollWindowsLocation(); assert(beginFix == 0 && posts == 0); /* no permission, no lookup */
    accessResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
    assert(beginFix == 1 && g_locationPhase == 2 && timerActive);
    assert(closeCalls == 1); /* consent operation released before the fix starts */
}
static void Complete(void) {
    fixResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
    assert(posts == 1 && !timerActive && g_locationPhase == 3);
    DeliverLocationMessage(postedTicket);
}

static void TestLayout(void) {
    const int dpis[] = {96, 120, 144, 192}, fits[] = {55, 75, 100};
    for (size_t d = 0; d < sizeof(dpis)/sizeof(dpis[0]); d++)
    for (size_t f = 0; f < sizeof(fits)/sizeof(fits[0]); f++) {
        g_dpi = dpis[d]; g_fit = fits[f];
        for (int i = 0; i < BTN_COUNT; i++) {
            RECT a; BtnRect(i, &a);
            assert(a.left >= 0 && a.top >= 0 && a.right > a.left && a.bottom > a.top);
            assert(HitBtn((a.left + a.right) / 2, (a.top + a.bottom) / 2) == i);
            for (int j = i + 1; j < BTN_COUNT; j++) { RECT b; BtnRect(j, &b); assert(!Overlap(a, b)); }
        }
        for (int matches = 0; matches <= MAX_SEARCH; matches++) {
            g_searchN = matches; g_placeN = MAX_PLACES;
            RECT settings, row; BtnRect(BTN_LOCATION_SETTINGS, &settings);
            if (matches) {
                assert(SearchRowRect(0, &row)); assert(row.top > settings.bottom);
                assert(SearchRowRect(matches - 1, &row)); assert(SavedHeadingY() >= row.bottom + S(8));
            }
            assert(SavedRowRect(0, &row)); assert(row.top > settings.bottom);
            assert(SavedRowRect(MAX_PLACES - 1, &row)); assert(row.bottom < S(WIN_LH - 56));
        }
    }
    g_dpi = 96; g_fit = 100;
}

int main(void) {
    TestLayout();
    Reset();
    assert(LocationTicketCurrent(11, 11, 1));
    assert(!LocationTicketCurrent(10, 11, 1));
    assert(!LocationTicketCurrent(0, 0, 1));
    assert(!LocationTicketCurrent(11, 11, 0));
    /* Merely polling idle/startup cannot prompt or look up anything. */
    PollWindowsLocation(); assert(beginAccess == 0 && beginFix == 0 && posts == 0);
    foreground = 2; RequestWindowsLocation(); assert(beginAccess == 0 && !g_locationPending);

    Reset(); SelectInitial(); ReadyFix();
    g_placeTicket++; /* ordinary weather auto-refresh does not cancel an explicit fix */
    Complete();
    assert(g_placeN == 2 && g_placeSel == 1 && weatherRequests == 1);
    assert(g_places[1].fromWindows && g_places[1].lat == mockSample.lat && g_places[1].lon == mockSample.lon);
    assert(g_places[1].accuracyM == 42.5 && g_places[1].fixEpoch == epoch);
    assert(g_locationNoticeKind == 1 && !g_locationPending && cancelCalls == 1);
    assert(wcsstr(g_locationNotice, L"not live tracking"));
    DWORD consumed = postedTicket; DeliverLocationMessage(consumed); assert(weatherRequests == 1);
    memset(g_places, 0, sizeof(g_places)); g_placeN = 0; g_placeSel = -1; LoadPlaces();
    assert(g_placeN == 2 && g_placeSel == 1 && g_places[1].fromWindows);
    assert(fabs(g_places[1].lat - mockSample.lat) < .000006 && g_places[1].accuracyM == 42.5 && g_places[1].fixEpoch == epoch);
    assert(!g_places[0].fromWindows && isnan(g_places[0].accuracyM) && g_places[0].fixEpoch == 0);

    /* One Windows snapshot is refreshed in place, distinct from search points
       even at identical coordinates. All metadata changes with that snapshot. */
    Place update = g_places[1];
    for (int i = 0; i < 20; i++) {
        update.lat += .001; update.accuracyM += 1; update.fixEpoch++;
        RememberPlace(&update); assert(g_placeN == 2 && g_placeSel == 1);
        assert(g_places[1].lat == update.lat && g_places[1].lon == update.lon);
        assert(g_places[1].accuracyM == update.accuracyM && g_places[1].fixEpoch == update.fixEpoch);
    }
    Place search = SearchPoint(); search.lat = update.lat; search.lon = update.lon;
    RememberPlace(&search); assert(g_placeN == 3 && !g_places[2].fromWindows);
    RememberPlace(&search); assert(g_placeN == 3);
    g_placeSel = 1; DropSelectedPlace();
    assert(g_placeN == 2 && FindKey(L"AccuracyM2") == -1 && FindKey(L"FixEpoch2") == -1 && FindKey(L"Source3") == -1);
    while (g_placeN) { g_placeSel = 0; DropSelectedPlace(); }
    LoadPlaces(); assert(g_placeN == 0 && g_placeSel == -1);
    assert(FindKey(L"AccuracyM1") == -1 && FindKey(L"FixEpoch1") == -1);

    /* Old files (name/region/coords only) remain search points, without an
       invented device-source, accuracy, timestamp, or automatic permission. */
    Reset(); SelectInitial();
    for (int i = 0; i < 128; i++) if (ini[i].used &&
        (wcsstr(ini[i].key, L"Source") || wcsstr(ini[i].key, L"AccuracyM") || wcsstr(ini[i].key, L"FixEpoch"))) ini[i].used = 0;
    LoadPlaces(); assert(!g_places[0].fromWindows && isnan(g_places[0].accuracyM) && !g_places[0].fixEpoch);
    assert(beginAccess == 0);

    const int failures[] = {LOCATION_PROVIDER_DENIED, LOCATION_PROVIDER_UNAVAILABLE, LOCATION_PROVIDER_CANCELED};
    for (int phase = 0; phase < 3; phase++) for (size_t i = 0; i < sizeof(failures)/sizeof(failures[0]); i++) {
        Reset(); SelectInitial();
        if (phase == 0) beginAccessResult = failures[i];
        RequestWindowsLocation();
        if (phase == 1) { accessResult = failures[i]; PollWindowsLocation(); }
        if (phase == 2) {
            accessResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
            fixResult = failures[i]; PollWindowsLocation();
        }
        DeliverLocationMessage(postedTicket);
        assert(g_placeN == 1 && g_placeSel == 0 && weatherRequests == 0 && !g_locationPending);
        assert(wcsstr(g_locationNotice, L"Previous place kept"));
        if (phase < 2) assert(beginFix == 0);
    }
    Reset(); SelectInitial(); RequestWindowsLocation();
    beginFixResult = LOCATION_PROVIDER_DENIED; accessResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
    DeliverLocationMessage(postedTicket); assert(g_placeN == 1 && weatherRequests == 0);

    /* Invalid coordinates/accuracy/time fail closed. A broad but real radius
       is honestly labelled approximate, rather than advertised as GPS. */
    for (int bad = 0; bad < 12; bad++) {
        Reset(); SelectInitial(); ReadyFix();
        switch (bad) {
        case 0: mockSample.lat = NAN; break;
        case 1: mockSample.lon = INFINITY; break;
        case 2: mockSample.lat = 86; break;
        case 3: mockSample.lon = 181; break;
        case 4: mockSample.accuracyM = NAN; break;
        case 5: mockSample.accuracyM = INFINITY; break;
        case 6: mockSample.accuracyM = 0; break;
        case 7: mockSample.accuracyM = -1; break;
        case 8: mockSample.fixEpoch = 0; break;
        case 9: mockSample.fixEpoch = epoch - 121; break;
        case 10: mockSample.fixEpoch = epoch + 16; break;
        case 11: mockSample.lat = -86; break;
        }
        Complete(); assert(g_placeN == 1 && g_placeSel == 0 && weatherRequests == 0);
    }
    Reset(); ReadyFix(); mockSample.accuracyM = 50000; Complete();
    assert(g_places[0].accuracyM == 50000 && g_locationNoticeKind == 2 && wcsstr(g_locationNotice, L"Approximate"));
    Reset(); ReadyFix(); mockSample.accuracyM = .0001; Complete(); LoadPlaces();
    assert(g_places[0].accuracyM == .0001); /* persistence must not round a positive estimate to zero */
    Reset(); ReadyFix(); mockSample.accuracyM = 1e300; Complete(); LoadPlaces();
    assert(isfinite(g_places[0].accuracyM) && g_places[0].accuracyM == 1e300); /* bounded scientific formatting */
    assert(LocationFixProblem(85.05112878, -180, .1, epoch - 120, epoch) == LOCATION_FIX_OK);
    assert(LocationFixProblem(-85.05112878, 180, .1, epoch + 15, epoch) == LOCATION_FIX_OK);

    /* Same-saved-row selection cancels, as does a different search point.
       Execute the actual UI handlers, including their early-return path. */
    Reset(); SelectInitial(); ReadyFix(); DWORD old = g_locationTicket;
    OnPickSavedRow(0); assert(!g_locationPending && g_placeSel == 0 && !weatherRequests);
    WindowsLocationResult late; memset(&late, 0, sizeof(late)); late.have = 1;
    PublishWindowsLocation(old, &late); assert(posts == 0); DeliverLocationMessage(old);
    assert(g_placeN == 1 && !weatherRequests);
    Reset(); SelectInitial(); ReadyFix();
    g_search[0] = SearchPoint(); g_search[0].lat = 51.5; g_searchN = 1;
    OnPickSearchRow(0); assert(!g_locationPending && g_placeN == 2 && g_places[g_placeSel].lat == 51.5);
    assert(weatherRequests == 1);

    /* Cancel after staging still rejects the posted old completion. A new
       request cannot take the old generation's sample, even after wrap. */
    Reset(); SelectInitial(); ReadyFix(); fixResult = LOCATION_PROVIDER_READY; PollWindowsLocation(); old = postedTicket;
    CancelWindowsLocation(); DeliverLocationMessage(old); assert(g_placeN == 1 && weatherRequests == 0);
    RequestWindowsLocation(); assert(beginAccess == 2 && g_locationPending);
    int beforePosts = posts;
    PublishWindowsLocation(old, &late); /* obsolete provider result while a new request is pending */
    assert(posts == beforePosts && g_locationPhase == 1);
    DeliverLocationMessage(old); assert(g_locationPending && !weatherRequests);
    RequestWindowsLocation(); assert(!g_locationPending && beginAccess == 2); /* second click is Cancel */
    g_locationTicket = UINT32_MAX; RequestWindowsLocation(); assert(g_locationTicket == 1);
    CancelWindowsLocation();

    /* A busy UI must revalidate freshness at message delivery, not just when
       the provider finishes. Timeouts and local I/O failures keep the place. */
    Reset(); SelectInitial(); ReadyFix(); fixResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
    epoch += 121; DeliverLocationMessage(postedTicket); assert(!weatherRequests && g_placeN == 1);
    for (int phase = 1; phase <= 2; phase++) {
        Reset(); SelectInitial(); RequestWindowsLocation();
        if (phase == 2) { accessResult = LOCATION_PROVIDER_READY; PollWindowsLocation(); }
        clockMs += phase == 1 ? 120001 : 35001; PollWindowsLocation();
        DeliverLocationMessage(postedTicket);
        assert(!weatherRequests && g_placeN == 1 && !timerActive && !g_locationPending);
        assert(wcsstr(g_locationNotice, L"timed out"));
        if (phase == 1) assert(!beginFix);
    }
    Reset(); SelectInitial(); timerOK = 0; RequestWindowsLocation();
    DeliverLocationMessage(postedTicket); assert(!beginAccess && !weatherRequests && g_placeN == 1);
    Reset(); SelectInitial(); ReadyFix(); postOK = 0; fixResult = LOCATION_PROVIDER_READY; PollWindowsLocation();
    assert(!g_locationPending && !timerActive && !weatherRequests && g_placeN == 1);
    assert(wcsstr(g_locationNotice, L"could not be delivered"));

    puts("Radar one-shot location scenarios passed (mocked Windows provider)");
    return 0;
}
