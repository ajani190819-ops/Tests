/* Settings-preserving review policy. Portable C11; no OS or filesystem calls. */
#ifndef APP_REVIEW_POLICY_H
#define APP_REVIEW_POLICY_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

#define REVIEW_VERSION "0.1.0"
#define REVIEW_VERSION_W L"0.1.0"
#define REVIEW_MAX_WINDOWS 16
#define REVIEW_MAX_SIDE 8192
#define REVIEW_MAX_PIXELS 16000000u
#define REVIEW_MAX_PNG (24u * 1024u * 1024u)
#define REVIEW_WORKER_MS 8000u

typedef enum ReviewStatus {
    REVIEW_IMAGE = 0, /* Image available, never a visual/functional pass. */
    REVIEW_IDENTITY,
    REVIEW_HIDDEN,
    REVIEW_MINIMIZED,
    REVIEW_CLOAKED,
    REVIEW_PROTECTED,
    REVIEW_QUERY_FAILED,
    REVIEW_GEOMETRY,
    REVIEW_UNRESPONSIVE,
    REVIEW_CHANGED,
    REVIEW_CAPTURE_FAILED,
    REVIEW_BLANK,
    REVIEW_TIMEOUT,
    REVIEW_CANCELLED,
    REVIEW_IO_ERROR,
    REVIEW_STATUS_COUNT
} ReviewStatus;

typedef struct ReviewConditions {
    int identity, visible, minimized, cloaked, queries_ok;
    unsigned affinity;
    int width, height;
} ReviewConditions;

static inline int ReviewDimensions(int width, int height) {
    return width > 0 && height > 0 && width <= REVIEW_MAX_SIDE &&
           height <= REVIEW_MAX_SIDE &&
           (uint64_t)(unsigned)width * (unsigned)height <= REVIEW_MAX_PIXELS;
}

static inline ReviewStatus ReviewGate(ReviewConditions c) {
    if (!c.identity) return REVIEW_IDENTITY;
    if (!c.visible) return REVIEW_HIDDEN;
    if (c.minimized) return REVIEW_MINIMIZED;
    if (c.cloaked) return REVIEW_CLOAKED;
    if (!c.queries_ok) return REVIEW_QUERY_FAILED;
    if (c.affinity != 0) return REVIEW_PROTECTED;
    if (!ReviewDimensions(c.width, c.height)) return REVIEW_GEOMETRY;
    return REVIEW_IMAGE;
}

static inline const char *ReviewStatusName(int status) {
    static const char *const names[] = {
        "IMAGE_UNVERIFIED", "WINDOW_IDENTITY_CHANGED", "HIDDEN_NOT_RESTORED",
        "MINIMIZED_NOT_RESTORED", "CLOAKED_NOT_REVEALED", "CAPTURE_PROTECTED",
        "WINDOW_QUERY_FAILED", "SIZE_OUT_OF_BOUNDS", "PING_NOT_ANSWERED",
        "WINDOW_CHANGED_DURING_CAPTURE", "COMPOSITOR_CAPTURE_FAILED",
        "BLANK_OR_INCOMPLETE_IMAGE", "CAPTURE_WORKER_TIMED_OUT", "CANCELLED",
        "OUTPUT_OR_WORKER_ERROR"
    };
    return status >= 0 && status < REVIEW_STATUS_COUNT ? names[status] : "INVALID_RESULT";
}

static inline const wchar_t *ReviewKnownApp(const wchar_t *image) {
    static const struct { const wchar_t *image, *name; } apps[] = {
        {L"ProjectHub.exe", L"Project Hub"},
        {L"BluetoothGuard.exe", L"Bluetooth Guard"},
        {L"KeyboardLighting.exe", L"Prism Lighting (legacy EXE name)"},
        {L"DnsPilot.exe", L"DNS Pilot"},
        {L"AgentNotes.exe", L"Agent Notes"},
        {L"AudioSwitcher.exe", L"Audio Switcher"},
        {L"WeatherRadar.exe", L"Weather Report"}
    };
    for (size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); ++i) {
        const wchar_t *a = image, *b = apps[i].image;
        while (*a && *b) {
            wchar_t x = *a++, y = *b++;
            if (x >= L'A' && x <= L'Z') x += L'a' - L'A';
            if (y >= L'A' && y <= L'Z') y += L'a' - L'A';
            if (x != y) break;
            if (!*a && !*b) return apps[i].name;
        }
    }
    return NULL; /* A name match is categorization, NOT a trust verdict. */
}

static inline int ReviewParseNumber(const wchar_t *s, uint64_t max, uint64_t *out) {
    uint64_t n = 0;
    if (!s || !*s) return 0;
    for (; *s; ++s) {
        unsigned digit = (unsigned)(*s - L'0');
        if (digit > 9 || n > max / 10 || (n == max / 10 && digit > max % 10)) return 0;
        n = n * 10 + digit;
    }
    *out = n;
    return 1;
}

/* A nearly uniform image is insufficient visual evidence. A few cursor-like
   outliers on an otherwise blank/protected surface must not make it usable. */
static inline int ReviewPixelsUsable(const uint32_t *pixels, size_t count) {
    if (!pixels || count == 0 || count > REVIEW_MAX_PIXELS) return 0;
    uint32_t first = pixels[0] & 0x00FFFFFFu;
    size_t different = 0;
    for (size_t i = 0; i < count; ++i) {
        uint32_t p = pixels[i] & 0x00FFFFFFu;
        for (unsigned channel = 0; channel < 3; ++channel) {
            int delta = (int)((p >> (8 * channel)) & 255u) - (int)((first >> (8 * channel)) & 255u);
            if (delta < -12 || delta > 12) { ++different; break; }
        }
    }
    return different * 100 > count; /* >1% materially different pixels. */
}

/* ASCII-only evidence escaping: never inject window titles into HTML/JSON. */
static inline void ReviewJsonText(FILE *out, const wchar_t *s) {
    fputc('"', out);
    for (; *s; ++s) {
        unsigned c = (unsigned)*s;
        if (c == '"' || c == '\\') { fputc('\\', out); fputc((int)c, out); }
        else if (c >= 32 && c < 127 && c != '<' && c != '>' && c != '&') fputc((int)c, out);
        else if (c <= 0xFFFFu) fprintf(out, "\\u%04x", c);
        else { /* wchar_t is 32-bit on the portable test host. */
            c -= 0x10000u;
            fprintf(out, "\\u%04x\\u%04x", 0xD800u + (c >> 10), 0xDC00u + (c & 1023u));
        }
    }
    fputc('"', out);
}

static inline void ReviewHtmlText(FILE *out, const wchar_t *s) {
    for (; *s; ++s) {
        unsigned c = (unsigned)*s;
        if (c == '&') fputs("&amp;", out);
        else if (c == '<') fputs("&lt;", out);
        else if (c == '>') fputs("&gt;", out);
        else if (c == '"') fputs("&quot;", out);
        else if (c == '\'') fputs("&#39;", out);
        else if (c >= 32 && c < 127) fputc((int)c, out);
        else {
            if (c >= 0xD800u && c <= 0xDBFFu && s[1] >= 0xDC00 && s[1] <= 0xDFFF) {
                c = 0x10000u + ((c - 0xD800u) << 10) + ((unsigned)*++s - 0xDC00u);
            } else if (c < 32 || (c >= 0xD800u && c <= 0xDFFFu)) c = 0xFFFDu;
            fprintf(out, "&#%u;", c);
        }
    }
}
#endif
