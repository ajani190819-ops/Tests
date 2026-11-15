/* ---------------- radar: RainViewer frames + WIC tiles ----------------

   Frames come from RainViewer's public catalog: a list of past (observed)
   and nowcast (modelled) timestamps, each addressable as ordinary slippy-map
   tiles. Tiles are transparent PNGs, so they composite over a basemap; the
   basemap is OpenStreetMap's, fetched once per place/zoom and cached a month
   because it barely changes, while radar frames cache two days (their
   timestamp is in the path, so a cache hit is exactly the same frame).

   Decoding goes through WIC, reached with CoCreateInstance so that
   windowscodecs.dll is loaded by COM at run time and never appears in this
   executable's import table - the suite's PE contract allows stock system
   DLLs only, and a dynamic COM client keeps it that way. */

static IWICImagingFactory *g_wicFactory = NULL;

static int WicEnsure(void) {
    if (g_wicFactory) return 1;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void **)&g_wicFactory);
    return SUCCEEDED(hr) && g_wicFactory;
}

/* Decode a PNG into a 256x256 BGRA tile. Returns 1 when at least one pixel
   carries alpha - that is how an all-transparent tile is told apart from a
   rainy one, which is how the zoom fallback knows the tile host served a
   placeholder instead of data. */
static int WicDecodePng256(const void *data, size_t len, uint32_t *dst) {
    for (int i = 0; i < TILE_PX * TILE_PX; i++) dst[i] = 0;
    if (!WicEnsure()) return 0;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hg) return 0;
    void *mem = GlobalLock(hg);
    if (!mem) { GlobalFree(hg); return 0; }
    memcpy(mem, data, len);
    GlobalUnlock(hg);
    IStream *st = NULL;
    if (FAILED(CreateStreamOnHGlobal(hg, TRUE, &st))) { GlobalFree(hg); return 0; }

    int any = 0;
    IWICBitmapDecoder *dec = NULL;
    if (SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(
            g_wicFactory, st, NULL, WICDecodeMetadataCacheOnDemand, &dec)) && dec) {
        IWICBitmapFrameDecode *frame = NULL;
        if (SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &frame)) && frame) {
            IWICFormatConverter *conv = NULL;
            if (SUCCEEDED(IWICImagingFactory_CreateFormatConverter(g_wicFactory, &conv)) && conv) {
                if (SUCCEEDED(IWICFormatConverter_Initialize(
                        conv, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppBGRA,
                        WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
                    UINT w = 0, h = 0;
                    IWICFormatConverter_GetSize(conv, &w, &h);
                    if (w == TILE_PX && h == TILE_PX &&
                        SUCCEEDED(IWICFormatConverter_CopyPixels(
                            conv, NULL, TILE_PX * 4, TILE_PX * TILE_PX * 4, (BYTE *)dst))) {
                        for (int i = 0; i < TILE_PX * TILE_PX; i++)
                            if (dst[i] & 0xFF000000u) { any = 1; break; }
                    }
                }
                IWICFormatConverter_Release(conv);
            }
            IWICBitmapFrameDecode_Release(frame);
        }
        IWICBitmapDecoder_Release(dec);
    }
    IStream_Release(st);      /* frees the HGLOBAL too (fDeleteOnRelease) */
    return any;
}

/* ---------------- slippy-map tile maths ---------------- */

static int Lon2Tile(double lon, int z) {
    int n = 1 << z;
    int x = (int)floor((lon + 180.0) / 360.0 * (double)n);
    if (x < 0) x = 0;
    if (x > n - 1) x = n - 1;
    return x;
}

static int Lat2Tile(double lat, int z) {
    int n = 1 << z;
    double r = lat * 3.14159265358979323846 / 180.0;
    int y = (int)floor((1.0 - log(tan(r) + 1.0 / cos(r)) /
                        3.14159265358979323846) / 2.0 * (double)n);
    if (y < 0) y = 0;
    if (y > n - 1) y = n - 1;
    return y;
}

/* The grid is placed so the queried point sits in the CENTRE tile, which is
   where the crosshair lands. Clamping at the map edges shifts the crosshair
   off-centre; the paint code derives its position from the same maths, so
   the marker stays on the place either way. */
static void GridFor(int z, double lat, double lon, int *tx, int *ty) {
    int n = 1 << z;
    int x = Lon2Tile(lon, z) - (GRID_TILES / 2);
    int y = Lat2Tile(lat, z) - (GRID_TILES / 2);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > n - GRID_TILES) x = n - GRID_TILES;
    if (y > n - GRID_TILES) y = n - GRID_TILES;
    *tx = x;
    *ty = y;
}

/* Where the place sits inside the stitched grid, in grid pixels. Same
   formula as the tile maths, so the crosshair is exact, clamped grid or not. */
static void PlacePixelInGrid(int z, double lat, double lon, int tx, int ty,
                             double *px, double *py) {
    int n = 1 << z;
    double fx = (lon + 180.0) / 360.0 * (double)n;
    double r = lat * 3.14159265358979323846 / 180.0;
    double fy = (1.0 - log(tan(r) + 1.0 / cos(r)) / 3.14159265358979323846) / 2.0 * (double)n;
    *px = (fx - (double)tx) * (double)TILE_PX;
    *py = (fy - (double)ty) * (double)TILE_PX;
}

/* ---------------- frame catalog ---------------- */

static void RadarCatalogFetch(void) {
    HttpBuf b;
    if (!HttpGetBuf(L"https://api.rainviewer.com/public/weather-maps.json", &b)) {
        PostMessageW(g_hwnd, WM_APP_STATUS, 2, (LPARAM)(intptr_t)_wcsdup(
            L"The radar service did not answer - the forecast below still works."));
        return;
    }
    JNode *root = JsonParse(b.data);
    JNode *host = root ? JGet(root, "host") : NULL;
    JNode *radar = root ? JGet(root, "radar") : NULL;
    JNode *past = radar ? JGet(radar, "past") : NULL;
    JNode *nowcast = radar ? JGet(radar, "nowcast") : NULL;
    EnterCriticalSection(&g_lock);
    g_frameN = 0;
    g_radarHost[0] = L'\0';
    if (host && host->str) Utf8ToWide(host->str, g_radarHost, 160);
    if (past) {
        int total = JLen(past);
        int start = total > 12 ? total - 12 : 0;
        for (int i = start; i < total && g_frameN < MAX_FRAMES; i++) {
            JNode *f = JAt(past, i);
            JNode *t = f ? JGet(f, "time") : NULL;
            JNode *p = f ? JGet(f, "path") : NULL;
            if (!t || !p || !p->str) continue;
            g_frames[g_frameN].time = (long long)t->num;
            strncpy(g_frames[g_frameN].path, p->str, 95);
            g_frames[g_frameN].path[95] = '\0';
            g_frames[g_frameN].kind = 0;
            g_frameN++;
        }
    }
    if (nowcast) {
        int total = JLen(nowcast);
        for (int i = 0; i < total && g_frameN < MAX_FRAMES; i++) {
            JNode *f = JAt(nowcast, i);
            JNode *t = f ? JGet(f, "time") : NULL;
            JNode *p = f ? JGet(f, "path") : NULL;
            if (!t || !p || !p->str) continue;
            g_frames[g_frameN].time = (long long)t->num;
            strncpy(g_frames[g_frameN].path, p->str, 95);
            g_frames[g_frameN].path[95] = '\0';
            g_frames[g_frameN].kind = 1;
            g_frameN++;
        }
    }
    /* land on the newest OBSERVED frame: that is "now", not a model guess */
    g_frameSel = 0;
    for (int i = 0; i < g_frameN; i++)
        if (g_frames[i].kind == 0) g_frameSel = i;
    LeaveCriticalSection(&g_lock);
    HttpBufFree(&b);
    if (g_frameN > 0)
        PostMessageW(g_hwnd, WM_APP_DATA, JOB_RADAR_CATALOG, 0);
    else
        PostMessageW(g_hwnd, WM_APP_STATUS, 2, (LPARAM)(intptr_t)_wcsdup(
            L"No radar frames are published right now - the numbers above still work."));
}

/* ---------------- tile fetching into a 768x768 grid ---------------- */

/* Fetch (or read from cache) the nine tiles of one layer and lay them into
   `dst`. `framePath` NULL means the OpenStreetMap basemap; anything else is
   a RainViewer frame. Returns 1 if any tile carried radar alpha. */
static int FetchTileGrid(const wchar_t *prefix, int z, int tx, int ty,
                         const char *framePath, uint32_t *dst, long long cacheSec) {
    int any = 0;
    for (int gy = 0; gy < GRID_TILES; gy++) {
        for (int gx = 0; gx < GRID_TILES; gx++) {
            wchar_t url[512];
            if (framePath)
                _snwprintf(url, 511, L"%s%hs/256/%d/%d/%d/2/1_1.png",
                           g_radarHost, framePath, z, tx + gx, ty + gy);
            else
                _snwprintf(url, 511, L"https://tile.openstreetmap.org/%d/%d/%d.png",
                           z, tx + gx, ty + gy);
            url[511] = L'\0';
            uint32_t *tile = dst + (size_t)(gy * TILE_PX) * GRID_PX + (size_t)gx * TILE_PX;
            wchar_t cpath[MAX_PATH];
            CacheNameFor(prefix, url, cpath, MAX_PATH);

            HttpBuf hb;
            hb.data = NULL;
            hb.len = 0;
            if (CacheAgeSecOk(cpath, cacheSec) && ReadWholeFileW(cpath, &hb.data, &hb.len)) {
                /* served from disk: same bytes, no second request */
            } else if (!HttpGetBuf(url, &hb)) {
                for (int i = 0; i < TILE_PX * TILE_PX; i++) tile[i] = 0;
                continue;
            } else {
                WriteWholeFileW(cpath, hb.data, hb.len);
            }
            if (WicDecodePng256(hb.data, hb.len, tile)) any = 1;
            HttpBufFree(&hb);
        }
    }
    return any;
}

/* The basemap is held per (zoom,tx,ty): it does not change between frames,
   so stepping the animation only re-reads the radar layer from cache. */
static int BaseGridEnsure(int z, int tx, int ty) {
    if (g_base && g_baseKey[0] == z && g_baseKey[1] == tx && g_baseKey[2] == ty) return 1;
    if (!g_base) g_base = (uint32_t *)malloc((size_t)GRID_PX * GRID_PX * 4);
    if (!g_base) return 0;
    FetchTileGrid(L"osm", z, tx, ty, NULL, g_base, 30 * 24 * 3600);
    g_baseKey[0] = z;
    g_baseKey[1] = tx;
    g_baseKey[2] = ty;
    return 1;
}

/* Composite one radar frame over the basemap into g_viewStage, then hand the
   buffer to the UI thread. Tries zoom 8 (about 470 km across) and, if every
   tile of the frame comes back transparent - the tile host answers 200 with
   a placeholder beyond its free zoom - retries the whole grid at zoom 7 and
   says so on the pane instead of silently showing a smaller map than asked. */
static void RadarViewBuild(const Place *p, int frameIdx) {
    if (!g_radarHost[0] || frameIdx < 0 || frameIdx >= g_frameN) return;
    if (!g_viewStage) g_viewStage = (uint32_t *)malloc((size_t)GRID_PX * GRID_PX * 4);
    if (!g_viewStage) return;
    uint32_t *radar = (uint32_t *)malloc((size_t)GRID_PX * GRID_PX * 4);
    if (!radar) return;

    int z = g_zoom, tx = 0, ty = 0, any = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        GridFor(z, p->lat, p->lon, &tx, &ty);
        any = FetchTileGrid(L"rv", z, tx, ty, g_frames[frameIdx].path, radar,
                            2 * 24 * 3600);
        if (any || z <= 7) break;
        z = 7;   /* free-tier placeholder fallback */
    }
    g_radarNote = (z != 8) ? 1 : 0;
    if (any) {
        BaseGridEnsure(z, tx, ty);
    } else {
        /* nothing to overlay: show the basemap anyway so the pane is still
           a map of the place, and let the paint code say "no rain data" */
        BaseGridEnsure(z, tx, ty);
    }

    for (int i = 0; i < GRID_PX * GRID_PX; i++) {
        uint32_t rp = radar[i];
        int a = (int)((rp >> 24) & 0xFF);
        if (a == 0) {
            g_viewStage[i] = g_base ? (g_base[i] | 0xFF000000u) : 0xFF202430u;
        } else {
            uint32_t bp = g_base ? g_base[i] : 0x002A2F3Cu;
            int br = (int)(bp & 0xFF), bg = (int)((bp >> 8) & 0xFF), bb = (int)((bp >> 16) & 0xFF);
            int rr = (int)(rp & 0xFF), rg = (int)((rp >> 8) & 0xFF), rb = (int)((rp >> 16) & 0xFF);
            int or_ = rr * a / 255 + br * (255 - a) / 255;
            int og = rg * a / 255 + bg * (255 - a) / 255;
            int ob = rb * a / 255 + bb * (255 - a) / 255;
            g_viewStage[i] = 0xFF000000u | (uint32_t)ob << 16 | (uint32_t)og << 8 | (uint32_t)or_;
        }
    }
    free(radar);

    EnterCriticalSection(&g_lock);
    uint32_t *old = g_view;
    g_view = g_viewStage;
    g_viewStage = old ? old : NULL;
    g_viewReady = any ? 2 : 1;
    g_baseKey[0] = z;
    g_baseKey[1] = tx;
    g_baseKey[2] = ty;
    LeaveCriticalSection(&g_lock);
    PostMessageW(g_hwnd, WM_APP_DATA, JOB_TILES, 0);
}
