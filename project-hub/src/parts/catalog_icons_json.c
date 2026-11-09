/* ---------------- dynamic app catalog (suite-apps.json) ----------------
   New suite apps are described in suite-apps.json on the active branch.
   The hub fetches it during its quiet check and builds cards for any entry
   it does not already know - so adding an app means dropping its files and
   one JSON entry, never a hub update. */
static char *g_catalogJson = NULL;

static void EnqueueTask(int task);

/* Load a .ico at the exact card-icon draw size. The assets ship 256px
   sources; LR_DEFAULTSIZE would pin 32px and blur when the card scales
   it up to S(44, dpi). */
static HICON LoadIconFilePx(const wchar_t *path) {
    int dpi = GetWinDpi(g_hwnd);
    int px = S(44, dpi);
    return (HICON)LoadImageW(NULL, path, IMAGE_ICON, px, px, LR_LOADFROMFILE);
}

static const wchar_t *PoolAdd(const char *utf8);

/* Cached download of a catalog icon. The cache file name carries a short
   hash of the catalog's icon path, so when an app's icon changes in the
   catalog the hub fetches the new art exactly once instead of showing the
   old cached .ico forever. On a failed download the card falls back to its
   embedded resource / letter tile. */
static void LoadCardIcon(ProjectCard *c, const char *icon) {
    wchar_t iconsDir[MAX_PATH];
    unsigned long key = 2166136261UL; /* FNV-1a of the catalog icon path */
    for (const unsigned char *p = (const unsigned char *)icon; *p; p++) {
        key ^= (unsigned long)*p;
        key *= 16777619UL;
    }
    _snwprintf(iconsDir, MAX_PATH - 1, L"%s\\icons", g_localApp);
    CreateDirectoryW(iconsDir, NULL);
    _snwprintf(c->iconFile, MAX_PATH - 1, L"%s\\%s-%08lx.ico", iconsDir, c->id, key);
    if (!FileExistsW(c->iconFile)) {
        wchar_t branch[64];
        EnsureActiveBranch(branch, 64);
        if (!DownloadRawRepoFile(branch, PoolAdd(icon), c->iconFile))
            c->iconFile[0] = L'\0';
    }
    if (c->iconFile[0])
        c->hIcon = LoadIconFilePx(c->iconFile);
}

static wchar_t g_strPool[12288];
static int     g_strPoolUsed = 0;

static const wchar_t *PoolAdd(const char *utf8) {
    wchar_t *w = &g_strPool[g_strPoolUsed];
    int cap = (int)(sizeof(g_strPool) / sizeof(wchar_t)) - g_strPoolUsed;
    if (cap < 8) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8 ? utf8 : "", -1, w, cap);
    if (n <= 0) return L"";
    g_strPoolUsed += n;
    return w;
}

/* Read one string value out of a flat JSON object slice.
   `obj` is a single `{...}` slice (RebuildExtraCards brace-matches it), so
   only top-level `"key": "value"` pairs have to be understood.

   Two things a bare strstr() gets wrong, both handled here:
     - the match has to be a KEY. `"id"` also matches inside a value such as
       `"blurb": "pick an id"`, and the old code then read whatever followed
       that text as the value. A key sits after `{` or `,` and before `:`.
     - escapes have to be decoded. The old code dropped the backslash and
       copied the next byte verbatim, which happens to work for `\"` and `\\`
       but turns `\n` into the letter n and `\u00e9` into `u00e9`. Every
       caller feeds the result to MultiByteToWideChar(CP_UTF8, ...), so
       `\uXXXX` is re-encoded as UTF-8 here, surrogate pairs included.

   Returns 1 and fills `out` when the key holds a non-empty string value,
   otherwise 0 with out[0] = '\0'. */
static int JsonGet(const char *obj, const char *key, char *out, int cap) {
    char pat[64];
    int patLen;
    if (!obj || !key || !out || cap <= 1) {
        if (out && cap > 0) out[0] = '\0';
        return 0;
    }
    out[0] = '\0';
    _snprintf(pat, sizeof(pat) - 1, "\"%s\"", key);
    pat[sizeof(pat) - 1] = '\0';
    patLen = (int)strlen(pat);
    if (patLen < 3 || patLen >= (int)sizeof(pat) - 1) return 0;

    const char *p = obj;
    for (;;) {
        p = strstr(p, pat);
        if (!p) return 0;
        const char *next = p + patLen;

        /* A key is preceded by `{` or `,` (whitespace allowed) ... */
        int isKey = 0;
        if (p > obj) {
            const char *b = p - 1;
            while (b > obj && (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n')) b--;
            isKey = (*b == '{' || *b == ',');
        }

        /* ... and followed by whitespace then `:`. Anything else is this same
           text occurring inside a value: keep scanning. */
        const char *q = next;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (!isKey || *q != ':') { p = next; continue; }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q != '"') return 0;   /* key present, but its value is not a string */
        q++;

        int i = 0;
        while (*q && *q != '"') {
            unsigned long cp;
            if (*q == '\\') {
                q++;
                switch (*q) {
                    case 'n':  cp = 0x0AUL; break;
                    case 't':  cp = 0x09UL; break;
                    case 'r':  cp = 0x0DUL; break;
                    case 'b':  cp = 0x08UL; break;
                    case 'f':  cp = 0x0CUL; break;
                    case '"':  cp = '"';    break;
                    case '\\': cp = '\\';   break;
                    case '/':  cp = '/';    break;
                    case 'u': {
                        unsigned long v = 0;
                        int h;
                        for (h = 0; h < 4; h++) {
                            char c = *++q;
                            if (!c) break;
                            v <<= 4;
                            if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
                            else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
                            else { v = 0xFFFDUL; break; }
                        }
                        if (h < 4 && v != 0xFFFDUL) v = 0xFFFDUL;
                        /* Combine a UTF-16 surrogate pair; q sits on the last
                           hex digit, so the low surrogate starts at q[1]. */
                        if (v >= 0xD800UL && v <= 0xDBFFUL &&
                            q[1] == '\\' && q[2] == 'u') {
                            unsigned long lo = 0;
                            int k;
                            for (k = 0; k < 4; k++) {
                                char c = q[3 + k];
                                if (!c) break;
                                lo <<= 4;
                                if (c >= '0' && c <= '9') lo |= (unsigned long)(c - '0');
                                else if (c >= 'a' && c <= 'f') lo |= (unsigned long)(c - 'a' + 10);
                                else if (c >= 'A' && c <= 'F') lo |= (unsigned long)(c - 'A' + 10);
                                else break;
                            }
                            if (k == 4 && lo >= 0xDC00UL && lo <= 0xDFFFUL) {
                                v = 0x10000UL + ((v - 0xD800UL) << 10) + (lo - 0xDC00UL);
                                q += 6;
                            } else {
                                v = 0xFFFDUL;      /* unpaired high surrogate */
                            }
                        } else if (v >= 0xD800UL && v <= 0xDFFFUL) {
                            v = 0xFFFDUL;          /* unpaired low surrogate */
                        }
                        cp = v;
                        break;
                    }
                    case '\0': return 0;                            /* truncated */
                    default:   cp = (unsigned char)*q; break;       /* keep the char */
                }
                if (*q) q++;
            } else {
                cp = (unsigned char)*q;
                q++;
            }

            /* Encode the code point as UTF-8: every caller converts the result
               with CP_UTF8, so this is the encoding it expects. */
            {
                char tmp[4];
                int tn = 0, k;
                if (cp < 0x80UL) {
                    tmp[tn++] = (char)cp;
                } else if (cp < 0x800UL) {
                    tmp[tn++] = (char)(0xC0UL | (cp >> 6));
                    tmp[tn++] = (char)(0x80UL | (cp & 0x3FUL));
                } else if (cp < 0x10000UL) {
                    tmp[tn++] = (char)(0xE0UL | (cp >> 12));
                    tmp[tn++] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
                    tmp[tn++] = (char)(0x80UL | (cp & 0x3FUL));
                } else {
                    tmp[tn++] = (char)(0xF0UL | (cp >> 18));
                    tmp[tn++] = (char)(0x80UL | ((cp >> 12) & 0x3FUL));
                    tmp[tn++] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
                    tmp[tn++] = (char)(0x80UL | (cp & 0x3FUL));
                }
                if (i + tn >= cap) break;        /* leave room for the NUL */
                for (k = 0; k < tn; k++) out[i++] = tmp[k];
            }
        }
        out[i] = '\0';
        return i > 0;
    }
}

static int BuiltInSlot(const char *id) {
    static const char *known[7] = { "btguard", "kbl", "hud", "orca", "dnspilot", "agentnotes", "audioswitcher" };
    for (int i = 0; i < 7; i++) if (strcmp(id, known[i]) == 0) return i;
    return -1;
}

static int BuiltInBtnId(int slot, const char *verb) {
    /* verb -> the control id the existing WM_COMMAND switch understands */
    static const struct { const char *verb; int id; } map[27] = {
        { "open",      ID_BTG_OPEN },     { "update",    ID_BTG_UPDATE },
        { "stop",      ID_BTG_STOP },     { "folder",    ID_BTG_FOLDER },
        { "open",      ID_KBL_OPEN },     { "unfreeze",  ID_KBL_RESTART },
        { "update",    ID_KBL_UPDATE },   { "folder",    ID_KBL_FOLDER },
        { "updatemod", ID_HUD_UPDATE },   { "modsdir",   ID_HUD_PICK },
        { "folder",    ID_HUD_FOLDER },   { "disable",   ID_HUD_DISABLE },
        { "remove",    ID_HUD_REMOVE },
        { "update",    ID_ORCA_UPDATE },  { "branch",    ID_ORCA_MENU },
        { "folder",    ID_ORCA_FOLDER },  { "disable",   ID_ORCA_DISABLE },
        { "remove",    ID_ORCA_REMOVE },
        { "open",      ID_DNS_OPEN },     { "update",    ID_DNS_UPDATE },
        { "folder",    ID_DNS_FOLDER },
        { "open",      ID_NOTES_OPEN },   { "update",    ID_NOTES_UPDATE },
        { "folder",    ID_NOTES_FOLDER },
        { "open",      ID_AUDIOSWITCHER_OPEN },  { "update",    ID_AUDIOSWITCHER_UPDATE },
        { "folder",    ID_AUDIOSWITCHER_FOLDER }
    };
    static const int off[10] = { 0, 4, 8, 13, 18, 21, 24, 27, 27, 27 };
    if (slot < 0 || slot > 6) return 0;
    for (int i = off[slot]; i < off[slot + 1]; i++)
        if (strcmp(map[i].verb, verb) == 0) return map[i].id;
    return 0;
}

static void FillBuiltInDef(int slot, const char *id, const char *title,
                           const char *blurb, const char *icon, const char *buttons) {
    if (slot < 0 || slot >= g_cardCount) return;
    ProjectCard *c = &g_cards[slot];
    MultiByteToWideChar(CP_UTF8, 0, id, -1, c->id, 23);
    if (title[0]) c->title = PoolAdd(title);
    if (blurb[0]) c->blurb = PoolAdd(blurb);

    /* Set exeRepo/exeName so install/update/open work for built-in cards.
       These must match the suite-apps.json entries exactly. */
    static const struct { const char *repo; const char *name; } exeMap[7] = {
        { "bluetooth-guard/BluetoothGuard.exe", "BluetoothGuard.exe" },
        { "keyboard-lighting/KeyboardLighting.exe", "KeyboardLighting.exe" },
        { "", "" },                                          /* HUD - no exe */
        { "", "" },                                          /* Orca - no exe */
        { "dns-pilot/DnsPilot.exe", "DnsPilot.exe" },
        { "agent-notes/AgentNotes.exe", "AgentNotes.exe" },
        { "audio-switcher/AudioSwitcher.exe", "AudioSwitcher.exe" },
    };
    if (slot >= 0 && slot < 7 && exeMap[slot].repo[0]) {
        c->exeRepo = PoolAdd(exeMap[slot].repo);
        c->exeName = PoolAdd(exeMap[slot].name);
    }
    if (buttons[0]) {
        char list[256];
        strncpy(list, buttons, sizeof(list) - 1);
        list[sizeof(list) - 1] = '\0';
        int n = 0;
        char *bp = list;
        while (*bp && n < 4) {
            char verb[24] = "", label[32] = "";
            int wpx = 74, k = 0;
            while (*bp && *bp != ':' && *bp != ',' && k < 23) verb[k++] = *bp++;
            verb[k] = '\0';
            if (*bp == ':') bp++;
            k = 0;
            while (*bp && *bp != ':' && *bp != ',' && k < 31) label[k++] = *bp++;
            label[k] = '\0';
            if (*bp == ':') {
                bp++;
                wpx = 0;
                while (*bp >= '0' && *bp <= '9') wpx = wpx * 10 + (*bp++ - '0');
                if (wpx < 40 || wpx > 160) wpx = 74;
            }
            while (*bp && *bp != ',') bp++;
            if (*bp == ',') bp++;
            int btnId = verb[0] ? BuiltInBtnId(slot, verb) : 0;
            if (!btnId) continue;
            c->btns[n].id = btnId;
            c->btns[n].label = PoolAdd(label[0] ? label : verb);
            c->btns[n].style = BTN_SECONDARY;
            c->btns[n].widthPx = wpx;
            c->btns[n].verb = PoolAdd(verb);
            ZeroMemory(&c->btns[n].rc, sizeof(RECT));
            n++;
        }
        if (n > 0) c->btnCount = n;
    }
    /* Disable/Remove are hub features of the mod card: always present, even
       if a stale catalog copy arrives without those verbs (the CDN in front
       of raw.githubusercontent can serve older files for a few minutes).
       Make room by dropping Folder first - the least essential of the four. */
    if (slot == 2) {
        static const struct { int id; const wchar_t *label; const wchar_t *verb; int w; } must[2] = {
            { ID_HUD_DISABLE, L"Disable", L"disable", 78 },
            { ID_HUD_REMOVE,  L"Remove",  L"remove",  74 }
        };
        for (int m = 0; m < 2; m++) {
            int have = 0, drop = -1;
            for (int b = 0; b < c->btnCount; b++) {
                if (c->btns[b].id == must[m].id) have = 1;
                if (c->btns[b].id == ID_HUD_FOLDER) drop = b;
            }
            if (have) continue;
            if (drop >= 0) {
                memmove(&c->btns[drop], &c->btns[drop + 1],
                        (size_t)(c->btnCount - drop - 1) * sizeof(ActionBtn));
                c->btnCount--;
            }
            if (c->btnCount >= 4) continue;
            ActionBtn *btn = &c->btns[c->btnCount];
            ZeroMemory(btn, sizeof(*btn));
            btn->id = must[m].id;
            btn->label = must[m].label;
            btn->style = BTN_SECONDARY;
            btn->widthPx = must[m].w;
            btn->verb = must[m].verb;
            c->btnCount++;
        }
    }
    if (icon[0])
        LoadCardIcon(c, icon);

    /* Set appDir for built-in cards so Folder/Open/Install all agree. The
       suite folder is the install target AND the folder the status sweep
       measures, so a card can never report a copy it is not updating. */
    if (slot >= 0 && slot < 7) {
        static const wchar_t *dirs[7] = {
            L"BluetoothGuard", L"KeyboardLighting", L"SpatialHudUpdater",
            L"OrcaPlugins", L"DnsPilot", L"AgentNotes",
            L"AudioSwitcher"
        };
        if (dirs[slot][0]) SuiteAppPath(c->appDir, MAX_PATH, dirs[slot]);
    }
}

static void AddCatalogCard(const char *obj) {
    char id[32], title[64], blurb[128], exe[128], dir[64], icon[128];
    char shortcut[64], buttons[256], args[64];
    if (!JsonGet(obj, "id", id, sizeof(id))) return;
    if (!JsonGet(obj, "exe", exe, sizeof(exe))) exe[0] = '\0';
    JsonGet(obj, "title", title, sizeof(title));
    JsonGet(obj, "blurb", blurb, sizeof(blurb));
    JsonGet(obj, "dir", dir, sizeof(dir));
    JsonGet(obj, "icon", icon, sizeof(icon));
    JsonGet(obj, "shortcut", shortcut, sizeof(shortcut));
    JsonGet(obj, "buttons", buttons, sizeof(buttons));
    JsonGet(obj, "launchArgs", args, sizeof(args));
    if (BuiltInSlot(id) >= 0) {
        /* built-in card: refresh its definition from the catalog */
        FillBuiltInDef(BuiltInSlot(id), id, title, blurb, icon, buttons);
        return;
    }
    if (!exe[0]) return;
    if (g_cardCount >= MAX_APPS) return;

    ProjectCard *c = &g_cards[g_cardCount];
    ZeroMemory(c, sizeof(*c));
    MultiByteToWideChar(CP_UTF8, 0, id, -1, c->id, 23);
    c->title = PoolAdd(title[0] ? title : id);
    c->blurb = PoolAdd(blurb[0] ? blurb : "Suite app");
    c->exeRepo = PoolAdd(exe);
    {
        const char *bs = strrchr(exe, '/');
        c->exeName = PoolAdd(bs ? bs + 1 : exe);
    }
    c->shortcutName = PoolAdd(shortcut[0] ? shortcut : (title[0] ? title : id));
    c->launchArgs = args[0] ? PoolAdd(args) : NULL;

    /* Install dir: the suite folder, exactly like the built-ins. A stray copy
       that already exists (next to the hub, or in a repo checkout) is
       registered so the same update overwrites it instead of leaving it at an
       older build. */
    wchar_t cand[MAX_PATH], parent[MAX_PATH], wtmp[64];
    wcsncpy(parent, g_exeDir, MAX_PATH - 1);
    parent[MAX_PATH - 1] = L'\0';
    {
        wchar_t *slash = wcsrchr(parent, L'\\');
        if (slash) *slash = L'\0';
    }
    MultiByteToWideChar(CP_UTF8, 0, dir[0] ? dir : (title[0] ? title : id), -1, wtmp, 63);
    SuiteAppPath(c->appDir, MAX_PATH, wtmp);
    _snwprintf(cand, MAX_PATH - 1, L"%s\\%s", g_exeDir, c->exeName);
    if (FileExistsW(cand)) AddMirrorDir(MIRROR_GENERIC, g_exeDir);
    const char *slash = strrchr(exe, '/');
    if (slash) {
        char folder[64];
        size_t fl = (size_t)(slash - exe);
        if (fl >= sizeof(folder)) fl = sizeof(folder) - 1;
        memcpy(folder, exe, fl);
        folder[fl] = '\0';
        MultiByteToWideChar(CP_UTF8, 0, folder, -1, wtmp, 63);
        _snwprintf(cand, MAX_PATH - 1, L"%s\\%s", parent, wtmp);
        if (FileExistsW(cand)) AddMirrorDir(MIRROR_GENERIC, cand);
    }

    /* buttons: "verb:Label:width,verb:Label:width" */
    if (!buttons[0])
        strncpy(buttons, "install:Install:68,update:Update:74,folder:Folder:68",
                sizeof(buttons) - 1);
    c->btnCount = 0;
    char *bp = buttons;
    while (*bp && c->btnCount < 4) {
        char verb[24] = "", label[32] = "";
        int wpx = 74, n = 0;
        while (*bp && *bp != ':' && *bp != ',' && n < 23) verb[n++] = *bp++;
        verb[n] = '\0';
        if (*bp == ':') bp++;
        n = 0;
        while (*bp && *bp != ':' && *bp != ',' && n < 31) label[n++] = *bp++;
        label[n] = '\0';
        if (*bp == ':') {
            bp++;
            wpx = 0;
            while (*bp >= '0' && *bp <= '9') wpx = wpx * 10 + (*bp++ - '0');
            if (wpx < 40 || wpx > 160) wpx = 74;
        }
        while (*bp && *bp != ',') bp++;
        if (*bp == ',') bp++;
        if (!verb[0]) continue;
        ActionBtn *btn = &c->btns[c->btnCount];
        btn->id = ID_DYN_BASE + g_cardCount * 10 + c->btnCount;
        btn->label = PoolAdd(label[0] ? label : verb);
        btn->style = BTN_SECONDARY;
        btn->widthPx = wpx;
        btn->verb = PoolAdd(verb);
        c->btnCount++;
    }

    /* icon: cached download of the catalog icon, letter tile until it lands */
    if (icon[0])
        LoadCardIcon(c, icon);
    g_cardCount++;
}

