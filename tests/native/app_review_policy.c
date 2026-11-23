/* Portable tests of the exact policy compiled into AppReview.exe. */
#include "../../app-review/src/review_policy.h"
#include <assert.h>
#include <string.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "gates")) {
        ReviewConditions c = {1, 1, 0, 0, 1, 0, 800, 600};
        assert(ReviewGate(c) == REVIEW_IMAGE);
        c.identity = 0; assert(ReviewGate(c) == REVIEW_IDENTITY); c.identity = 1;
        c.visible = 0; assert(ReviewGate(c) == REVIEW_HIDDEN); c.visible = 1;
        c.minimized = 1; assert(ReviewGate(c) == REVIEW_MINIMIZED); c.minimized = 0;
        c.cloaked = 1; assert(ReviewGate(c) == REVIEW_CLOAKED); c.cloaked = 0;
        c.queries_ok = 0; assert(ReviewGate(c) == REVIEW_QUERY_FAILED); c.queries_ok = 1;
        const unsigned values[] = {1, 0x11, 2, 0xFFFFFFFFu};
        for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
            c.affinity = values[i]; assert(ReviewGate(c) == REVIEW_PROTECTED);
        }
        c.affinity = 0;
        c.width = 0; assert(ReviewGate(c) == REVIEW_GEOMETRY);
        /* No other combination of the safety flags may authorize capture. */
        for (unsigned mask = 0; mask < 64; ++mask) {
            c.identity = !!(mask & 1); c.visible = !!(mask & 2); c.minimized = !!(mask & 4);
            c.cloaked = !!(mask & 8); c.queries_ok = !!(mask & 16); c.affinity = mask & 32;
            c.width = 800;
            assert((ReviewGate(c) == REVIEW_IMAGE) == (mask == 19));
        }
    } else if (!strcmp(argv[1], "dimensions")) {
        assert(ReviewDimensions(1, 1)); assert(ReviewDimensions(4000, 4000));
        assert(ReviewDimensions(8192, 1)); assert(ReviewDimensions(1, 8192));
        assert(!ReviewDimensions(8193, 1)); assert(!ReviewDimensions(1, 8193));
        assert(!ReviewDimensions(-1, 600)); assert(!ReviewDimensions(800, 0));
        assert(!ReviewDimensions(4001, 4000)); assert(!ReviewDimensions(2147483647, 2147483647));
    } else if (!strcmp(argv[1], "numbers")) {
        uint64_t n = 77;
        assert(ReviewParseNumber(L"18446744073709551615", UINT64_MAX, &n) && n == UINT64_MAX);
        assert(!ReviewParseNumber(L"18446744073709551616", UINT64_MAX, &n));
        assert(ReviewParseNumber(L"0", 0, &n) && n == 0);
        assert(!ReviewParseNumber(L"1", 0, &n));
        assert(ReviewParseNumber(L"15", 15, &n) && n == 15);
        assert(!ReviewParseNumber(L"16", 15, &n));
        const wchar_t *bad[] = {L"", L"-1", L"+1", L" 1", L"1 ", L"0x10", L"1.0", L"1&calc", L"999999999999999999999999999"};
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) assert(!ReviewParseNumber(bad[i], UINT64_MAX, &n));
    } else if (!strcmp(argv[1], "apps")) {
        assert(ReviewKnownApp(L"ProjectHub.exe")); assert(ReviewKnownApp(L"WEATHERRADAR.EXE"));
        assert(ReviewKnownApp(L"KeyboardLighting.exe")); assert(ReviewKnownApp(L"BluetoothGuard.exe"));
        assert(ReviewKnownApp(L"DnsPilot.exe")); assert(ReviewKnownApp(L"AgentNotes.exe"));
        assert(ReviewKnownApp(L"AudioSwitcher.exe"));
        const wchar_t *bad[] = {L"", L"Hub.exe", L"ProjectHub.exe.old", L"FakeProjectHub.exe", L"C:\\ProjectHub.exe", L"ProjectHub.ex"};
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) assert(!ReviewKnownApp(bad[i]));
    } else if (!strcmp(argv[1], "pixels")) {
        uint32_t p[200];
        for (int i = 0; i < 200; ++i) p[i] = 0;
        assert(!ReviewPixelsUsable(p, 200));
        for (int i = 0; i < 200; ++i) p[i] = 0xFFFFFFFFu;
        assert(!ReviewPixelsUsable(p, 200));
        for (int i = 0; i < 200; ++i) p[i] = i & 1 ? 0xFF101010u : 0x00111111u;
        assert(!ReviewPixelsUsable(p, 200));
        for (int i = 0; i < 200; ++i) p[i] = i & 1 ? 0xFF101010u : 0x00EFEFEFu;
        assert(ReviewPixelsUsable(p, 200));
        for (int i = 0; i < 200; ++i) p[i] = 0;
        p[1] = p[2] = 0x00FFFFFFu; assert(!ReviewPixelsUsable(p, 200));
        p[3] = 0x00FFFFFFu; assert(ReviewPixelsUsable(p, 200));
        assert(!ReviewPixelsUsable(NULL, 200)); assert(!ReviewPixelsUsable(p, 0));
    } else if (!strcmp(argv[1], "statuses")) {
        for (int i = 0; i < REVIEW_STATUS_COUNT; ++i) {
            assert(ReviewStatusName(i)[0]); assert(!strstr(ReviewStatusName(i), "PASS"));
        }
        assert(!strcmp(ReviewStatusName(REVIEW_IMAGE), "IMAGE_UNVERIFIED"));
        assert(!strcmp(ReviewStatusName(-1), "INVALID_RESULT"));
        assert(!strcmp(ReviewStatusName(REVIEW_STATUS_COUNT), "INVALID_RESULT"));
    } else if (!strcmp(argv[1], "json")) {
        ReviewJsonText(stdout, L"Chosen <script>alert('x')</script> & \"quote\" \\ tab\t newline\n caf\u00e9 \U0001f41f");
    } else if (!strcmp(argv[1], "html")) {
        ReviewHtmlText(stdout, L"Chosen <script>alert('x')</script> & \"quote\" \\ caf\u00e9 \U0001f41f");
    } else return 2;
    return 0;
}
