/* Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer, ported 1:1
   from tools/generate_suite_icons.py (trispoke_sdf_alpha + header
   gradient). The mark is drawn straight at cache resolution, so the
   spinning emblem is razor sharp at any DPI with no bitmap upscaling. */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

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
            /* Store colour + REAL alpha in the top byte; the blit blends
               over whatever is behind, so no flat cache box can ever show
               as a demarcation square around the emblem. */
            dst[j * n + i] = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5)) |
                             ((DWORD)(int)(alpha * 255.0 + 0.5) << 24);
        }
    }
}

/* Emblem cache: rasterized once at 4x by the SDF renderer; every paint
   only rotates it with bilinear sampling - smooth at any angle, and
   nothing at all is allocated per frame. */
typedef struct EmbCache {
    int      sz;        /* on-screen size */
    int      srcSz;     /* 4x supersample */
    uint32_t *src;      /* srcSz * srcSz colour rows, pre-blended on bg */
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
    /* One full turn every 30 seconds: slow enough to read as alive,
       slow enough to never pull the eye off the controls. */
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

static void DrawSuiteEmblemIcon(uint32_t *px, int W, int H, int x, int y, int sz) {
    EmbCacheEnsure(sz, COL_BG);
    EmbSpinDraw(px, W, H, x, y, sz, COL_BG);
}

static void AddHit(RECT r, int kind, int index) {
    if (g_hitCount < 96) {
        g_hits[g_hitCount].rect  = r;
        g_hits[g_hitCount].kind  = kind;
        g_hits[g_hitCount].index = index;
        g_hitCount++;
    }
}

static void DrawSuiteBtn(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                         const wchar_t *label, int style, BOOL hot, int dpi, int ct, int cb) {
    COLORREF fill = hot ? COL_SEC_HOV : COL_SEC_BTN;
    COLORREF border = hot ? COL_HOVER_RIM : COL_BTN_BORDER;
    COLORREF fg = COL_TEXT;
    if (style == 1) { fill = hot ? COL_ACCENT_HOV : COL_ACCENT_STRONG; border = fill; }
    else if (style == 2) { fill = hot ? RGB(88, 66, 30) : COL_WARN_BG; border = COL_WARN; fg = COL_WARN; }
    SmoothRoundRect(px, W, H, &r, (float)S(9, dpi), fill, border, ct, cb);
    SelectObject(hdc, font);
    SetTextColor(hdc, fg);
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void DrawPillSwitch(HDC hdc, uint32_t *px, int W, int H, HFONT font, RECT r,
                           const wchar_t *label, BOOL isOn, BOOL hot, int dpi, int ct, int cb) {
    SmoothRoundRect(px, W, H, &r, (float)S(9, dpi),
                    hot ? COL_SEC_HOV : COL_SEC_BTN,
                    hot ? COL_HOVER_RIM : COL_BTN_BORDER, ct, cb);
    int tw = S(34, dpi), th = S(18, dpi);
    int tx = r.right - S(10, dpi) - tw;
    int ty = r.top + (r.bottom - r.top - th) / 2;
    RECT trk = { tx, ty, tx + tw, ty + th };
    SmoothRoundRect(px, W, H, &trk, (float)th * 0.5f,
                    isOn ? COL_ACCENT_STRONG : RGB(24, 28, 38),
                    isOn ? COL_ACCENT : COL_BORDER, ct, cb);
    int kd = th - S(6, dpi);
    int kx = isOn ? (trk.right - S(3, dpi) - kd) : (trk.left + S(3, dpi));
    int ky = trk.top + (th - kd) / 2;
    RECT knb = { kx, ky, kx + kd, ky + kd };
    COLORREF kc = isOn ? RGB(255, 255, 255) : COL_MUTED;
    SmoothRoundRect(px, W, H, &knb, (float)kd * 0.5f, kc, kc, ct, cb);

    SelectObject(hdc, font);
    SetTextColor(hdc, isOn ? COL_TEXT : COL_MUTED);
    RECT lr = { r.left + S(12, dpi), r.top, tx - S(6, dpi), r.bottom };
    DrawTextW(hdc, label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void DrawSmoothSlider(HDC hdc, uint32_t *px, int W, int H, HFONT fLabel, HFONT fVal,
                             RECT trackBox, const wchar_t *title, const wchar_t *valStr,
                             double norm01, BOOL hot, int dpi, int ct, int cb) {
    if (norm01 < 0.0) norm01 = 0.0;
    if (norm01 > 1.0) norm01 = 1.0;

    SelectObject(hdc, fLabel);
    SetTextColor(hdc, COL_MUTED);
    RECT lr = { trackBox.left, trackBox.top - S(19, dpi), trackBox.right - S(72, dpi), trackBox.top - S(1, dpi) };
    DrawTextW(hdc, title, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, fVal);
    SetTextColor(hdc, COL_ACCENT);
    RECT vr = { trackBox.right - S(72, dpi), trackBox.top - S(19, dpi), trackBox.right, trackBox.top - S(1, dpi) };
    DrawTextW(hdc, valStr, -1, &vr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int barH = S(8, dpi);
    int cy = (trackBox.top + trackBox.bottom) / 2;
    RECT groove = { trackBox.left, cy - barH / 2, trackBox.right, cy + barH / 2 };
    SmoothRoundRect(px, W, H, &groove, (float)barH * 0.5f, RGB(22, 26, 35), COL_BORDER, ct, cb);

    int thumbR = S(hot ? 9 : 8, dpi);
    int usable = (groove.right - groove.left) - thumbR * 2;
    if (usable < 1) usable = 1;
    int cx = groove.left + thumbR + (int)(norm01 * usable);

    RECT fillR = { groove.left, groove.top, cx, groove.bottom };
    SmoothRoundRect(px, W, H, &fillR, (float)barH * 0.5f, COL_ACCENT_STRONG, COL_ACCENT, ct, cb);

    RECT thumb = { cx - thumbR, cy - thumbR, cx + thumbR, cy + thumbR };
    SmoothRoundRect(px, W, H, &thumb, (float)thumbR, RGB(248, 250, 252), COL_ACCENT, ct, cb);
}

static void RenderKblWindow(HWND hwnd, HDC targetDc) {
    RECT cli;
    GetClientRect(hwnd, &cli);
    int W = cli.right - cli.left, H = cli.bottom - cli.top;
    if (W <= 0 || H <= 0) return;

    int dpi = GetWinDpi(hwnd);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC mem = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(mem); return; }
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    HFONT fTitle = CreateFontW(-S(19, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fSub   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fCard  = CreateFontW(-S(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBtn   = CreateFontW(-S(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fBadge = CreateFontW(-S(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT fSmall = CreateFontW(-S(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    int M = S(28, dpi);
    int CW = W - M * 2;
    int headerH = S(76, dpi);
    int footerH = S(44, dpi);
    int footerTop = H - footerH;

    FillCanvasRect(px, W, H, 0, 0, W, footerTop, COL_BG);
    FillCanvasRect(px, W, H, 0, footerTop, W, H, COL_FOOTER_BG);

    g_hitCount = 0;

    /* 1. Unified Suite Header (matches Project Hub & Bluetooth Guard) */
    int embSz = S(52, dpi);
    DrawSuiteEmblemIcon(px, W, H, M, (headerH - embSz) / 2, embSz);

    int topH = S(34, dpi), topY = (headerH - topH) / 2;
    RECT btnRst  = { W - M - S(88, dpi), topY, W - M, topY + topH };
    RECT togAuto = { btnRst.left - S(130, dpi), topY, btnRst.left - S(8, dpi), topY + topH };
    RECT togLive = { togAuto.left - S(122, dpi), topY, togAuto.left - S(8, dpi), topY + topH };

    int iLive = g_hitCount; AddHit(togLive, HIT_HDR_LIGHTS, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togLive, L"Lights", !g_wantOff, g_hoverIdx == iLive, dpi, 0, headerH);

    int iAuto = g_hitCount; AddHit(togAuto, HIT_HDR_STARTUP, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togAuto, L"Startup", g_autostart, g_hoverIdx == iAuto, dpi, 0, headerH);

    int iRst = g_hitCount; AddHit(btnRst, HIT_HDR_RESTART, 0);
    DrawSuiteBtn(mem, px, W, H, fBtn, btnRst, L"Restart", 0, g_hoverIdx == iRst, dpi, 0, headerH);

    int hdrLeft = M + embSz + S(14, dpi);
    SelectObject(mem, fTitle);
    SetTextColor(mem, COL_TEXT);
    RECT rHdrT = { hdrLeft, S(13, dpi), togLive.left - S(12, dpi), S(40, dpi) };
    DrawTextW(mem, L"Keyboard Lighting", -1, &rHdrT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    wchar_t subHdr[160];
    _snwprintf(subHdr, 159, L"ROG Strix G16  \x00B7  Keys: %S  \x00B7  Light Bar: %S",
               g_wantOff || !g_cfg.kbd.on ? "Off" : kEffects[g_cfg.kbd.effectIdx].label,
               g_wantOff || !g_cfg.bar.on ? "Off" : kEffects[g_cfg.bar.effectIdx].label);
    SelectObject(mem, fSub);
    SetTextColor(mem, COL_TELEMETRY);
    RECT rHdrS = { hdrLeft, S(40, dpi), togLive.left - S(12, dpi), S(60, dpi) };
    DrawTextW(mem, subHdr, -1, &rHdrS, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    FillCanvasRect(px, W, H, M, headerH, W - M, headerH + 1, COL_DIVIDER);

    /* Scrollable Cards Region */
    int listTop = headerH + S(12, dpi);
    int listBot = footerTop - S(2, dpi);
    int gap = S(12, dpi);
    int hCard1 = S(180, dpi), hCard2 = S(118, dpi), hCard3 = S(118, dpi), hCard4 = S(148, dpi);
    g_contentH = hCard1 + hCard2 + hCard3 + hCard4 + gap * 3 + S(16, dpi);
    int maxSc = g_contentH - (listBot - listTop);
    if (maxSc < 0) maxSc = 0;
    if (g_scrollY > maxSc) g_scrollY = maxSc;
    if (g_scrollY < 0) g_scrollY = 0;

    int saved = SaveDC(mem);
    IntersectClipRect(mem, 0, listTop, W, listBot);

    int y = listTop - g_scrollY;
    GroupCfg *cg = CurGroup();
    BOOL alive = IsEngineAlive();

    /* ---- CARD 1: Live ROG Strix G16 Preview + Master Brightness ---- */
    RECT c1 = { M, y, M + CW, y + hCard1 };
    SmoothRoundRect(px, W, H, &c1, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    COLORREF stCol = g_wantOff ? COL_IDLE_SLATE : (alive ? COL_GOOD : COL_WARN);
    COLORREF stBg  = g_wantOff ? COL_PILL_MUTED : (alive ? COL_GOOD_BG : COL_WARN_BG);
    const wchar_t *stTxt = g_wantOff ? L"Lights Off" : (alive ? L"Engine Active" : L"Preview Ready");

    RECT bar1 = { c1.left + S(1, dpi), c1.top + S(16, dpi), c1.left + S(5, dpi), c1.bottom - S(16, dpi) };
    SmoothRoundRect(px, W, H, &bar1, (float)S(2, dpi), stCol, stCol, listTop, listBot);

    int icoSz = S(38, dpi);
    HICON hAppIco = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP),
                                      IMAGE_ICON, icoSz, icoSz, LR_DEFAULTCOLOR);
    if (hAppIco) {
        DrawIconEx(mem, c1.left + S(16, dpi), c1.top + S(12, dpi), hAppIco, icoSz, icoSz, 0, NULL, DI_NORMAL);
        DestroyIcon(hAppIco);
    }

    SelectObject(mem, fCard);
    SetTextColor(mem, COL_TEXT);
    RECT c1Title = { c1.left + S(64, dpi), c1.top + S(14, dpi), c1.right - S(150, dpi), c1.top + S(38, dpi) };
    DrawTextW(mem, L"ROG Strix G16 Live Strip", -1, &c1Title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* Status Pill on Card 1 */
    SelectObject(mem, fBadge);
    SIZE bsz = {0, 0};
    GetTextExtentPoint32W(mem, stTxt, (int)wcslen(stTxt), &bsz);
    int pillW = bsz.cx + S(26, dpi), pillH = S(22, dpi);
    RECT pill1 = { c1.right - S(16, dpi) - pillW, c1.top + S(12, dpi), c1.right - S(16, dpi), c1.top + S(12, dpi) + pillH };
    SmoothRoundRect(px, W, H, &pill1, (float)pillH * 0.5f, stBg, stBg, listTop, listBot);
    int d6 = S(6, dpi);
    RECT pDot = { pill1.left + S(9, dpi), pill1.top + (pillH - d6)/2, pill1.left + S(9, dpi) + d6, pill1.top + (pillH - d6)/2 + d6 };
    SmoothRoundRect(px, W, H, &pDot, (float)d6 * 0.5f, stCol, stCol, listTop, listBot);
    RECT pTxt = { pill1.left + S(18, dpi), pill1.top, pill1.right - S(6, dpi), pill1.bottom };
    SetTextColor(mem, stCol);
    DrawTextW(mem, stTxt, -1, &pTxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* 4-Zone Keyboard strip + 12-Zone Light Bar ring. The bar is a closed
       loop on the real machine: its last lamp sits right next to its first,
       so the preview draws it as a ring (clockwise from the top). The old
       straight strip hid the seam and read the zones out of order. */
    int stripY = c1.top + S(82, dpi), stripH = S(28, dpi);
    int innerW = CW - S(32, dpi);
    int splitGap = S(12, dpi);
    int kbdStripW = (innerW - splitGap) * 32 / 100;
    int barStripW = innerW - splitGap - kbdStripW;
    int cellGap = S(3, dpi);

    int kn = g_liveKbdN > 0 ? g_liveKbdN : 4;
    for (int i = 0; i < kn; i++) {
        int x0 = c1.left + S(16, dpi) + (kbdStripW * i) / kn;
        int x1 = c1.left + S(16, dpi) + (kbdStripW * (i + 1)) / kn - cellGap;
        RECT zr = { x0, stripY, x1, stripY + stripH };
        SmoothRoundRect(px, W, H, &zr, (float)S(6, dpi), g_liveCols[i], COL_BORDER, listTop, listBot);
    }

    int bn = g_liveBarN > 0 ? g_liveBarN : 12;
    if (bn < 1) bn = 1;
    int barLeft = c1.left + S(16, dpi) + kbdStripW + splitGap;
    float ringCx = (float)(barLeft + barStripW / 2);
    float ringCy = (float)(c1.top + S(96, dpi));
    float ringR  = (float)S(44, dpi);
    float dotR   = (float)S(9, dpi);
    SmoothRingTrack(px, W, H, ringCx, ringCy, ringR, (float)S(3, dpi),
                    COL_BORDER, listTop, listBot);
    for (int i = 0; i < bn; i++) {
        /* Place each lamp at its established ring angle (layout.txt), so the
           dots show the real loop order the engine's effects travel through.
           Without the layout the ring falls back to even index order. */
        int lamp = kn + i;
        double frac = (double)i / (double)bn;
        if (i < g_barLayoutN) {
            lamp = g_barOrder[i];
            if (lamp < 0 || lamp >= 64) lamp = kn + i;
            frac = g_barQ[i];
        }
        double ang = -1.5707963267948966 + 6.283185307179586 * frac;
        float dx = ringCx + ringR * (float)cos(ang);
        float dy = ringCy + ringR * (float)sin(ang);
        SmoothCircle(px, W, H, dx, dy, dotR,
                     g_liveCols[lamp], COL_BORDER, listTop, listBot);
    }

    /* Master Brightness Slider */
    wchar_t mbVal[16];
    _snwprintf(mbVal, 15, L"%d%%", g_cfg.masterBrightness);
    RECT slMaster = { c1.left + S(16, dpi), c1.top + S(152, dpi), c1.right - S(16, dpi), c1.top + S(176, dpi) };
    int iMas = g_hitCount; AddHit(slMaster, HIT_SLIDER_MASTER, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slMaster, L"Master Brightness (Fn F2 / F3)", mbVal,
                     (double)g_cfg.masterBrightness / 100.0,
                     g_hoverIdx == iMas || g_dragKind == HIT_SLIDER_MASTER, dpi, listTop, listBot);

    y += hCard1 + gap;

    /* ---- CARD 2: Zone Tabs + Effect Picker ---- */
    RECT c2 = { M, y, M + CW, y + hCard2 };
    SmoothRoundRect(px, W, H, &c2, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    RECT segBox = { c2.left + S(16, dpi), c2.top + S(12, dpi), c2.left + S(256, dpi), c2.top + S(44, dpi) };
    SmoothRoundRect(px, W, H, &segBox, (float)S(10, dpi), RGB(24, 28, 38), COL_BORDER, listTop, listBot);
    RECT tab0 = { segBox.left + S(3, dpi), segBox.top + S(3, dpi), (segBox.left + segBox.right)/2 - S(1, dpi), segBox.bottom - S(3, dpi) };
    RECT tab1 = { (segBox.left + segBox.right)/2 + S(1, dpi), segBox.top + S(3, dpi), segBox.right - S(3, dpi), segBox.bottom - S(3, dpi) };
    int iT0 = g_hitCount; AddHit(tab0, HIT_TAB_KBD, 0);
    int iT1 = g_hitCount; AddHit(tab1, HIT_TAB_BAR, 0);
    if (g_tab == 0) {
        SmoothRoundRect(px, W, H, &tab0, (float)S(8, dpi), COL_ACCENT_STRONG, COL_ACCENT_STRONG, listTop, listBot);
        if (g_hoverIdx == iT1) SmoothRoundRect(px, W, H, &tab1, (float)S(8, dpi), COL_SEC_HOV, COL_SEC_HOV, listTop, listBot);
    } else {
        SmoothRoundRect(px, W, H, &tab1, (float)S(8, dpi), COL_ACCENT_STRONG, COL_ACCENT_STRONG, listTop, listBot);
        if (g_hoverIdx == iT0) SmoothRoundRect(px, W, H, &tab0, (float)S(8, dpi), COL_SEC_HOV, COL_SEC_HOV, listTop, listBot);
    }
    SelectObject(mem, fBtn);
    SetTextColor(mem, g_tab == 0 ? RGB(255,255,255) : COL_MUTED);
    DrawTextW(mem, L"Keyboard", -1, &tab0, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(mem, g_tab == 1 ? RGB(255,255,255) : COL_MUTED);
    DrawTextW(mem, L"Light Bar", -1, &tab1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT togZone = { c2.right - S(292, dpi), c2.top + S(12, dpi), c2.right - S(156, dpi), c2.top + S(44, dpi) };
    RECT togLink = { c2.right - S(148, dpi), c2.top + S(12, dpi), c2.right - S(16, dpi),  c2.top + S(44, dpi) };
    int iZO = g_hitCount; AddHit(togZone, HIT_TOG_ZONE_ON, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togZone, L"Zone On", cg->on, g_hoverIdx == iZO, dpi, listTop, listBot);
    int iLK = g_hitCount; AddHit(togLink, HIT_TOG_LINK, 0);
    DrawPillSwitch(mem, px, W, H, fBtn, togLink, L"Link Zones", g_cfg.link, g_hoverIdx == iLK, dpi, listTop, listBot);

    RECT pickR = { c2.left + S(16, dpi), c2.top + S(58, dpi), c2.left + S(296, dpi), c2.top + S(98, dpi) };
    int iPk = g_hitCount; AddHit(pickR, HIT_PICK_EFFECT, 0);
    SmoothRoundRect(px, W, H, &pickR, (float)S(10, dpi),
                    g_hoverIdx == iPk ? COL_SEC_HOV : COL_SEC_BTN,
                    g_hoverIdx == iPk ? COL_HOVER_RIM : COL_BTN_BORDER, listTop, listBot);
    wchar_t effLabelW[96];
    _snwprintf(effLabelW, 95, L"Pattern:  %S", kEffects[cg->effectIdx].label);
    SelectObject(mem, fBtn);
    SetTextColor(mem, COL_TEXT);
    RECT pickTxt = { pickR.left + S(14, dpi), pickR.top, pickR.right - S(28, dpi), pickR.bottom };
    DrawTextW(mem, effLabelW, -1, &pickTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT pickChev = { pickR.right - S(28, dpi), pickR.top, pickR.right - S(8, dpi), pickR.bottom };
    SetTextColor(mem, COL_ACCENT);
    DrawTextW(mem, L"\x25BE", -1, &pickChev, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    wchar_t hintW[160];
    _snwprintf(hintW, 159, L"%S", kEffects[cg->effectIdx].hint);
    SelectObject(mem, fSmall);
    SetTextColor(mem, COL_MUTED);
    RECT rHint = { pickR.right + S(14, dpi), pickR.top, c2.right - S(16, dpi), pickR.bottom };
    DrawTextW(mem, hintW, -1, &rHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    y += hCard2 + gap;

    /* ---- CARD 3: Interactive Colour Swatches + Band Width Bars ---- */
    RECT c3 = { M, y, M + CW, y + hCard3 };
    SmoothRoundRect(px, W, H, &c3, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    BOOL usesPal = kEffects[cg->effectIdx].usesPalette != 0;
    SelectObject(mem, fCard);
    SetTextColor(mem, COL_TEXT);
    RECT c3Title = { c3.left + S(16, dpi), c3.top + S(10, dpi), c3.left + S(200, dpi), c3.top + S(32, dpi) };
    DrawTextW(mem, L"Colour Palette", -1, &c3Title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, fSmall);
    SetTextColor(mem, COL_MUTED);
    RECT c3Sub = { c3.left + S(150, dpi), c3.top + S(10, dpi), c3.right - S(16, dpi), c3.top + S(32, dpi) };
    DrawTextW(mem, usesPal ? L"Click a swatch to pick colour  \x00B7  Drag bar beneath to adjust width"
                           : L"Selected pattern generates its own automatic colour palette",
              -1, &c3Sub, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int sx = c3.left + S(16, dpi), sy = c3.top + S(40, dpi), swSz = S(44, dpi);
    for (int i = 0; i < cg->swatchCount; i++) {
        RECT sr = { sx, sy, sx + swSz, sy + swSz };
        int hIdx = g_hitCount;
        if (usesPal) AddHit(sr, HIT_SWATCH_0 + i, i);
        COLORREF fillC = usesPal ? cg->swatches[i] : RGB(45, 50, 64);
        SmoothRoundRect(px, W, H, &sr, (float)S(11, dpi), fillC,
                        (usesPal && g_hoverIdx == hIdx) ? RGB(255, 255, 255) : COL_BTN_BORDER,
                        listTop, listBot);

        RECT wr = { sx, sy + swSz + S(6, dpi), sx + swSz, sy + swSz + S(16, dpi) };
        if (usesPal) AddHit(wr, HIT_WIDTH_0 + i, i);
        SmoothRoundRect(px, W, H, &wr, (float)S(5, dpi), RGB(22, 26, 35), COL_BORDER, listTop, listBot);
        double wt = log(cg->widths[i] / 0.25) / log(16.0);
        if (wt < 0.0) wt = 0.0;
        if (wt > 1.0) wt = 1.0;
        int fw = (int)(wt * swSz);
        if (fw < S(6, dpi)) fw = S(6, dpi);
        RECT wf = { wr.left, wr.top, wr.left + fw, wr.bottom };
        SmoothRoundRect(px, W, H, &wf, (float)S(5, dpi), usesPal ? COL_ACCENT : COL_IDLE_SLATE,
                        usesPal ? COL_ACCENT : COL_IDLE_SLATE, listTop, listBot);

        sx += swSz + S(10, dpi);
    }
    if (usesPal && cg->swatchCount < 8) {
        RECT addR = { sx, sy, sx + S(36, dpi), sy + swSz };
        int hAdd = g_hitCount; AddHit(addR, HIT_SWATCH_ADD, 0);
        DrawSuiteBtn(mem, px, W, H, fCard, addR, L"+", 0, g_hoverIdx == hAdd, dpi, listTop, listBot);
        sx += S(44, dpi);
    }
    if (usesPal && cg->swatchCount > 2) {
        RECT remR = { sx, sy, sx + S(36, dpi), sy + swSz };
        int hRem = g_hitCount; AddHit(remR, HIT_SWATCH_REM, 0);
        DrawSuiteBtn(mem, px, W, H, fCard, remR, L"-", 0, g_hoverIdx == hRem, dpi, listTop, listBot);
    }

    y += hCard3 + gap;

    /* ---- CARD 4: Speed, Zone Brightness & Motion Switches ---- */
    RECT c4 = { M, y, M + CW, y + hCard4 };
    SmoothRoundRect(px, W, H, &c4, (float)S(14, dpi), COL_CARD, COL_BORDER, listTop, listBot);

    int halfW = (CW - S(48, dpi)) / 2;
    wchar_t spdStr[16], brtStr[16];
    _snwprintf(spdStr, 15, L"%.1fx", (double)cg->speed / 10.0);
    _snwprintf(brtStr, 15, L"%d%%", cg->brightness);

    RECT slSpd = { c4.left + S(16, dpi), c4.top + S(32, dpi), c4.left + S(16, dpi) + halfW, c4.top + S(56, dpi) };
    int iSpd = g_hitCount; AddHit(slSpd, HIT_SLIDER_SPEED, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slSpd, L"Effect Speed", spdStr,
                     (double)(cg->speed - 1) / 49.0,
                     g_hoverIdx == iSpd || g_dragKind == HIT_SLIDER_SPEED, dpi, listTop, listBot);

    RECT slBrt = { c4.right - S(16, dpi) - halfW, c4.top + S(32, dpi), c4.right - S(16, dpi), c4.top + S(56, dpi) };
    int iBrt = g_hitCount; AddHit(slBrt, HIT_SLIDER_BRIGHT, 0);
    DrawSmoothSlider(mem, px, W, H, fBadge, fBadge, slBrt, L"Zone Brightness", brtStr,
                     (double)cg->brightness / 100.0,
                     g_hoverIdx == iBrt || g_dragKind == HIT_SLIDER_BRIGHT, dpi, listTop, listBot);

    int colW = (CW - S(48, dpi)) / 3;
    int r1Y = c4.top + S(66, dpi), r2Y = c4.top + S(104, dpi), thH = S(32, dpi);
    struct { int kind; const wchar_t *lbl; BOOL val; int row; int col; } opts[6] = {
        { HIT_TOG_LOOP,    L"Loop Travel",     cg->loop,      0, 0 },
        { HIT_TOG_EQ,      L"Balance Colours", cg->equalise,  0, 1 },
        { HIT_TOG_MIRROR,  L"Mirror",          cg->mirror,    0, 2 },
        { HIT_TOG_REVERSE, L"Reverse",         cg->reverse,   1, 0 },
        { HIT_TOG_SMOOTH,  L"Steady Colours",  g_cfg.smooth,  1, 1 },
        { HIT_TOG_OVERLAY, L"Battery Alerts",  g_cfg.overlay, 1, 2 }
    };
    for (int o = 0; o < 6; o++) {
        int ox = c4.left + S(16, dpi) + opts[o].col * (colW + S(8, dpi));
        int oy = (opts[o].row == 0) ? r1Y : r2Y;
        RECT orRc = { ox, oy, ox + colW, oy + thH };
        int hO = g_hitCount; AddHit(orRc, opts[o].kind, 0);
        DrawPillSwitch(mem, px, W, H, fBtn, orRc, opts[o].lbl, opts[o].val,
                       g_hoverIdx == hO, dpi, listTop, listBot);
    }

    RestoreDC(mem, saved);

    /* Unified Suite Footer */
    FillCanvasRect(px, W, H, 0, footerTop, W, footerTop + 1, COL_DIVIDER);
    COLORREF fCol = (!g_isAdmin) ? COL_WARN : (g_wantOff ? COL_IDLE_SLATE : COL_GOOD);
    int fdSz = S(8, dpi);
    RECT fdRc = { M, footerTop + (footerH - fdSz)/2, M + fdSz, footerTop + (footerH - fdSz)/2 + fdSz };
    SmoothRoundRect(px, W, H, &fdRc, (float)fdSz * 0.5f, fCol, fCol, footerTop, H);

    RECT quitR = { W - M - S(84, dpi), footerTop + S(7, dpi), W - M, H - S(8, dpi) };
    int iQuit = g_hitCount; AddHit(quitR, HIT_FOOT_QUIT, 0);
    DrawSuiteBtn(mem, px, W, H, fBtn, quitR, L"Quit", 0, g_hoverIdx == iQuit, dpi, footerTop, H);

    int rightLimit = quitR.left - S(8, dpi);
    if (!g_isAdmin) {
        RECT elR = { rightLimit - S(136, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iEl = g_hitCount; AddHit(elR, HIT_FOOT_ELEVATE, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, elR, L"Run as Admin", 2, g_hoverIdx == iEl, dpi, footerTop, H);
        rightLimit = elR.left - S(8, dpi);
    } else if (FileExistsW(g_zonesBat)) {
        RECT znR = { rightLimit - S(116, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iZn = g_hitCount; AddHit(znR, HIT_FOOT_ZONES, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, znR, L"Zones Setup", 0, g_hoverIdx == iZn, dpi, footerTop, H);
        rightLimit = znR.left - S(8, dpi);

        /* Named colour presets: Save snapshots the look, Pattern swaps it */
        RECT svR = { rightLimit - S(74, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iSv = g_hitCount; AddHit(svR, HIT_FOOT_SAVEP, 0);
        DrawSuiteBtn(mem, px, W, H, fBtn, svR, L"Save", 0, g_hoverIdx == iSv, dpi, footerTop, H);
        rightLimit = svR.left - S(8, dpi);

        RECT ptR = { rightLimit - S(158, dpi), footerTop + S(7, dpi), rightLimit, H - S(8, dpi) };
        int iPt = g_hitCount; AddHit(ptR, HIT_FOOT_PATTERN, 0);
        wchar_t ptLabel[48];
        if (g_presetIdx >= 0 && g_presetUsed[g_presetIdx])
            _snwprintf(ptLabel, 48, L"Pattern: %s", g_presetName[g_presetIdx]);
        else
            _snwprintf(ptLabel, 48, L"Pattern: -");
        DrawSuiteBtn(mem, px, W, H, fBtn, ptR, ptLabel, 0, g_hoverIdx == iPt, dpi, footerTop, H);
        rightLimit = ptR.left - S(8, dpi);
    }

    SelectObject(mem, fSub);
    SetTextColor(mem, (!g_isAdmin) ? COL_WARN : COL_MUTED);
    RECT fTxt = { M + S(16, dpi), footerTop, rightLimit - S(8, dpi), H };
    const wchar_t *helpTxt = HelpForHit(g_hoverIdx);
    DrawTextW(mem, (!g_isAdmin) ? L"Click 'Run as Admin' to unlock direct ROG Strix G16 USB RGB control"
                                : (helpTxt ? helpTxt : g_statusText),
              -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    /* Floating brightness toast (shown after Fn F2 / F3 presses) */
    if (g_toastAt > GetTickCount64() && g_toastText[0]) {
        SelectObject(mem, fBtn);
        SIZE tsz = { 0, 0 };
        GetTextExtentPoint32W(mem, g_toastText, (int)wcslen(g_toastText), &tsz);
        int tW = tsz.cx + S(40, dpi), tH = S(34, dpi);
        RECT toast = { (W - tW) / 2, footerTop - tH - S(14, dpi), (W + tW) / 2, footerTop - S(14, dpi) };
        SmoothRoundRect(px, W, H, &toast, (float)tH * 0.5f, RGB(28, 33, 45), COL_ACCENT, 0, H);
        SetTextColor(mem, COL_TEXT);
        RECT tt = { toast.left + S(16, dpi), toast.top, toast.right - S(16, dpi), toast.bottom };
        DrawTextW(mem, g_toastText, -1, &tt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    BitBlt(targetDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

    DeleteObject(fTitle); DeleteObject(fSub); DeleteObject(fCard);
    DeleteObject(fBtn); DeleteObject(fBadge); DeleteObject(fSmall);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

