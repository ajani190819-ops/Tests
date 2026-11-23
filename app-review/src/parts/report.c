/* Reports contain selected windows only. No external assets, script, network
   requests, automatic upload, clipboard, or whole-desktop image. */
#include <io.h>
#include <fcntl.h>

static FILE *ReviewOpenNewText(const wchar_t *path) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    int fd = _open_osfhandle((intptr_t)file, _O_WRONLY | _O_BINARY);
    if (fd == -1) { CloseHandle(file); return NULL; }
    FILE *stream = _fdopen(fd, "wb");
    if (!stream) _close(fd);
    return stream;
}

static uint32_t ReviewBigEndian(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static int ReviewPngValid(const wchar_t *path, const ReviewResult *result) {
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) return 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    unsigned char header[24];
    DWORD read = 0;
    LARGE_INTEGER size;
    int ok = GetFileSizeEx(file, &size) && size.QuadPart >= 24 && size.QuadPart <= REVIEW_MAX_PNG &&
        size.QuadPart == result->png_bytes && ReadFile(file, header, 24, &read, NULL) && read == 24 &&
        memcmp(header, "\x89PNG\r\n\x1a\n", 8) == 0 && memcmp(header + 12, "IHDR", 4) == 0 &&
        ReviewBigEndian(header + 16) == (uint32_t)result->capture_width &&
        ReviewBigEndian(header + 20) == (uint32_t)result->capture_height;
    CloseHandle(file);
    return ok;
}

static int ReviewBase64(FILE *out, const wchar_t *path) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    FILE *input = _wfopen(path, L"rb");
    if (!input) return 0;
    unsigned char bytes[3];
    size_t count;
    while ((count = fread(bytes, 1, 3, input)) != 0) {
        unsigned value = (unsigned)bytes[0] << 16;
        if (count > 1) value |= (unsigned)bytes[1] << 8;
        if (count > 2) value |= bytes[2];
        fputc(alphabet[value >> 18], out);
        fputc(alphabet[(value >> 12) & 63], out);
        fputc(count > 1 ? alphabet[(value >> 6) & 63] : '=', out);
        fputc(count > 2 ? alphabet[value & 63] : '=', out);
    }
    int ok = !ferror(input) && !ferror(out);
    fclose(input);
    return ok;
}

static void ReviewTimestamp(FILE *out, const SYSTEMTIME *time) {
    if (!time->wYear) { fputs("not attempted", out); return; }
    fprintf(out, "%04u-%02u-%02uT%02u:%02u:%02uZ", time->wYear, time->wMonth, time->wDay,
             time->wHour, time->wMinute, time->wSecond);
}

static int ReviewJsonReport(const ReviewRun *run) {
    wchar_t path[REVIEW_PATH];
    if (!ReviewJoin(path, run->directory, L"report.json")) return 0;
    FILE *out = ReviewOpenNewText(path);
    if (!out) return 0;
    fputs("{\n  \"schema\": \"tests.app-review.v1\",\n  \"tool_version\": \"" REVIEW_VERSION "\",\n"
          "  \"capture_kind\": \"os_composited_window\",\n"
          "  \"capture_api\": \"Windows.Graphics.Capture(CreateForWindow)\",\n"
          "  \"whole_desktop_capture\": false,\n  \"owner_acceptance\": false,\n"
          "  \"visual_acceptance\": \"NOT_EVALUATED\",\n  \"functional_acceptance\": \"NOT_EVALUATED\",\n"
          "  \"target_input_or_settings_requests\": false,\n  \"automatic_upload\": false,\n", out);
#ifdef REVIEW_NATIVE_TEST
    fputs("  \"synthetic_fixture\": true,\n", out);
#else
    fputs("  \"synthetic_fixture\": false,\n", out);
#endif
    fputs("  \"windows\": [\n", out);
    for (int i = 0; i < run->count; ++i) {
        const ReviewResult *r = &run->results[i];
        fputs("    {\"image_name\": ", out); ReviewJsonText(out, r->window.image);
        fputs(", \"title\": ", out); ReviewJsonText(out, r->window.title);
        fputs(", \"class\": ", out); ReviewJsonText(out, r->window.class_name);
        fputs(", \"file_version\": ", out); ReviewJsonText(out, r->window.version);
        fprintf(out, ", \"pid\": %lu, \"process_created_filetime\": \"%llu\", \"status\": \"%s\", "
            "\"native_error_code\": %lu, \"window_rect\": [%ld,%ld,%ld,%ld], \"dpi\": %u, "
            "\"capture_dimensions\": [%d,%d], \"affinity_query_known\": %s, \"affinity\": %lu, "
            "\"ping_answered\": %s, \"placement_unchanged\": %s, \"standard_child_controls\": %lu, "
            "\"possible_clipping_hints\": %lu, \"child_scan_limited\": %s, \"foreground_at_capture\": %s, "
            "\"png_bytes\": %lu, \"png\": ",
            (unsigned long)r->window.pid, (unsigned long long)r->window.created,
            ReviewStatusName((int)r->status), (unsigned long)r->error,
            r->rect.left, r->rect.top, r->rect.right, r->rect.bottom, r->dpi,
            r->capture_width, r->capture_height, r->affinity_query_known ? "true" : "false", (unsigned long)r->affinity,
            r->ping_answered ? "true" : "false", r->placement_unchanged ? "true" : "false",
            (unsigned long)r->child_controls, (unsigned long)r->clipped_hints,
            r->child_scan_limited ? "true" : "false", r->foreground_at_capture ? "true" : "false",
            (unsigned long)r->png_bytes);
        if (r->status == REVIEW_IMAGE) fprintf(out, "\"window-%02d.png\"", i + 1);
        else fputs("null", out);
        fputs(", \"capture_attempt_utc\": ", out);
        if (r->utc.wYear) { fputc('"', out); ReviewTimestamp(out, &r->utc); fputc('"', out); }
        else fputs("null", out); /* Cancelled before scheduling: no fabricated timestamp. */
        fprintf(out, "}%s\n", i + 1 == run->count ? "" : ",");
    }
    fputs("  ],\n  \"boundary\": \"WM_NULL is only a responsiveness ping. Standard-child bounds are hints, not "
          "coverage of custom drawing. OS capture may be unavailable or incomplete. Images are not whole-desktop "
          "screenshots, approved baselines, security verdicts or functional/hardware/print acceptance. "
          "File versions are on-disk resource labels, not loaded-image hashes or signature checks. "
          "The utility does not read or write app configuration, send input, move/restore windows, or launch apps. "
          "Apps may continue their own normal background work.\"\n}\n", out);
    int ok = !ferror(out);
    return fclose(out) == 0 && ok;
}

static int ReviewHtmlReport(const ReviewRun *run) {
    wchar_t path[REVIEW_PATH];
    if (!ReviewJoin(path, run->directory, L"report.html")) return 0;
    FILE *out = ReviewOpenNewText(path);
    if (!out) return 0;
    fputs("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
          "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src data:; "
          "style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'\">"
          "<title>App Review - local window evidence</title><style>"
          "body{font:16px/1.55 system-ui,sans-serif;background:#181b24;color:#ecedf3;max-width:1100px;margin:32px auto;padding:0 20px}"
          "h1,h2{line-height:1.2}section{background:#232736;border:1px solid #41495e;border-radius:16px;padding:20px;margin:24px 0}"
          "img{max-width:100%;height:auto;background:#08090d;border-radius:8px}code{overflow-wrap:anywhere;color:#b6d5ff}"
          "p{overflow-wrap:anywhere}.note{color:#c2c8d8}.warning{border-left:4px solid #efc176;padding-left:16px}"
          "</style></head><body><h1>App Review " REVIEW_VERSION "</h1>"
          "<p class=\"warning\"><strong>Evidence only. Visual and functional acceptance: NOT EVALUATED.</strong> "
          "These are OS-composited screenshots of selected running windows using Windows.Graphics.Capture, "
          "not whole-desktop captures or synthetic app previews. Review the images and titles for private information before sharing this file.</p>"
          "<p>No automatic upload, target input, settings-change requests, app launch/update, window activation, move, "
          "resize or restore. No app configuration is read or written. Apps can still do their own normal background work. "
          "Minimized, hidden, cloaked or capture-protected windows are skipped, never revealed or bypassed.</p>"
          "<p class=\"note\">A WM_NULL reply only shows the window answered a ping. Native child-control bounds are "
          "possible clipping hints, not checks of custom-painted controls. Unsupported/GPU areas can be missing even "
          "when an image exists. Blank/incomplete captures and timeouts are not passes. No Bluetooth, location, "
          "settings persistence, game, security or physical-print acceptance is inferred.</p>", out);
#ifdef REVIEW_NATIVE_TEST
    fputs("<p class=\"warning\">SYNTHETIC NATIVE FIXTURE REPORT. Not installed-app or owner desktop evidence.</p>", out);
#endif
    int ok = 1;
    for (int i = 0; i < run->count; ++i) {
        const ReviewResult *r = &run->results[i];
        fprintf(out, "<section><h2>%d. ", i + 1);
        ReviewHtmlText(out, r->window.image);
        fputs("</h2><p><strong>", out); fputs(ReviewStatusName((int)r->status), out);
        fputs("</strong></p><p>Selected title: ", out); ReviewHtmlText(out, r->window.title);
        fputs("</p><p>Window class: <code>", out); ReviewHtmlText(out, r->window.class_name);
        fprintf(out, "</code> &middot; PID %lu &middot; Process creation FILETIME <code>%llu</code><br>"
                "On-disk version label: <code>", (unsigned long)r->window.pid, (unsigned long long)r->window.created);
        ReviewHtmlText(out, r->window.version[0] ? r->window.version : L"unavailable");
        fputs("</code> (not a hash, signature or trust check)</p><p>Capture attempt: ", out);
        ReviewTimestamp(out, &r->utc);
        fprintf(out, "<br>Window rect [%ld, %ld, %ld, %ld] &middot; DPI %u (0 = unknown)<br>"
            "Captured image: %d x %d pixels<br>Ping answered: %s &middot; Placement unchanged during capture: %s<br>"
            "Standard child controls: %lu &middot; Possible clipping hints: %lu%s &middot; Native error (Win32/HRESULT): %lu</p>",
            r->rect.left, r->rect.top, r->rect.right, r->rect.bottom, r->dpi,
            r->capture_width, r->capture_height,
            r->ping_answered ? "yes" : "not established", r->placement_unchanged ? "yes" : "not established",
            (unsigned long)r->child_controls, (unsigned long)r->clipped_hints,
            r->child_scan_limited ? " (scan limit reached)" : "", (unsigned long)r->error);
        if (r->status == REVIEW_IMAGE) {
            wchar_t png[REVIEW_PATH];
            fputs("<img alt=\"Unverified image of the selected running window\" src=\"data:image/png;base64,", out);
            if (!ReviewLeaf(png, run->directory, i, L"png") || !ReviewBase64(out, png)) ok = 0;
            fputs("\">", out);
        } else {
            fputs("<p class=\"warning\">No usable image established. No desktop/clipboard fallback was attempted. "
                  "This is an evidence gap, not proof of an app defect or a successful review.</p>", out);
        }
        fputs("</section>", out);
    }
    fputs("<p class=\"note\">Self-contained local report: selected images are embedded, with no external assets, "
          "scripts or network calls. Share only after reviewing. Exact privacy-sensitive titles or contents can "
          "appear in selected images. Nothing from unselected windows is included. No owner acceptance was changed.</p>"
          "</body></html>\n", out);
    if (ferror(out)) ok = 0;
    return fclose(out) == 0 && ok;
}

static DWORD WINAPI ReviewBatchThread(void *unused) {
    (void)unused;
    for (int i = 0; i < g_run.count; ++i) {
        if (InterlockedCompareExchange(&g_cancel, 0, 0)) {
            ZeroMemory(&g_run.results[i], sizeof(ReviewResult));
            g_run.results[i].window = g_run.selected[i];
            g_run.results[i].status = REVIEW_CANCELLED;
            continue;
        }
        ReviewRunWorker(&g_run.selected[i], g_run.directory, i, REVIEW_WORKER_MS, &g_run.results[i]);
        if (g_run.results[i].status == REVIEW_IMAGE) {
            wchar_t png[REVIEW_PATH];
            if (!ReviewLeaf(png, g_run.directory, i, L"png") || !ReviewPngValid(png, &g_run.results[i]))
                g_run.results[i].status = REVIEW_IO_ERROR;
        }
        PostMessageW(g_main, REVIEW_PROGRESS, (WPARAM)(i + 1), 0);
    }
    int json_ok = ReviewJsonReport(&g_run);
    int html_ok = ReviewHtmlReport(&g_run);
    g_run.report_ok = json_ok && html_ok;
    PostMessageW(g_main, REVIEW_DONE, 0, 0);
    return 0;
}
