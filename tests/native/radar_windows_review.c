/* Windows-only review of the production Radar renderer, click routing, WIC
 * cache/composite path and place-generation guard. Fixture HTTP sends are
 * refused by this test-only transport guard; live sends forward unchanged.
 * No test hooks or edits in the shipped app. This is NOT an owner-desktop acceptance
 * test: entry-point fitting, monitor DPI changes and real mouse/focus UX still
 * need the installed EXE. All files live in the supplied disposable directory.
 */
#define COBJMACROS
#include <windows.h>
#include <winhttp.h>
static int reviewLive = 0;
static volatile LONG reviewHttpRequests = 0, reviewBlockedHttp = 0;
static HINTERNET WINAPI ReviewHttpOpen(LPCWSTR agent, DWORD access,
    LPCWSTR proxy, LPCWSTR bypass, DWORD flags);
static BOOL WINAPI ReviewSendRequest(HINTERNET request, LPCWSTR headers,
    DWORD headersLength, LPVOID optional, DWORD optionalLength,
    DWORD totalLength, DWORD_PTR context);
#define WinHttpOpen ReviewHttpOpen
#define WinHttpSendRequest ReviewSendRequest
#include "../../weather-radar/src/weather_radar.c"
#undef WinHttpOpen
#undef WinHttpSendRequest

/* A missing seeded tile must fail closed before even opening an HTTP session.
   Live mode forwards to the actual production WinHTTP boundary unchanged. */
static HINTERNET WINAPI ReviewHttpOpen(LPCWSTR agent, DWORD access,
    LPCWSTR proxy, LPCWSTR bypass, DWORD flags) {
    if (!reviewLive) {
        InterlockedIncrement(&reviewBlockedHttp);
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }
    return WinHttpOpen(agent, access, proxy, bypass, flags);
}

static BOOL WINAPI ReviewSendRequest(HINTERNET request, LPCWSTR headers,
    DWORD headersLength, LPVOID optional, DWORD optionalLength,
    DWORD totalLength, DWORD_PTR context) {
    InterlockedIncrement(&reviewHttpRequests);
    return WinHttpSendRequest(request, headers, headersLength, optional,
                              optionalLength, totalLength, context);
}

static volatile LONG checks = 0;
static int clicks = 0, overflows = 0;
static wchar_t outputDir[MAX_PATH];
static volatile LONG stopPainterRace = 0;
static LONG raceBuilds = 0;
/* Independent Web-Mercator control coordinates at source level 7 (256px
   tiles), calculated with asinh(tan(latitude)), not PlacePixelInGrid. */
static const double controlX[2] = {8712.583395555555, 5249.0604088888895};
static const double controlY[2] = {13473.09523628543, 11443.995010804105};

#define CHECK(expr) do { InterlockedIncrement(&checks); if (!(expr)) { \
    fprintf(stderr, "FAILED line %d: %s (Win32 %lu)\n", __LINE__, #expr, GetLastError()); \
    exit(1); } } while (0)

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ old;
    uint32_t *bits;
    RECT rect;
    int width, height;
} Surface;

static void PathFor(const wchar_t *name, wchar_t *out) {
    CHECK(_snwprintf(out, MAX_PATH, L"%s\\%s", outputDir, name) > 0);
}

static void WritePng(const wchar_t *path, uint32_t *pixels, int width, int height) {
    IWICStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    CHECK(WicEnsure());
    CHECK(SUCCEEDED(IWICImagingFactory_CreateStream(g_wicFactory, &stream)));
    CHECK(SUCCEEDED(IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE)));
    CHECK(SUCCEEDED(IWICImagingFactory_CreateEncoder(g_wicFactory, &GUID_ContainerFormatPng,
                                                   NULL, &encoder)));
    CHECK(SUCCEEDED(IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache)));
    CHECK(SUCCEEDED(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL)));
    CHECK(SUCCEEDED(IWICBitmapFrameEncode_Initialize(frame, NULL)));
    CHECK(SUCCEEDED(IWICBitmapFrameEncode_SetSize(frame, (UINT)width, (UINT)height)));
    CHECK(SUCCEEDED(IWICBitmapFrameEncode_SetPixelFormat(frame, &format)));
    CHECK(IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGRA));
    CHECK(SUCCEEDED(IWICBitmapFrameEncode_WritePixels(frame, (UINT)height, (UINT)width * 4,
                    (UINT)width * (UINT)height * 4, (BYTE *)pixels)));
    CHECK(SUCCEEDED(IWICBitmapFrameEncode_Commit(frame)));
    CHECK(SUCCEEDED(IWICBitmapEncoder_Commit(encoder)));
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(encoder);
    IWICStream_Release(stream);
}

static Surface Render(int planner) {
    Surface s;
    ZeroMemory(&s, sizeof(s));
    if (planner) PlannerRect(&s.rect);
    else SetRect(&s.rect, S(260), S(512), S(WIN_LW - 16), S(WIN_LH - 56));
    s.width = s.rect.right - s.rect.left;
    s.height = s.rect.bottom - s.rect.top;
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = s.width;
    bi.bmiHeader.biHeight = -s.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s.dc = CreateCompatibleDC(NULL);
    s.bitmap = CreateDIBSection(s.dc, &bi, DIB_RGB_COLORS, (void **)&s.bits, NULL, 0);
    CHECK(s.dc && s.bitmap && s.bits);
    s.old = SelectObject(s.dc, s.bitmap);
    CHECK(s.old && s.old != HGDI_ERROR);
    SetViewportOrgEx(s.dc, -s.rect.left, -s.rect.top, NULL);
    HBRUSH bg = CreateSolidBrush(COL_BG);
    FillRect(s.dc, &s.rect, bg);
    DeleteObject(bg);
    if (planner) PaintPlannerCard(s.dc, &s.rect);
    else PaintRadarPane(s.dc, &s.rect);
    GdiFlush();
    return s;
}

static void FreeSurface(Surface *s) {
    SelectObject(s->dc, s->old);
    DeleteObject(s->bitmap);
    DeleteDC(s->dc);
}

static void Capture(int planner, const wchar_t *name) {
    Surface s = Render(planner);
    wchar_t path[MAX_PATH];
    PathFor(name, path);
    for (int i = 0; i < s.width * s.height; i++) s.bits[i] |= 0xff000000u;
    WritePng(path, s.bits, s.width, s.height);
    FreeSurface(&s);
}

static void SetScale(UINT dpi, int fit) {
    HFONT fonts[] = {g_fontUI, g_fontSmall, g_fontTitle, g_fontBig, g_fontBtn};
    for (int i = 0; i < 5; i++) if (fonts[i]) DeleteObject(fonts[i]);
    g_dpi = dpi;
    g_fit = fit;
    MakeFonts();
    CHECK(g_fontUI && g_fontSmall && g_fontTitle && g_fontBig && g_fontBtn);
    LayoutSearchEdit();
}

static void ClickRect(const RECT *r) {
    SendMessageW(g_hwnd, WM_LBUTTONUP, 0,
                 MAKELPARAM((r->left + r->right) / 2, (r->top + r->bottom) / 2));
    clicks++;
}

static void SeedForecast(void) {
    ZeroMemory(&g_fc, sizeof(g_fc));
    ZeroMemory(&g_ens, sizeof(g_ens));
    g_fc.have = 2;
    g_fc.dayN = FC_DAYS;
    g_fc.hN = FC_DAYS * 24;
    g_fc.tempF = 72; g_fc.feelsF = 73; g_fc.rh = 61;
    g_fc.windMph = 8; g_fc.cloudCover = 45;
    g_ens.have = 1; g_ens.members = 10; g_ens.hours = FC_DAYS * 24;
    wcscpy(g_ens.model, L"SYNTHETIC ensemble fixture");
    wcscpy(g_fc.fetched, L"fixture");
    for (int d = 0; d < FC_DAYS; d++) {
        ForecastDay *day = &g_fc.days[d];
        static const wchar_t *labels[] = {L"Mon 11/16", L"Tue 11/17", L"Wed 11/18", L"Thu 11/19", L"Fri 11/20"};
        wcscpy(day->label, labels[d]);
        day->maxF = 81 + d; day->minF = 61 - d;
        day->regionalPoP = 75 - d * 10; day->rainSumIn = 0.24 + d * 0.03;
        day->cloudMean = 35 + d * 12; day->windMaxMph = 12 + d * 4;
        day->firstHour = d * 24; day->hourCount = 24;
        for (int h = 0; h < 24; h++) {
            int i = d * 24 + h;
            double wave = (sin((h - 8) * 3.141592653589793 / 12) + 1) / 2;
            g_fc.hDay[i] = d;
            _snwprintf(g_fc.hLabel[i], 8, L"%02d:00", h);
            g_fc.hT[i] = day->minF + (day->maxF - day->minF) * wave;
            g_fc.hPoP[i] = day->regionalPoP * wave;
            g_fc.hCloud[i] = 20 + 65 * wave;
            g_fc.hWindMph[i] = day->windMaxMph * wave;
            g_ens.prob[i] = h >= 12 && h <= 17 ? (double)(d + 2) / 10 : 0;
            g_ens.reported[i] = 10;
            for (int m = 0; m < 10; m++)
                g_ens.mm[m][i] = m < d + 2 && h >= 12 && h <= 17 ? 0.4 : 0;
        }
    }
}

static void TextFits(HDC dc, HFONT font, const wchar_t *text, int width) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size;
    CHECK(GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size));
    if (size.cx > width) {
        overflows++;
        fwprintf(stderr, L"TEXT_OVERFLOW dpi=%u fit=%d width=%d needed=%ld text=%s\n",
                 g_dpi, g_fit, width, size.cx, text);
    }
    SelectObject(dc, old);
}

static void TestPlanner(void) {
    const int scales[][2] = {{96,100}, {96,75}, {96,55}, {144,75}, {192,55}};
    for (int scale = 0; scale < 5; scale++) {
        SetScale((UINT)scales[scale][0], scales[scale][1]);
        RECT table, detail, prior = {0};
        CHECK(PlannerTableRects(&table, &detail));
        CHECK(table.right < detail.left && table.bottom == detail.bottom);
        Surface s = Render(1);
        for (int d = 0; d < FC_DAYS; d++) {
            RECT row, date;
            CHECK(PlannerRowRect(d, &row) && PlannerDateRect(d, &date));
            CHECK(date.left == table.left && date.top == row.top && date.bottom == row.bottom);
            CHECK(row.bottom <= table.bottom && (d == 0 || row.top >= prior.bottom));
            prior = row;
            TextFits(s.dc, g_fontUI, g_fc.days[d].label, date.right - date.left);
            for (int category = 0; category < PLAN_CATEGORY_COUNT; category++) {
                RECT cell, header;
                CHECK(PlannerCellRect(d, category, &cell) && PlannerHeaderRect(category, &header));
                CHECK(cell.top == row.top && cell.bottom == row.bottom && cell.left >= date.right);
                CHECK(cell.left == header.left && cell.right == header.right && cell.right <= table.right);
                ClickRect(&cell);
                CHECK(g_planDay == d && g_planCategory == category);
                wchar_t heading[224], reading[224], axis[24], value[64];
                PlannerDetailHeading(&g_fc.days[d], category, heading, 224);
                PlannerReading(&g_fc.days[d], category, reading, 224);
                PlannerAxisText(category, 50, axis, 24);
                PlannerCellText(d, category, value, 64);
                CHECK(wcsstr(heading, g_fc.days[d].label) && reading[0] && axis[0]);
                CHECK(!isnan(PlannerHourlyValue(category, d * 24 + 14, 0)));
                TextFits(s.dc, g_fontSmall, PlannerCategoryName(category), cell.right - cell.left);
                if (category != PLAN_RAIN) TextFits(s.dc, g_fontUI, value, cell.right - cell.left);
                if (scale == 0 && d == 2) {
                    wchar_t name[96];
                    _snwprintf(name, 96, L"fixture-planner-category%d-dpi96-fit100.png", category);
                    Capture(1, name);
                }
            }
            int category = g_planCategory;
            ClickRect(&date);
            CHECK(g_planDay == d && g_planCategory == category);
        }
        FreeSurface(&s);
        g_planDay = 0; g_planCategory = PLAN_RAIN;
        wchar_t name[96];
        _snwprintf(name, 96, L"fixture-planner-rain-dpi%u-fit%d.png", g_dpi, g_fit);
        Capture(1, name);
    }
    CHECK(fabs(PlannerPointRainChance(&g_fc, &g_ens, &g_fc.days[0]) - .2) < .0001);
    CHECK(g_fc.days[0].regionalPoP == 75); /* the two probabilities are not interchangeable */
}

static void SeedTiles(void) {
    wcscpy(g_radarHost, L"https://radar-fixture.invalid");
    g_frameN = 2;
    strcpy(g_frames[0].path, "/fixture-observed"); g_frames[0].time = 1794830400;
    strcpy(g_frames[1].path, "/fixture-nowcast"); g_frames[1].time = 1794831000;
    g_frames[1].kind = 1;
    uint32_t *pixels = (uint32_t *)malloc(TILE_PX * TILE_PX * 4);
    CHECK(pixels);
    for (int city = 0; city < 2; city++) {
        int tx, ty;
        GridFor(7, g_places[city].lat, g_places[city].lon, &tx, &ty);
        for (int layer = -1; layer < 2; layer++) {
            for (int gy = 0; gy < 3; gy++) for (int gx = 0; gx < 3; gx++) {
                for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
                    double wx = (tx + gx) * 256 + x, wy = (ty + gy) * 256 + y;
                    uint32_t color = city ? 0xff4c323du : 0xff1f3550u;
                    if (x % 32 < 2 || y % 32 < 2) color += 0x00101010;
                    if (fabs(wx - controlX[city]) < 4 || fabs(wy - controlY[city]) < 4)
                        color = 0xffefa835u; /* synthetic surveyed crossroads */
                    if (layer >= 0) color = x < 40 + layer * 35 ? 0x80368addu : 0;
                    pixels[y * 256 + x] = color;
                }
                wchar_t url[512], cache[MAX_PATH];
                if (layer < 0) _snwprintf(url, 512, L"https://tile.openstreetmap.org/7/%d/%d.png", tx + gx, ty + gy);
                else _snwprintf(url, 512, L"%s%hs/256/7/%d/%d/2/1_1.png", g_radarHost, g_frames[layer].path, tx + gx, ty + gy);
                CacheNameFor(layer < 0 ? L"osm" : L"rv", url, cache, MAX_PATH);
                WritePng(cache, pixels, 256, 256);
                CHECK(CacheAgeSecOk(cache, 60));
            }
        }
    }
    free(pixels);
}

static void CheckMarker(Surface *s, int city) {
    RECT img;
    RadarImageRect(&img);
    int side = img.bottom - img.top;
    int focus = RADAR_FOCUS_PX;
    for (int z = 7; z < g_zoom && focus > RADAR_FOCUS_MIN_PX; z++) focus /= 2;
    if (focus < RADAR_FOCUS_MIN_PX) focus = RADAR_FOCUS_MIN_PX;
    double px = controlX[city] - g_viewKey[1] * 256, py = controlY[city] - g_viewKey[2] * 256;
    int left = (int)llround(px - focus / 2.0), top = (int)llround(py - focus / 2.0);
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (left > GRID_PX - focus) left = GRID_PX - focus;
    if (top > GRID_PX - focus) top = GRID_PX - focus;
    int cx = img.left + ((img.right - img.left) - side) / 2 + (int)((px - left) * side / focus);
    int cy = img.top + (int)((py - top) * side / focus);
    int white = 0;
    for (int y = cy - S(2); y <= cy + S(2); y++)
        for (int x = cx + S(7); x <= cx + S(11); x++) {
            COLORREF c = GetPixel(s->dc, x, y);
            if (GetRValue(c) > 235 && GetGValue(c) > 235 && GetBValue(c) > 235) white++;
        }
    CHECK(white > 0); /* real GDI crosshair at the independent control point */
}

static DWORD WINAPI RaceBuild(LPVOID unused) {
    (void)unused;
    CHECK(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)));
    while (!InterlockedCompareExchange(&stopPainterRace, 0, 0)) {
        EnterCriticalSection(&g_lock);
        Place p = g_reqPlace;
        DWORD ticket = g_placeTicket;
        int frame = g_frameSel;
        LeaveCriticalSection(&g_lock);
        RadarViewBuild(&p, frame, ticket);
        InterlockedIncrement(&raceBuilds);
    }
    CoUninitialize();
    return 0;
}

static void TestRadar(void) {
    SetScale(96, 100);
    for (int city = 0; city < 2; city++) {
        RECT row;
        CHECK(SavedRowRect(city, &row));
        ClickRect(&row);
        DWORD ticket = g_placeTicket;
        g_zoom = 7;
        RadarViewBuild(&g_places[city], 0, ticket);
        CHECK(g_viewReady == 2 && g_viewTicket == ticket && g_viewKey[0] == 7);
        uint32_t *view = g_view;
        for (int zoom = 7; zoom <= 10; zoom++) {
            CHECK(g_zoom == zoom && RadarTileZoomForDisplay(g_zoom) == 7);
            Surface s = Render(0);
            CheckMarker(&s, city);
            FreeSurface(&s);
            if (zoom == 7 || zoom == 10) {
                wchar_t name[96];
                _snwprintf(name, 96, L"fixture-radar-city%d-zoom%d.png", city, zoom);
                Capture(0, name);
            }
            RECT btn;
            BtnRect(BTN_ZOOM, &btn);
            ClickRect(&btn);
            CHECK(g_view == view); /* local crops do not replace/fetch source imagery */
        }
        CHECK(g_zoom == 7);
        RequestPlaceData(&g_places[1 - city]);
        CHECK(g_viewReady == 0 && g_placeTicket != ticket);
        RadarViewBuild(&g_places[city], 1, ticket); /* a real late, obsolete build */
        CHECK(g_viewReady == 0 && g_viewTicket == ticket);
        g_viewReady = 2; /* even a stale ready bit must not paint the old map */
        Surface stale = Render(0);
        RECT img;
        RadarImageRect(&img);
        COLORREF c = GetPixel(stale.dc, img.left + S(410), img.top + S(210));
        CHECK(c == COL_INPUT);
        FreeSurface(&stale);
        g_viewReady = 0;
    }
    HANDLE thread = CreateThread(NULL, 0, RaceBuild, NULL, 0, NULL);
    CHECK(thread);
    for (int n = 0; n < 120; n++) {
        RECT row, btn;
        CHECK(SavedRowRect(n & 1, &row));
        ClickRect(&row);
        BtnRect(BTN_NEXT, &btn);
        ClickRect(&btn);
        Surface s = Render(0);
        FreeSurface(&s);
        Sleep(2);
    }
    InterlockedExchange(&stopPainterRace, 1);
    CHECK(WaitForSingleObject(thread, 30000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    CHECK(raceBuilds > 0);
}

static void PumpData(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, WM_APP_DATA, WM_APP_STATUS, PM_REMOVE))
        DispatchMessageW(&msg);
}

static int LiveReview(void) {
    int all = 1;
    for (int city = 0; city < 2; city++) {
        g_placeSel = city;
        RequestPlaceData(&g_places[city]);
        DWORD ticket = g_placeTicket;
        FetchForecast(&g_places[city], ticket);
        FetchEnsemble(&g_places[city], ticket);
        g_frameN = 0;
        g_radarHost[0] = L'\0'; /* a previous successful catalogue is not this run */
        RadarCatalogFetch();
        PumpData();
        RadarViewBuild(&g_places[city], g_frameSel, ticket);
        PumpData();
        int basePixels = 0;
        if (g_base) for (int i = 0; i < GRID_PX * GRID_PX; i++)
            if (g_base[i] & 0x00ffffffu) basePixels++;
        int ok = g_fc.have == 2 && g_fc.dayN == FC_DAYS && g_ens.have &&
                 g_ens.members >= 2 && g_frameN > 0 && g_viewReady &&
                 g_viewTicket == ticket && basePixels > 1000;
        printf("LIVE city=%d forecast=%d days=%d ensemble=%d members=%d frames=%d view=%d base_pixels=%d passed=%d\n",
               city, g_fc.have, g_fc.dayN, g_ens.have, g_ens.members, g_frameN, g_viewReady, basePixels, ok);
        wchar_t name[96], path[MAX_PATH];
        char details[512];
        int len = snprintf(details, sizeof(details),
            "{\"city\":%d,\"forecast\":%d,\"days\":%d,\"ensemble\":%d,\"members\":%d,"
            "\"frames\":%d,\"view\":%d,\"base_pixels\":%d,\"passed\":%s}\n",
            city, g_fc.have, g_fc.dayN, g_ens.have, g_ens.members, g_frameN,
            g_viewReady, basePixels, ok ? "true" : "false");
        CHECK(len > 0 && len < (int)sizeof(details));
        _snwprintf(name, 96, L"live-city%d-result.json", city);
        PathFor(name, path);
        CHECK(WriteWholeFileW(path, details, (size_t)len));
        _snwprintf(name, 96, L"live-city%d-planner.png", city);
        Capture(1, name);
        for (int zoom = 7; zoom <= 10; zoom += 3) {
            g_zoom = zoom;
            _snwprintf(name, 96, L"live-city%d-radar-zoom%d.png", city, zoom);
            Capture(0, name);
        }
        if (!ok) all = 0;
    }
    return all;
}

int wmain(int argc, wchar_t **argv) {
    if ((argc != 2 && argc != 3) || wcslen(argv[1]) > MAX_PATH - 40 ||
        (argc == 3 && wcscmp(argv[2], L"--live") != 0)) return 2;
    wcscpy(outputDir, argv[1]);
    /* Atomic new-directory requirement: direct use cannot reuse app state. */
    if (!CreateDirectoryW(outputDir, NULL)) return 2;
    PathFor(L"cache", g_cacheDir); EnsureDirW(g_cacheDir);
    PathFor(L"config.ini", g_cfgPath);
    wcscpy(g_cfgDir, outputDir);
    int live = argc == 3;
    reviewLive = live;
    InitializeCriticalSection(&g_lock);
    g_jobEvent = CreateEventW(NULL, FALSE, FALSE, NULL); /* queue, no background network */
    CHECK(g_jobEvent);
    g_hInst = GetModuleHandleW(NULL);
    InitPalette();
    SetScale(96, 100);
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst; wc.lpszClassName = L"RadarNativeReviewOnly";
    CHECK(RegisterClassExW(&wc));
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Radar native review harness",
                             WS_OVERLAPPEDWINDOW, 0, 0, 1120, 960, NULL, NULL, g_hInst, NULL);
    CHECK(g_hwnd && g_hSearch);
    g_placeN = 2;
    g_places[0] = (Place){.name=L"Tallahassee", .region=L"Florida, United States", .lat=30.4383, .lon=-84.2807};
    g_places[1] = (Place){.name=L"Seattle", .region=L"Washington, United States", .lat=47.6062, .lon=-122.3321};
    g_rainAlertEpisode = 1; /* alerts are outside this review; no tray is registered */
    int passed = 1;
    if (live) passed = LiveReview();
    else {
        SeedForecast();
        TestPlanner();
        SeedTiles();
        TestRadar();
        CHECK(overflows == 0);
        CHECK(reviewBlockedHttp == 0 && reviewHttpRequests == 0);
    }
    wchar_t result[MAX_PATH];
    PathFor(L"result.json", result);
    char json[1024];
    int len = snprintf(json, sizeof(json),
        "{\"mode\":\"%s\",\"native_windows\":true,\"passed\":%s,\"checks\":%ld,\"clicks\":%d,"
        "\"text_overflows\":%d,\"concurrent_builds\":%ld,\"blocked_http\":%ld,\"http_requests\":%ld,\"theme_background\":%lu,"
        "\"boundary\":\"Production components and click handlers; not installed-EXE startup, monitor-DPI transitions or owner desktop acceptance\"}\n",
        live ? "live" : "fixture", passed ? "true" : "false", checks, clicks, overflows, raceBuilds,
        reviewBlockedHttp, reviewHttpRequests, COL_BG);
    CHECK(len > 0 && len < (int)sizeof(json));
    CHECK(WriteWholeFileW(result, json, (size_t)len));
    printf("%s", json);
    fflush(stdout);
    return passed ? 0 : 1;
}
