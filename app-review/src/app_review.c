/* App Review: an on-demand, local-only window-evidence utility.
   No target-app input, launch, settings access, activation or desktop capture. */
#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <winver.h>
#include <string.h>
#include <math.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <strsafe.h>
#include "review_policy.h"
#include "../../shared/win32/suite_emblem.c"

#define REVIEW_MAGIC 0x31525041u
#define REVIEW_MAX_LIST 256
#define REVIEW_PATH 260
#define REVIEW_DONE (WM_APP + 1)
#define REVIEW_PROGRESS (WM_APP + 2)

typedef struct ReviewWindow {
    uint64_t hwnd, created;
    DWORD pid;
    wchar_t image[128], title[256], class_name[128], version[48];
} ReviewWindow;

typedef struct ReviewResult {
    DWORD magic, bytes, status, error;
    ReviewWindow window;
    RECT rect;
    UINT dpi;
    INT32 capture_width, capture_height;
    DWORD affinity_query_known, affinity;
    DWORD ping_answered, placement_unchanged, child_controls, clipped_hints, child_scan_limited;
    DWORD foreground_at_capture, png_bytes;
    SYSTEMTIME utc;
} ReviewResult;

typedef struct ReviewRun {
    wchar_t directory[REVIEW_PATH];
    int count, report_ok;
    ReviewWindow selected[REVIEW_MAX_WINDOWS];
    ReviewResult results[REVIEW_MAX_WINDOWS];
} ReviewRun;

static HINSTANCE g_instance;
static HWND g_main, g_list, g_refresh, g_other, g_capture, g_open, g_status;
static HFONT g_font, g_heading;
static HBRUSH g_surface;
static SuiteEmblemEngine g_emblem;
static ReviewWindow g_windows[REVIEW_MAX_LIST];
static int g_window_count, g_list_limited, g_busy, g_close_after;
static HANDLE g_thread;
static volatile LONG g_cancel;
static ReviewRun g_run;

static uint64_t ReviewFileTime(FILETIME t) {
    return ((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime;
}

static int ReviewJoin(wchar_t *out, const wchar_t *directory, const wchar_t *leaf) {
    return SUCCEEDED(StringCchPrintfW(out, REVIEW_PATH, L"%ls\\%ls", directory, leaf));
}

static int ReviewLeaf(wchar_t *out, const wchar_t *directory, int index, const wchar_t *suffix) {
    wchar_t leaf[48];
    return index >= 0 && index < REVIEW_MAX_WINDOWS &&
        SUCCEEDED(StringCchPrintfW(leaf, 48, L"window-%02d.%ls", index + 1, suffix)) &&
        ReviewJoin(out, directory, leaf);
}

/* The only output tree is the current user's LocalAppData\TestsSuiteReview.
   Never follow reparse ancestors or accept redirected/remote output paths. */
static int ReviewLocalDirectory(const wchar_t *path) {
    wchar_t copy[REVIEW_PATH];
    size_t length = wcslen(path);
    if (length < 3 || length >= REVIEW_PATH || path[1] != L':' || path[2] != L'\\' ||
        GetDriveTypeW((wchar_t[]){path[0], L':', L'\\', 0}) != DRIVE_FIXED) return 0;
    if (FAILED(StringCchCopyW(copy, REVIEW_PATH, path))) return 0;
    for (size_t i = 3; i <= length; ++i) {
        if (copy[i] == L'\\' || copy[i] == 0) {
            wchar_t saved = copy[i];
            copy[i] = 0;
            DWORD attributes = GetFileAttributesW(copy);
            copy[i] = saved;
            if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
        }
    }
    return 1;
}

static int ReviewRoot(wchar_t *out, int create) {
    PWSTR local = NULL;
    int ok = 0;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &local))) return 0;
    if (ReviewLocalDirectory(local) && ReviewJoin(out, local, L"TestsSuiteReview")) {
        DWORD attributes = GetFileAttributesW(out);
        if (attributes == INVALID_FILE_ATTRIBUTES && create) {
            if (!CreateDirectoryW(out, NULL)) goto done;
        }
        ok = ReviewLocalDirectory(out);
    }
done:
    CoTaskMemFree(local);
    return ok;
}

static int ReviewSessionPath(const wchar_t *directory) {
    wchar_t root[REVIEW_PATH];
    if (!ReviewRoot(root, 0)) return 0;
    size_t n = wcslen(root);
    if (_wcsnicmp(root, directory, n) != 0 || directory[n] != L'\\') return 0;
    const wchar_t *leaf = directory + n + 1;
    if (wcslen(leaf) != 46 || wcsncmp(leaf, L"AppReview-", 10) != 0) return 0;
    for (int i = 10; i < 46; ++i) {
        wchar_t c = leaf[i];
        if (i == 18 || i == 23 || i == 28 || i == 33) { if (c != L'-') return 0; }
        else if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') ||
                   (c >= L'A' && c <= L'F'))) return 0;
    }
    return ReviewLocalDirectory(directory);
}

static int ReviewNewSession(wchar_t *out) {
    wchar_t root[REVIEW_PATH], guid[40], leaf[56];
    GUID id;
    if (!ReviewRoot(root, 1) || FAILED(CoCreateGuid(&id)) || !StringFromGUID2(&id, guid, 40)) return 0;
    guid[37] = 0; /* Remove braces; leave exactly one GUID-named child. */
    if (FAILED(StringCchPrintfW(leaf, 56, L"AppReview-%ls", guid + 1)) ||
        !ReviewJoin(out, root, leaf)) return 0;
    if (!CreateDirectoryW(out, NULL)) return 0; /* No reuse or overwrite. */
    return ReviewSessionPath(out);
}

static int ReviewWriteNew(const wchar_t *path, const void *data, DWORD size) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    DWORD written = 0;
    int ok = WriteFile(file, data, size, &written, NULL) && written == size && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

static int ReviewDescribe(HWND window, ReviewWindow *out) {
    DWORD pid = 0, session = 0, own_session = 0;
    FILETIME created, exited, kernel, user;
    wchar_t path[1024];
    DWORD length = 1024;
    ZeroMemory(out, sizeof(*out));
    if (!IsWindow(window) || !GetWindowThreadProcessId(window, &pid) || !pid ||
        !ProcessIdToSessionId(pid, &session) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &own_session) || session != own_session) return 0;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 0;
    int ok = GetProcessTimes(process, &created, &exited, &kernel, &user) &&
             QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!ok) return 0;
    if (length >= 1024) return 0;
    path[length] = 0;
    const wchar_t *image = wcsrchr(path, L'\\');
    if (!image || FAILED(StringCchCopyW(out->image, 128, image + 1))) return 0;
    out->hwnd = (uint64_t)(uintptr_t)window;
    out->pid = pid;
    out->created = ReviewFileTime(created);
    if (!GetClassNameW(window, out->class_name, 128)) return 0;
    GetWindowTextW(window, out->title, 256); /* Cross-process top-level caption only. */
    return 1;
}

static int ReviewSameIdentity(const ReviewWindow *a, const ReviewWindow *b) {
    return a->hwnd == b->hwnd && a->pid == b->pid && a->created == b->created &&
           _wcsicmp(a->image, b->image) == 0 && wcscmp(a->class_name, b->class_name) == 0;
}

#include "parts/wgc.c"
#include "parts/capture.c"
#include "parts/report.c"
#include "parts/ui.c"

#ifndef REVIEW_NATIVE_TEST
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR command, int show) {
    (void)previous; (void)command;
    g_instance = instance;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    if (argc > 1) {
        int result = ReviewWorkerMain(argc, argv);
        LocalFree(argv);
        return result;
    }
    LocalFree(argv);
    return ReviewLaunchUi(show);
}
#endif
