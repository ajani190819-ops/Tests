/* ---- global brightness hotkeys --------------------------------------
 * Two system-wide hotkeys (default Ctrl+Alt+Up / Ctrl+Alt+Down) so
 * brightness can live on keys G-Helper does not fight over. Bind M4/M5 to
 * these combos in G-Helper's key remap and the spare keys become smooth
 * brightness control with no Fn traffic at all. Override in presets.ini:
 * [Keys] UpMod/UpKey/DownMod/DownKey use the Win32 RegisterHotKey codes.
 */
static void StepMasterBrightness(int dir) {
    int pct = g_cfg.masterBrightness + dir * 10;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (pct == g_cfg.masterBrightness) return;
    g_cfg.masterBrightness = pct;
    if (pct <= 0)
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  Off");
    else
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  %d%%", pct);
    g_toastAt = GetTickCount64() + 1400ULL;
    SavePanelCfg();
    ScheduleLiveApply();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void RegisterBrightnessHotkeys(HWND hwnd) {
    wchar_t path[MAX_PATH];
    PresetPath(path);
    UINT upMod = GetPrivateProfileIntW(L"Keys", L"UpMod", MOD_CONTROL | MOD_ALT, path);
    UINT upKey = GetPrivateProfileIntW(L"Keys", L"UpKey", 0x26, path);
    UINT dnMod = GetPrivateProfileIntW(L"Keys", L"DownMod", MOD_CONTROL | MOD_ALT, path);
    UINT dnKey = GetPrivateProfileIntW(L"Keys", L"DownKey", 0x28, path);
    if (!RegisterHotKey(hwnd, 1, upMod, upKey))
        RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 0x74);   /* fallback Ctrl+Alt+F5 */
    if (!RegisterHotKey(hwnd, 2, dnMod, dnKey))
        RegisterHotKey(hwnd, 2, MOD_CONTROL | MOD_ALT, 0x75);   /* fallback Ctrl+Alt+F6 */
}

/* Lightweight parser preserving existing user panel.json settings */
static const char *JsonVal(const char *src, const char *end, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = src;
    while (p && p < end) {
        const char *hit = strstr(p, pat);
        if (!hit || hit >= end) return NULL;
        const char *q = hit + strlen(pat);
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q < end && *q == ':') {
            q++;
            while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
            return q;
        }
        p = hit + 1;
    }
    return NULL;
}

static BOOL JsonBool(const char *src, const char *end, const char *key, BOOL defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v) return defVal;
    if (strncmp(v, "true", 4) == 0) return TRUE;
    if (strncmp(v, "false", 5) == 0) return FALSE;
    return defVal;
}

static int JsonInt(const char *src, const char *end, const char *key, int defVal) {
    const char *v = JsonVal(src, end, key);
    if (!v) return defVal;
    return atoi(v);
}

static void JsonStr(const char *src, const char *end, const char *key, char *out, size_t cap) {
    const char *v = JsonVal(src, end, key);
    if (!v || *v != '"') return;
    v++;
    size_t i = 0;
    while (v < end && *v && *v != '"' && i + 1 < cap) out[i++] = *v++;
    out[i] = '\0';
}

static void ParseGroupJson(const char *src, const char *end, GroupCfg *g) {
    g->on         = JsonBool(src, end, "On", g->on);
    g->speed      = JsonInt(src, end, "Speed", g->speed);
    g->brightness = JsonInt(src, end, "Brightness", g->brightness);
    g->mirror     = JsonBool(src, end, "Mirror", g->mirror);
    g->reverse    = JsonBool(src, end, "Reverse", g->reverse);
    g->equalise   = JsonBool(src, end, "Equalise", g->equalise);
    g->loop       = JsonBool(src, end, "Loop", g->loop);
    if (g->speed < 1) g->speed = 1;
    if (g->speed > 50) g->speed = 50;
    if (g->brightness < 0) g->brightness = 0;
    if (g->brightness > 100) g->brightness = 100;

    char eff[64] = "";
    JsonStr(src, end, "Effect", eff, sizeof(eff));
    if (eff[0]) g->effectIdx = FindEffectIndex(eff);

    const char *sw = JsonVal(src, end, "Swatches");
    if (sw && *sw == '[') {
        const char *brEnd = strchr(sw, ']');
        if (brEnd && brEnd < end) {
            int n = 0;
            const char *p = sw + 1;
            while (p < brEnd && n < 8) {
                const char *q1 = strchr(p, '"');
                if (!q1 || q1 >= brEnd) break;
                const char *q2 = strchr(q1 + 1, '"');
                if (!q2 || q2 >= brEnd) break;
                g->swatches[n++] = ParseHexColor(q1 + 1, RGB(56, 189, 248));
                p = q2 + 1;
            }
            if (n >= 1) g->swatchCount = n;
        }
    }

    const char *wd = JsonVal(src, end, "Widths");
    if (wd && *wd == '[') {
        const char *brEnd = strchr(wd, ']');
        if (brEnd && brEnd < end) {
            int n = 0;
            const char *p = wd + 1;
            while (p < brEnd && n < 8) {
                while (p < brEnd && (*p == ' ' || *p == ',' || *p == '\r' || *p == '\n')) p++;
                if (p >= brEnd || *p == ']') break;
                char *next = NULL;
                double val = strtod(p, &next);
                if (next == p) break;
                if (val < 0.25) val = 0.25;
                if (val > 4.0) val = 4.0;
                g->widths[n++] = val;
                p = next;
            }
        }
    }
}

static void LoadPanelCfg(void) {
    InitDefaultCfg();
    DWORD len = 0;
    char *json = ReadFileUtf8(g_panelPath, &len);
    if (!json) return;
    const char *end = json + len;

    const char *kbdPos = JsonVal(json, end, "Kbd");
    const char *topEnd = kbdPos ? kbdPos : end;
    g_cfg.masterBrightness = JsonInt(json, topEnd, "Brightness", 100);
    if (g_cfg.masterBrightness < 0) g_cfg.masterBrightness = 0;
    if (g_cfg.masterBrightness > 100) g_cfg.masterBrightness = 100;
    g_cfg.overlay = JsonBool(json, topEnd, "Overlay", g_cfg.overlay);
    g_cfg.link    = JsonBool(json, topEnd, "Link", g_cfg.link);
    g_cfg.smooth  = JsonBool(json, topEnd, "Smooth", g_cfg.smooth);
    JsonStr(json, topEnd, "OnExit", g_cfg.onExit, sizeof(g_cfg.onExit));

    const char *barPos = JsonVal(json, end, "Bar");
    if (kbdPos) {
        const char *kEnd = (barPos && barPos > kbdPos) ? barPos : end;
        ParseGroupJson(kbdPos, kEnd, &g_cfg.kbd);
    }
    if (barPos) {
        ParseGroupJson(barPos, end, &g_cfg.bar);
    }
    free(json);
}

static void FormatGroupPanelJson(const GroupCfg *g, char *out, size_t cap) {
    char swBuf[256] = "", wdBuf[256] = "";
    size_t so = 0, wo = 0;
    for (int i = 0; i < g->swatchCount; i++) {
        char hx[8];
        FormatHexColor(g->swatches[i], hx);
        so += (size_t)snprintf(swBuf + so, sizeof(swBuf) - so, "%s\"%s\"", i ? ", " : "", hx);
        wo += (size_t)snprintf(wdBuf + wo, sizeof(wdBuf) - wo, "%s%.3f", i ? ", " : "", g->widths[i]);
    }
    snprintf(out, cap,
        "  \"%s\": {\r\n"
        "    \"On\": %s,\r\n"
        "    \"Effect\": \"%s\",\r\n"
        "    \"Speed\": %d,\r\n"
        "    \"Brightness\": %d,\r\n"
        "    \"Mirror\": %s,\r\n"
        "    \"Reverse\": %s,\r\n"
        "    \"Equalise\": %s,\r\n"
        "    \"Loop\": %s,\r\n"
        "    \"Swatches\": [ %s ],\r\n"
        "    \"Widths\": [ %s ]\r\n"
        "  }",
        (g == &g_cfg.kbd) ? "Kbd" : "Bar",
        g->on ? "true" : "false",
        kEffects[g->effectIdx].label,
        g->speed,
        g->brightness,
        g->mirror ? "true" : "false",
        g->reverse ? "true" : "false",
        g->equalise ? "true" : "false",
        g->loop ? "true" : "false",
        swBuf, wdBuf);
}

static void SavePanelCfg(void) {
    char kJson[1024], bJson[1024], custBuf[512] = "", full[3200];
    FormatGroupPanelJson(&g_cfg.kbd, kJson, sizeof(kJson));
    FormatGroupPanelJson(&g_cfg.bar, bJson, sizeof(bJson));
    size_t co = 0;
    for (int i = 0; i < 16; i++) {
        if (g_cfg.customColors[i] != RGB(255, 255, 255)) {
            co += (size_t)snprintf(custBuf + co, sizeof(custBuf) - co, "%s%lu",
                                   co ? ", " : "", (unsigned long)g_cfg.customColors[i]);
        }
    }
    snprintf(full, sizeof(full),
        "{\r\n"
        "  \"Brightness\": %d,\r\n"
        "  \"Overlay\": %s,\r\n"
        "  \"Link\": %s,\r\n"
        "  \"OnExit\": \"%s\",\r\n"
        "  \"Smooth\": %s,\r\n"
        "  \"Custom\": [ %s ],\r\n"
        "%s,\r\n"
        "%s\r\n"
        "}\r\n",
        g_cfg.masterBrightness,
        g_cfg.overlay ? "true" : "false",
        g_cfg.link ? "true" : "false",
        g_cfg.onExit[0] ? g_cfg.onExit : "off",
        g_cfg.smooth ? "true" : "false",
        custBuf, kJson, bJson);
    WriteAtomicUtf8(g_panelPath, full);
}

static void WriteLiveBrightness(void) {
    char buf[32];
    int lvl = g_cfg.masterBrightness * 10;
    if (lvl < 0) lvl = 0;
    if (lvl > 1000) lvl = 1000;
    snprintf(buf, sizeof(buf), "%d\r\n", lvl);
    WriteAtomicUtf8(g_livePath, buf);
}

/* Mirror external master-brightness changes (Fn F2 / F3 pressed while this
 * window is open or in tray) back into the panel. The engine owns the keys
 * and writes live.txt; anything we did not just write ourselves is adopted
 * here, so the slider, panel.json and the on-screen toast all follow along
 * exactly like screen-brightness keys drive the system OSD. */
static void ScheduleLiveApply(void);

static void OsdShow(int pct);

static void PollFnBrightness(void) {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastFnPoll < 150ULL) return;
    g_lastFnPoll = now;
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_livePath, &len);
    if (!txt) return;
    int val = atoi(txt);
    free(txt);
    if (val < 0) val = 0;
    if (val > 1000) val = 1000;
    int pct = (val + 5) / 10;
    if (pct == g_cfg.masterBrightness) return;
    /* Value moved without the slider: a Fn key (or another panel) changed it. */
    g_cfg.masterBrightness = pct;
    if (pct <= 0)
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  Off");
    else
        _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  %d%%", pct);
    g_toastAt = now + 1400ULL;
    OsdShow(pct);
    SavePanelCfg();
    ScheduleLiveApply();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* One-line plain-English explanation for every control, shown in the footer
 * while the mouse is over it. This is the built-in manual: no settings are
 * mysterious, even the subtle ones. */
static const wchar_t *HelpForHit(int hitIdx) {
    if (hitIdx < 0 || hitIdx >= g_hitCount) return NULL;
    int kind = g_hits[hitIdx].kind;
    switch (kind) {
    case HIT_HDR_LIGHTS:    return L"Master on/off for every light. The Fn F2 / F3 brightness keys still work while off.";
    case HIT_HDR_STARTUP:   return L"Launch Keyboard Lighting automatically when you sign in to Windows.";
    case HIT_HDR_RESTART:   return L"Restart the lighting engine - use if the lights ever freeze or after an update.";
    case HIT_SLIDER_MASTER: return L"Overall brightness of ALL lights, 0-100%. Same scale the Fn F2 / F3 keys move. 0% = full off.";
    case HIT_TAB_KBD:
    case HIT_TAB_BAR:       return L"Choose which zone the settings below edit. 'Link Zones' keeps both zones identical.";
    case HIT_TOG_ZONE_ON:   return L"Turn just this zone (Keyboard / Light Bar) on or off without losing its pattern.";
    case HIT_TOG_LINK:      return L"Link Zones: every change you make here is copied to the other zone automatically.";
    case HIT_PICK_EFFECT:   return L"Pick the animation pattern. Each entry lists what it does in this bar.";
    case HIT_SWATCH_ADD:    return L"Add another colour band to the palette (up to 8).";
    case HIT_SWATCH_REM:    return L"Remove the last colour band from the palette.";
    case HIT_SLIDER_SPEED:  return L"How fast the pattern moves: 0.1x (slow drift) to 5.0x (fast).";
    case HIT_SLIDER_BRIGHT: return L"Brightness of just this zone, 0-100%. 0% = this zone dark. Multiplies with Master.";
    case HIT_TOG_LOOP:      return L"Loop Travel: pattern circles the Light Bar ring. Off: it bounces back and forth.";
    case HIT_TOG_EQ:        return L"Balance Colours: evens out palette brightness so no single colour dominates.";
    case HIT_TOG_MIRROR:    return L"Mirror: the pattern runs out from the centre to both ends.";
    case HIT_TOG_REVERSE:   return L"Reverse: the pattern travels in the opposite direction.";
    case HIT_TOG_SMOOTH:    return L"Steady Colours: on = no shimmer at any level. Off = fine dithering for ultra-smooth dimming.";
    case HIT_TOG_OVERLAY:   return L"Battery Alerts: briefly show battery status on the lights (restarts engine when toggled).";
    case HIT_FOOT_SAVEP:    return L"Save the current colours and effect as a named pattern (presets.ini).";
    case HIT_FOOT_PATTERN:  return L"Open the pattern list - click any saved pattern to swap the whole look. Ctrl+Alt+Up/Down also step brightness on keys G-Helper does not touch.";
    case HIT_FOOT_ZONES:    return L"Open Zones Setup if patterns jump about - lets you fix which lamp is which.";
    case HIT_FOOT_ELEVATE:  return L"Restart as Administrator to unlock direct USB RGB control of the ROG Strix G16.";
    case HIT_FOOT_QUIT:     return L"Quit Keyboard Lighting and turn the lights off.";
    default:
        if (kind >= HIT_SWATCH_0 && kind < HIT_SWATCH_0 + 8)
            return L"Click to pick this colour band with the Windows colour picker.";
        if (kind >= HIT_WIDTH_0 && kind < HIT_WIDTH_0 + 8)
            return L"Drag to widen or narrow how much of the strip this colour takes.";
        return NULL;
    }
}

static void FormatThemeBlock(const GroupCfg *g, const char *name, char *out, size_t cap) {
    char cols[128] = "", wds[128] = "";
    size_t co = 0, wo = 0;
    for (int i = 0; i < g->swatchCount; i++) {
        char hx[8];
        FormatHexColor(g->swatches[i], hx);
        co += (size_t)snprintf(cols + co, sizeof(cols) - co, "%s%s", i ? "," : "", hx);
        wo += (size_t)snprintf(wds + wo, sizeof(wds) - wo, "%s%.3f", i ? "," : "", g->widths[i]);
    }
    snprintf(out, cap,
        "  \"%s\": {\r\n"
        "    \"On\": %s,\r\n"
        "    \"Effect\": \"%s\",\r\n"
        "    \"Colors\": \"%s\",\r\n"
        "    \"Widths\": \"%s\",\r\n"
        "    \"Speed\": %.2f,\r\n"
        "    \"Brightness\": %.3f,\r\n"
        "    \"Mirror\": %s,\r\n"
        "    \"Reverse\": %s,\r\n"
        "    \"Equalise\": %s,\r\n"
        "    \"Loop\": %s\r\n"
        "  }",
        name,
        g->on ? "true" : "false",
        kEffects[g->effectIdx].token,
        cols, wds,
        (double)g->speed / 10.0,
        (double)g->brightness / 100.0,
        g->mirror ? "true" : "false",
        g->reverse ? "true" : "false",
        g->equalise ? "true" : "false",
        g->loop ? "true" : "false");
}

static void WriteThemeJson(void) {
    char kb[768], bb[768], full[2048];
    FormatThemeBlock(&g_cfg.kbd, "Kbd", kb, sizeof(kb));
    FormatThemeBlock(&g_cfg.bar, "Bar", bb, sizeof(bb));
    snprintf(full, sizeof(full),
        "{\r\n"
        "  \"Brightness\": %.3f,\r\n"
        "  \"Smooth\": %s,\r\n"
        "%s,\r\n"
        "%s\r\n"
        "}\r\n",
        (double)g_cfg.masterBrightness / 100.0,
        g_cfg.smooth ? "true" : "false",
        kb, bb);
    WriteAtomicUtf8(g_themePath, full);
}

static void SyncLinkedZone(void) {
    if (!g_cfg.link) return;
    GroupCfg *src = CurGroup();
    GroupCfg *dst = (g_tab == 1) ? &g_cfg.kbd : &g_cfg.bar;
    *dst = *src;
}

static void WriteHeartbeat(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu\r\n", (unsigned long)GetCurrentProcessId());
    HANDLE hf = CreateFileW(g_lockPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(hf, buf, (DWORD)strlen(buf), &wr, NULL);
        CloseHandle(hf);
    }
}

