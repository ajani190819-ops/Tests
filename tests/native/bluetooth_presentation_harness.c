/* Execute the real, read-only device presenter. Not a native Windows/DPI test. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef int BOOL;
typedef uint32_t DWORD;
typedef uint32_t COLORREF;
typedef uint64_t ULONGLONG;
typedef int32_t HRESULT;
#define TRUE 1
#define FALSE 0
#define RGB(r,g,b) ((COLORREF)((r) | ((g) << 8) | ((b) << 16)))
#define _snwprintf swprintf
static int WcsIcmp(const wchar_t *a, const wchar_t *b) {
    while (*a && towlower(*a) == towlower(*b)) { a++; b++; }
    return (int)towlower(*a) - (int)towlower(*b);
}
#define _wcsicmp WcsIcmp

#include "bluetooth_presentation_production.h"

static const char *scenario;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s, C line %d: %s\n", scenario, __LINE__, #c); exit(1); } } while (0)
#define TEXT(a,b) (wcscmp((a), (b)) == 0)
static const ULONGLONG now = 20000;
static DeviceEntry PairedAudio(void) {
    DeviceEntry d;
    memset(&d, 0, sizeof(d));
    d.address = 0xAABBCCDDEEFFULL;
    wcscpy(d.kind, L"Audio"); d.paired = d.visible = d.allowAuto = TRUE;
    return d;
}
static BtDeviceVisual Describe(const DeviceEntry *d, BOOL radio) {
    DeviceEntry before = *d;
    BtDeviceVisual v;
    memset(&v, 0x7F, sizeof(v));
    DescribeDevice(d, radio, now, &v);
    CHECK(memcmp(d, &before, sizeof(*d)) == 0);
    CHECK(wcslen(v.detail) < 128 && v.status && v.preference);
    return v;
}
static void Connections(void) {
    DeviceEntry d = PairedAudio(); BtDeviceVisual v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Paired") && v.statusFg == COL_ACCENT && v.statusBg == COL_ACCENT_BG);
    CHECK(TEXT(v.preference, L"Auto-Connect on"));
    d.allowAuto = FALSE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Paired") && v.statusFg == COL_ACCENT);
    CHECK(TEXT(v.preference, L"Auto-Connect off"));
    d.audioConnected = TRUE; v = Describe(&d, TRUE);
    CHECK(v.statusFg != COL_GOOD); /* invalid/stale observation is not ready */
    d.audioStatusValid = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Audio connected") && v.statusFg == COL_GOOD && v.statusBg == COL_GOOD_BG);
    d.allowAuto = d.autoPaused = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Audio connected") && v.statusFg == COL_GOOD);
    CHECK(TEXT(v.preference, L"Auto-Connect paused") && v.preferenceFg == COL_WARN);
    CHECK(TEXT(v.detail, L"Windows audio endpoint connected"));
    v = Describe(&d, FALSE);
    CHECK(TEXT(v.status, L"Radio unavailable") && v.statusFg == COL_MUTED);
    CHECK(wcsstr(v.detail, L"saved choices kept"));
    d = PairedAudio(); d.connected = d.linkConnected = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Link only") && v.statusFg == COL_ACCENT);
    CHECK(TEXT(v.detail, L"Bluetooth link only; audio not connected"));
    d.connectState = BT_CONNECT_REQUESTED; d.connectRequestTick = now - 500;
    v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Connecting...") && v.statusFg != COL_GOOD);
    CHECK(TEXT(v.detail, L"Request sent; waiting for Windows audio"));
    d.connectRequestTick = now - 10000; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Link only"));
    wcscpy(d.kind, L"Phone"); d.connectState = BT_CONNECT_WINDOWS;
    v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Connected") && v.statusFg == COL_GOOD);
    CHECK(wcsstr(v.detail, L"Windows manages this device"));
    d.connected = FALSE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Paired") && v.statusFg == COL_ACCENT);
    puts("Bluetooth connection and preference colors passed");
}
static void Approval(void) {
    DeviceEntry d = PairedAudio(); d.visible = FALSE;
    d.audioConnected = d.audioStatusValid = TRUE;
    BtDeviceVisual v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Needs approval") && v.statusFg == COL_WARN && v.statusBg == COL_WARN_BG);
    CHECK(TEXT(v.preference, L"Approval required"));
    d.paired = FALSE; d.remembered = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Needs approval") && wcsstr(v.detail, L"pairing unconfirmed"));
    d.denied = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Denied") && v.statusFg == COL_DANGER && v.statusBg == COL_DANGER_BG);
    CHECK(TEXT(v.preference, L"Auto-Connect disabled") && wcsstr(v.detail, L"pairing kept"));
    d = PairedAudio(); d.paired = d.allowAuto = FALSE; d.remembered = TRUE;
    v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Saved device") && v.statusFg == COL_MUTED);
    d.allowAuto = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Auto configured") && v.statusFg == COL_ACCENT);
    puts("Bluetooth approval and saved-only distinctions passed");
}
static void Details(void) {
    DeviceEntry d = PairedAudio(); d.connectState = BT_CONNECT_FAILED;
    d.connectError = (HRESULT)0x80070005u;
    BtDeviceVisual v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Paired") && v.statusFg == COL_ACCENT);
    CHECK(TEXT(v.detail, L"Connect failed (0x80070005); retry pending"));
    d.allowAuto = FALSE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.detail, L"Connect failed (0x80070005); try Windows Connect"));
    d.connectState = BT_CONNECT_NO_ENDPOINT; v = Describe(&d, TRUE);
    CHECK(TEXT(v.detail, L"No address-matched audio endpoint"));
    d.connectState = BT_CONNECT_IDLE; d.allowAuto = d.autoPaused = TRUE;
    v = Describe(&d, TRUE);
    CHECK(TEXT(v.status, L"Paired") && TEXT(v.preference, L"Auto-Connect paused"));
    CHECK(TEXT(v.detail, L"Automatic retry paused; Connect resumes it"));
    d.allowAuto = FALSE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.preference, L"Auto-Connect off") && wcsstr(v.detail, L"one session"));
    d.autoPaused = FALSE; d.allowAuto = TRUE; v = Describe(&d, TRUE);
    CHECK(TEXT(v.detail, L"Waiting to retry; up to 60s between attempts"));
    puts("Bluetooth request, error and pause details passed");
}
static void Matrix(void) {
    unsigned checked = 0;
    for (unsigned bits = 0; bits < 16384; bits++) {
        DeviceEntry d = PairedAudio();
        d.visible = !!(bits & 1); d.paired = !!(bits & 2); d.remembered = !!(bits & 4);
        d.denied = !!(bits & 8); d.allowAuto = !!(bits & 16); d.autoPaused = !!(bits & 32);
        d.connected = !!(bits & 64); d.audioConnected = !!(bits & 128);
        d.audioStatusValid = !!(bits & 256);
        wcscpy(d.kind, bits & 512 ? L"Audio" : L"Phone");
        BOOL radio = !!(bits & 1024);
        d.connectState = (int)((bits >> 11) & 3);
        d.connectRequestTick = now - 50;
        d.audioEndpointFound = !!(bits & 8192);
        if (!d.visible && !d.denied && !d.paired && !d.remembered) continue;
        BtDeviceVisual v = Describe(&d, radio);
        BOOL audio = !!(bits & 512) || d.audioEndpointFound;
        BOOL ready = (d.audioStatusValid && d.audioConnected) || (!audio && d.connected);
        if (d.denied) CHECK(v.statusFg == COL_DANGER);
        else if (!d.visible && (d.paired || d.remembered)) CHECK(v.statusFg == COL_WARN);
        else if (!radio) CHECK(v.statusFg == COL_MUTED);
        else if (ready) CHECK(v.statusFg == COL_GOOD);
        else {
            CHECK(v.statusFg != COL_GOOD);
            if (d.paired || d.allowAuto || (audio && d.connected) || d.connectState == BT_CONNECT_REQUESTED)
                CHECK(v.statusFg == COL_ACCENT);
            else CHECK(v.statusFg == COL_MUTED);
        }
        checked++;
    }
    CHECK(checked == 15360);
    puts("Bluetooth 15360 read-only presentation combinations passed");
}
int main(int argc, char **argv) {
    CHECK(argc == 2); scenario = argv[1];
    if (!strcmp(scenario, "connection")) Connections();
    else if (!strcmp(scenario, "approval")) Approval();
    else if (!strcmp(scenario, "details")) Details();
    else if (!strcmp(scenario, "matrix")) Matrix();
    else CHECK(0);
    return 0;
}
