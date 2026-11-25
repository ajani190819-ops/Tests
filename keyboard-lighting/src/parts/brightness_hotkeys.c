#include "brightness_master.c"

static void StepMasterBrightness(int dir) {
    SyncMasterBrightness();
    SetMasterBrightnessLevel(g_brightness.level + dir * 100);
    SavePanelCfg();
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

#include "panel_config.c"

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
    case HIT_TOG_EQ:        return L"Balance Colours: gently moves palette stops toward similar visual lightness while preserving hue.";
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
        (double)g_brightness.level / 1000.0,
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

