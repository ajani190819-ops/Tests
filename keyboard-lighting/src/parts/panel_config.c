/* Saved preferences are independent of live brightness. Parse direct JSON
   members, not text matches; merge local edits against the last read snapshot
   before saving, and patch the latest object so unknown settings survive. */
static PanelCfg g_savedCfg;
static int g_savedMasterLevel;

static int JsonSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *JsonWhite(const char *p, const char *end) {
    while (p < end && JsonSpace(*p)) p++;
    return p;
}

static const char *JsonStringEnd(const char *p, const char *end) {
    if (p >= end || *p++ != '"') return NULL;
    while (p < end) {
        unsigned char c = (unsigned char)*p++;
        if (c == '"') return p;
        if (c < 32) return NULL;
        if (c == '\\') {
            if (p >= end) return NULL;
            char escape = *p++;
            if (escape == 'u') {
                for (int i = 0; i < 4; i++) {
                    if (p >= end || !((*p >= '0' && *p <= '9') ||
                        (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) return NULL;
                    p++;
                }
            } else if (!strchr("\"\\/bfnrt", escape)) return NULL;
        }
    }
    return NULL;
}

static const char *JsonValueEnd(const char *p, const char *end, int depth) {
    p = JsonWhite(p, end);
    if (p >= end || depth > 32) return NULL;
    if (*p == '"') return JsonStringEnd(p, end);
    if (*p == '{' || *p == '[') {
        int object = *p == '{';
        char close = object ? '}' : ']';
        p = JsonWhite(p + 1, end);
        if (p < end && *p == close) return p + 1;
        for (;;) {
            if (object) {
                p = JsonStringEnd(p, end);
                if (!p) return NULL;
                p = JsonWhite(p, end);
                if (p >= end || *p++ != ':') return NULL;
            }
            p = JsonValueEnd(p, end, depth + 1);
            if (!p) return NULL;
            p = JsonWhite(p, end);
            if (p >= end) return NULL;
            if (*p == close) return p + 1;
            if (*p++ != ',') return NULL;
            p = JsonWhite(p, end);
        }
    }
    for (int i = 0; i < 3; i++) {
        const char *word = i == 0 ? "true" : i == 1 ? "false" : "null";
        size_t n = strlen(word);
        if ((size_t)(end - p) >= n && !memcmp(p, word, n)) return p + n;
    }
    if (*p == '-') p++;
    if (p >= end || *p < '0' || *p > '9') return NULL;
    if (*p == '0') p++;
    else while (p < end && *p >= '0' && *p <= '9') p++;
    if (p < end && *p == '.') {
        p++;
        if (p >= end || *p < '0' || *p > '9') return NULL;
        while (p < end && *p >= '0' && *p <= '9') p++;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (p >= end || *p < '0' || *p > '9') return NULL;
        while (p < end && *p >= '0' && *p <= '9') p++;
    }
    return p;
}

static const char *JsonRoot(const char *json, const char *end) {
    if (end - json >= 3 && (unsigned char)json[0] == 0xEF &&
        (unsigned char)json[1] == 0xBB && (unsigned char)json[2] == 0xBF) json += 3;
    const char *root = JsonWhite(json, end);
    if (root >= end || *root != '{') return NULL;
    const char *after = JsonValueEnd(root, end, 0);
    return after && JsonWhite(after, end) == end ? root : NULL;
}

/* src must be the opening brace of ONE object. Nested keys and text inside
   strings cannot shadow a root/zone preference, whatever the property order. */
static const char *JsonVal(const char *src, const char *end, const char *key) {
    if (!src || src >= end || *src != '{') return NULL;
    const char *p = JsonWhite(src + 1, end), *found = NULL;
    size_t n = strlen(key);
    while (p < end && *p != '}') {
        const char *keyEnd = JsonStringEnd(p, end);
        if (!keyEnd) return NULL;
        int match = (size_t)(keyEnd - p) == n + 2 && !memcmp(p + 1, key, n);
        p = JsonWhite(keyEnd, end);
        if (p >= end || *p++ != ':') return NULL;
        p = JsonWhite(p, end);
        if (match) found = p;
        p = JsonValueEnd(p, end, 0);
        if (!p) return NULL;
        p = JsonWhite(p, end);
        if (p < end && *p == ',') p = JsonWhite(p + 1, end);
        else break;
    }
    return found;
}

static BOOL JsonBool(const char *src, const char *end, const char *key, BOOL defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v) return defVal;
    const char *after = JsonValueEnd(v, end, 0);
    if (!after) return defVal;
    if (after - v == 4 && !memcmp(v, "true", 4)) return TRUE;
    if (after - v == 5 && !memcmp(v, "false", 5)) return FALSE;
    return defVal;
}

static int JsonInt(const char *src, const char *end, const char *key, int defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v || (*v != '-' && (*v < '0' || *v > '9'))) return defVal;
    char *after = NULL;
    long value = strtol(v, &after, 10);
    if (after != JsonValueEnd(v, end, 0) || value < -2147483647L || value > 2147483647L) return defVal;
    return (int)value;
}

static void JsonStr(const char *src, const char *end, const char *key, char *out, size_t cap) {
    const char *v = JsonVal(src, end, key);
    if (!v || *v != '"' || !cap) return;
    const char *after = JsonStringEnd(v, end);
    if (!after) return;
    size_t n = 0;
    for (v++; v < after - 1 && n + 1 < cap; v++) {
        char c = *v;
        if (c == '\\') {
            c = *++v;
            if (c == 'u') {
                char hex[5]; memcpy(hex, v + 1, 4); hex[4] = 0;
                long code = strtol(hex, NULL, 16); c = code > 0 && code < 128 ? (char)code : '?';
                v += 4;
            } else {
                const char *escapes = "bfnrt";
                const char *escape = strchr(escapes, c);
                if (escape) c = "\b\f\n\r\t"[escape - escapes];
            }
        }
        out[n++] = c;
    }
    out[n] = 0;
}

static void ParseGroupJson(const char *src, const char *end, GroupCfg *g) {
    g->on = JsonBool(src, end, "On", g->on);
    g->speed = JsonInt(src, end, "Speed", g->speed);
    g->brightness = JsonInt(src, end, "Brightness", g->brightness);
    g->mirror = JsonBool(src, end, "Mirror", g->mirror);
    g->reverse = JsonBool(src, end, "Reverse", g->reverse);
    g->equalise = JsonBool(src, end, "Equalise", g->equalise);
    g->loop = JsonBool(src, end, "Loop", g->loop);
    if (g->speed < 1) g->speed = 1;
    if (g->speed > 50) g->speed = 50;
    if (g->brightness < 0) g->brightness = 0;
    if (g->brightness > 100) g->brightness = 100;
    char effect[64] = "";
    JsonStr(src, end, "Effect", effect, sizeof(effect));
    if (effect[0]) g->effectIdx = FindEffectIndex(effect);
    const char *p = JsonVal(src, end, "Swatches");
    if (p && *p == '[') {
        int n = 0;
        p = JsonWhite(p + 1, end);
        while (p < end && *p == '"' && n < 8) {
            g->swatches[n++] = ParseHexColor(p + 1, RGB(56, 189, 248));
            p = JsonStringEnd(p, end);
            if (!p) break;
            p = JsonWhite(p, end);
            if (p >= end || *p != ',') break;
            p = JsonWhite(p + 1, end);
        }
        if (n) g->swatchCount = n;
    }
    p = JsonVal(src, end, "Widths");
    if (p && *p == '[') {
        p++;
        for (int n = 0; n < 8; n++) {
            p = JsonWhite(p, end);
            if (p >= end || *p == ']') break;
            char *next = NULL;
            double value = strtod(p, &next);
            if (next == p || !isfinite(value)) break;
            g->widths[n] = value < 0.25 ? 0.25 : value > 4.0 ? 4.0 : value;
            p = JsonWhite(next, end);
            if (p >= end || *p != ',') break;
            p++;
        }
    }
}

static BOOL ParsePanelJson(const char *json, size_t len, PanelCfg *cfg, int *level) {
    const char *end = json + len, *root = JsonRoot(json, end);
    if (!root) return FALSE;
    int pct = JsonInt(root, end, "Brightness", cfg->masterBrightness);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int exact = JsonInt(root, end, "MasterLevel", pct * 10);
    /* Legacy writers know only percent. A changed percent invalidates an old
       exact projection, but native half-percent steps survive their roundtrip. */
    *level = exact >= 0 && exact <= 1000 && (exact + 5) / 10 == pct ? exact : pct * 10;
    cfg->masterBrightness = (*level + 5) / 10;
    cfg->overlay = JsonBool(root, end, "Overlay", cfg->overlay);
    cfg->link = JsonBool(root, end, "Link", cfg->link);
    cfg->smooth = JsonBool(root, end, "Smooth", cfg->smooth);
    char onExit[16] = "";
    JsonStr(root, end, "OnExit", onExit, sizeof(onExit));
    if (!strcmp(onExit, "off") || !strcmp(onExit, "white") || !strcmp(onExit, "firmware"))
        strcpy(cfg->onExit, onExit);
    const char *kbd = JsonVal(root, end, "Kbd"), *bar = JsonVal(root, end, "Bar");
    if (kbd && *kbd == '{') ParseGroupJson(kbd, end, &cfg->kbd);
    else {
        int zoneBrightness = cfg->kbd.brightness;
        ParseGroupJson(root, end, &cfg->kbd);
        cfg->kbd.brightness = zoneBrightness; /* flat Brightness was the master */
    }
    if (bar && *bar == '{') ParseGroupJson(bar, end, &cfg->bar);
    const char *p = JsonVal(root, end, "Custom");
    if (p && *p == '[') {
        p++;
        for (int i = 0; i < 16; i++) {
            p = JsonWhite(p, end);
            if (p >= end || *p == ']') break;
            char *next = NULL;
            unsigned long color = strtoul(p, &next, 10);
            if (next == p || color > 0xFFFFFFUL) break;
            cfg->customColors[i] = (COLORREF)color;
            p = JsonWhite(next, end);
            if (p >= end || *p != ',') break;
            p++;
        }
    }
    return TRUE;
}

static void LoadPanelCfg(void) {
    InitDefaultCfg();
    DWORD len = 0;
    char *json = ReadFileUtf8(g_panelPath, &len);
    int level = g_cfg.masterBrightness * 10;
    if (json) { ParsePanelJson(json, len, &g_cfg, &level); free(json); }
    BrightnessInit(&g_brightness, level);
    g_brightnessChannelReady = FALSE;
    g_savedCfg = g_cfg;
    g_savedMasterLevel = level;
}

static void MergeGroupCfg(GroupCfg *local, const GroupCfg *base, const GroupCfg *disk) {
#define MERGE_GROUP(field) if (!memcmp(&local->field, &base->field, sizeof(local->field))) \
    memcpy(&local->field, &disk->field, sizeof(local->field))
    MERGE_GROUP(on); MERGE_GROUP(effectIdx); MERGE_GROUP(speed); MERGE_GROUP(brightness);
    MERGE_GROUP(mirror); MERGE_GROUP(reverse); MERGE_GROUP(equalise); MERGE_GROUP(loop);
    MERGE_GROUP(swatchCount); MERGE_GROUP(swatches); MERGE_GROUP(widths);
#undef MERGE_GROUP
}

static void MergePanelCfg(const PanelCfg *disk, int diskLevel) {
    int before = g_brightness.level;
    if (g_brightness.level == g_savedMasterLevel && before != diskLevel)
        BrightnessAbsolute(&g_brightness, diskLevel);
#define MERGE_PANEL(field) if (!memcmp(&g_cfg.field, &g_savedCfg.field, sizeof(g_cfg.field))) \
    memcpy(&g_cfg.field, &disk->field, sizeof(g_cfg.field))
    MERGE_PANEL(overlay); MERGE_PANEL(link); MERGE_PANEL(smooth);
    MERGE_PANEL(onExit); MERGE_PANEL(customColors);
#undef MERGE_PANEL
    MergeGroupCfg(&g_cfg.kbd, &g_savedCfg.kbd, &disk->kbd);
    MergeGroupCfg(&g_cfg.bar, &g_savedCfg.bar, &disk->bar);
    g_savedCfg = *disk;
    g_savedMasterLevel = diskLevel;
    g_cfg.masterBrightness = (g_brightness.level + 5) / 10;
    PublishMasterBrightness(before, TRUE);
}

/* Replace/append one member without reserializing any unowned member. */
static BOOL PatchJsonMember(char **json, const char *group, const char *key, const char *value) {
    size_t len = strlen(*json);
    const char *end = *json + len, *root = JsonRoot(*json, end);
    if (!root) return FALSE;
    const char *object = group ? JsonVal(root, end, group) : root;
    if (group && (!object || *object != '{')) {
        if (!PatchJsonMember(json, NULL, group, "{}")) return FALSE;
        end = *json + strlen(*json); root = JsonRoot(*json, end);
        object = JsonVal(root, end, group);
    }
    const char *after = JsonValueEnd(object, end, 0);
    if (!after) return FALSE;
    const char *old = JsonVal(object, after, key), *tail;
    char prefix[96] = "";
    if (old) tail = JsonValueEnd(old, after, 0);
    else {
        old = tail = after - 1;
        snprintf(prefix, sizeof(prefix), "%s\r\n  \"%s\": ",
                 JsonWhite(object + 1, after) == after - 1 ? "" : ",", key);
    }
    if (!tail) return FALSE;
    size_t head = (size_t)(old - *json), suffix = (size_t)(end - tail);
    size_t size = head + strlen(prefix) + strlen(value) + suffix;
    if (size > 4 * 1024 * 1024) return FALSE;
    char *out = (char *)malloc(size + 1);
    if (!out) return FALSE;
    memcpy(out, *json, head);
    size_t at = head;
    memcpy(out + at, prefix, strlen(prefix)); at += strlen(prefix);
    memcpy(out + at, value, strlen(value)); at += strlen(value);
    memcpy(out + at, tail, suffix); out[size] = 0;
    free(*json); *json = out;
    return TRUE;
}

static BOOL PatchPanelJson(char **json) {
    char value[512];
#define PATCH(group, key, text) do { if (!PatchJsonMember(json, group, key, text)) return FALSE; } while (0)
#define NUMBER(group, key, number) do { snprintf(value, sizeof(value), "%d", (int)(number)); PATCH(group, key, value); } while (0)
#define BOOLEAN(group, key, flag) PATCH(group, key, (flag) ? "true" : "false")
    NUMBER(NULL, "Brightness", g_cfg.masterBrightness);
    NUMBER(NULL, "MasterLevel", g_brightness.level);
    BOOLEAN(NULL, "Overlay", g_cfg.overlay); BOOLEAN(NULL, "Link", g_cfg.link);
    BOOLEAN(NULL, "Smooth", g_cfg.smooth);
    snprintf(value, sizeof(value), "\"%s\"", g_cfg.onExit); PATCH(NULL, "OnExit", value);
    size_t n = (size_t)snprintf(value, sizeof(value), "[");
    for (int i = 0; i < 16; i++)
        n += (size_t)snprintf(value + n, sizeof(value) - n, "%s%lu", i ? "," : "", (unsigned long)g_cfg.customColors[i]);
    snprintf(value + n, sizeof(value) - n, "]"); PATCH(NULL, "Custom", value);
    for (int zone = 0; zone < 2; zone++) {
        const char *name = zone ? "Bar" : "Kbd";
        const GroupCfg *g = zone ? &g_cfg.bar : &g_cfg.kbd;
        BOOLEAN(name, "On", g->on); NUMBER(name, "Speed", g->speed); NUMBER(name, "Brightness", g->brightness);
        BOOLEAN(name, "Mirror", g->mirror); BOOLEAN(name, "Reverse", g->reverse);
        BOOLEAN(name, "Equalise", g->equalise); BOOLEAN(name, "Loop", g->loop);
        snprintf(value, sizeof(value), "\"%s\"", kEffects[g->effectIdx].label); PATCH(name, "Effect", value);
        n = (size_t)snprintf(value, sizeof(value), "[");
        for (int i = 0; i < g->swatchCount; i++) {
            char hex[8]; FormatHexColor(g->swatches[i], hex);
            n += (size_t)snprintf(value + n, sizeof(value) - n, "%s\"%s\"", i ? "," : "", hex);
        }
        snprintf(value + n, sizeof(value) - n, "]"); PATCH(name, "Swatches", value);
        n = (size_t)snprintf(value, sizeof(value), "[");
        for (int i = 0; i < g->swatchCount; i++)
            n += (size_t)snprintf(value + n, sizeof(value) - n, "%s%.3f", i ? "," : "", g->widths[i]);
        snprintf(value + n, sizeof(value) - n, "]"); PATCH(name, "Widths", value);
    }
#undef BOOLEAN
#undef NUMBER
#undef PATCH
    return TRUE;
}

static void SyncExternalPanelCfg(void) {
    DWORD len = 0;
    char *json = ReadFileUtf8(g_panelPath, &len);
    PanelCfg disk = g_savedCfg;
    int level = g_savedMasterLevel;
    if (json && ParsePanelJson(json, len, &disk, &level)) MergePanelCfg(&disk, level);
    free(json);
}

static void SavePanelCfg(void) {
    DWORD len = 0;
    char *json = ReadFileUtf8(g_panelPath, &len);
    PanelCfg disk = g_savedCfg;
    int level = g_savedMasterLevel;
    g_panelSaveFailed = TRUE;
    if (json) {
        if (!ParsePanelJson(json, len, &disk, &level)) { free(json); return; }
        MergePanelCfg(&disk, level);
    } else {
        if (FileExistsW(g_panelPath)) return; /* unreadable is NOT missing */
        json = (char *)malloc(3);
        if (!json) return;
        strcpy(json, "{}");
    }
    if (PatchPanelJson(&json) && WriteAtomicUtf8(g_panelPath, json)) {
        g_savedCfg = g_cfg;
        g_savedMasterLevel = g_brightness.level;
        g_panelSaveFailed = FALSE;
    }
    free(json);
}
