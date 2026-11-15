/* ---------------- places: config.ini + geocoding ----------------

   Places live in the app's own config.ini under [Places] - the same
   one-file-per-app rule the hub's Settings page follows for every other
   suite app, so there is no second schema to keep in sync. */

static void InitPaths(void) {
    wchar_t base[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base) != S_OK)
        wcscpy(base, L".");
    _snwprintf(g_cfgDir, MAX_PATH - 1, L"%s\\%s", base, APP_DIRNAME_W);
    g_cfgDir[MAX_PATH - 1] = L'\0';
    EnsureDirW(g_cfgDir);
    _snwprintf(g_cfgPath, MAX_PATH - 1, L"%s\\config.ini", g_cfgDir);
    g_cfgPath[MAX_PATH - 1] = L'\0';
    _snwprintf(g_cacheDir, MAX_PATH - 1, L"%s\\cache", g_cfgDir);
    g_cacheDir[MAX_PATH - 1] = L'\0';
    EnsureDirW(g_cacheDir);
    /* Radar frames carry their timestamp in the path, so an old file is just
       an old frame nobody will ask for again; basemap tiles change on OSM's
       own schedule and are worth keeping a month. */
    PruneCacheDir(g_cacheDir, L"rv", 2 * 24 * 3600);
    PruneCacheDir(g_cacheDir, L"osm", 30 * 24 * 3600);
}

static void LoadPlaces(void) {
    g_placeN = GetPrivateProfileIntW(L"Places", L"Count", 0, g_cfgPath);
    if (g_placeN > MAX_PLACES) g_placeN = MAX_PLACES;
    if (g_placeN < 0) g_placeN = 0;
    for (int i = 0; i < g_placeN; i++) {
        wchar_t key[24];
        _snwprintf(key, 23, L"Name%d", i + 1);
        GetPrivateProfileStringW(L"Places", key, L"", g_places[i].name, 95, g_cfgPath);
        _snwprintf(key, 23, L"Region%d", i + 1);
        GetPrivateProfileStringW(L"Places", key, L"", g_places[i].region, 95, g_cfgPath);
        wchar_t num[48];
        _snwprintf(key, 23, L"Lat%d", i + 1);
        GetPrivateProfileStringW(L"Places", key, L"nan", num, 47, g_cfgPath);
        g_places[i].lat = wcstod(num, NULL);
        _snwprintf(key, 23, L"Lon%d", i + 1);
        GetPrivateProfileStringW(L"Places", key, L"nan", num, 47, g_cfgPath);
        g_places[i].lon = wcstod(num, NULL);
    }
    g_placeSel = GetPrivateProfileIntW(L"Places", L"Selected", -1, g_cfgPath);
    if (g_placeSel < 0 || g_placeSel >= g_placeN) g_placeSel = g_placeN ? 0 : -1;
}

static void SavePlaces(void) {
    wchar_t num[64];
    _snwprintf(num, 63, L"%d", g_placeN);
    WritePrivateProfileStringW(L"Places", L"Count", num, g_cfgPath);
    for (int i = 0; i < MAX_PLACES; i++) {
        wchar_t key[24];
        _snwprintf(key, 23, L"Name%d", i + 1);
        WritePrivateProfileStringW(L"Places", key, i < g_placeN ? g_places[i].name : NULL, g_cfgPath);
        _snwprintf(key, 23, L"Region%d", i + 1);
        WritePrivateProfileStringW(L"Places", key, i < g_placeN ? g_places[i].region : NULL, g_cfgPath);
        _snwprintf(key, 23, L"Lat%d", i + 1);
        if (i < g_placeN) _snwprintf(num, 63, L"%.5f", g_places[i].lat); else num[0] = L'\0';
        WritePrivateProfileStringW(L"Places", key, i < g_placeN ? num : NULL, g_cfgPath);
        _snwprintf(key, 23, L"Lon%d", i + 1);
        if (i < g_placeN) _snwprintf(num, 63, L"%.5f", g_places[i].lon); else num[0] = L'\0';
        WritePrivateProfileStringW(L"Places", key, i < g_placeN ? num : NULL, g_cfgPath);
    }
    _snwprintf(num, 63, L"%d", g_placeSel);
    WritePrivateProfileStringW(L"Places", L"Selected", num, g_cfgPath);
}

/* Remembering a place is the only persistent write: name + coordinates, so a
   re-open shows the last place's weather before anything is typed. */
static void RememberPlace(const Place *p) {
    for (int i = 0; i < g_placeN; i++) {
        if (fabs(g_places[i].lat - p->lat) < 1e-6 && fabs(g_places[i].lon - p->lon) < 1e-6) {
            wcsncpy(g_places[i].name, p->name, 95);
            wcsncpy(g_places[i].region, p->region, 95);
            g_placeSel = i;
            SavePlaces();
            return;
        }
    }
    if (g_placeN >= MAX_PLACES) {
        memmove(&g_places[0], &g_places[1], sizeof(Place) * (size_t)(MAX_PLACES - 1));
        g_placeN = MAX_PLACES - 1;
        if (g_placeSel > 0) g_placeSel--;
    }
    g_places[g_placeN] = *p;
    g_placeSel = g_placeN;
    g_placeN++;
    SavePlaces();
}

static void DropSelectedPlace(void) {
    if (g_placeSel < 0 || g_placeSel >= g_placeN) return;
    memmove(&g_places[g_placeSel], &g_places[g_placeSel + 1],
            sizeof(Place) * (size_t)(g_placeN - g_placeSel - 1));
    g_placeN--;
    if (g_placeSel >= g_placeN) g_placeSel = g_placeN - 1;
    SavePlaces();
}

/* Geocoding: Open-Meteo's own search endpoint, no key, no third-party
   middleman. Results are shown as rows the owner clicks - picking a place
   must be a choice, never a guess. */
static int SearchTicketIsCurrent(DWORD ticket) {
    int current;
    EnterCriticalSection(&g_lock);
    current = (ticket == g_searchTicket);
    LeaveCriticalSection(&g_lock);
    return current;
}

/* Geocoding is a worker job. It builds a local list first, then publishes one
   complete answer to the UI; an older HTTP response cannot overwrite a newer
   query while the owner is typing. */
static void GeocodeSearch(const wchar_t *query, DWORD ticket) {
    Place matches[MAX_SEARCH];
    int matchN = 0;
    char utf[256], enc[700];
    WideToUtf8(query, utf, 256);
    UrlEncodeUtf8(utf, enc, 700);
    wchar_t url[1024];
    _snwprintf(url, 1023,
               L"https://geocoding-api.open-meteo.com/v1/search?name=%hs&count=%d&format=json&language=en",
               enc, MAX_SEARCH);
    url[1023] = L'\0';
    HttpBuf b;
    if (!HttpGetBuf(url, &b)) {
        if (SearchTicketIsCurrent(ticket))
            PostMessageW(g_hwnd, WM_APP_STATUS, 2, (LPARAM)(intptr_t)_wcsdup(
                L"Could not reach the geocoding service - check the connection."));
        return;
    }
    JNode *root = JsonParse(b.data);
    JNode *res = root ? JGet(root, "results") : NULL;
    if (res && res->type == J_ARR) {
        for (JNode *c = res->child; c && matchN < MAX_SEARCH; c = c->next) {
            JNode *lat = JGet(c, "latitude"), *lon = JGet(c, "longitude");
            JNode *nm = JGet(c, "name");
            if (!lat || !lon || !nm || !nm->str) continue;
            Place *p = &matches[matchN++];
            ZeroMemory(p, sizeof(*p));
            Utf8ToWide(nm->str, p->name, 96);
            wchar_t a1[96] = L"", ct[96] = L"";
            JNode *j = JGet(c, "admin1");
            if (j && j->str) Utf8ToWide(j->str, a1, 96);
            j = JGet(c, "country");
            if (j && j->str) Utf8ToWide(j->str, ct, 96);
            if (a1[0] && ct[0]) _snwprintf(p->region, 95, L"%s, %s", a1, ct);
            else wcsncpy(p->region, a1[0] ? a1 : ct, 95);
            p->region[95] = L'\0';
            p->lat = lat->num;
            p->lon = lon->num;
        }
    }
    HttpBufFree(&b);

    EnterCriticalSection(&g_lock);
    int current = (ticket == g_searchTicket);
    if (current) {
        memcpy(g_searchStage, matches, sizeof(Place) * (size_t)matchN);
        g_searchStageN = matchN;
        g_searchStageTicket = ticket;
    }
    LeaveCriticalSection(&g_lock);
    if (!current) return;

    if (matchN == 0)
        PostMessageW(g_hwnd, WM_APP_STATUS, 2, (LPARAM)(intptr_t)_wcsdup(
            L"No place by that name - try the city, or the nearest bigger town."));
    PostMessageW(g_hwnd, WM_APP_DATA, JOB_GEOCODE, 0);
}
