/* Read-only window checks + OS-composited capture. No target messages other
   than WM_NULL, and no capture fallback, activation, input or settings access. */

static int ReviewCloaked(HWND window, DWORD *cloaked) {
    typedef HRESULT (WINAPI *AttributeFn)(HWND, DWORD, PVOID, DWORD);
    HMODULE library = LoadLibraryExW(L"dwmapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return 0;
    AttributeFn attribute = (AttributeFn)(void *)GetProcAddress(library, "DwmGetWindowAttribute");
    int ok = attribute && SUCCEEDED(attribute(window, 14 /* DWMWA_CLOAKED */, cloaked, sizeof(*cloaked)));
    FreeLibrary(library);
    return ok;
}

static UINT ReviewDpi(HWND window) {
    typedef UINT (WINAPI *DpiFn)(HWND);
    DpiFn dpi = (DpiFn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return dpi ? dpi(window) : 0; /* 0 stays unknown; never invent native DPI. */
}

static ReviewStatus ReviewSnapshot(HWND window, const ReviewWindow *expected,
                                   ReviewWindow *current, RECT *rect, WINDOWPLACEMENT *placement,
                                   DWORD *affinity_known, DWORD *affinity_value) {
    DWORD cloaked = 0, affinity = 0;
    ReviewConditions c = {0};
    c.identity = ReviewDescribe(window, current) && ReviewSameIdentity(expected, current);
    c.visible = IsWindowVisible(window);
    c.minimized = IsIconic(window);
    if (!c.identity || !c.visible || c.minimized) return ReviewGate(c);
    if (!ReviewCloaked(window, &cloaked)) return REVIEW_QUERY_FAILED;
    c.cloaked = cloaked != 0;
    /* GetWindowDisplayAffinity can be unavailable for a non-layered window.
       Unknown is NOT treated as permission for a print/screen-copy fallback.
       Windows.Graphics.Capture itself enforces OS capture policy/protection. */
    BOOL known = GetWindowDisplayAffinity(window, &affinity);
    if (affinity_known) *affinity_known = known != FALSE;
    if (affinity_value) *affinity_value = known ? affinity : 0;
    if (known) c.affinity = affinity;
    c.queries_ok = 1;
    placement->length = sizeof(*placement);
    if (!GetWindowRect(window, rect) || !GetWindowPlacement(window, placement)) return REVIEW_QUERY_FAILED;
    int64_t width = (int64_t)rect->right - rect->left, height = (int64_t)rect->bottom - rect->top;
    c.width = width > 0 && width <= REVIEW_MAX_SIDE ? (int)width : 0;
    c.height = height > 0 && height <= REVIEW_MAX_SIDE ? (int)height : 0;
    return ReviewGate(c);
}

static void ReviewFileVersion(const ReviewWindow *window, wchar_t *out) {
    wchar_t path[1024];
    DWORD length = 1024, unused = 0;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, window->pid);
    if (!process) return;
    int ok = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!ok) return;
    DWORD size = GetFileVersionInfoSizeW(path, &unused);
    if (!size || size > 1024 * 1024) return;
    void *block = malloc(size);
    if (!block) return;
    VS_FIXEDFILEINFO *version = NULL;
    UINT bytes = 0;
    if (GetFileVersionInfoW(path, 0, size, block) && VerQueryValueW(block, L"\\", (void **)&version, &bytes) &&
        bytes >= sizeof(*version) && version->dwSignature == 0xFEEF04BD) {
        StringCchPrintfW(out, 48, L"%u.%u.%u.%u", HIWORD(version->dwFileVersionMS),
            LOWORD(version->dwFileVersionMS), HIWORD(version->dwFileVersionLS), LOWORD(version->dwFileVersionLS));
    }
    free(block);
}

static void ReviewChildHints(HWND window, ReviewResult *result) {
    RECT client;
    if (!GetClientRect(window, &client)) return;
    MapWindowPoints(window, NULL, (POINT *)&client, 2);
    HWND child = GetWindow(window, GW_CHILD);
    unsigned visited = 0;
    for (; child && visited < 256; child = GetWindow(child, GW_HWNDNEXT), ++visited) {
        RECT rect;
        DWORD pid = 0;
        GetWindowThreadProcessId(child, &pid);
        if (pid != result->window.pid || !IsWindowVisible(child) || !GetWindowRect(child, &rect)) continue;
        ++result->child_controls;
        if (rect.left < client.left || rect.top < client.top || rect.right > client.right || rect.bottom > client.bottom)
            ++result->clipped_hints;
    }
    result->child_scan_limited = child != NULL;
}

static int ReviewSavePng(const wchar_t *path, int width, int height, UINT dpi,
                          BYTE *pixels, DWORD *written) {
    IWICImagingFactory *factory = NULL;
    IStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory, (void **)&factory);
    if (SUCCEEDED(hr)) hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(frame, (UINT)width, (UINT)height);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetResolution(frame, dpi ? dpi : 96, dpi ? dpi : 96);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
    if (SUCCEEDED(hr) && !IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGRA)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WritePixels(frame, (UINT)height, (UINT)width * 4,
                                                             (UINT)width * height * 4, pixels);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(frame);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(encoder);
    STATSTG stat;
    HGLOBAL memory = NULL;
    if (SUCCEEDED(hr)) hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    if (SUCCEEDED(hr) && (stat.cbSize.QuadPart <= 0 || stat.cbSize.QuadPart > REVIEW_MAX_PNG)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = GetHGlobalFromStream(stream, &memory);
    int ok = 0;
    if (SUCCEEDED(hr)) {
        void *data = GlobalLock(memory);
        if (data) {
            ok = ReviewWriteNew(path, data, stat.cbSize.LowPart);
            if (ok) *written = stat.cbSize.LowPart;
            GlobalUnlock(memory);
        }
    }
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (encoder) IWICBitmapEncoder_Release(encoder);
    if (stream) IStream_Release(stream);
    if (factory) IWICImagingFactory_Release(factory);
    return ok;
}

static void ReviewCaptureWindow(const ReviewWindow *expected, const wchar_t *path, ReviewResult *result) {
    HWND window = (HWND)(uintptr_t)expected->hwnd;
    WINDOWPLACEMENT before = {0}, after = {0};
    ReviewWindow current;
    RECT after_rect;
    ZeroMemory(result, sizeof(*result));
    result->magic = REVIEW_MAGIC;
    result->bytes = sizeof(*result);
    result->window = *expected;
    GetSystemTime(&result->utc);
    result->status = ReviewSnapshot(window, expected, &current, &result->rect, &before,
                                    &result->affinity_query_known, &result->affinity);
    if (result->status != REVIEW_IMAGE) return;
    result->window = current;
    result->dpi = ReviewDpi(window);
    result->foreground_at_capture = GetForegroundWindow() == window;
    DWORD_PTR reply = 0;
    if (!SendMessageTimeoutW(window, WM_NULL, 0, 0,
                             SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 750, &reply)) {
        result->status = REVIEW_UNRESPONSIVE;
        result->error = GetLastError();
        return;
    }
    result->ping_answered = 1;
    uint32_t *pixels = NULL;
    ReviewSize size = {0};
    HRESULT captured = ReviewWindowPixels(window, &pixels, &size);
    result->capture_width = size.width;
    result->capture_height = size.height;
    result->error = FAILED(captured) ? (DWORD)captured : 0;
    ReviewStatus after_status = ReviewSnapshot(window, expected, &current, &after_rect, &after, NULL, NULL);
    result->placement_unchanged = after_status == REVIEW_IMAGE &&
        EqualRect(&result->rect, &after_rect) && before.showCmd == after.showCmd &&
        before.flags == after.flags && EqualRect(&before.rcNormalPosition, &after.rcNormalPosition) &&
        before.ptMinPosition.x == after.ptMinPosition.x && before.ptMinPosition.y == after.ptMinPosition.y &&
        before.ptMaxPosition.x == after.ptMaxPosition.x && before.ptMaxPosition.y == after.ptMaxPosition.y &&
        result->dpi == ReviewDpi(window);
    if (after_status != REVIEW_IMAGE || !result->placement_unchanged) result->status = REVIEW_CHANGED;
    else if (FAILED(captured)) result->status = REVIEW_CAPTURE_FAILED;
    else if (!ReviewPixelsUsable(pixels, (size_t)size.width * size.height)) result->status = REVIEW_BLANK;
    else {
        size_t count = (size_t)size.width * size.height;
        for (size_t i = 0; i < count; ++i) pixels[i] |= 0xFF000000u;
        ReviewFileVersion(expected, result->window.version);
        ReviewChildHints(window, result);
        if (!ReviewSavePng(path, size.width, size.height, result->dpi, (BYTE *)pixels, &result->png_bytes))
            result->status = REVIEW_IO_ERROR;
    }
    free(pixels);
}

static int ReviewReadFixed(const wchar_t *path, void *data, DWORD bytes) {
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) return 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;
    DWORD read = 0;
    int ok = GetFileSizeEx(file, &size) && size.QuadPart == bytes &&
        ReadFile(file, data, bytes, &read, NULL) && read == bytes;
    CloseHandle(file);
    return ok;
}

static int ReviewWindowStringsValid(const ReviewWindow *window) {
    return window->image[0] && window->class_name[0] && window->image[127] == 0 &&
        window->title[255] == 0 && window->class_name[127] == 0 && window->version[47] == 0;
}

static int ReviewWorkerMain(int argc, wchar_t **argv) {
    uint64_t hwnd, pid, created, index;
    if (argc != 8 || wcscmp(argv[1], L"--capture-worker") != 0 ||
        !ReviewParseNumber(argv[2], UINTPTR_MAX, &hwnd) || !hwnd ||
        !ReviewParseNumber(argv[3], UINT32_MAX, &pid) || !pid ||
        !ReviewParseNumber(argv[4], UINT64_MAX, &created) || !created ||
        !ReviewParseNumber(argv[5], REVIEW_MAX_WINDOWS - 1, &index) ||
        !ReviewSessionPath(argv[6]) || wcscmp(argv[7], L"local-only-v1") != 0) return 2;
    wchar_t png[REVIEW_PATH], binary[REVIEW_PATH], request[REVIEW_PATH];
    if (!ReviewLeaf(png, argv[6], (int)index, L"png") ||
        !ReviewLeaf(binary, argv[6], (int)index, L"result") ||
        !ReviewLeaf(request, argv[6], (int)index, L"request")) return 2;
    ReviewWindow expected;
    if (!ReviewReadFixed(request, &expected, sizeof(expected)) || !ReviewWindowStringsValid(&expected) ||
        expected.hwnd != hwnd || expected.pid != pid || expected.created != created) return 3;
    HRESULT hr = ReviewRuntimeBegin();
    ReviewResult result;
    if (SUCCEEDED(hr)) {
        ReviewCaptureWindow(&expected, png, &result);
        ReviewRuntimeEnd();
    } else {
        ZeroMemory(&result, sizeof(result));
        result.magic = REVIEW_MAGIC; result.bytes = sizeof(result); result.window = expected;
        result.status = REVIEW_CAPTURE_FAILED; result.error = (DWORD)hr;
        GetSystemTime(&result.utc);
    }
    return ReviewWriteNew(binary, &result, sizeof(result)) ? 0 : 4;
}

static int ReviewReadResult(const wchar_t *path, ReviewResult *result) {
    return ReviewReadFixed(path, result, sizeof(*result)) && result->magic == REVIEW_MAGIC &&
        result->bytes == sizeof(*result) && result->status < REVIEW_STATUS_COUNT &&
        ReviewWindowStringsValid(&result->window);
}

/* OS/driver capture can stall. Isolate it in a job-owned child of THIS utility.
   Never terminate, signal, restart or change the target. */
static void ReviewRunWorker(const ReviewWindow *window, const wchar_t *directory, int index,
                             DWORD timeout_ms, ReviewResult *result) {
    ZeroMemory(result, sizeof(*result));
    result->magic = REVIEW_MAGIC;
    result->bytes = sizeof(*result);
    result->window = *window;
    result->status = REVIEW_IO_ERROR;
    GetSystemTime(&result->utc); /* An attempted worker can fail before its IPC result. */
    wchar_t exe[REVIEW_PATH], command[1200], binary[REVIEW_PATH], request[REVIEW_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, REVIEW_PATH);
    if (!n || n >= REVIEW_PATH || !ReviewSessionPath(directory) ||
        !ReviewLeaf(binary, directory, index, L"result") ||
        !ReviewLeaf(request, directory, index, L"request") ||
        FAILED(StringCchPrintfW(command, 1200,
            L"\"%ls\" --capture-worker %llu %lu %llu %d \"%ls\" local-only-v1", exe,
            (unsigned long long)window->hwnd, (unsigned long)window->pid,
            (unsigned long long)window->created, index, directory))) return;
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (!job) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(job);
        return;
    }
    if (!ReviewWriteNew(request, window, sizeof(*window))) { CloseHandle(job); return; }
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(exe, command, NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &child)) { CloseHandle(job); DeleteFileW(request); return; }
    if (!AssignProcessToJobObject(job, child.hProcess)) {
        TerminateProcess(child.hProcess, 4); /* Our still-suspended child only. */
        CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(job);
        DeleteFileW(request);
        return;
    }
    if (ResumeThread(child.hThread) == (DWORD)-1) {
        CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(job);
        DeleteFileW(request);
        return;
    }
    CloseHandle(child.hThread);
    ULONGLONG started = GetTickCount64();
    DWORD wait;
    while ((wait = WaitForSingleObject(child.hProcess, 50)) == WAIT_TIMEOUT) {
        if (InterlockedCompareExchange(&g_cancel, 0, 0) || GetTickCount64() - started >= timeout_ms) break;
    }
    if (wait != WAIT_OBJECT_0) {
        result->status = InterlockedCompareExchange(&g_cancel, 0, 0) ? REVIEW_CANCELLED : REVIEW_TIMEOUT;
        TerminateJobObject(job, 5); /* Only this utility's capture worker. */
        WaitForSingleObject(child.hProcess, 2000);
    } else {
        DWORD exit_code = 1;
        ReviewResult captured;
        if (GetExitCodeProcess(child.hProcess, &exit_code) && exit_code == 0 &&
            ReviewReadResult(binary, &captured) && ReviewSameIdentity(window, &captured.window)) *result = captured;
    }
    CloseHandle(child.hProcess);
    CloseHandle(job);
    /* Remove only our disposable IPC file, never the target or old evidence. */
    DeleteFileW(binary);
    DeleteFileW(request);
}
