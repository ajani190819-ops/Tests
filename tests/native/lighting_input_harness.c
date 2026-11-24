/* Execute the production M4/M5 receiver with deterministic Win32 I/O.
   Synthetic packets only: no host input, device, profile or filesystem access.
   These checks do not establish what the owner's keyboard actually emits. */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef int BOOL;
typedef uint32_t DWORD;
typedef unsigned UINT;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint64_t ULONGLONG;
typedef intptr_t LPARAM;
typedef void *HANDLE;
typedef void *HWND;
typedef void *HRAWINPUT;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define RIM_TYPEKEYBOARD 1
#define RIM_TYPEHID 2
#define RIDEV_INPUTSINK 0x100
#define RIDI_DEVICEINFO 0x2000000b
#define RID_INPUT 0x10000003
#define RI_KEY_BREAK 1
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_DATA 13
#define ERROR_INVALID_PARAMETER 87
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_INVALID_WINDOW_HANDLE 1400
#define _snprintf snprintf

typedef struct { WORD usUsagePage, usUsage; DWORD dwFlags; HWND hwndTarget; } RAWINPUTDEVICE;
typedef struct { HANDLE hDevice; DWORD dwType; } RAWINPUTDEVICELIST;
typedef struct {
    DWORD cbSize, dwType;
    struct { DWORD dwVendorId; WORD usUsagePage, usUsage; } hid;
} RID_DEVICE_INFO;
typedef struct { DWORD dwType, dwSize; HANDLE hDevice; uintptr_t wParam; } RAWINPUTHEADER;
typedef struct { WORD MakeCode, Flags, Reserved, VKey; DWORD Message, ExtraInformation; } RAWKEYBOARD;
typedef struct { DWORD dwSizeHid, dwCount; BYTE bRawData[1]; } RAWHID;
typedef struct { RAWINPUTHEADER header; union { RAWKEYBOARD keyboard; RAWHID hid; } data; } RAWINPUT;

static const char *scenario;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s, C line %d: %s\n", scenario, __LINE__, #c); exit(1); } } while (0)
static ULONGLONG tick;
static DWORD lastError;
static short keyState;
static int enumerateMode, registerMode, listQueries, listFills, infoCalls, registerCalls;
static UINT requestedCount;
static int snapshotWrites, tokenWrites[2], profileWrites, modeWrites;
static BOOL writeOk;
static char tokens[2][32], modeText[32], snapshot[2048];
static wchar_t snapshotPath[MAX_PATH];
static BYTE packet[256];
static UINT packetSize;
static HWND target = (HWND)(uintptr_t)123;
static wchar_t g_cfgDir[MAX_PATH] = L"C:\\Synthetic\\KeyboardLighting";
static const wchar_t *g_brightDownPath = L"down.flag";
static const wchar_t *g_brightUpPath = L"up.flag";
static const wchar_t *g_holdModePath = L"hold_mode.txt";

static ULONGLONG GetTickCount64(void) { return tick; }
static DWORD GetCurrentProcessId(void) { return 42; }
static DWORD GetLastError(void) { return lastError; }
static short GetAsyncKeyState(int vk) { CHECK(vk > 0 && vk < 255); return keyState; }
/* Adapt the Windows wide-printf %s spelling used by existing binding code. */
static int _snwprintf(wchar_t *out, size_t cap, const wchar_t *format, ...) {
    wchar_t portable[512];
    size_t n = 0;
    for (size_t i = 0; format[i] && n + 3 < 512; i++) {
        portable[n++] = format[i];
        if (format[i] == L'%' && format[i + 1] == L's') portable[n++] = L'l';
    }
    portable[n] = 0;
    va_list args;
    va_start(args, format);
    int result = vswprintf(out, cap, portable, args);
    va_end(args);
    return result;
}
static char *ReadFileUtf8(const wchar_t *path, DWORD *length) {
    const char *text = path == g_brightDownPath ? tokens[0] :
                       path == g_brightUpPath ? tokens[1] : modeText;
    CHECK(path == g_brightDownPath || path == g_brightUpPath || path == g_holdModePath);
    *length = (DWORD)strlen(text);
    if (!*length) return NULL;
    char *out = malloc(*length + 1);
    CHECK(out != NULL);
    memcpy(out, text, *length + 1);
    return out;
}
static BOOL WriteAtomicUtf8(const wchar_t *path, const char *text) {
    CHECK(path && text);
    if (path == g_brightDownPath || path == g_brightUpPath) {
        int dir = path == g_brightUpPath;
        tokenWrites[dir]++;
        if (writeOk) snprintf(tokens[dir], sizeof(tokens[dir]), "%s", text);
    } else if (path == g_holdModePath) {
        modeWrites++;
        if (writeOk) snprintf(modeText, sizeof(modeText), "%s", text);
    } else {
        CHECK(wcscmp(path, L"C:\\Synthetic\\KeyboardLighting\\input-status.txt") == 0);
        CHECK(strlen(text) < sizeof(snapshot));
        CHECK(wcslen(path) + 4 < MAX_PATH);
        snapshotWrites++;
        if (writeOk) {
            wcscpy(snapshotPath, path);
            strcpy(snapshot, text);
        }
    }
    return writeOk;
}
static BOOL WritePrivateProfileStringW(const wchar_t *section, const wchar_t *key,
                                       const wchar_t *value, const wchar_t *path) {
    CHECK(wcscmp(section, L"Keys") == 0 && key[0] && value[0]);
    CHECK(wcsstr(path, L"\\presets.ini") != NULL);
    profileWrites++;
    return TRUE;
}
static UINT GetRawInputDeviceList(RAWINPUTDEVICELIST *list, UINT *count, UINT size) {
    CHECK(size == sizeof(*list));
    if (!list) {
        listQueries++;
        *count = enumerateMode == 3 ? 65 : enumerateMode == 4 ? 64 : 4;
        if (enumerateMode == 1) { lastError = ERROR_ACCESS_DENIED; return (UINT)-1; }
        return 0;
    }
    listFills++;
    CHECK(*count <= 64 && *count >= 4);
    if (enumerateMode == 2) {
        *count = 96; lastError = ERROR_INSUFFICIENT_BUFFER; return (UINT)-1;
    }
    if (enumerateMode == 5) return 65; /* Reject an invalid success count too. */
    UINT found = enumerateMode == 4 ? 64 : 4;
    CHECK(*count >= found);
    for (UINT i = 0; i < found; i++) {
        list[i].hDevice = (HANDLE)(uintptr_t)(i + 1);
        list[i].dwType = RIM_TYPEHID;
    }
    *count = found;
    return found;
}
static UINT GetRawInputDeviceInfoW(HANDLE device, UINT command, RID_DEVICE_INFO *info, UINT *size) {
    CHECK(command == RIDI_DEVICEINFO && *size == sizeof(*info) && info->cbSize == sizeof(*info));
    /* A failure/oversized return is not a list. This assertion diagnoses the
       old path without depending on an out-of-bounds crash or uninitialized data. */
    CHECK(enumerateMode != 1 && enumerateMode != 2 && enumerateMode != 3 && enumerateMode != 5);
    uintptr_t id = (uintptr_t)device;
    CHECK(id > 0 && id <= 64);
    infoCalls++;
    info->dwType = RIM_TYPEHID;
    info->hid.dwVendorId = id <= 3 ? 0x0B05 : 0x1234;
    info->hid.usUsagePage = id == 1 ? 1 : 0xFF31;
    info->hid.usUsage = id == 1 ? 6 : 0x74;
    return sizeof(*info);
}
static BOOL RegisterRawInputDevices(const RAWINPUTDEVICE *devices, UINT count, UINT size) {
    CHECK(size == sizeof(*devices) && count > 0 && count <= 16);
    CHECK(devices[0].usUsagePage == 1 && devices[0].usUsage == 6);
    for (UINT i = 0; i < count; i++) {
        CHECK(devices[i].dwFlags == RIDEV_INPUTSINK && devices[i].hwndTarget == target);
        for (UINT j = 0; j < i; j++)
            CHECK(devices[i].usUsagePage != devices[j].usUsagePage || devices[i].usUsage != devices[j].usUsage);
    }
    registerCalls++;
    requestedCount = count;
    if (registerMode && registerCalls == 1) { lastError = ERROR_INVALID_PARAMETER; return FALSE; }
    if (registerMode == 2) { lastError = ERROR_ACCESS_DENIED; return FALSE; }
    return TRUE;
}
static UINT GetRawInputData(HRAWINPUT input, UINT command, void *out, UINT *size, UINT headerSize) {
    CHECK(input == (HRAWINPUT)(uintptr_t)1 && command == RID_INPUT && headerSize == sizeof(RAWINPUTHEADER));
    if (!out) { *size = packetSize; return 0; }
    CHECK(*size >= packetSize);
    memcpy(out, packet, packetSize);
    *size = packetSize;
    return packetSize;
}

#ifndef LIGHTING_INPUT_PRODUCTION
#define LIGHTING_INPUT_PRODUCTION "../../keyboard-lighting/src/parts/m4_m5_hold.c"
#endif
#include LIGHTING_INPUT_PRODUCTION

static void Reset(void) {
    memset(&g_holdInputDiag, 0, sizeof(g_holdInputDiag));
    memset(g_keyRing, 0, sizeof(g_keyRing));
    memset(g_hidRing, 0, sizeof(g_hidRing));
    memset(g_seenTok, 0, sizeof(g_seenTok));
    memset(g_scanBound, 0, sizeof(g_scanBound));
    memset(g_scanVk, 0, sizeof(g_scanVk));
    memset(g_hidCode, 0, sizeof(g_hidCode));
    memset(g_hidRelSeen, 0, sizeof(g_hidRelSeen));
    memset(g_holdOn, 0, sizeof(g_holdOn));
    memset(g_hidDown, 0, sizeof(g_hidDown));
    memset(g_sigAt, 0, sizeof(g_sigAt));
    memset(g_scanNext, 0, sizeof(g_scanNext));
    memset(g_learnCand, 0, sizeof(g_learnCand));
    memset(g_learnN, 0, sizeof(g_learnN));
    memset(g_learnHid, 0, sizeof(g_learnHid));
    memset(g_learnHidN, 0, sizeof(g_learnHidN));
    memset(g_pendingLearnAt, 0, sizeof(g_pendingLearnAt));
    memset(g_pendingLearnUntil, 0, sizeof(g_pendingLearnUntil));
    memset(tokens, 0, sizeof(tokens));
    memset(tokenWrites, 0, sizeof(tokenWrites));
    g_scanArmed[0] = g_scanArmed[1] = 1;
    g_keyRingHead = g_hidRingHead = 0;
    g_hidLastCode = 0;
    g_holdDiagAt = 0;
    tick = 100000;
    lastError = 0;
    keyState = (short)0x8000;
    enumerateMode = registerMode = listQueries = listFills = infoCalls = registerCalls = 0;
    snapshotWrites = profileWrites = modeWrites = 0;
    snapshot[0] = modeText[0] = 0;
    writeOk = TRUE;
    wcscpy(g_cfgDir, L"C:\\Synthetic\\KeyboardLighting");
}
static void Keyboard(WORD scan, WORD vk, int down) {
    RAWINPUT input = {0};
    input.header.dwType = RIM_TYPEKEYBOARD;
    input.data.keyboard.MakeCode = scan;
    input.data.keyboard.VKey = vk;
    input.data.keyboard.Flags = down ? 0 : RI_KEY_BREAK;
    packetSize = sizeof(input);
    memcpy(packet, &input, packetSize);
    OnRawInputMsg(target, 1);
}
static void Hid(const BYTE *bytes, UINT count) {
    RAWINPUT input = {0};
    input.header.dwType = RIM_TYPEHID;
    input.data.hid.dwSizeHid = 2;
    input.data.hid.dwCount = count;
    size_t offset = offsetof(RAWINPUT, data.hid.bRawData);
    packetSize = (UINT)offset + 2 * count;
    CHECK(packetSize <= sizeof(packet));
    memcpy(packet, &input, offset);
    memcpy(packet + offset, bytes, 2 * count);
    OnRawInputMsg(target, 1);
}
static void Token(int dir) { snprintf(tokens[dir], sizeof(tokens[dir]), "%llu", (unsigned long long)tick); }
static void Field(const char *key, unsigned long long value) {
    char wanted[128];
    snprintf(wanted, sizeof(wanted), "\r\n%s=%llu\r\n", key, value);
    CHECK(strstr(snapshot, wanted) != NULL);
}
static void TestEnumeration(void) {
    Reset(); InitRawInput(target);
    CHECK(listQueries == 1 && listFills == 1 && infoCalls == 4 && registerCalls == 1);
    CHECK(requestedCount == 2 && g_holdInputDiag.requestedUsages == 2);
    CHECK(g_holdInputDiag.keyboardReady && g_holdInputDiag.vendorUsages == 1);
    CHECK(!g_holdInputDiag.enumerateError && !g_holdInputDiag.registrationError);
    Reset(); enumerateMode = 4; InitRawInput(target);
    CHECK(infoCalls == 64 && g_holdInputDiag.keyboardReady && g_holdInputDiag.vendorUsages == 1);
}
static void TestEnumerationErrors(void) {
    for (int mode = 1; mode <= 5; mode++) {
        if (mode == 4) continue;
        Reset(); enumerateMode = mode; InitRawInput(target);
        CHECK(infoCalls == 0 && registerCalls == 1 && requestedCount == 1);
        CHECK(listFills == (mode == 2 || mode == 5));
        CHECK(g_holdInputDiag.keyboardReady && !g_holdInputDiag.vendorUsages);
        CHECK(g_holdInputDiag.enumerateError == (DWORD)(mode == 1 ? ERROR_ACCESS_DENIED :
              mode == 5 ? ERROR_INVALID_DATA : ERROR_INSUFFICIENT_BUFFER));
    }
}
static void TestRegistration(void) {
    Reset(); registerMode = 1; InitRawInput(target);
    CHECK(registerCalls == 2 && requestedCount == 1 && g_holdInputDiag.keyboardReady);
    CHECK(!g_holdInputDiag.vendorUsages && g_holdInputDiag.requestedUsages == 2);
    CHECK(g_holdInputDiag.registrationError == ERROR_INVALID_PARAMETER);
    Reset(); registerMode = 2; InitRawInput(target);
    CHECK(registerCalls == 2 && !g_holdInputDiag.keyboardReady && !g_holdInputDiag.vendorUsages);
    CHECK(g_holdInputDiag.registrationError == ERROR_ACCESS_DENIED);
    Reset(); InitRawInput(NULL);
    CHECK(!registerCalls && !listQueries && !g_holdInputDiag.keyboardReady);
    CHECK(g_holdInputDiag.registrationError == ERROR_INVALID_WINDOW_HANDLE);
}
static void TestHold(void) {
    for (int dir = 0; dir < 2; dir++) {
        Reset(); InitRawInput(target); SeedBrightTokenWatch();
        Keyboard((WORD)(0x70 + dir), 255, TRUE); Token(dir); WatchBrightTokens();
        CHECK(g_scanBound[dir] == 0x70 + dir && g_scanVk[dir] == 255);
        CHECK(g_holdOn[dir] && !tokenWrites[dir] && profileWrites == 2);
        CHECK(g_holdInputDiag.tokenObservations[dir] == 1);
        tick += 29; HoldPump(); CHECK(!tokenWrites[dir]);
        tick++; HoldPump(); CHECK(tokenWrites[dir] == 1 && g_holdInputDiag.repeatWrites[dir] == 1);
        WatchBrightTokens(); CHECK(g_holdInputDiag.tokenObservations[dir] == 2);
        Keyboard((WORD)(0x70 + dir), 255, FALSE);
        tick += 30; HoldPump(); CHECK(!g_holdOn[dir] && tokenWrites[dir] == 1);
        CHECK(g_holdInputDiag.keyboardReports == 2);
        WriteHoldDiagnostics(TRUE);
        Field(dir ? "scan_bound_up" : "scan_bound_down", 1);
        Field(dir ? "release_source_up" : "release_source_down", 1);
        Field(dir ? "hold_active_up" : "hold_active_down", 0);
    }
}
static void TestHidBatch(void) {
    const BYTE pair[] = { 0x5A, 0x38, 0x5A, 0x00 };
    const BYTE press[] = { 0x5A, 0x38 }, release[] = { 0x5A, 0xB8 };
    Reset(); InitRawInput(target); SeedBrightTokenWatch();
    Hid(pair, 2); Token(0); WatchBrightTokens();
    CHECK(g_hidCode[0] == 0x38 && g_hidRelSeen[0] == 1 && !g_holdOn[0]);
    CHECK(g_holdInputDiag.asusReports == 2 && !g_holdInputDiag.keyboardReports);
    Hid(press, 1); CHECK(g_holdOn[0]);
    tick += 30; HoldPump(); CHECK(tokenWrites[0] == 1);
    Hid(release, 1); CHECK(!g_holdOn[0] && !g_hidDown[0]);
    WriteHoldDiagnostics(TRUE);
    Field("asus_reports", 4); Field("hid_bound_down", 1); Field("release_source_down", 1);
}
static void TestSnapshot(void) {
    static const char *keys[] = {
        "panel_pid", "sample_tick_ms", "keyboard_registered", "requested_usages",
        "vendor_usages_registered", "enumeration_error", "registration_error",
        "keyboard_reports", "asus_reports", "token_observations_down", "token_observations_up",
        "repeat_tokens_written_down", "repeat_tokens_written_up", "scan_bound_down", "scan_bound_up",
        "hid_bound_down", "hid_bound_up", "release_source_down", "release_source_up",
        "hold_active_down", "hold_active_up"
    };
    Reset(); InitRawInput(target);
    g_scanBound[0] = 987654; g_hidCode[1] = 456789; g_hidRelSeen[1] = 1;
    WriteHoldDiagnostics(TRUE); CHECK(snapshotWrites == 1);
    CHECK(strncmp(snapshot, "schema=tests.lighting-input.v1\r\n", 32) == 0);
    CHECK(strstr(snapshot, "987654") == NULL && strstr(snapshot, "456789") == NULL);
    CHECK(strstr(snapshot, "C:\\") == NULL && strlen(snapshot) < 1024);
    Field("panel_pid", 42); Field("scan_bound_down", 1); Field("hid_bound_up", 1);
    Field("release_source_down", 1); Field("release_source_up", 1);
    char copy[2048]; strcpy(copy, snapshot);
    size_t lines = 0;
    for (char *line = strtok(copy, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (!lines++) continue;
        char *equals = strchr(line, '='); CHECK(equals != NULL); *equals++ = 0;
        BOOL allowed = FALSE;
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
            if (strcmp(line, keys[i]) == 0) allowed = TRUE;
        CHECK(allowed && equals[0]);
        for (; *equals; equals++) CHECK(*equals >= '0' && *equals <= '9');
    }
    CHECK(lines == 1 + sizeof(keys) / sizeof(keys[0]));
    tick += 59999; WriteHoldDiagnostics(FALSE); CHECK(snapshotWrites == 1);
    tick++; WriteHoldDiagnostics(FALSE); CHECK(snapshotWrites == 2);
    CHECK(!profileWrites && !modeWrites && !tokenWrites[0] && !tokenWrites[1]);
    writeOk = FALSE; tick += 60000; WriteHoldDiagnostics(FALSE); CHECK(snapshotWrites == 3);
    WriteHoldDiagnostics(FALSE); CHECK(snapshotWrites == 3);
    CHECK(g_scanBound[0] == 987654 && g_hidCode[1] == 456789);
    CHECK(!profileWrites && !modeWrites && !tokenWrites[0] && !tokenWrites[1]);
    for (int i = 0; i < MAX_PATH - 1; i++) g_cfgDir[i] = L'x';
    g_cfgDir[MAX_PATH - 1] = 0;
    tick += 60000; WriteHoldDiagnostics(FALSE); CHECK(snapshotWrites == 3);
}
/* Latch/burst consume one Custom action per press. Native hold repeats
   otherwise toggle a latch repeatedly or restart a burst on every tick. */
static void TestNonHoldModes(void) {
    for (int mode = 1; mode <= 2; mode++) {
        for (int dir = 0; dir < 2; dir++) {
            /* Keyboard, HID high-bit release, and HID zero release. */
            for (int hid = 0; hid < 3; hid++) {
                Reset(); InitRawInput(target); SeedBrightTokenWatch(); HoldModeWrite(mode);
                BYTE press[] = { 0x5A, (BYTE)(0x38 + dir) };
                BYTE release[] = { 0x5A, hid == 2 ? 0 : (BYTE)(0xB8 + dir) };
                if (hid) Hid(press, 1);
                else Keyboard((WORD)(0x70 + dir), 255, TRUE);
                Token(dir); WatchBrightTokens();
                CHECK(g_holdOn[dir]); /* Real input still learns the binding. */
                CHECK(!g_scanBound[1 - dir] && !g_hidCode[1 - dir]);
                char initial[32]; strcpy(initial, tokens[dir]);
                int savedProfiles = profileWrites, savedModes = modeWrites;
                for (int n = 0; n < 12; n++) {
                    tick += 30;
                    if (hid) Hid(press, 1);
                    else Keyboard((WORD)(0x70 + dir), 255, TRUE);
                    WatchBrightTokens(); HoldPump();
                    if (hid) CHECK(g_hidDown[dir]); /* Suppress repeats, not release tracking. */
                }
                CHECK(!tokenWrites[0] && !tokenWrites[1]);
                CHECK(!g_holdOn[0] && !g_holdOn[1]);
                CHECK(strcmp(initial, tokens[dir]) == 0);
                CHECK(profileWrites == savedProfiles && modeWrites == savedModes);
                CHECK(HoldModeRead() == mode);
                if (hid) Hid(release, 1);
                else Keyboard((WORD)(0x70 + dir), 255, FALSE);
                CHECK(g_scanArmed[dir] && !g_holdOn[dir]);
                if (hid) CHECK(g_hidCode[dir] == (DWORD)(0x38 + dir) && g_hidRelSeen[dir]);
                else CHECK(g_scanBound[dir] == 0x70 + dir && g_scanVk[dir] == 255);
            }
        }
    }
    /* A mode change during a real hold stops repeats without resetting the
       learned key, and switching back still permits a fresh tap-mode hold. */
    for (int dir = 0; dir < 2; dir++) {
        Reset(); InitRawInput(target); SeedBrightTokenWatch();
        Keyboard((WORD)(0x70 + dir), 255, TRUE); Token(dir); WatchBrightTokens();
        tick += 30; HoldPump(); CHECK(tokenWrites[dir] == 1);
        HoldModeWrite(1); tick += 30; HoldPump();
        CHECK(tokenWrites[dir] == 1 && !g_holdOn[dir]);
        Keyboard((WORD)(0x70 + dir), 255, FALSE);
        HoldModeWrite(0); tick++;
        Keyboard((WORD)(0x70 + dir), 255, TRUE); Token(dir); WatchBrightTokens();
        tick += 30; HoldPump(); CHECK(tokenWrites[dir] == 2 && g_holdOn[dir]);
        Keyboard((WORD)(0x70 + dir), 255, FALSE);
        tick += 30; HoldPump(); CHECK(tokenWrites[dir] == 2 && !g_holdOn[dir]);
    }
    Reset(); HoldModeWrite(1);
    g_scanBound[0] = 0x70; g_scanBound[1] = 0x71;
    g_scanVk[0] = g_scanVk[1] = 255;
    PressSignal(0, 0, tick); PressSignal(1, 0, tick);
    tick += 30; HoldPump();
    CHECK(!g_holdOn[0] && !g_holdOn[1] && !tokenWrites[0] && !tokenWrites[1]);
}
static void TestModesAndCeiling(void) {
    Reset();
    CHECK(wcscmp(HoldReadyLabel(), L"M4/M5 Keys: Not Recognised") == 0);
    CHECK(kHoldLabels[0][0] && kHoldLabels[1][0] && kHoldLabels[2][0]);
    for (int mode = 0; mode < 3; mode++) { HoldModeWrite(mode); CHECK(HoldModeRead() == mode); }
    HoldModeWrite(9); CHECK(HoldModeRead() == 0);
    g_scanBound[0] = 1; g_scanVk[0] = 65;
    PressSignal(0, 0, tick); CHECK(g_holdOn[0]);
    keyState = 0; tick += 30; HoldPump(); CHECK(!g_holdOn[0] && !tokenWrites[0]);
    g_scanVk[0] = 255; PressSignal(0, 0, tick);
    tick += 10001; HoldPump();
    CHECK(!g_holdOn[0] && g_scanArmed[0] && !g_hidDown[0] && !tokenWrites[0]);
    writeOk = FALSE; HoldTokenWrite(0);
    CHECK(tokenWrites[0] == 1 && !g_holdInputDiag.repeatWrites[0]);
}
int main(int argc, char **argv) {
    CHECK(argc == 2); scenario = argv[1];
    if (!strcmp(scenario, "enumeration")) TestEnumeration();
    else if (!strcmp(scenario, "enumeration-errors")) TestEnumerationErrors();
    else if (!strcmp(scenario, "registration")) TestRegistration();
    else if (!strcmp(scenario, "hold")) TestHold();
    else if (!strcmp(scenario, "hid-batch")) TestHidBatch();
    else if (!strcmp(scenario, "snapshot")) TestSnapshot();
    else if (!strcmp(scenario, "modes-ceiling")) TestModesAndCeiling();
    else if (!strcmp(scenario, "non-hold-modes")) TestNonHoldModes();
    else CHECK(FALSE);
    printf("Lighting input %s passed\n", scenario);
    return 0;
}
