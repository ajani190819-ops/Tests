#ifndef PROJECT_HUB_BRANCH_JSON_H
#define PROJECT_HUB_BRANCH_JSON_H

/* Small, dependency-free reader for GitHub's branches API. Keep this separate
   from WinHTTP so whitespace/escaping and retry behavior can be exercised on
   non-Windows builders too. */
#include <stddef.h>
#include <string.h>

typedef struct {
    char name[64];
    char sha[48];
} HubBranchTip;

#define HUB_BRANCH_SUCCESS_INTERVAL_MS (6ULL * 60ULL * 60ULL * 1000ULL)
#define HUB_BRANCH_RETRY_INITIAL_MS    (5ULL * 60ULL * 1000ULL)
#define HUB_BRANCH_RETRY_MAX_MS        (60ULL * 60ULL * 1000ULL)

static inline unsigned long long HubBranchCheckInterval(unsigned long long retryDelay) {
    return retryDelay ? retryDelay : HUB_BRANCH_SUCCESS_INTERVAL_MS;
}

static inline unsigned long long HubBranchRetryAfterFailure(unsigned long long previousDelay) {
    if (previousDelay < HUB_BRANCH_RETRY_INITIAL_MS)
        return HUB_BRANCH_RETRY_INITIAL_MS;
    if (previousDelay >= HUB_BRANCH_RETRY_MAX_MS / 2ULL)
        return HUB_BRANCH_RETRY_MAX_MS;
    return previousDelay * 2ULL;
}

static void HubBranchJsonSkipWhitespace(const char **cursor) {
    const char *p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    *cursor = p;
}

static int HubBranchJsonAppend(char *out, size_t cap, size_t *used,
                               int *overflow, unsigned char ch) {
    if (!out) return 1;
    if (*used + 1 >= cap) {
        *overflow = 1;
        return 1;
    }
    out[(*used)++] = (char)ch;
    return 1;
}

static int HubBranchJsonReadString(const char **cursor, char *out, size_t cap) {
    const char *p = *cursor;
    size_t used = 0;
    int overflow = 0;
    HubBranchJsonSkipWhitespace(&p);
    if (*p != '"') return 0;
    p++;
    while (*p) {
        unsigned char ch = (unsigned char)*p++;
        if (ch == '"') {
            if (out && cap) out[overflow ? cap - 1 : used] = '\0';
            *cursor = p;
            return !overflow;
        }
        if (ch == '\\') {
            unsigned int code = 0;
            int i;
            ch = (unsigned char)*p++;
            if (!ch) return 0;
            switch (ch) {
            case '"': case '\\': case '/': break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u':
                for (i = 0; i < 4; i++) {
                    unsigned char h = (unsigned char)*p++;
                    if (h >= '0' && h <= '9') code = (code << 4) | (unsigned int)(h - '0');
                    else if (h >= 'a' && h <= 'f') code = (code << 4) | (unsigned int)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code = (code << 4) | (unsigned int)(h - 'A' + 10);
                    else return 0;
                }
                if (code > 0x7fU) {
                    if (out) overflow = 1;  /* Arena branch names/SHA are ASCII. */
                    continue;
                }
                ch = (unsigned char)code;
                break;
            default:
                return 0;
            }
        } else if (ch < 0x20U) {
            return 0;
        }
        HubBranchJsonAppend(out, cap, &used, &overflow, ch);
    }
    return 0;
}

static int HubBranchJsonSkipValue(const char **cursor, unsigned int depth) {
    const char *p = *cursor;
    HubBranchJsonSkipWhitespace(&p);
    if (depth > 32U) return 0;
    if (*p == '"') {
        if (!HubBranchJsonReadString(&p, NULL, 0)) return 0;
        *cursor = p;
        return 1;
    }
    if (*p == '{') {
        p++;
        HubBranchJsonSkipWhitespace(&p);
        if (*p == '}') { *cursor = p + 1; return 1; }
        for (;;) {
            if (!HubBranchJsonReadString(&p, NULL, 0)) return 0;
            HubBranchJsonSkipWhitespace(&p);
            if (*p++ != ':') return 0;
            if (!HubBranchJsonSkipValue(&p, depth + 1U)) return 0;
            HubBranchJsonSkipWhitespace(&p);
            if (*p == '}') { *cursor = p + 1; return 1; }
            if (*p++ != ',') return 0;
        }
    }
    if (*p == '[') {
        p++;
        HubBranchJsonSkipWhitespace(&p);
        if (*p == ']') { *cursor = p + 1; return 1; }
        for (;;) {
            if (!HubBranchJsonSkipValue(&p, depth + 1U)) return 0;
            HubBranchJsonSkipWhitespace(&p);
            if (*p == ']') { *cursor = p + 1; return 1; }
            if (*p++ != ',') return 0;
        }
    }
    if (*p == '\0' || *p == ',' || *p == ']' || *p == '}' || *p == ':') return 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' &&
           *p != ',' && *p != ']' && *p != '}') p++;
    if (p == *cursor) return 0;
    *cursor = p;
    return 1;
}

static int HubBranchJsonReadSha(const char **cursor, char *sha, size_t cap) {
    const char *p = *cursor;
    int found = 0;
    HubBranchJsonSkipWhitespace(&p);
    if (*p != '{') return HubBranchJsonSkipValue(cursor, 0U);
    p++;
    HubBranchJsonSkipWhitespace(&p);
    if (*p == '}') { *cursor = p + 1; return 1; }
    for (;;) {
        char key[32] = "";
        if (!HubBranchJsonReadString(&p, key, sizeof(key))) return 0;
        HubBranchJsonSkipWhitespace(&p);
        if (*p++ != ':') return 0;
        HubBranchJsonSkipWhitespace(&p);
        if (strcmp(key, "sha") == 0 && *p == '"') {
            if (!HubBranchJsonReadString(&p, sha, cap)) return 0;
            found = 1;
        } else if (!HubBranchJsonSkipValue(&p, 0U)) {
            return 0;
        }
        HubBranchJsonSkipWhitespace(&p);
        if (*p == '}') { *cursor = p + 1; return found; }
        if (*p++ != ',') return 0;
    }
}

static int HubBranchJsonReadBranch(const char **cursor, HubBranchTip *tip) {
    const char *p = *cursor;
    memset(tip, 0, sizeof(*tip));
    HubBranchJsonSkipWhitespace(&p);
    if (*p != '{') return 0;
    p++;
    HubBranchJsonSkipWhitespace(&p);
    if (*p == '}') { *cursor = p + 1; return 1; }
    for (;;) {
        char key[32] = "";
        if (!HubBranchJsonReadString(&p, key, sizeof(key))) return 0;
        HubBranchJsonSkipWhitespace(&p);
        if (*p++ != ':') return 0;
        HubBranchJsonSkipWhitespace(&p);
        if (strcmp(key, "name") == 0 && *p == '"') {
            if (!HubBranchJsonReadString(&p, tip->name, sizeof(tip->name))) return 0;
        } else if (strcmp(key, "commit") == 0) {
            if (!HubBranchJsonReadSha(&p, tip->sha, sizeof(tip->sha))) return 0;
        } else if (!HubBranchJsonSkipValue(&p, 0U)) {
            return 0;
        }
        HubBranchJsonSkipWhitespace(&p);
        if (*p == '}') { *cursor = p + 1; return 1; }
        if (*p++ != ',') return 0;
    }
}

static int HubBranchJsonIsArenaTestBranch(const char *name) {
    static const char prefix[] = "arena/";
    static const char suffix[] = "-tests";
    size_t len = name ? strlen(name) : 0;
    size_t prefixLen = sizeof(prefix) - 1;
    size_t suffixLen = sizeof(suffix) - 1;
    return len > prefixLen + suffixLen &&
           memcmp(name, prefix, prefixLen) == 0 &&
           memcmp(name + len - suffixLen, suffix, suffixLen) == 0;
}

/* Returns -1 for malformed JSON/capacity overflow; otherwise returns the
   number of Arena test branches and their tip SHAs. The API uses an array of
   branch objects, with each object's SHA nested under its "commit" property. */
static int HubBranchJsonExtractArenaTips(const char *json, HubBranchTip *out, size_t cap) {
    const char *p = json;
    size_t count = 0;
    if (!p || !out || cap == 0) return -1;
    HubBranchJsonSkipWhitespace(&p);
    if (*p++ != '[') return -1;
    HubBranchJsonSkipWhitespace(&p);
    if (*p == ']') return 0;
    for (;;) {
        HubBranchTip tip;
        if (!HubBranchJsonReadBranch(&p, &tip)) return -1;
        if (HubBranchJsonIsArenaTestBranch(tip.name)) {
            if (count >= cap) return -1;
            out[count++] = tip;
        }
        HubBranchJsonSkipWhitespace(&p);
        if (*p == ']') return (int)count;
        if (*p++ != ',') return -1;
        HubBranchJsonSkipWhitespace(&p);
    }
}

#endif
