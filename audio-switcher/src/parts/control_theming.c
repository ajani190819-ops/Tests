/* ============================================================================
 *  Control theming
 *
 *  Stock Win32 buttons and trackbars render with the light OS chrome, which on
 *  the Soft Slate dark canvas looked like unfinished controls dropped onto a
 *  finished background. Everything user-facing here is drawn from the palette:
 *  buttons are owner-drawn, sliders are custom-drawn, and text uses Segoe UI at
 *  consistent sizes instead of the default 8px system font.
 * ============================================================================ */

static HFONT  g_font_ui    = NULL;   /* Segoe UI 9pt            */
static HFONT  g_font_head  = NULL;   /* Segoe UI 9pt semibold   */
static HFONT  g_font_small = NULL;   /* Segoe UI 8pt            */
static HWND   g_hover_btn  = NULL;   /* button currently under the cursor */

/* Small brush cache: WM_DRAWITEM and WM_PAINT fire constantly, and creating a
 * brush per paint would leak GDI objects until the desktop ran out. */
static struct { COLORREF c; HBRUSH b; } g_brush_cache[24];

static HBRUSH ui_brush(COLORREF c) {
    for (int i = 0; i < 24; i++) {
        if (g_brush_cache[i].b && g_brush_cache[i].c == c) return g_brush_cache[i].b;
    }
    for (int i = 0; i < 24; i++) {
        if (!g_brush_cache[i].b) {
            g_brush_cache[i].c = c;
            g_brush_cache[i].b = CreateSolidBrush(c);
            return g_brush_cache[i].b;
        }
    }
    return (HBRUSH)GetStockObject(WHITE_BRUSH);
}

static void ui_fonts_create(void) {
    LOGFONTW lf = {0};
    wcscpy(lf.lfFaceName, L"Segoe UI");
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfHeight = -MulDiv(9, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    g_font_ui = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    g_font_head = CreateFontIndirectW(&lf);
    lf.lfHeight = -MulDiv(8, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    g_font_small = CreateFontIndirectW(&lf);
}

static void ui_fonts_destroy(void) {
    if (g_font_ui)    { DeleteObject(g_font_ui);    g_font_ui = NULL; }
    if (g_font_head)  { DeleteObject(g_font_head);  g_font_head = NULL; }
    if (g_font_small) { DeleteObject(g_font_small); g_font_small = NULL; }
    for (int i = 0; i < 24; i++) {
        if (g_brush_cache[i].b) { DeleteObject(g_brush_cache[i].b); g_brush_cache[i].b = NULL; }
    }
}

/* Hover tracking for owner-drawn buttons: ODS_HOTLIGHT is not reliably set for
 * BS_OWNERDRAW, so track the cursor ourselves and invalidate. */
static LRESULT CALLBACK BtnHoverProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                     UINT_PTR uid, DWORD_PTR ref) {
    (void)uid; (void)ref;
    if (msg == WM_MOUSEMOVE && g_hover_btn != hwnd) {
        g_hover_btn = hwnd;
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (msg == WM_MOUSELEAVE && g_hover_btn == hwnd) {
        g_hover_btn = NULL;
        InvalidateRect(hwnd, NULL, FALSE);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* A trackbar paints its own background in light chrome. Take the erase over so
 * the whole control sits on the card colour, and let NM_CUSTOMDRAW paint the
 * channel and thumb. */
static LRESULT CALLBACK TrackbarSkinProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR uid, DWORD_PTR ref) {
    (void)uid; (void)ref;
    if (msg == WM_ERASEBKGND) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, ui_brush((COLORREF)ref ? COL_CARD : COL_BG));
        return 1;
    }
    /* A clicked slider holds the focus, and WM_MOUSEWHEEL goes to the focused
       window - so without this the panel would stop scrolling the moment the
       user touched a volume slider. */
    if (msg == WM_MOUSEWHEEL) {
        HWND panel = GetParent(hwnd);
        if (panel) return SendMessageW(panel, WM_MOUSEWHEEL, wp, lp);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1 from Project Hub */
static void EmbRasterize(uint32_t *dst, int n, COLORREF bg) {
    (void)bg;
    const double scale = EMB_SDF_SCALE, b = EMB_SDF_BOOST;
    const double pxSize = 2.0 / (double)n;
    const double aa = (pxSize * 1.35 > 1e-4) ? (pxSize * 1.35) : 1e-4;
    const double deg = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double nrm[3] = { 90.0, -150.0, -30.0 };
    for (int j = 0; j < n; j++) {
        double y0 = ((double)j + 0.5) / (double)n;
        double y = -1.0 + 2.0 * y0;
        for (int i = 0; i < n; i++) {
            double x0 = ((double)i + 0.5) / (double)n;
            double x = -1.0 + 2.0 * x0;
            double r = sqrt(x * x + y * y) / scale;
            double ang = atan2(y, x) / deg;
            double dOuter = fabs(r - 0.895) - (0.022 + b * 0.45);
            double dRad   = fabs(r - 0.742) - (0.056 + b * 0.5);
            double dGap = 180.0;
            for (int k = 0; k < 3; k++) {
                double m = fmod(ang - gaps[k] + 180.0, 360.0);
                if (m < 0) m += 360.0;
                double diff = fabs(m - 180.0);
                if (diff < dGap) dGap = diff;
            }
            double dCut  = (12.5 - dGap) * deg * 0.742;
            double dArcs = (dRad > dCut) ? dRad : dCut;
            double dSpokes = 10.0;
            double pMid = 0.052 + b * 0.12, pHw = 0.024 + b * 0.42;
            double sMid = 0.5 * (0.258 + 0.722), sHw = 0.5 * (0.722 - 0.258);
            for (int k = 0; k < 3; k++) {
                double rad = gaps[k] * deg;
                double ux = cos(rad), uy = sin(rad);
                double proj = (x * ux + y * uy) / scale;
                double perp = fabs(-x * uy + y * ux) / scale;
                double dAlong = fabs(proj - sMid) - sHw;
                double dBars  = fabs(perp - pMid) - pHw;
                double dThis  = (dAlong > dBars) ? dAlong : dBars;
                if (dThis < dSpokes) dSpokes = dThis;
            }
            double dHubOut = r - (0.224 + b * 0.25);
            double dTri = -10.0;
            for (int k = 0; k < 3; k++) {
                double rad = nrm[k] * deg;
                double dHalf = (x * cos(rad) + y * sin(rad)) / scale - 0.074;
                if (dHalf > dTri) dTri = dHalf;
            }
            double dHub = (dHubOut > -dTri) ? dHubOut : -dTri;
            double u1 = (dOuter < dArcs) ? dOuter : dArcs;
            double u2 = (dSpokes < dHub) ? dSpokes : dHub;
            double u  = (u1 < u2) ? u1 : u2;
            double alpha = 0.5 - (u * scale) / aa;
            if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
            double t = x0 * 0.3 + y0 * 0.7;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            double fr = 246.0 * (1 - t) + 196.0 * t;
            double fg = 249.0 * (1 - t) + 212.0 * t;
            double fb = 254.0 * (1 - t) + 234.0 * t;
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

typedef struct EmbCache {
    int      sz;
    int      srcSz;
    uint32_t *src;
} EmbCache;
static EmbCache g_embCache = { 0, 0, NULL };

static void EmbCacheEnsure(int sz, COLORREF bg) {
    if (g_embCache.src && g_embCache.sz == sz) return;
    free(g_embCache.src);
    g_embCache.src = NULL;
    g_embCache.sz = 0;
    if (sz < 8 || sz > 512) return;
    int srcSz = sz * 4;
    uint32_t *src = (uint32_t *)malloc((size_t)srcSz * srcSz * sizeof(uint32_t));
    if (!src) return;
    EmbRasterize(src, srcSz, bg);
    g_embCache.sz    = sz;
    g_embCache.srcSz = srcSz;
    g_embCache.src   = src;
}

static void EmbSpinDraw(uint32_t *px, int W, int H, int x, int y, int sz, COLORREF bg) {
    EmbCacheEnsure(sz, bg);
    const uint32_t *src = g_embCache.src;
    if (!src) return;
    int srcSz = g_embCache.srcSz;
    double ang = ((double)(GetTickCount64() % 30000ULL) / 30000.0) * 6.283185307179586;
    double ca = cos(ang), sa = sin(ang);
    double c = (sz - 1) * 0.5;
    double sc = (double)(srcSz - 1) / (double)(sz > 1 ? sz : 1);
    int maxs = srcSz - 1;
    for (int dy = 0; dy < sz; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= H) continue;
        double v = dy - c;
        for (int dx = 0; dx < sz; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= W) continue;
            double u = dx - c;
            double fx = (ca * u + sa * v + c) * sc;
            double fy = (-sa * u + ca * v + c) * sc;
            if (fx < 0) fx = 0; else if (fx > maxs) fx = maxs;
            if (fy < 0) fy = 0; else if (fy > maxs) fy = maxs;
            int x0 = (int)fx, y0 = (int)fy;
            double ax = fx - x0, ay = fy - y0;
            double w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay);
            double w01 = (1 - ax) * ay,       w11 = ax * ay;
            const uint32_t *p00 = &src[y0 * srcSz + x0];
            uint32_t c00 = p00[0];
            uint32_t c10 = (x0 < maxs) ? p00[1] : c00;
            const uint32_t *p01 = (y0 < maxs) ? p00 + srcSz : p00;
            uint32_t c01 = p01[0];
            uint32_t c11 = (x0 < maxs) ? p01[1] : c01;
            double r = (c00 & 255) * w00 + (c10 & 255) * w10 + (c01 & 255) * w01 + (c11 & 255) * w11;
            double g = ((c00 >> 8) & 255) * w00 + ((c10 >> 8) & 255) * w10 +
                       ((c01 >> 8) & 255) * w01 + ((c11 >> 8) & 255) * w11;
            double b = ((c00 >> 16) & 255) * w00 + ((c10 >> 16) & 255) * w10 +
                       ((c01 >> 16) & 255) * w01 + ((c11 >> 16) & 255) * w11;
            double a = ((c00 >> 24) & 255) * w00 + ((c10 >> 24) & 255) * w10 +
                       ((c01 >> 24) & 255) * w01 + ((c11 >> 24) & 255) * w11;
            int ai = (int)(a + 0.5);
            uint32_t d = px[ty * W + tx];
            if (ai >= 255) {
                px[ty * W + tx] = RGB((int)(r + 0.5), (int)(g + 0.5), (int)(b + 0.5));
            } else if (ai > 0) {
                int dr = (int)(d & 255), dg = (int)((d >> 8) & 255), db = (int)((d >> 16) & 255);
                px[ty * W + tx] = RGB(((int)(r + 0.5) * ai + dr * (255 - ai) + 127) / 255,
                                      ((int)(g + 0.5) * ai + dg * (255 - ai) + 127) / 255,
                                      ((int)(b + 0.5) * ai + db * (255 - ai) + 127) / 255);
            }
        }
    }
}

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). */
static void PaintEmblemBlit(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    uint32_t bgP = ((uint32_t)GetRValue(COL_BG) << 16) |
                   ((uint32_t)GetGValue(COL_BG) << 8) |
                   (uint32_t)GetBValue(COL_BG);
    for (int i = 0; i < sz * sz; i++) px[i] = bgP;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

static int GetWinDpi(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi ? dpi : 96;
}

static void update_volume_text(HWND hwnd_txt, float vol) {
    wchar_t text[32];
    swprintf(text, 32, L"%d%%", (int)(vol * 100.0f + 0.5f));
    SetWindowTextW(hwnd_txt, text);
}

