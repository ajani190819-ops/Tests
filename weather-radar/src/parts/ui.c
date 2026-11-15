/* ---------------- ui: paint, hit testing, WndProc ----------------

   Everything is owner-drawn onto one double-buffered DIB (the same trick the
   rest of the suite uses to keep resizing flicker-free) except the search
   box, which is a real EDIT so keyboard, IME and clipboard behave. The one
   RichEdit lesson from Agent Notes applies in reverse here: an EDIT DOES
   honour WM_CTLCOLOREDIT, so the hint text colour lives in that handler. */

#define WIN_LW 1010
#define WIN_LH 800
#define LEFT_W 252

static void BtnRect(int i, RECT *r) {
    switch (i) {
    case BTN_SEARCH:  SetRect(r, S(16), S(110), S(120), S(140)); break;
    case BTN_KEEP:    SetRect(r, S(126), S(110), S(182), S(140)); break;
    case BTN_DROP:    SetRect(r, S(188), S(110), S(244), S(140)); break;
    case BTN_REFRESH: SetRect(r, S(WIN_LW - 16 - 104), S(16), S(WIN_LW - 16), S(48)); break;
    case BTN_PREV:    SetRect(r, S(272), S(452), S(308), S(480)); break;
    case BTN_PLAY:    SetRect(r, S(314), S(452), S(372), S(480)); break;
    case BTN_NEXT:    SetRect(r, S(378), S(452), S(414), S(480)); break;
    case BTN_ZOOM:    SetRect(r, S(420), S(452), S(496), S(480)); break;
    default:          SetRect(r, 0, 0, 0, 0); break;
    }
}

static void RadarImageRect(RECT *r) {
    SetRect(r, S(272), S(492), S(WIN_LW - 28), S(WIN_LH - 108));
}

/* Search-result and saved-place rows are plain painted rows: clickable,
   hoverable, no list-view theming to fight. */
static int SearchRowRect(int i, RECT *r) {
    if (i < 0 || i >= g_searchN) return 0;
    SetRect(r, S(16), S(172) + i * S(30), S(244), S(172) + i * S(30) + S(28));
    return 1;
}

/* Search results vary from a city to a very broad postcode. Saved places
   begin immediately below them, so neither list can paint over the other. */
static int SavedHeadingY(void) {
    return S(172 + g_searchN * 30 + 12);
}

static int SavedRowRect(int i, RECT *r) {
    if (i < 0 || i >= g_placeN) return 0;
    int top = SavedHeadingY() + S(22) + i * S(30);
    SetRect(r, S(16), top, S(244), top + S(28));
    return 1;
}

/* The edit is a real child window, unlike the rest of this painted UI.
   Give it the same scaled inner rectangle as the drawn search field; creating
   it before the window knows its DPI was what left it over the header/buttons. */
static void LayoutSearchEdit(void) {
    if (!g_hSearch) return;
    RECT r = { S(18), S(73), S(242), S(100) };
    SetWindowPos(g_hSearch, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_fontUI) SendMessageW(g_hSearch, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
}

static LRESULT CALLBACK SearchEditSubclass(HWND edit, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR id, DWORD_PTR ref) {
    (void)id;
    (void)ref;
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        PostMessageW(GetParent(edit), WM_APP_SEARCH, 0, 0);
        return 0;
    }
    return DefSubclassProc(edit, msg, wp, lp);
}

static int HitBtn(int x, int y) {
    RECT r;
    POINT pt = { x, y };
    for (int i = 0; i < BTN_COUNT; i++) {
        BtnRect(i, &r);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary) {
    RECT r;
    BtnRect(idx, &r);
    if (r.right <= r.left) return;
    COLORREF fill = primary ? COL_ACCENT : COL_INPUT;
    COLORREF edge = (g_hoverBtn == idx) ? COL_HOT : COL_DIV;
    DrawRoundedBox(dc, &r, S(9), fill, edge);
    DrawTextAt(dc, label, &r, g_fontBtn, primary ? COL_BG : COL_TXT,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/* unix seconds -> "14:20" (plus the day when it is not today), local time */
static void FrameTimeText(long long t, int kind, wchar_t *out, int cap) {
    LONGLONG ft = t * 10000000LL + 116444736000000000LL;
    FILETIME f;
    f.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFF);
    f.dwHighDateTime = (DWORD)(ft >> 32);
    FILETIME loc;
    FileTimeToLocalFileTime(&f, &loc);
    SYSTEMTIME st;
    FileTimeToSystemTime(&loc, &st);
    SYSTEMTIME now;
    GetLocalTime(&now);
    if (st.wYear == now.wYear && st.wMonth == now.wMonth && st.wDay == now.wDay)
        _snwprintf(out, cap - 1, L"%02d:%02d %s", st.wHour, st.wMinute,
                   kind ? L"(forecast)" : L"(observed)");
    else
        _snwprintf(out, cap - 1, L"%02d-%02d %02d:%02d %s", st.wMonth, st.wDay,
                   st.wHour, st.wMinute, kind ? L"(forecast)" : L"(observed)");
    out[cap - 1] = L'\0';
}

static void PaintNowCard(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT inner = { card->left + S(18), card->top + S(10), card->right - S(18), card->bottom - S(10) };
    if (!g_fc.have) {
        DrawTextAt(mem, L"Fetching the current conditions...", &inner, g_fontUI,
                   COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    wchar_t big[24];
    _snwprintf(big, 23, L"%d°", (int)llround(g_fc.tempF));
    /* The temperature owns the left block. Forecast details sit beside it,
       never underneath the large glyph where they became unreadable. */
    RECT tr = { inner.left, inner.top, inner.left + S(132), inner.bottom };
    DrawTextAt(mem, big, &tr, g_fontBig, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int detailLeft = inner.left + S(138);
    RECT cr = { detailLeft, inner.top + S(1), inner.right, inner.top + S(27) };
    DrawTextAt(mem, WxCodeText(g_fc.wcode), &cr, g_fontTitle, COL_TXT,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wchar_t line[192];
    _snwprintf(line, 191, L"Feels %d°  ·  Humidity %d%%  ·  Wind %d mph%s",
               (int)llround(g_fc.feelsF), (int)llround(g_fc.rh),
               (int)llround(g_fc.windMph), WxCodeIsWet(g_fc.wcode) ? L"  ·  raining now" : L"");
    line[191] = L'\0';
    RECT lr = { detailLeft, inner.top + S(27), inner.right, inner.top + S(48) };
    DrawTextAt(mem, line, &lr, g_fontUI, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wchar_t today[96];
    _snwprintf(today, 95, L"Today: high %d°  ·  low %d°",
               (int)llround(g_fc.dMaxF), (int)llround(g_fc.dMinF));
    today[95] = L'\0';
    RECT dr = { detailLeft, inner.top + S(49), inner.right, inner.bottom };
    DrawTextAt(mem, today, &dr, g_fontUI, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* The headline: the ensemble fraction, in words and as a bar, next to the
   coverage-blended figure so the two can be compared at a glance. */
static void PaintHonestCard(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT lbl = { card->left + S(18), card->top + S(10), card->right - S(18), card->top + S(30) };
    DrawTextAt(mem, L"Chance of Rain Here · Next 6 Hours", &lbl, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int start = g_fc.have ? g_fc.nowIdx : 0;
    double honest = EnsWindowProb(&g_ens, start, 6);
    double blended = g_fc.have ? PoPWindowMax(&g_fc, 0, 6) : NAN;

    RECT big = { card->left + S(18), card->top + S(28), card->left + S(150), card->top + S(78) };
    wchar_t pct[16];
    if (isnan(honest)) wcscpy(pct, L"--");
    else _snwprintf(pct, 15, L"%d%%", (int)llround(honest * 100.0));
    DrawTextAt(mem, pct, &big, g_fontBig, COL_RAIN, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* the bar: honest odds as a fill, blended PoP as a hollow tick */
    RECT bar = { card->left + S(160), card->top + S(44), card->right - S(24), card->top + S(62) };
    DrawRoundedBox(mem, &bar, S(9), COL_INPUT, COL_DIV);
    if (!isnan(honest)) {
        RECT fill = bar;
        int w = (int)((bar.right - bar.left) * honest);
        if (w > S(6)) fill.right = bar.left + w;
        else fill.right = bar.left + S(6);
        DrawRoundedBox(mem, &fill, S(9), COL_RAIN, COL_RAIN);
    }
    if (!isnan(blended)) {
        int bx = bar.left + (int)((bar.right - bar.left) * blended / 100.0);
        HPEN p = CreatePen(PS_SOLID, S(2), COL_WARN);
        HGDIOBJ op = SelectObject(mem, p);
        MoveToEx(mem, bx, bar.top - S(4), NULL);
        LineTo(mem, bx, bar.bottom + S(4));
        SelectObject(mem, op);
        DeleteObject(p);
    }

    RECT den = { card->left + S(18), card->top + S(80), card->right - S(18), card->top + S(100) };
    wchar_t dentxt[256];
    if (g_ens.have && !isnan(honest)) {
        int wet = (int)llround(honest * 100.0 + 0.0001);
        _snwprintf(dentxt, 255,
                   L"Ensemble: %d%% from %d %s members at this exact point.",
                   wet, g_ens.members, g_ens.model);
    } else {
        wcscpy(dentxt, L"The ensemble did not answer - no honest number beats a borrowed one.");
    }
    dentxt[255] = L'\0';
    DrawTextAt(mem, dentxt, &den, g_fontUI, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT cmp = { card->left + S(18), card->top + S(100), card->right - S(18), card->bottom - S(8) };
    wchar_t cmptxt[256];
    if (!isnan(blended))
        _snwprintf(cmptxt, 255,
                   L"Gold mark: %d%% coverage-blended forecast — shown for comparison, not as the headline.",
                   (int)llround(blended));
    else
        wcscpy(cmptxt, L"Gold marks show the coverage-blended forecast when it is available.");
    cmptxt[255] = L'\0';
    DrawTextAt(mem, cmptxt, &cmp, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void PaintHourlyCard(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT lbl = { card->left + S(18), card->top + S(8), card->right - S(18), card->top + S(26) };
    DrawTextAt(mem, L"Next 12 Hours", &lbl, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT key = { card->left + S(150), card->top + S(8), card->right - S(18), card->top + S(26) };
    DrawTextAt(mem, L"Blue = ensemble share   ·   Gold = coverage blend", &key,
               g_fontSmall, COL_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (!g_fc.have || g_fc.hN <= 0) {
        RECT w = { card->left + S(18), card->top + S(40), card->right - S(18), card->bottom - S(24) };
        DrawTextAt(mem, L"Waiting for the hourly forecast...", &w, g_fontUI, COL_MUTED,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    int n = g_fc.hN < HOURLY_SHOWN ? g_fc.hN : HOURLY_SHOWN;
    /* Reserve one clean line above for temperatures and one below for times.
       The old legend shared the time-label strip, which made both illegible. */
    RECT plot = { card->left + S(18), card->top + S(50), card->right - S(18), card->bottom - S(24) };
    int colW = (plot.right - plot.left) / n;
    int start = g_fc.nowIdx;
    for (int k = 0; k < n; k++) {
        int x0 = plot.left + k * colW + S(4);
        int x1 = plot.left + (k + 1) * colW - S(4);
        double honest = (start + k < g_ens.hours && g_ens.have) ? g_ens.prob[start + k] : NAN;
        double blended = g_fc.hPoP[k];
        int hgt = (plot.bottom - plot.top);
        if (!isnan(honest)) {
            int bh = (int)(hgt * honest);
            if (bh < S(2) && honest > 0.0) bh = S(2);
            RECT b = { x0, plot.bottom - bh, x1, plot.bottom };
            COLORREF c = MixCol(COL_INPUT, COL_RAIN, (int)(honest * 100));
            DrawRoundedBox(mem, &b, S(4), c, c);
        }
        if (!isnan(blended)) {
            int ty = plot.bottom - (int)(hgt * blended / 100.0);
            HPEN p = CreatePen(PS_SOLID, S(2), COL_WARN);
            HGDIOBJ op = SelectObject(mem, p);
            MoveToEx(mem, x0, ty, NULL);
            LineTo(mem, x1, ty);
            SelectObject(mem, op);
            DeleteObject(p);
        }
        RECT hr = { x0 - S(6), plot.bottom + S(4), x1 + S(6), card->bottom - S(4) };
        DrawTextAt(mem, g_fc.hLabel[k], &hr, g_fontSmall, COL_MUTED,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        wchar_t tmp[12];
        if (!isnan(g_fc.hT[k])) {
            _snwprintf(tmp, 11, L"%d°", (int)llround(g_fc.hT[k]));
            RECT tr2 = { x0 - S(6), card->top + S(30), x1 + S(6), plot.top - S(4) };
            DrawTextAt(mem, tmp, &tr2, g_fontSmall, COL_TXT,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void PaintRadarPane(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT img;
    RadarImageRect(&img);

    EnterCriticalSection(&g_lock);
    uint32_t *view = g_view;
    int ready = g_viewReady;
    int note = g_radarNote;
    int frames = g_frameN;
    int fsel = g_frameSel;
    long long ftime = (frames > 0 && fsel < frames) ? g_frames[fsel].time : 0;
    int fkind = (frames > 0 && fsel < frames) ? g_frames[fsel].kind : 0;
    Place pl = g_reqPlace;
    int hasPlace = g_reqHas;
    LeaveCriticalSection(&g_lock);

    if (view && ready) {
        /* fit the square grid into the pane, letterboxed */
        int side = img.bottom - img.top;
        int wide = img.right - img.left;
        if (side > wide) side = wide;
        int ox = img.left + (wide - side) / 2;
        RECT d = { ox, img.top, ox + side, img.bottom };
        SetStretchBltMode(mem, HALFTONE);
        SetBrushOrgEx(mem, 0, 0, NULL);
        BITMAPINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = GRID_PX;
        bi.bmiHeader.biHeight = -GRID_PX;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        EnterCriticalSection(&g_lock);
        StretchDIBits(mem, d.left, d.top, side, side, 0, 0, GRID_PX, GRID_PX,
                      g_view, &bi, DIB_RGB_COLORS, SRCCOPY);
        LeaveCriticalSection(&g_lock);
        /* crosshair on the exact place pixel */
        if (hasPlace) {
            int tx = 0, ty = 0;
            GridFor(g_baseKey[0] > 0 ? g_baseKey[0] : g_zoom, pl.lat, pl.lon, &tx, &ty);
            double px = 0, py = 0;
            PlacePixelInGrid(g_baseKey[0] > 0 ? g_baseKey[0] : g_zoom, pl.lat, pl.lon,
                             tx, ty, &px, &py);
            int cx = d.left + (int)(px / (double)GRID_PX * (double)side);
            int cy = d.top + (int)(py / (double)GRID_PX * (double)side);
            HPEN p = CreatePen(PS_SOLID, S(2), RGB(255, 255, 255));
            HGDIOBJ op = SelectObject(mem, p);
            MoveToEx(mem, cx - S(9), cy, NULL); LineTo(mem, cx - S(3), cy);
            MoveToEx(mem, cx + S(3), cy, NULL); LineTo(mem, cx + S(9), cy);
            MoveToEx(mem, cx, cy - S(9), NULL); LineTo(mem, cx, cy - S(3));
            MoveToEx(mem, cx, cy + S(3), NULL); LineTo(mem, cx, cy + S(9));
            SelectObject(mem, op);
            DeleteObject(p);
            Ellipse(mem, cx - S(3), cy - S(3), cx + S(3), cy + S(3));
        }
    } else {
        DrawRoundedBox(mem, &img, S(10), COL_INPUT, COL_DIV);
        wchar_t msg[96];
        if (!hasPlace) wcscpy(msg, L"Pick a place and the radar for it appears here.");
        else if (frames == 0) wcscpy(msg, L"No radar frames published right now.");
        else wcscpy(msg, L"Loading radar tiles...");
        DrawTextAt(mem, msg, &img, g_fontUI, COL_MUTED,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* frame stamp + observed/nowcast chip */
    if (frames > 0) {
        wchar_t ft[64];
        FrameTimeText(ftime, fkind, ft, 64);
        RECT tr = { img.left + S(8), img.top + S(6), img.left + S(330), img.top + S(28) };
        DrawRoundedBox(mem, &tr, S(8), COL_CARD, COL_CARD_BD);
        DrawTextAt(mem, ft, &tr, g_fontSmall, COL_TXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (ready == 1 && view) {
        RECT nr = { img.left + S(8), img.top + S(32), img.left + S(330), img.top + S(54) };
        DrawTextAt(mem, L"No rain echoes in this frame.", &nr, g_fontSmall, COL_MUTED,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (note) {
        RECT zr = { img.right - S(250), img.top + S(6), img.right - S(8), img.top + S(28) };
        DrawTextAt(mem, L"zoom capped by the tile host", &zr, g_fontSmall, COL_WARN,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    RECT at = { img.left, card->bottom - S(22), card->right - S(14), card->bottom - S(8) };
    DrawTextAt(mem, L"Radar: RainViewer  ·  Map © OpenStreetMap contributors", &at,
               g_fontSmall, COL_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void PaintLeftColumn(HDC mem) {
    RECT sr = { S(16), S(70), S(244), S(102) };
    DrawRoundedBox(mem, &sr, S(9), COL_INPUT, COL_DIV);

    DrawBtn(mem, BTN_SEARCH, L"Search", 1);
    DrawBtn(mem, BTN_KEEP, L"Keep", 0);
    DrawBtn(mem, BTN_DROP, L"Drop", 0);

    if (g_searchN > 0) {
        RECT lbl = { S(16), S(148), S(244), S(168) };
        DrawTextAt(mem, L"Matches — choose one", &lbl, g_fontSmall, COL_MUTED,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        for (int i = 0; i < g_searchN; i++) {
            RECT r;
            SearchRowRect(i, &r);
            COLORREF bg = (i == g_searchSel) ? COL_INPUT : COL_CARD;
            DrawRoundedBox(mem, &r, S(8), bg, (i == g_searchSel) ? COL_ACCENT : COL_CARD_BD);
            RECT t1 = { r.left + S(8), r.top, r.right - S(8), r.top + S(15) };
            DrawTextAt(mem, g_search[i].name, &t1, g_fontSmall, COL_TXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT t2 = { r.left + S(8), r.top + S(14), r.right - S(8), r.bottom - S(1) };
            DrawTextAt(mem, g_search[i].region, &t2, g_fontSmall, COL_MUTED,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    int savedY = SavedHeadingY();
    RECT sl = { S(16), savedY, S(244), savedY + S(20) };
    DrawTextAt(mem, L"Saved Places", &sl, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_placeN == 0) {
        RECT e = { S(16), savedY + S(22), S(244), savedY + S(64) };
        DrawTextAt(mem, L"No saved places yet — search, choose a match, then Keep.",
                   &e, g_fontSmall, COL_MUTED, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    for (int i = 0; i < g_placeN; i++) {
        RECT r;
        SavedRowRect(i, &r);
        int sel = (i == g_placeSel);
        DrawRoundedBox(mem, &r, S(8), sel ? COL_INPUT : COL_CARD,
                       sel ? COL_ACCENT : COL_CARD_BD);
        RECT t1 = { r.left + S(8), r.top, r.right - S(8), r.bottom };
        wchar_t line[160];
        _snwprintf(line, 159, L"%s  %.2f, %.2f", g_places[i].name, g_places[i].lat, g_places[i].lon);
        line[159] = L'\0';
        DrawTextAt(mem, line, &t1, g_fontSmall, sel ? COL_TXT : COL_MUTED,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void PaintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT cr;
    GetClientRect(hwnd, &cr);
    int W = cr.right, H = cr.bottom;

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    uint32_t *bits = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = NULL;
    if (dib && bits) old = SelectObject(mem, dib);

    RECT full = { 0, 0, W, H };
    FillRect(mem, &full, g_bgBrush);

    /* header */
    HICON ico = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCE(1), IMAGE_ICON, S(32), S(32),
                                  LR_DEFAULTCOLOR);
    if (ico) { DrawIconEx(mem, S(16), S(14), ico, S(32), S(32), 0, NULL, DI_NORMAL); DestroyIcon(ico); }
    RECT tt = { S(56), S(12), S(560), S(38) };
    DrawTextAt(mem, APP_TITLE_W, &tt, g_fontTitle, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT stt = { S(56), S(36), S(560), S(56) };
    DrawTextAt(mem, L"Honest rain odds from the ensemble, plus live radar", &stt,
               g_fontSmall, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawBtn(mem, BTN_REFRESH, L"Refresh", 0);
    if (g_placeSel >= 0 && g_placeSel < g_placeN) {
        RECT pn = { S(580), S(16), S(WIN_LW - 16 - 112), S(48) };
        DrawTextAt(mem, g_places[g_placeSel].name, &pn, g_fontTitle, COL_ACCENT,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    PaintLeftColumn(mem);

    RECT nowCard = { S(260), S(70), S(WIN_LW - 16), S(166) };
    PaintNowCard(mem, &nowCard);
    RECT honestCard = { S(260), S(174), S(WIN_LW - 16), S(300) };
    PaintHonestCard(mem, &honestCard);
    RECT hourCard = { S(260), S(308), S(WIN_LW - 16), S(436) };
    PaintHourlyCard(mem, &hourCard);
    RECT radarCard = { S(260), S(444), S(WIN_LW - 16), S(WIN_LH - 56) };
    /* Paint the card first: it is opaque, so controls must be drawn over it. */
    PaintRadarPane(mem, &radarCard);
    DrawBtn(mem, BTN_PREV, L"<", 0);
    DrawBtn(mem, BTN_PLAY, g_playing ? L"Pause" : L"Play", 0);
    DrawBtn(mem, BTN_NEXT, L">", 0);
    wchar_t zl[24];
    _snwprintf(zl, 23, L"zoom %d", g_zoom);
    DrawBtn(mem, BTN_ZOOM, zl, 0);

    /* status strip */
    RECT ss = { S(16), S(WIN_LH - 44), S(WIN_LW - 16), S(WIN_LH - 16) };
    COLORREF scol = g_statusKind == 2 ? COL_WARN : (g_statusKind == 1 ? COL_GOOD : COL_MUTED);
    DrawTextAt(mem, g_status, &ss, g_fontUI, scol,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (bits) GdiFlush();
    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    if (dib) { SelectObject(mem, old); DeleteObject(dib); }
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void StatusSet(const wchar_t *txt, int kind) {
    wcsncpy(g_status, txt, 319);
    g_status[319] = L'\0';
    g_statusKind = kind;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static const Place *CurrentPlace(void) {
    return (g_placeSel >= 0 && g_placeSel < g_placeN) ? &g_places[g_placeSel] : NULL;
}

static void OnPickSearchRow(int i) {
    if (i < 0 || i >= g_searchN) return;
    g_searchSel = i;
    RememberPlace(&g_search[i]);
    wchar_t msg[160];
    _snwprintf(msg, 159, L"%s - fetching forecast, ensemble and radar...", g_search[i].name);
    msg[159] = L'\0';
    StatusSet(msg, 0);
    RequestPlaceData(&g_places[g_placeSel]);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void OnPickSavedRow(int i) {
    if (i < 0 || i >= g_placeN) return;
    if (i == g_placeSel) return;
    g_placeSel = i;
    SavePlaces();
    wchar_t msg[160];
    _snwprintf(msg, 159, L"%s - fetching forecast, ensemble and radar...", g_places[i].name);
    msg[159] = L'\0';
    StatusSet(msg, 0);
    RequestPlaceData(&g_places[i]);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void StepFrame(int delta) {
    EnterCriticalSection(&g_lock);
    if (g_frameN > 0) {
        g_frameSel += delta;
        if (g_frameSel < 0) g_frameSel = g_frameN - 1;
        if (g_frameSel >= g_frameN) g_frameSel = 0;
    }
    LeaveCriticalSection(&g_lock);
    RequestTiles();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static int SearchTextIsBlank(const wchar_t *text) {
    while (*text == L' ' || *text == L'\t' || *text == L'\r' || *text == L'\n') text++;
    return *text == L'\0';
}

static void SubmitSearch(void) {
    if (!g_hSearch || g_searchHint) {
        StatusSet(L"Type a city or postcode first.", 2);
        return;
    }
    GetWindowTextW(g_hSearch, g_query, 96);
    if (SearchTextIsBlank(g_query)) {
        StatusSet(L"Type a city or postcode first.", 2);
        return;
    }
    /* Clear the old choices before the new worker response is ready, so a
       click can never select a result for the previous query. */
    g_searchN = 0;
    g_searchSel = -1;
    StatusSet(L"Searching...", 0);
    RequestSearch(g_query);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hSearch = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 1, 1, hwnd, (HMENU)(intptr_t)ID_EDIT_SEARCH, g_hInst, NULL);
        SetWindowTextW(g_hSearch, HINT_SEARCH);
        SetWindowSubclass(g_hSearch, SearchEditSubclass, 1, 0);
        LayoutSearchEdit();
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC h = (HDC)wp;
        SetTextColor(h, g_searchHint ? COL_MUTED : COL_TXT);
        SetBkColor(h, COL_INPUT);
        return (LRESULT)(g_inputBrush ? g_inputBrush : g_bgBrush);
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == ID_EDIT_SEARCH) {
            if (code == EN_SETFOCUS && g_searchHint) {
                g_searchHint = 0;
                SetWindowTextW(g_hSearch, L"");
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (code == EN_KILLFOCUS && GetWindowTextLengthW(g_hSearch) <= 0) {
                g_searchHint = 1;
                SetWindowTextW(g_hSearch, HINT_SEARCH);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (code == EN_CHANGE && !g_searchHint) {
                GetWindowTextW(g_hSearch, g_query, 95);
            }
        }
        return 0;
    }
    case WM_APP_SEARCH:
        SubmitSearch();
        return 0;
    case WM_SIZE:
        /* A child EDIT is not repainted by the parent's DIB. Re-apply its
           final scaled rectangle once Windows has sized the top-level window. */
        if (wp != SIZE_MINIMIZED) LayoutSearchEdit();
        return 0;
    case WM_PAINT:
        PaintAll(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b != g_hoverBtn) {
            g_hoverBtn = b;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int b = HitBtn(x, y);
        if (b >= 0) {
            switch (b) {
            case BTN_SEARCH:
                SubmitSearch();
                break;
            case BTN_REFRESH: {
                const Place *p = CurrentPlace();
                if (p) {
                    StatusSet(L"Refreshing forecast and radar...", 0);
                    RequestPlaceData(p);
                } else {
                    StatusSet(L"Pick a place first - search on the left.", 2);
                }
                break;
            }
            case BTN_PLAY:
                g_playing = !g_playing;
                if (g_playing) SetTimer(hwnd, TIMER_ANIM, 600, NULL);
                else KillTimer(hwnd, TIMER_ANIM);
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            case BTN_PREV:  StepFrame(-1); break;
            case BTN_NEXT:  StepFrame(1); break;
            case BTN_ZOOM:
                EnterCriticalSection(&g_lock);
                g_zoom = (g_zoom == 8) ? 7 : 8;
                LeaveCriticalSection(&g_lock);
                RequestTiles();
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            case BTN_KEEP: {
                const Place *p = CurrentPlace();
                if (p) {
                    wchar_t m[160];
                    _snwprintf(m, 159, L"%s is in your saved places.", p->name);
                    StatusSet(m, 1);
                }
                break;
            }
            case BTN_DROP:
                if (g_placeSel >= 0) {
                    wchar_t m[160];
                    _snwprintf(m, 159, L"Removed %s.", g_places[g_placeSel].name);
                    DropSelectedPlace();
                    StatusSet(m, 1);
                    g_fc.have = 0;
                    g_ens.have = 0;
                    g_viewReady = 0;
                    const Place *np = CurrentPlace();
                    if (np) RequestPlaceData(np);
                }
                break;
            default: break;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        for (int i = 0; i < g_searchN; i++) {
            RECT r;
            SearchRowRect(i, &r);
            POINT pt = { x, y };
            if (PtInRect(&r, pt)) { OnPickSearchRow(i); return 0; }
        }
        for (int i = 0; i < g_placeN; i++) {
            RECT r;
            SavedRowRect(i, &r);
            POINT pt = { x, y };
            if (PtInRect(&r, pt)) { OnPickSavedRow(i); return 0; }
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == TIMER_EMBLEM) {
            /* no emblem here; the timer just retires: nothing to animate in
               the header, so this arm stays quiet on purpose */
            return 0;
        }
        if (wp == TIMER_ANIM) {
            if (g_playing) StepFrame(1);
            return 0;
        }
        if (wp == TIMER_REFRESH) {
            const Place *p = CurrentPlace();
            if (p) {
                RequestPlaceData(p);
                StatusSet(L"Auto-refreshed the forecast.", 1);
            }
            return 0;
        }
        return 0;
    case WM_APP_DATA: {
        int kind = (int)wp;
        EnterCriticalSection(&g_lock);
        if (kind == JOB_FORECAST) {
            g_fc = g_fcStage;
            wchar_t m[160];
            _snwprintf(m, 159, L"Forecast for %s updated at %s.",
                       (g_placeSel >= 0 && g_placeSel < g_placeN) ? g_places[g_placeSel].name : L"the place",
                       g_fc.fetched);
            m[159] = L'\0';
            LeaveCriticalSection(&g_lock);
            StatusSet(m, 1);
        } else if (kind == JOB_ENSEMBLE) {
            g_ens = g_ensStage;
            LeaveCriticalSection(&g_lock);
        } else if (kind == JOB_GEOCODE) {
            int n = 0;
            int current = (g_searchStageTicket == g_searchTicket);
            if (current) {
                g_searchN = g_searchStageN;
                memcpy(g_search, g_searchStage, sizeof(Place) * (size_t)g_searchN);
                g_searchSel = -1;
                n = g_searchN;
            }
            LeaveCriticalSection(&g_lock);
            if (!current) return 0;
            wchar_t m[96];
            if (n > 0) {
                _snwprintf(m, 95, L"%d match%s - click the one you mean.", n, n == 1 ? L"" : L"es");
                StatusSet(m, 1);
            }
        } else if (kind == JOB_RADAR_CATALOG) {
            LeaveCriticalSection(&g_lock);
            RequestTiles();
        } else {
            LeaveCriticalSection(&g_lock);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_APP_STATUS: {
        wchar_t *txt = (wchar_t *)(intptr_t)lp;
        if (txt) {
            StatusSet(txt, (int)wp);
            free(txt);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        g_dpi = GetWinDpi(hwnd);
        if (g_fontUI) DeleteObject(g_fontUI);
        if (g_fontTitle) DeleteObject(g_fontTitle);
        if (g_fontBig) DeleteObject(g_fontBig);
        if (g_fontBtn) DeleteObject(g_fontBtn);
        if (g_fontSmall) DeleteObject(g_fontSmall);
        MakeFonts();
        RECT *pr = (RECT *)lp;
        SetWindowPos(hwnd, NULL, pr->left, pr->top, pr->right - pr->left,
                     pr->bottom - pr->top, SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutSearchEdit();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ANIM);
        KillTimer(hwnd, TIMER_REFRESH);
        KillTimer(hwnd, TIMER_EMBLEM);
        if (g_inputBrush) { DeleteObject(g_inputBrush); g_inputBrush = NULL; }
        if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
