/* Built-in card text as static literals, so a catalog entry that is missing
   or malformed still leaves a card that can be drawn and clicked.
   Every pool-backed field is listed here; `label` must never be NULL because
   DrawActionBtn hands it straight to DrawTextW. */
static const struct { int id; const wchar_t *label; const wchar_t *verb; } g_btnFallback[] = {
    { ID_BTG_OPEN,   L"Open",   L"open"   }, { ID_BTG_UPDATE, L"Update", L"update" },
    { ID_BTG_STOP,   L"Stop",   L"stop"   }, { ID_BTG_FOLDER, L"Folder", L"folder" },
    { ID_KBL_OPEN,   L"Open",   L"open"   }, { ID_KBL_RESTART, L"Unfreeze", L"unfreeze" },
    { ID_KBL_UPDATE, L"Update", L"update" }, { ID_KBL_FOLDER, L"Folder",  L"folder" },
    { ID_HUD_UPDATE, L"Update Mod", L"updatemod" }, { ID_HUD_PICK, L"Mods Dir", L"modsdir" },
    { ID_HUD_FOLDER, L"Folder", L"folder" }, { ID_HUD_DISABLE, L"Disable", L"disable" },
    { ID_HUD_REMOVE, L"Remove", L"remove" },
    { ID_ORCA_UPDATE, L"Update", L"update" }, { ID_ORCA_MENU, L"Branch", L"branch" },
    { ID_ORCA_FOLDER, L"Folder", L"folder" }, { ID_ORCA_DISABLE, L"Disable", L"disable" },
    { ID_ORCA_REMOVE, L"Remove", L"remove" },
    { ID_INFILL_UPDATE, L"Update", L"update" }, { ID_INFILL_FOLDER, L"Folder", L"folder" },
    { ID_INFILL_REMOVE, L"Remove", L"remove" }, { ID_INFILL_DISABLE, L"Disable", L"disable" },
    { ID_DNS_OPEN,   L"Open",   L"open"   }, { ID_DNS_UPDATE, L"Update", L"update" },
    { ID_DNS_FOLDER, L"Folder", L"folder" },
    { ID_NOTES_OPEN, L"Open",   L"open"   }, { ID_NOTES_UPDATE, L"Update", L"update" },
    { ID_NOTES_FOLDER, L"Folder", L"folder" },
    { ID_AUDIOSWITCHER_OPEN, L"Open", L"open" },
    { ID_AUDIOSWITCHER_UPDATE, L"Update", L"update" },
    { ID_AUDIOSWITCHER_FOLDER, L"Folder", L"folder" },
};

/* Revert every pool-backed field of the built-in cards to its literal.
   Called just before the string pool is reset: a card whose catalog entry is
   absent (or whose "id" failed to parse) is never repopulated by
   FillBuiltInDef, so without this its pointers would survive the reset and
   then read whatever the NEXT rebuild happens to write at those offsets -
   which is how the Audio Switcher card came to show another app's blurb.
   Button order, ids, widths and `hidden` are deliberately left alone:
   RefreshProjectStatuses mutates them at runtime ("Open" -> "Install",
   "Disable" -> "Enable"), and only the two string pointers go stale. */
static void RestoreBuiltInCardText(void) {
    static const wchar_t *titles[8] = {
        L"Bluetooth Guard", L"Keyboard Lighting", L"Spatial HUD",
        L"Undahang", L"DNS Pilot", L"Agent Notes", L"Audio Switcher", L"Unlayered Infill" };
    static const wchar_t *blurbs[8] = {
        L"Phone audio protection", L"ROG Strix G16 RGB lighting",
        L"Minecraft 26.3 Fabric mod", L"Wave overhang paths",
        L"Fast DNS benchmark & auto-pilot", L"Quick notes straight to the agent",
        L"Per-app volume & audio output switching", L"Nonplanar internal infill" };
    /* Must mirror BuiltInSlot's known[] / FillBuiltInDef's exeMap ordering. */
    static const wchar_t *exeRepo[8] = {
        L"bluetooth-guard/BluetoothGuard.exe", L"keyboard-lighting/KeyboardLighting.exe",
        NULL, NULL,
        L"dns-pilot/DnsPilot.exe", L"agent-notes/AgentNotes.exe",
        L"audio-switcher/AudioSwitcher.exe", NULL };
    static const wchar_t *exeName[8] = {
        L"BluetoothGuard.exe", L"KeyboardLighting.exe", NULL, NULL,
        L"DnsPilot.exe", L"AgentNotes.exe",
        L"AudioSwitcher.exe", NULL };
    for (int i = 0; i < BUILTIN_CARD_COUNT; i++) {
        /* Plugin text/actions are already pinned literals; preserve live
           Install/Enable/hidden state while the background check continues. */
        if (i == 3 || i == 7) continue;
        ProjectCard *c = &g_cards[i];
        c->category = i == 2 ? HUB_GROUP_MINECRAFT : HUB_GROUP_GENERAL;
        c->title = titles[i];
        c->blurb = blurbs[i];
        c->exeRepo = exeRepo[i];
        c->exeName = exeName[i];
        c->shortcutName = titles[i];
        c->launchArgs = NULL;
        for (int b = 0; b < c->btnCount && b < 4; b++) {
            const wchar_t *label = NULL, *verb = NULL;
            for (size_t k = 0; k < sizeof(g_btnFallback) / sizeof(g_btnFallback[0]); k++) {
                if (g_btnFallback[k].id == c->btns[b].id) {
                    label = g_btnFallback[k].label;
                    verb = g_btnFallback[k].verb;
                    break;
                }
            }
            /* Unknown id (a verb the catalog invented): drop the button rather
               than leave DrawActionBtn holding a pointer into a wiped pool. */
            if (!label) { c->btnCount = b; break; }
            c->btns[b].label = label;
            c->btns[b].verb = verb;
        }
    }
    InitOrcaCard(0);
    InitOrcaCard(1);
}

static void RebuildExtraCards(const char *json) {
    /* Keep owned descriptors, not pool pointers. A partial response must not
       erase an already-known installed app; replay omitted entries by id after
       the new definitions. Required built-ins and Weather also exist offline. */
    char retained[MAX_APPS][1024];
    int retainedCount = 0;
    BOOL refreshed[MAX_APPS] = {0};
    for (int i = BUILTIN_CARD_COUNT; i < g_cardCount; i++) {
        if (g_cards[i].catalogDef[0])
            strcpy(retained[retainedCount++], g_cards[i].catalogDef);
    }
    /* Shrink first so a repaint mid-rebuild never reads half-built cards. */
    g_cardCount = BUILTIN_CARD_COUNT;
    for (int i = BUILTIN_CARD_COUNT; i < MAX_APPS; i++) {
        ReplaceCardIcon(&g_cards[i], NULL);
        ZeroMemory(&g_cards[i], sizeof(g_cards[i]));
    }
    /* Revert the built-ins to their literals BEFORE wiping the pool, so no
       pointer is left aimed at memory the next pass is about to reuse. */
    RestoreBuiltInCardText();
    g_strPoolUsed = 0;
    const char *apps = json ? strstr(json, "\"apps\"") : NULL;
    const char *p = apps ? strchr(apps, '[') : NULL;
    while (p && *p && *p != ']') {
        const char *b = strchr(p, '{');
        if (!b) break;
        int depth = 0;
        BOOL quoted = FALSE, escaped = FALSE;
        const char *q = b;
        for (; *q; q++) {
            if (quoted) {
                if (escaped) escaped = FALSE;
                else if (*q == '\\') escaped = TRUE;
                else if (*q == '"') quoted = FALSE;
                continue;
            }
            if (*q == '"') quoted = TRUE;
            else if (*q == '{') depth++;
            else if (*q == '}') { depth--; if (depth == 0) break; }
        }
        if (*q != '}') break;
        size_t len = (size_t)(q - b) + 1;
        if (len < 1024) {
            char obj[1024];
            memcpy(obj, b, len);
            obj[len] = '\0';
            char id[32], exe[128];
            if (JsonGet(obj, "id", id, sizeof(id))) {
                if (BuiltInSlot(id) >= 0) AddCatalogCard(obj, TRUE);
                else if (strlen(id) < 24 && JsonGet(obj, "exe", exe, sizeof(exe))) {
                    /* Merge by stable id before rebuilding. Known extras keep
                       their slots/priority, even if a response adds many new
                       apps while omitting a previously installed one. */
                    int at;
                    for (at = 0; at < retainedCount; at++) {
                        char oldId[32];
                        JsonGet(retained[at], "id", oldId, sizeof(oldId));
                        if (strcmp(id, oldId) == 0) break;
                    }
                    if (at == retainedCount && retainedCount < MAX_APPS - BUILTIN_CARD_COUNT)
                        retainedCount++;
                    if (at < retainedCount && !refreshed[at]) {
                        strcpy(retained[at], obj);
                        refreshed[at] = TRUE;
                    }
                }
            }
        }
        p = q + 1;
    }
    for (int i = 0; i < retainedCount; i++)
        AddCatalogCard(retained[i], TRUE);
    /* Weather Radar is a core suite app, not an optional remote-only card. A
       missing, truncated, or branch-specific catalog must not erase its Hub row. */
    EnsureWeatherRadarCatalogCard(TRUE);
}

