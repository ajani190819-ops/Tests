/* ---- Native-style brightness OSD (sun + bar + percent) -------------
   Bottom-centre popup built to look like the Windows OSD: neutral dark
   body, the REAL Windows brightness glyph (U+E706 from Segoe Fluent
   Icons / Segoe MDL2 Assets - the exact icon Windows draws), a thin
   rounded bar, and the live percent. Appears whenever keyboard
   brightness changes from outside the window and hides after ~1.4s. */
static HWND g_osdHwnd = NULL;
static int  g_osdPct = -1;

static void OsdDrawSunFallback(HDC mem, int cx, int cy, int r) {
    HPEN sunPen = CreatePen(PS_SOLID, r / 3 + 1, RGB(240, 240, 240));
    HBRUSH sunBr = CreateSolidBrush(RGB(240, 240, 240));
    HGDIOBJ op = SelectObject(mem, sunPen);
    HGDIOBJ ob = SelectObject(mem, sunBr);
    Ellipse(mem, cx - r, cy - r, cx + r, cy + r);
    for (int a = 0; a < 8; a++) {
        double th = a * 0.7853981633974483;
        int rayIn = r + r / 2, rayOut = r * 2 + r / 2;
        MoveToEx(mem, cx + (int)(rayIn * cos(th)), cy + (int)(rayIn * sin(th)), NULL);
        LineTo(mem, cx + (int)(rayOut * cos(th)), cy + (int)(rayOut * sin(th)));
    }
    SelectObject(mem, op);
    SelectObject(mem, ob);
    DeleteObject(sunPen);
    DeleteObject(sunBr);
}

static LRESULT CALLBACK OsdWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == 3) { KillTimer(hwnd, 3); ShowWindow(hwnd, SW_HIDE); }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int W = rc.right, H = rc.bottom;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ oldBm = SelectObject(mem, bmp);
        HBRUSH bg = CreateSolidBrush(RGB(44, 44, 44));
        HPEN bgPen = CreatePen(PS_SOLID, 1, RGB(44, 44, 44));
        HGDIOBJ oldBr = SelectObject(mem, bg);
        HGDIOBJ oldPen = SelectObject(mem, bgPen);
        RoundRect(mem, 0, 0, W, H, H / 5, H / 5);
        int pad = H * 32 / 100;
        int iconCx = pad + H * 22 / 100;
        /* The exact Windows brightness glyph (Segoe Fluent Icons E706,
           Segoe MDL2 Assets E706 on Win10). Same font, same icon. */
        int glyphOk = 0;
        HFONT f1 = CreateFontW(-H * 52 / 100, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                               0, 0, 0, 0, 0, L"Segoe Fluent Icons");
        HFONT f2 = CreateFontW(-H * 52 / 100, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                               0, 0, 0, 0, 0, L"Segoe MDL2 Assets");
        HFONT useF = f1;
        {
            HGDIOBJ of = SelectObject(mem, f1);
            WORD gi = 0xFFFF;
            if (GetGlyphIndicesW(mem, L"\xE706", 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && gi != 0xFFFF) {
                glyphOk = 1;
            } else {
                SelectObject(mem, f2);
                gi = 0xFFFF;
                if (GetGlyphIndicesW(mem, L"\xE706", 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && gi != 0xFFFF) {
                    glyphOk = 1;
                    useF = f2;
                }
            }
            SelectObject(mem, of);
        }
        if (glyphOk) {
            HGDIOBJ of = SelectObject(mem, useF);
            RECT ir;
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(240, 240, 240));
            SetRect(&ir, iconCx - H * 30 / 100, 0, iconCx + H * 30 / 100, H);
            DrawTextW(mem, L"\xE706", -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(mem, of);
        } else {
            OsdDrawSunFallback(mem, iconCx, H / 2, H * 11 / 100);
        }
        DeleteObject(f1);
        DeleteObject(f2);
        /* thin native-style bar + percent */
        int barH = H * 15 / 100;
        if (barH < 5) barH = 5;
        int barY = (H - barH) / 2;
        int barX = iconCx + H * 38 / 100;
        int pctW = H * 84 / 100;
        int barW = W - barX - pctW - pad / 2;
        if (barW < 40) barW = 40;
        HBRUSH trBr = CreateSolidBrush(RGB(90, 90, 90));
        HPEN trPen = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
        SelectObject(mem, trBr);
        SelectObject(mem, trPen);
        RoundRect(mem, barX, barY, barX + barW, barY + barH, barH, barH);
        int pct = g_osdPct;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        int fillW = (barW - 2) * pct / 100;
        if (fillW > 0 && pct > 0) {
            HBRUSH fBr = CreateSolidBrush(RGB(232, 226, 208));
            HPEN fPen = CreatePen(PS_SOLID, 1, RGB(232, 226, 208));
            SelectObject(mem, fBr);
            SelectObject(mem, fPen);
            RoundRect(mem, barX + 1, barY + 1, barX + 1 + fillW, barY + barH - 1, barH, barH);
            DeleteObject(fBr);
            DeleteObject(fPen);
        }
        {
            HFONT font = CreateFontW(-H * 24 / 100, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                     0, 0, 0, 0, 0, L"Segoe UI");
            HGDIOBJ of = SelectObject(mem, font);
            wchar_t t[16];
            _snwprintf(t, 15, L"%d%%", pct);
            t[15] = L'\0';
            RECT tr;
            SetRect(&tr, barX + barW + H * 2 / 100, 0, W - pad / 3, H);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(235, 235, 235));
            DrawTextW(mem, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(mem, of);
            DeleteObject(font);
        }
        BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBr);
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBm);
        DeleteObject(bg); DeleteObject(bgPen);
        DeleteObject(trBr); DeleteObject(trPen);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OsdCreate(HINSTANCE hInst) {
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OsdWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"KBLSuiteOsdWindow";
    RegisterClassExW(&wc);
    g_osdHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                wc.lpszClassName, L"", WS_POPUP,
                                0, 0, 10, 10, NULL, NULL, hInst, NULL);
    if (g_osdHwnd) SetLayeredWindowAttributes(g_osdHwnd, 0, 242, LWA_ALPHA);
}

static void OsdShow(int pct) {
    if (!g_osdHwnd) return;
    HDC sdc = GetDC(NULL);
    int dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(NULL, sdc);
    int W = MulDiv(320, dpi, 96), H = MulDiv(54, dpi, 96);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + ((wa.right - wa.left) - W) / 2;
    int y = wa.bottom - H - MulDiv(26, dpi, 96);
    g_osdPct = pct;
    SetWindowPos(g_osdHwnd, HWND_TOPMOST, x, y, W, H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_osdHwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(g_osdHwnd, NULL, FALSE);
    SetTimer(g_osdHwnd, 3, 1400, NULL);
}

static void PollLayoutTxt(void) {
    ULONGLONG now = GetTickCount64();
    if (now - g_layoutReadAt < 500ULL) return;
    g_layoutReadAt = now;
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_layoutPath, &len);
    if (txt) { ParseLayoutTxt(txt); free(txt); }
}

/* Read live lamp frame from Aura-Background.ps1 or synthesize accurate live palette preview */
static void PollLivePreviewFrame(void) {
    PollLayoutTxt();
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_framePath, &len);
    if (txt && len > 8) {
        char *semi = strchr(txt, ';');
        if (semi) {
            int n = atoi(txt);
            const char *hex = semi + 1;
            if (n >= 1 && n <= 64 && strlen(hex) >= (size_t)(n * 6)) {
                for (int i = 0; i < n; i++) {
                    char cbuf[8] = { '#', hex[i*6], hex[i*6+1], hex[i*6+2], hex[i*6+3], hex[i*6+4], hex[i*6+5], 0 };
                    g_liveCols[i] = ParseHexColor(cbuf, RGB(34, 39, 52));
                }
                g_liveKbdN = (n >= 4) ? 4 : n;
                g_liveBarN = (n > 4) ? (n - 4) : 0;
                g_hasLiveFrame = TRUE;
                g_lastFrameTick = GetTickCount64();
                free(txt);
                return;
            }
        }
    }
    free(txt);

    /* Smooth animated preview using exact active settings when off-hardware or starting */
    if (g_hasLiveFrame && GetTickCount64() - g_lastFrameTick < 2500ULL) return;
    g_hasLiveFrame = FALSE;
    g_liveKbdN = 4;
    g_liveBarN = 12;

    double phase = (double)(GetTickCount64() % 10000ULL) / 10000.0;
    for (int z = 0; z < 16; z++) {
        const GroupCfg *g = (z < 4) ? &g_cfg.kbd : &g_cfg.bar;
        double u = (z < 4) ? ((double)z / 4.0) : ((double)(z - 4) / 12.0);
        /* Follow the established ring order so the synthesised gradient
           also travels round the circle instead of jumping at the seam. */
        if (z >= 4) {
            for (int k = 0; k < g_barLayoutN; k++) {
                if (g_barOrder[k] == z) { u = g_barQ[k]; break; }
            }
        }
        if (g_wantOff || !g->on) {
            g_liveCols[z] = RGB(24, 28, 38);
            continue;
        }
        double spd = (double)g->speed / 10.0;
        double pos = fmod(u + phase * spd * (g->reverse ? -1.0 : 1.0) + 10.0, 1.0);
        COLORREF base;
        if (!kEffects[g->effectIdx].usesPalette) {
            double h6 = pos * 6.0;
            int hi = (int)floor(h6) % 6;
            double f = h6 - floor(h6);
            int q = (int)(255.0 * (1.0 - f)), t = (int)(255.0 * f);
            switch (hi) {
            case 0: base = RGB(255, t, 0); break;
            case 1: base = RGB(q, 255, 0); break;
            case 2: base = RGB(0, 255, t); break;
            case 3: base = RGB(0, q, 255); break;
            case 4: base = RGB(t, 0, 255); break;
            default: base = RGB(255, 0, q); break;
            }
        } else {
            int n = g->swatchCount > 0 ? g->swatchCount : 1;
            double scaled = pos * n;
            int i0 = ((int)floor(scaled)) % n;
            int i1 = (i0 + 1) % n;
            double f = scaled - floor(scaled);
            COLORREF c0 = g->swatches[i0], c1 = g->swatches[i1];
            base = RGB(
                (int)(GetRValue(c0) + (GetRValue(c1) - GetRValue(c0)) * f),
                (int)(GetGValue(c0) + (GetGValue(c1) - GetGValue(c0)) * f),
                (int)(GetBValue(c0) + (GetBValue(c1) - GetBValue(c0)) * f));
        }
        double dim = ((double)g_cfg.masterBrightness / 100.0) * ((double)g->brightness / 100.0);
        g_liveCols[z] = RGB((int)(GetRValue(base) * dim),
                            (int)(GetGValue(base) * dim),
                            (int)(GetBValue(base) * dim));
    }
}

