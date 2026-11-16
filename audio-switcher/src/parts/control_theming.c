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

/* Audio Switcher adapter for the shared transparent Tri-Spoke engine.
   No local SDF/cache/blit implementation belongs in an individual suite app. */
static SuiteEmblemEngine g_audioSwitcherEmblem = { 0 };

static void DrawAudioSwitcherEmblem(uint32_t *pixels, int width, int height,
                                    int x, int y, int size) {
    COLORREF nearColor, farColor;
    SuiteEmblemResolvePalette(COL_BG, COL_ACCENT, COL_TEXT, &nearColor, &farColor);
    SuiteEmblemDraw(&g_audioSwitcherEmblem, pixels, width, height, x, y, size,
                    SuiteEmblemAngleNow(GetTickCount64(), 30000ULL),
                    nearColor, farColor);
}

static void ReleaseAudioSwitcherEmblem(void) {
    SuiteEmblemRelease(&g_audioSwitcherEmblem);
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). */
static void PaintEmblemBlit(HDC dc, int x, int y, int size) {
    if (size < 8 || size > 512) return;
    uint32_t *pixels = (uint32_t *)malloc((size_t)size * size * sizeof(uint32_t));
    if (!pixels) return;
    uint32_t background = SuiteEmblemDibPixel(COL_BG);
    for (int i = 0; i < size * size; i++) pixels[i] = background;
    DrawAudioSwitcherEmblem(pixels, size, size, 0, 0, size);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, size, size, 0, 0, 0, size, pixels, &bmi, DIB_RGB_COLORS);
    free(pixels);
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

