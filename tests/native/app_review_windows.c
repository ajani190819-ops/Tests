/* Synthetic Windows fixture. It includes the actual product capture pipeline,
   but NEVER launches an installed suite app or grants owner acceptance. */
#define REVIEW_NATIVE_TEST
#include "../../app-review/src/app_review.c"

static volatile LONG fixture_mode, fixture_setting = 73, fixture_inputs;
static HWND fixture_window;
static HANDLE fixture_ready;
static int fixture_checks;
#define REQUIRE(expression) do { ++fixture_checks; if (!(expression)) { \
    fprintf(stderr, "fixture failure at line %d: %s (Win32 %lu)\n", __LINE__, #expression, (unsigned long)GetLastError()); \
    return 1; } } while (0)

static void FixturePaint(HWND window, HDC dc) {
    RECT rect;
    GetClientRect(window, &rect);
    HBRUSH background = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &rect, background); DeleteObject(background);
    if (InterlockedCompareExchange(&fixture_mode, 0, 0) == 1) return;
    RECT marker = {32, 24, 170, 130};
    HBRUSH color = CreateSolidBrush(RGB(40, 160, 220));
    FillRect(dc, &marker, color); DeleteObject(color);
    marker.left = 200; marker.right = 380;
    color = CreateSolidBrush(RGB(240, 190, 60));
    FillRect(dc, &marker, color); DeleteObject(color);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(230, 235, 245));
    const wchar_t *text = L"Synthetic App Review fixture - no owner app data";
    TextOutW(dc, 30, 170, text, (int)wcslen(text));
}

static LRESULT CALLBACK FixtureProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_APP + 50) {
        InterlockedExchange(&fixture_mode, (LONG)wparam);
        InvalidateRect(window, NULL, FALSE); UpdateWindow(window);
        return 0;
    }
    if (message == WM_NULL && InterlockedCompareExchange(&fixture_mode, 0, 0) == 2) {
        Sleep(1600); return 0; /* The worker, NOT this fixture, must be timed out. */
    }
    if (message == WM_COMMAND || message == WM_KEYDOWN || message == WM_KEYUP ||
        message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_SETTEXT)
        InterlockedIncrement(&fixture_inputs);
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint); FixturePaint(window, dc); EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static DWORD WINAPI FixtureThread(void *unused) {
    (void)unused;
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = FixtureProc; cls.hInstance = GetModuleHandleW(NULL); cls.lpszClassName = L"AppReview_NativeFixture";
    if (RegisterClassW(&cls)) fixture_window = CreateWindowExW(0, cls.lpszClassName,
        L"Chosen <script>not code</script> & \"fixture\" caf\x00e9", WS_POPUP | WS_VISIBLE,
        20, 20, 420, 260, NULL, NULL, cls.hInstance, NULL);
    if (fixture_window) UpdateWindow(fixture_window);
    SetEvent(fixture_ready);
    MSG message;
    while (fixture_window && GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    return 0;
}

static int FixtureDecodePng(const wchar_t *path) {
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory, (void **)&factory);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateDecoderFromFilename(factory, path, NULL, GENERIC_READ,
                                                                         WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame,
        &GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
    BYTE pixel[4] = {0};
    WICRect rect = {60, 50, 1, 1};
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_CopyPixels(converter, &rect, 4, 4, pixel);
    int ok = SUCCEEDED(hr) && pixel[0] == 220 && pixel[1] == 160 && pixel[2] == 40;
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);
    return ok;
}

static int FixtureTests(void) {
    REQUIRE(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)));
    fixture_ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    REQUIRE(fixture_ready != NULL);
    HANDLE thread = CreateThread(NULL, 0, FixtureThread, NULL, 0, NULL);
    REQUIRE(thread != NULL);
    REQUIRE(WaitForSingleObject(fixture_ready, 10000) == WAIT_OBJECT_0 && fixture_window != NULL);
    ReviewWindow selected;
    REQUIRE(ReviewDescribe(fixture_window, &selected));
    wchar_t directory[REVIEW_PATH], png[REVIEW_PATH], sentinel[REVIEW_PATH];
    REQUIRE(ReviewNewSession(directory));
    REQUIRE(ReviewSessionPath(directory));
    REQUIRE(ReviewJoin(sentinel, directory, L"settings-sentinel.ini"));
    const char intended[] = "intended-preferences=unchanged\r\n";
    REQUIRE(ReviewWriteNew(sentinel, intended, sizeof(intended)));
    REQUIRE(!ReviewWriteNew(sentinel, "overwritten", 12));
    WINDOWPLACEMENT before = {0}, after = {0};
    before.length = sizeof(before); after.length = sizeof(after);
    REQUIRE(GetWindowPlacement(fixture_window, &before));
    LONG inputs_before = InterlockedCompareExchange(&fixture_inputs, 0, 0);
    ReviewResult result;
    ReviewRunWorker(&selected, directory, 0, REVIEW_WORKER_MS, &result);
    if (result.status != REVIEW_IMAGE) fprintf(stderr, "first capture status=%s error=%lu\n", ReviewStatusName((int)result.status), (unsigned long)result.error);
    REQUIRE(result.status == REVIEW_IMAGE);
    REQUIRE(result.ping_answered && result.placement_unchanged);
    REQUIRE(ReviewLeaf(png, directory, 0, L"png") && ReviewPngValid(png, &result));
    REQUIRE(FixtureDecodePng(png));
    REQUIRE(GetWindowPlacement(fixture_window, &after));
    REQUIRE(before.showCmd == after.showCmd && EqualRect(&before.rcNormalPosition, &after.rcNormalPosition));
    REQUIRE(InterlockedCompareExchange(&fixture_setting, 0, 0) == 73);
    REQUIRE(InterlockedCompareExchange(&fixture_inputs, 0, 0) == inputs_before);
    char unchanged[sizeof(intended)];
    REQUIRE(ReviewReadFixed(sentinel, unchanged, sizeof(unchanged)) && memcmp(unchanged, intended, sizeof(intended)) == 0);
    ReviewResult first = result;
    ReviewRunWorker(&selected, directory, 0, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_IO_ERROR); /* Existing image is never overwritten. */
    REQUIRE(ReviewPngValid(png, &first) && FixtureDecodePng(png));
    ReviewWindow mismatch = selected; ++mismatch.created;
    ReviewRunWorker(&mismatch, directory, 1, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_IDENTITY);
    mismatch = selected; StringCchCopyW(mismatch.class_name, 128, L"WrongSelectedClass");
    ReviewRunWorker(&mismatch, directory, 2, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_IDENTITY);
    ShowWindow(fixture_window, SW_MINIMIZE); /* Fixture setup only, never product automation. */
    REQUIRE(IsIconic(fixture_window));
    ReviewRunWorker(&selected, directory, 3, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_MINIMIZED && IsIconic(fixture_window));
    ShowWindow(fixture_window, SW_RESTORE);
    Sleep(100);
    REQUIRE(SetWindowDisplayAffinity(fixture_window, WDA_MONITOR));
    ReviewRunWorker(&selected, directory, 4, REVIEW_WORKER_MS, &result);
    /* A non-layered getter may be unknown. Either its known-affinity gate or
       WGC's protected/blank result must prevent an image; never accept a timeout
       or arbitrary worker failure as protection evidence. */
    REQUIRE(result.status == REVIEW_PROTECTED || result.status == REVIEW_BLANK || result.status == REVIEW_CAPTURE_FAILED);
    REQUIRE(ReviewLeaf(png, directory, 4, L"png") && GetFileAttributesW(png) == INVALID_FILE_ATTRIBUTES);
    REQUIRE(result.utc.wYear >= 2020);
    if (result.status == REVIEW_PROTECTED) REQUIRE(result.affinity_query_known && result.affinity != WDA_NONE);
    REQUIRE(SetWindowDisplayAffinity(fixture_window, WDA_NONE));
    ReviewRunWorker(&selected, directory, 7, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_IMAGE); /* Same content works again after fixture-only protection is removed. */
    REQUIRE(ReviewLeaf(png, directory, 7, L"png") && ReviewPngValid(png, &result) && FixtureDecodePng(png));
    SendMessageW(fixture_window, WM_APP + 50, 1, 0); /* Uniform black fixture. */
    ReviewRunWorker(&selected, directory, 5, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_BLANK);
    SendMessageW(fixture_window, WM_APP + 50, 2, 0); /* Slow WM_NULL fixture. */
    ULONGLONG started = GetTickCount64();
    ReviewRunWorker(&selected, directory, 6, 250, &result);
    REQUIRE(result.status == REVIEW_TIMEOUT && GetTickCount64() - started < 4000 && result.utc.wYear >= 2020);
    REQUIRE(IsWindow(fixture_window)); /* Target survived; only utility child was stopped. */
    SendMessageW(fixture_window, WM_APP + 50, 2, 0);
    InterlockedExchange(&g_cancel, 1);
    ReviewRunWorker(&selected, directory, 8, REVIEW_WORKER_MS, &result);
    REQUIRE(result.status == REVIEW_CANCELLED && result.utc.wYear >= 2020 && IsWindow(fixture_window));
    InterlockedExchange(&g_cancel, 0);
    SendMessageW(fixture_window, WM_APP + 50, 0, 0);
    REQUIRE(InterlockedCompareExchange(&fixture_setting, 0, 0) == 73);
    REQUIRE(InterlockedCompareExchange(&fixture_inputs, 0, 0) == inputs_before);
    REQUIRE(ReviewReadFixed(sentinel, unchanged, sizeof(unchanged)) && memcmp(unchanged, intended, sizeof(intended)) == 0);
    ReviewRun run = {0};
    StringCchCopyW(run.directory, REVIEW_PATH, directory);
    run.count = 1; run.results[0] = first;
    REQUIRE(ReviewJsonReport(&run) && ReviewHtmlReport(&run));
    REQUIRE(!ReviewJsonReport(&run) && !ReviewHtmlReport(&run));
    /* Copy only this known synthetic report into the runner's NEW fixture cwd.
       No owner-app report or unselected window inventory is discovered. */
    wchar_t fixture_output[REVIEW_PATH], source[REVIEW_PATH], destination[REVIEW_PATH];
    REQUIRE(GetCurrentDirectoryW(REVIEW_PATH, fixture_output) > 0);
    const wchar_t *source_leaf[] = {L"report.json", L"report.html", L"window-01.png"};
    const wchar_t *destination_leaf[] = {L"fixture-report.json", L"fixture-report.html", L"window-01.png"};
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ReviewJoin(source, directory, source_leaf[i]) && ReviewJoin(destination, fixture_output, destination_leaf[i]));
        REQUIRE(CopyFileW(source, destination, TRUE));
    }

    /* Smoke the real native chooser. Every enumerated window starts unchecked. */
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    REQUIRE(InitCommonControlsEx(&controls));
    WNDCLASSW ui_class = {0};
    ui_class.lpfnWndProc = ReviewWindowProc; ui_class.hInstance = g_instance; ui_class.lpszClassName = L"AppReview_ChooserFixture";
    REQUIRE(RegisterClassW(&ui_class));
    HWND chooser = CreateWindowExW(0, ui_class.lpszClassName, L"Synthetic chooser fixture", WS_OVERLAPPEDWINDOW,
                                   0, 0, 920, 640, NULL, NULL, g_instance, NULL);
    REQUIRE(chooser && g_list && g_capture && g_notice);
    for (int row = 0; row < ListView_GetItemCount(g_list); ++row) REQUIRE(!ListView_GetCheckState(g_list, row));
    REQUIRE(Button_GetCheck(g_other) == BST_UNCHECKED);
    REQUIRE(!IsWindowEnabled(g_open));
    DestroyWindow(chooser);
    PostMessageW(fixture_window, WM_CLOSE, 0, 0);
    REQUIRE(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CloseHandle(thread); CloseHandle(fixture_ready);
    CoUninitialize();
    printf("{\"schema\":\"tests.app-review-native.v1\",\"native_windows\":true,\"synthetic_fixture\":true,"
           "\"checks\":%d,\"images\":1,\"settings_unchanged\":true,\"placement_unchanged\":true,"
           "\"timeout_contained\":true,\"cancellation_contained\":true,\"protected_skipped\":true,\"minimized_skipped\":true,\"mismatch_skipped\":true,"
           "\"blank_not_passed\":true,\"overwrite_refused\":true,\"evidence_not_accepted\":true,"
           "\"target_input_count\":0,\"owner_acceptance\":false}\n", fixture_checks);
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    g_instance = GetModuleHandleW(NULL);
    if (argc > 1) return ReviewWorkerMain(argc, argv);
    return FixtureTests();
}
