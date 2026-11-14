/* ---- UI ---- */
enum { BTN_TEST = 0, BTN_APPLY, BTN_SPEED, BTN_AUTO, BTN_INTERVAL, BTN_ADMIN, BTN_COUNT };

/* Spinning Tri-Spoke Core Emblem - analytic sub-pixel SDF renderer,
   identical to Project Hub (ported 1:1 from tools/generate_suite_icons.py).
   Drawn straight at cache resolution so it is razor sharp at any DPI. */
#define EMB_SDF_SCALE 0.92
#define EMB_SDF_BOOST 0.012

static RECT g_rcEmblem;

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
    /* One full turn every 30 seconds: slow enough to read as alive. */
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

/* GDI blit wrapper: rasterize the emblem over the background colour and
   hand the finished square to SetDIBitsToDevice. */
static void PaintEmblem(HDC dc, int x, int y, int sz) {
    if (sz < 8 || sz > 512) return;
    uint32_t *px = (uint32_t *)malloc((size_t)sz * sz * sizeof(uint32_t));
    if (!px) return;
    /* Fill the square in the hub's canvas pixel format (PackRgb in the hub:
       R<<16 | G<<8 | B, the 32bpp DIB order). A raw COLORREF fill blits
       with red and blue swapped and shows as a box around the emblem -
       the old demarcation-square issue the hub already solved. */
    uint32_t bgP = ((uint32_t)GetRValue(COL_BG) << 16) |
                   ((uint32_t)GetGValue(COL_BG) << 8) |
                   (uint32_t)GetBValue(COL_BG);
    for (int i = 0; i < sz * sz; i++) px[i] = bgP;
    DrawWayfinderEmblem(px, sz, sz, 0, 0, sz);
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sz;
    bmi.bmiHeader.biHeight = -sz;   /* top-down rows */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, x, y, sz, sz, 0, 0, 0, sz, px, &bmi, DIB_RGB_COLORS);
    free(px);
}

static void BtnRect(int i, RECT *r) {
    int x = S(20), y = S(96), h = S(36);
    int w[BTN_COUNT] = { S(140), S(150), S(130), S(140), S(150), S(170) };
    for (int k = 0; k < i; k++) x += w[k] + S(10);
    SetRect(r, x, y, x + w[i], y + h);
}

static void FillSolid(HDC dc, const RECT *r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary, int amber) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : (amber ? RGB(84, 70, 38) : COL_CARD);
    COLORREF edge = primary ? COL_ACCENT : (amber ? COL_WARN : COL_DIV);
    if (g_hoverBtn == idx) edge = COL_ACCENT;
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, S(10), S(10));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, amber ? COL_WARN : COL_TXT);
    SelectObject(dc, g_fontBtn);
    DrawTextW(dc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void ShowTrayContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open DNS Pilot");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TEST, L"Run Test & Rank Now");
    AppendMenuW(hMenu, MF_STRING | (g_auto ? MF_CHECKED : 0), IDM_TRAY_AUTO,
                L"Auto-Pilot Enabled");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit DNS Pilot");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static void RowRect(int vis, RECT *r) {
    SetRect(r, S(20), S(166) + vis * S(34), S(960), S(166) + (vis + 1) * S(34));
}

/* Static-frame cache for the spin fast-path: the 50 ms emblem timer only
   blits this snapshot plus the spinning square instead of re-rendering the
   whole window (text, rows, stats) 20 times a second. */
static HBITMAP g_frameCache = NULL;
static int    g_fcW = 0, g_fcH = 0;

static void PaintAll(HWND hwnd) {
    (void)hwnd;
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    /* Spin fast-path: an emblem-only update rect plus a cached frame means
       we blit the snapshot and repaint just the spinning square. */
    RECT ur = ps.rcPaint;
    if (g_frameCache && g_fcW == W && g_fcH == H &&
        ur.right > ur.left && ur.bottom > ur.top &&
        g_rcEmblem.right > g_rcEmblem.left &&
        ur.left >= g_rcEmblem.left && ur.top >= g_rcEmblem.top &&
        ur.right <= g_rcEmblem.right && ur.bottom <= g_rcEmblem.bottom) {
        HDC cacheDc = CreateCompatibleDC(dc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(dc, 0, 0, W, H, cacheDc, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
        PaintEmblem(dc, g_rcEmblem.left, g_rcEmblem.top,
                    g_rcEmblem.right - g_rcEmblem.left);
        EndPaint(hwnd, &ps);
        return;
    }

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    RECT all = rc;
    FillSolid(mem, &all, COL_BG);

    SetBkMode(mem, TRANSPARENT);

    /* Spinning Tri-Spoke Core emblem - identical renderer to Project Hub */
    int embSz = S(52);
    SetRect(&g_rcEmblem, S(28), S(12), S(28) + embSz, S(12) + embSz);
    PaintEmblem(mem, g_rcEmblem.left, g_rcEmblem.top, embSz);

    SetTextColor(mem, COL_TXT);
    SelectObject(mem, g_fontTitle);
    RECT tr;
    SetRect(&tr, S(94), S(12), S(600), S(42));
    DrawTextW(mem, L"DNS Pilot", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTE);
    wchar_t sub[256];
    _snwprintf(sub, 255, L"Benchmarks real DNS queries and routes Windows through the fastest  |  %s  |  ifIndex %d  |  %s",
               g_adName, g_ifIndex, g_elevated ? L"Administrator" : L"not elevated");
    sub[255] = L'\0';
    SetRect(&tr, S(94), S(42), W - S(20), S(64));
    DrawTextW(mem, sub, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    /* live stats strip: GW/DNS ping + passive download/upload traffic */
    RECT sr;
    SetRect(&sr, S(94), S(62), W - S(20), S(88));
    wchar_t statsSnap[160];
    EnterCriticalSection(&g_statsCs);
    wcsncpy(statsSnap, g_statsLine, 159);
    LeaveCriticalSection(&g_statsCs);
    statsSnap[159] = L'\0';
    SetTextColor(mem, COL_TXT);
    DrawTextW(mem, statsSnap, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    wchar_t autoLabel[48];
    wchar_t intLabel[48];
    _snwprintf(autoLabel, 47, L"Auto-Pilot: %s", g_auto ? L"ON" : L"OFF");
    autoLabel[47] = L'\0';
    if (g_intervalMin == 0) WCpy(intLabel, 48, L"Interval: Auto");
    else _snwprintf(intLabel, 47, L"Every %d min", g_intervalMin);
    intLabel[47] = L'\0';
    DrawBtn(mem, BTN_TEST, L"Test & Rank", 1, 0);
    DrawBtn(mem, BTN_APPLY, L"Apply Fastest", 0, 0);
    DrawBtn(mem, BTN_SPEED, g_speedBusy ? L"Testing Speed..." : L"Speed Test", 0, g_speedBusy ? 1 : 0);
    DrawBtn(mem, BTN_AUTO, autoLabel, 0, g_auto);
    DrawBtn(mem, BTN_INTERVAL, intLabel, 0, 0);
    DrawBtn(mem, BTN_ADMIN, g_elevated ? L"Running as Admin" : L"Restart as Admin", 0, 0);

    /* table header - columns use the full window width and never overlap */
    int cwid[6] = { S(36), S(190), S(240), S(62), S(112), S(236) };
    int cx[6] = { S(32), S(72), S(272), S(522), S(592), S(712) };
    SetTextColor(mem, COL_MUTE);
    SelectObject(mem, g_fontUI);
    {
        const wchar_t *heads[6] = { L"#", L"Resolver", L"Server IP", L"Family", L"Median", L"Samples" };
        for (int i = 0; i < 6; i++) {
            RECT hr;
            SetRect(&hr, cx[i], S(142), cx[i] + cwid[i], S(166));
            DrawTextW(mem, heads[i], -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    int tableH = H - S(166) - S(52);
    int visRows = tableH / S(34);
    if (visRows < 1) visRows = 1;
    if (g_topRow > g_rowCount - visRows) g_topRow = g_rowCount - visRows;
    if (g_topRow < 0) g_topRow = 0;

    for (int v = 0; v < visRows; v++) {
        int idx = g_topRow + v;
        if (idx >= g_rowCount) break;
        Row *r = &g_rows[idx];
        RECT rr;
        RowRect(v, &rr);
        FillSolid(mem, &rr, (idx & 1) ? COL_ROW2 : COL_CARD);
        if (r->rank == 1) {
            RECT hl = rr;
            HPEN hp = CreatePen(PS_SOLID, 1, COL_GOLD);
            HGDIOBJ op = SelectObject(mem, hp);
            HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
            RoundRect(mem, rr.left + 1, rr.top + 1, rr.right - 1, rr.bottom - 1, S(8), S(8));
            SelectObject(mem, op);
            SelectObject(mem, ob);
            DeleteObject(hp);
            (void)hl;
        }
        wchar_t wname[64], wip[64], wfam[8], wmed[32], wsmp[48];
        Widen(SERVERS[r->srv].name, wname, 64);
        Widen(r->ip, wip, 64);
        _snwprintf(wfam, 7, L"IPv%d", r->family);
        wfam[7] = L'\0';
        if (!r->tested) {
            WCpy(wmed, 32, L"-");
            WCpy(wsmp, 48, L"not tested");
        } else if (!r->ok) {
            WCpy(wmed, 32, L"-");
            WCpy(wsmp, 48, L"timeout / no route");
        } else {
            if (r->usedTcp) _snwprintf(wmed, 31, L"%d ms (TCP)", r->med);
            else _snwprintf(wmed, 31, L"%d ms", r->med);
            wmed[31] = L'\0';
            wchar_t s1[8], s2[8], s3[8];
            if (r->ms[0] >= 0) _snwprintf(s1, 7, L"%d", r->ms[0]); else wcscpy_s(s1, 8, L"-");
            if (r->ms[1] >= 0) _snwprintf(s2, 7, L"%d", r->ms[1]); else wcscpy_s(s2, 8, L"-");
            if (r->ms[2] >= 0) _snwprintf(s3, 7, L"%d", r->ms[2]); else wcscpy_s(s3, 8, L"-");
            _snwprintf(wsmp, 47, L"%s / %s / %s ms", s1, s2, s3);
            wsmp[47] = L'\0';
        }
        wchar_t *cols[6];
        wchar_t wrank[8];
        if (r->tested && r->ok && r->rank > 0) _snwprintf(wrank, 7, L"%d", r->rank);
        else WCpy(wrank, 8, L"");
        cols[0] = wrank; cols[1] = wname; cols[2] = wip; cols[3] = wfam; cols[4] = wmed; cols[5] = wsmp;
        for (int c = 0; c < 6; c++) {
            RECT cr;
            SetRect(&cr, cx[c], rr.top, cx[c] + cwid[c], rr.bottom);
            if (c == 0 && r->rank == 1) SetTextColor(mem, COL_GOLD);
            else if (c == 4 && r->tested && r->ok) SetTextColor(mem, r->rank == 1 ? COL_GOLD : COL_TXT);
            else if (c == 5 && r->tested && !r->ok) SetTextColor(mem, COL_BAD);
            else SetTextColor(mem, c == 1 ? COL_TXT : COL_MUTE);
            if (c == 1 && r->isBackup) {
                wchar_t wnb[80];
                _snwprintf(wnb, 79, L"%s (backup)", wname);
                wnb[79] = L'\0';
                DrawTextW(mem, wnb, -1, &cr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else {
                DrawTextW(mem, cols[c], -1, &cr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
    }

    /* footer */
    RECT fr;
    SetRect(&fr, 0, H - S(52), W, H);
    FillSolid(mem, &fr, COL_CARD);
    SetRect(&fr, S(20), H - S(52), W - S(20), H - S(10));
    SetTextColor(mem, COL_TXT);
    DrawTextW(mem, g_status, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    /* snapshot the composed frame for the spin fast-path */
    if (!g_frameCache || g_fcW != W || g_fcH != H) {
        if (g_frameCache) DeleteObject(g_frameCache);
        g_frameCache = CreateCompatibleBitmap(dc, W, H);
        g_fcW = W;
        g_fcH = H;
    }
    if (g_frameCache) {
        HDC cacheDc = CreateCompatibleDC(dc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(cacheDc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
    }

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static int HitBtn(int x, int y) {
    POINT pt;
    RECT r;
    pt.x = x; pt.y = y;
    for (int i = 0; i < BTN_COUNT; i++) {
        BtnRect(i, &r);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void UpdateStatusAfterTest(void) {
    if (g_best4 >= 0 && g_best6 >= 0) {
        wchar_t n4[64], n6[64], ip4[64], ip6[64];
        Widen(SERVERS[g_rows[g_best4].srv].name, n4, 64);
        Widen(SERVERS[g_rows[g_best6].srv].name, n6, 64);
        Widen(g_rows[g_best4].ip, ip4, 64);
        Widen(g_rows[g_best6].ip, ip6, 64);
        _snwprintf(g_status, 511, L"Fastest IPv4: %s %s (%d ms)  |  Fastest IPv6: %s %s (%d ms)  |  press Apply Fastest",
                   n4, ip4, g_rows[g_best4].med, n6, ip6, g_rows[g_best6].med);
    } else if (g_best4 >= 0) {
        wchar_t n4[64], ip4[64];
        Widen(SERVERS[g_rows[g_best4].srv].name, n4, 64);
        Widen(g_rows[g_best4].ip, ip4, 64);
        _snwprintf(g_status, 511, L"Fastest IPv4: %s %s (%d ms)  |  no working IPv6 resolver  |  press Apply Fastest",
                   n4, ip4, g_rows[g_best4].med);
    } else {
        wcsncpy(g_status, L"No resolver answered over UDP or TCP port 53 - this network may block outside DNS.", 511);
    }
    {
        int reach = 0, v6n = 0, v6ok = 0;
        wchar_t tail[160];
        for (int i = 0; i < g_rowCount; i++) {
            Row *r = &g_rows[i];
            if (r->tested && r->ok) reach++;
            if (r->family == 6) { v6n++; if (r->tested && r->ok) v6ok++; }
        }
        _snwprintf(tail, 79, L"  |  %d/%d endpoints reachable", reach, g_rowCount);
        tail[79] = L'\0';
        wcsncat(g_status, tail, 511 - wcslen(g_status));
        /* Say WHY the IPv6 rows failed instead of leaving mystery timeouts. */
        if (v6n > 0 && v6ok == 0) {
            wcsncat(g_status,
                g_hasGlobalV6
                    ? L"  |  IPv6: this PC has a v6 address but no v6 DNS answered - the network filters or does not route IPv6 DNS (common on school/filtered Wi-Fi)"
                    : L"  |  IPv6: no global IPv6 address on this network - v6 rows were skipped",
                511 - wcslen(g_status));
        } else if (v6n > 0 && v6ok < v6n) {
            wchar_t d[112];
            _snwprintf(d, 111,
                       L"  |  IPv6 partial: %d/%d answered - far v6 resolvers are often unreachable on half-working IPv6",
                       v6ok, v6n);
            d[111] = L'\0';
            wcsncat(g_status, d, 511 - wcslen(g_status));
        }
    }
    g_status[511] = L'\0';
}

static void DoApplyFastest(int announce) {
    if (!g_elevated) {
        wchar_t args[384], wv[5][128];
        const char *pick[5];
        if (g_best4 < 0 && g_best6 < 0) {
            wcsncpy(g_status, L"Run Test & Rank first - nothing to apply.", 511);
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
        pick[0] = g_best4 >= 0 ? g_rows[g_best4].ip : "-";
        pick[1] = (g_best4 >= 0 && SERVERS[g_rows[g_best4].srv].v4b[0]) ? SERVERS[g_rows[g_best4].srv].v4b : "-";
        pick[2] = g_best6 >= 0 ? g_rows[g_best6].ip : "-";
        pick[3] = (g_best6 >= 0 && SERVERS[g_rows[g_best6].srv].v6b[0]) ? SERVERS[g_rows[g_best6].srv].v6b : "-";
        for (int i = 0; i < 4; i++) Widen(pick[i], wv[i], 128);
        _snwprintf(args, 383, L"-ApplyNow %d %s %s %s %s", g_ifIndex, wv[0], wv[1], wv[2], wv[3]);
        args[383] = L'\0';
        RelaunchElevated(args);
        wcsncpy(g_status, L"Approving via the UAC prompt - DNS changes need administrator rights.", 511);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    const char *v4a = g_best4 >= 0 ? g_rows[g_best4].ip : "";
    const char *v4b = (g_best4 >= 0 && SERVERS[g_rows[g_best4].srv].v4b[0]) ? SERVERS[g_rows[g_best4].srv].v4b : "";
    const char *v6a = g_best6 >= 0 ? g_rows[g_best6].ip : "";
    const char *v6b = (g_best6 >= 0 && SERVERS[g_rows[g_best6].srv].v6b[0]) ? SERVERS[g_rows[g_best6].srv].v6b : "";
    ApplyDns((int)g_ifIndex, v4a, v4b, v6a, v6b);
    SaveConfig();
    if (announce) {
        wchar_t n4[64];
        Widen(g_best4 >= 0 ? SERVERS[g_rows[g_best4].srv].name : "", n4, 64);
        _snwprintf(g_status, 511, L"Applied: IPv4 via %s  |  IPv6 via %s  |  DNS cache flushed",
                   g_best4 >= 0 ? n4 : L"(unchanged)",
                   g_best6 >= 0 ? L"tested winner" : L"(unchanged)");
        g_status[511] = L'\0';
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
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
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b == BTN_TEST) {
            StartTest(0);
            wcsncpy(g_status, L"Testing - real DNS queries to every listed resolver...", 511);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_APPLY) {
            DoApplyFastest(1);
        } else if (b == BTN_SPEED) {
            StartSpeedTest();
        } else if (b == BTN_AUTO) {
            g_auto = !g_auto;
            SaveConfig();
            if (g_auto && !g_elevated) {
                wcsncpy(g_status, L"Auto-Pilot needs administrator rights - click Restart as Admin.", 511);
            } else if (g_auto && g_intervalMin == 0) {
                if (g_lastTestAt == 0 && !g_testing) StartTest(1);
                else RebuildWatch();
                WCpy(g_status, 512,
                     L"Auto-Pilot enabled (live watch) - it switches the instant a rival is clearly "
                     L"and consistently faster. Switching mid-game is safe - existing "
                     L"connections are never touched.");
            } else {
                _snwprintf(g_status, 511, L"Auto-Pilot %s - re-tests every %d min and only switches when clearly faster.",
                           g_auto ? L"enabled" : L"disabled", g_intervalMin);
            }
            g_status[511] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_INTERVAL) {
            int steps[6] = { 0, 15, 30, 60, 120, 300 };
            int cur = 2;
            for (int i = 0; i < 6; i++) if (steps[i] == g_intervalMin) cur = i;
            g_intervalMin = steps[(cur + 1) % 6];
            SaveConfig();
            if (g_intervalMin == 0) {
                WCpy(g_status, 512,
                     L"Auto mode: live watch - switches only when a rival is 20%/20 ms faster, "
                     L"3 rounds in a row, 60 s settle gap, auto-revert if it dies.");
                RebuildWatch();
            } else {
                _snwprintf(g_status, 511, L"Auto-Pilot interval set to %d minutes.", g_intervalMin);
            }
            g_status[511] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_ADMIN) {
            if (!g_elevated) {
                RelaunchElevated(L"-TakeOver");
                DestroyWindow(hwnd);   /* the elevated copy takes over */
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        g_topRow -= (delta > 0) ? 3 : -3;
        if (g_topRow < 0) g_topRow = 0;
        if (g_topRow > g_rowCount - 1) g_topRow = g_rowCount - 1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_APP_DONE:
        UpdateStatusAfterTest();
        if (g_autoRun && g_intervalMin == 0) {
            RebuildWatch();
        } else if (g_autoRun) {
            int better = 0;
            if (g_best4 >= 0) {
                Row *b = &g_rows[g_best4];
                if (g_curV4[0] && strcmp(b->ip, g_curV4) != 0) {
                    better = 1;   /* unknown current latency: only switch if the current is a stale pick */
                } else if (!g_curV4[0]) {
                    better = 1;
                }
            }
            if (better) DoApplyFastest(0);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_TIMER:
        if (wp == 2) {
            InvalidateRect(hwnd, &g_rcEmblem, FALSE);
            return 0;
        }
        if (wp == 1 && g_auto && g_elevated && !g_testing) {
            ULONGLONG now = GetTickCount64();
            if (g_intervalMin == 0) {
                /* live watch on a worker thread: cheap probes every tick and a
                   full re-rank twice an hour - probe timeouts can never stall
                   the UI or the switch path */
                if (InterlockedCompareExchange(&g_watchBusy, 0, 0) == 0) {
                    InterlockedExchange(&g_watchBusy, 1);
                    HANDLE hW = CreateThread(NULL, 0, WatchThread, NULL, 0, NULL);
                    if (hW) CloseHandle(hW);
                }
                if (g_lastTestAt == 0 || now - g_lastTestAt >= AUTO_FULLTEST_MS) StartTest(1);
            } else {
                if (g_lastTestAt == 0 || now - g_lastTestAt >= (ULONGLONG)g_intervalMin * 60000ULL)
                    StartTest(1);
            }
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_TRAY_OPEN:
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            return 0;
        case IDM_TRAY_TEST:
            StartTest(0);
            return 0;
        case IDM_TRAY_AUTO:
            g_auto = !g_auto;
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case IDM_TRAY_QUIT:
            g_quitting = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowTrayContextMenu(hwnd);
        }
        return 0;
    case WM_CLOSE:
        /* Closing hides to the tray just like Keyboard Lighting: Auto-Pilot,
           the watch and the stats strip all keep running in the background. */
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            if (!g_trayHinted) {
                g_trayHinted = 1;
                g_nid.uFlags = NIF_INFO;
                wcsncpy(g_nid.szInfoTitle, L"DNS Pilot is running in the tray", 63);
                wcsncpy(g_nid.szInfo, L"Right-click the tray icon to run a test or quit.", 255);
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
                g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_statsRun = 0;
        InterlockedExchange(&g_exitStop, 1);
        /* Never free g_watchCs while a watch tick can still hold it - that
           race crashed on exit whenever Auto-Pilot was mid-tick. Park the
           thread first (bounded wait); if it is somehow still winding down we
           leave the section for the process teardown to reclaim. */
        for (int t = 0; t < 150 && InterlockedCompareExchange(&g_watchBusy, 0, 0); t++) Sleep(10);
        if (!InterlockedCompareExchange(&g_watchBusy, 0, 0))
            DeleteCriticalSection(&g_watchCs);
        PostQuitMessage(0);
        return 0;
    }
    if (msg == g_exitMsg) {
        g_quitting = 1;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev;
    (void)nShow;
    WSADATA wsa;
    g_exitMsg = RegisterWindowMessageW(L"KBLSuiteDnsPilot_Exit");

    /* --stop: ask a running pilot (even one hiding in the tray) to quit for
       real, so an updater can replace the exe. Same contract as BTG/KBL. */
    if (cmdLine && (wcsstr(cmdLine, L"--stop") != NULL || wcsstr(cmdLine, L"-Stop") != NULL)) {
        HWND other = FindWindowW(L"KBLSuiteDnsPilotWindow", NULL);
        if (other) {
            PostMessageW(other, g_exitMsg, 0, 0);
            for (int i = 0; i < 40; i++) {
                if (!FindWindowW(L"KBLSuiteDnsPilotWindow", NULL)) break;
                Sleep(50);
            }
        }
        return 0;
    }

    int skipSingle = (cmdLine && (wcsstr(cmdLine, L"-ApplyNow") != NULL ||
                                  wcsstr(cmdLine, L"-TakeOver") != NULL));
    if (!skipSingle) {
        HANDLE hMu = CreateMutexW(NULL, TRUE, SINGLE_MUTEX_NAME);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND other = FindWindowW(L"KBLSuiteDnsPilotWindow", NULL);
            if (other) {
                ShowWindow(other, SW_RESTORE);
                SetForegroundWindow(other);
            }
            if (hMu) CloseHandle(hMu);
            return 0;
        }
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBoxW(NULL, L"Winsock failed to start.", APP_TITLE_W, MB_ICONERROR);
        return 1;
    }

    g_elevated = IsElevated();
    LoadConfig();
    EnsureAutostart();
    BuildRows();
    FindActiveAdapter();

    /* Elevated one-shot apply from an unelevated session's Apply button. */
    if (cmdLine && wcsstr(cmdLine, L"-ApplyNow") != NULL) {
        int idx = 0;
        wchar_t args[5][128];
        int argc = 0;
        {
            wchar_t *p = cmdLine;
            while (*p && argc < 5) {
                while (*p == L' ') p++;
                if (!*p) break;
                wchar_t *q = p;
                while (*q && *q != L' ') q++;
                {
                    wchar_t save = *q;
                    *q = L'\0';
                    if (wcscmp(p, L"-ApplyNow") != 0) {
                        wcsncpy(args[argc], p, 127);
                        args[argc][127] = L'\0';
                        argc++;
                    }
                    *q = save;
                }
                p = q;
            }
        }
        if (argc >= 1) idx = _wtoi(args[0]);
        {
            char a[5][128];
            (void)argc;
            for (int i = 0; i < 5; i++) {
                if (i < argc && wcscmp(args[i], L"-") != 0)
                    WideCharToMultiByte(CP_UTF8, 0, args[i], -1, a[i], 128, NULL, NULL);
                else
                    a[i][0] = '\0';
            }
            ApplyDns(idx, a[1], a[2], a[3], a[4]);
        }
        return 0;
    }

    g_dpi = GetDeviceCaps(GetDC(NULL), LOGPIXELSX);
    {
        HDC sdc = GetDC(NULL);
        g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
        ReleaseDC(NULL, sdc);
    }
    g_fontUI = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(19), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontBtn = CreateFontW(-S(12), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    g_hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIcon = g_hIcon;
    wc.lpszClassName = L"KBLSuiteDnsPilotWindow";
    RegisterClassExW(&wc);

    RECT wrc = { 0, 0, S(980), S(780) };
    AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE_W,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             wrc.right - wrc.left, wrc.bottom - wrc.top,
                             NULL, NULL, hInst, NULL);

    /* dark title bar matching the background + smooth corners (dwmapi is
       loaded dynamically so the PE imports stay clean) */
    ApplyModernWindowFrame(g_hwnd);

    /* tray icon: closing the window hides to the tray like Keyboard Lighting */
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_hIcon;
    wcsncpy(g_nid.szTip, L"DNS Pilot - Auto-Pilot DNS switching", 127);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    /* -Silent: start hidden in the tray (the hub uses this to put an
       updated pilot back the way it was instead of popping the window) */
    BOOL silent = (cmdLine && (wcsstr(cmdLine, L"-Silent") != NULL ||
                               wcsstr(cmdLine, L"--silent") != NULL));
    if (!silent) { ShowWindow(g_hwnd, SW_SHOW); SetForegroundWindow(g_hwnd); }
    InitializeCriticalSection(&g_watchCs);
    SetTimer(g_hwnd, 1, 10000, NULL);
    SetTimer(g_hwnd, 2, 50, NULL);     /* emblem spin repaint */

    /* live stats strip: 1 Hz, one worker thread, ping + passive counters */
    InitializeCriticalSection(&g_statsCs);
    HANDLE hS = CreateThread(NULL, 0, StatsThread, NULL, 0, NULL);
    if (hS) CloseHandle(hS);

    /* every start kicks off a real test right away - including a quiet
       tray start, so an autostarted pilot benchmarks from the moment it
       appears instead of idling until someone opens the window */
    StartTest(0);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    WSACleanup();
    return 0;
}
