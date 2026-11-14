/* Exposed symbol reference kept in sync with suite style tests */

static void DrawWayfinderEmblem(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

static void DrawStatusTag(HDC hdc, uint32_t *px, int W, int H, HFONT font,
                          int rightX, int y, const wchar_t *text,
                          COLORREF fg, COLORREF bg, int dpi, int clipTop, int clipBottom) {
    SelectObject(hdc, font);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    int h = S(22, dpi);
    int w = sz.cx + S(28, dpi);
    RECT r = { rightX - w, y, rightX, y + h };
    SmoothRoundRect(px, W, H, &r, (float)h * 0.5f, bg, bg, clipTop, clipBottom);

    int dotD = S(6, dpi);
    RECT dotRc = {
        r.left + S(9, dpi),
        r.top + (h - dotD) / 2,
        r.left + S(9, dpi) + dotD,
        r.top + (h - dotD) / 2 + dotD
    };
    SmoothRoundRect(px, W, H, &dotRc, (float)dotD * 0.5f, fg, fg, clipTop, clipBottom);

    RECT tr = { r.left + S(19, dpi), y, r.right - S(8, dpi), y + h };
    SetTextColor(hdc, fg);
    DrawTextW(hdc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* style: 0=soft secondary, 1=primary cobalt, 2=coral disconnect, 3=primary connect */
static void DrawActionBtn(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                          const wchar_t *label, int style, BOOL hovered, int dpi,
                          int clipTop, int clipBottom) {
    COLORREF fill = hovered ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border = hovered ? COL_HOVER_RIM : COL_BTN_BORDER;
    COLORREF fg = COL_TEXT;

    if (style == 1 || style == 3) {
        fill = hovered ? COL_ACCENT_HOV : COL_ACCENT_STRONG;
        border = fill;
    } else if (style == 2) {
        fill = hovered ? RGB(74, 36, 44) : COL_DANGER_BG;
        border = COL_DANGER;
        fg = COL_DANGER;
    }

    SmoothRoundRect(px, W, H, &r, (float)S(7, dpi), fill, border, clipTop, clipBottom);
    SelectObject(hdc, font);
    SetTextColor(hdc, fg);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* Smooth rounded pill switch control with label + sliding circular knob */
static void DrawPillToggle(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                           const wchar_t *label, BOOL isOn, BOOL hovered, int dpi,
                           int clipTop, int clipBottom) {
    COLORREF cardFill = hovered ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border   = hovered ? COL_HOVER_RIM : COL_BTN_BORDER;
    SmoothRoundRect(px, W, H, &r, (float)S(7, dpi), cardFill, border, clipTop, clipBottom);

    int trackW = S(34, dpi), trackH = S(18, dpi);
    int trackX = r.right - S(10, dpi) - trackW;
    int trackY = r.top + (r.bottom - r.top - trackH) / 2;
    RECT trk = { trackX, trackY, trackX + trackW, trackY + trackH };
    COLORREF trkFill = isOn ? COL_ACCENT_STRONG : RGB(24, 28, 38);
    COLORREF trkBorder = isOn ? COL_ACCENT : COL_BORDER;
    SmoothRoundRect(px, W, H, &trk, (float)trackH * 0.5f, trkFill, trkBorder, clipTop, clipBottom);

    int knobD = trackH - S(6, dpi);
    int knobX = isOn ? (trk.right - S(3, dpi) - knobD) : (trk.left + S(3, dpi));
    int knobY = trk.top + (trackH - knobD) / 2;
    RECT knb = { knobX, knobY, knobX + knobD, knobY + knobD };
    COLORREF knbCol = isOn ? RGB(255, 255, 255) : COL_MUTED;
    SmoothRoundRect(px, W, H, &knb, (float)knobD * 0.5f, knbCol, knbCol, clipTop, clipBottom);

    SelectObject(hdc, font);
    SetTextColor(hdc, isOn ? COL_TEXT : COL_MUTED);
    RECT lbl = { r.left + S(12, dpi), r.top, trackX - S(8, dpi), r.bottom };
    DrawTextW(hdc, label, -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void AddHit(RECT r, int kind, int index) {
    if (g_hitCount < MAX_HITBOXES) {
        g_hits[g_hitCount].rect  = r;
        g_hits[g_hitCount].kind  = kind;
        g_hits[g_hitCount].index = index;
        g_hitCount++;
    }
}

