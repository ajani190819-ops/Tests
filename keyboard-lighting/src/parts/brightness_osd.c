/* ---- Native-style brightness OSD -----------------------------------
   Bottom-centre popup built to look like the Windows brightness flyout
   the owner actually runs: body and icon in the CURRENT app theme, the
   real Windows glyph (U+E706 from Segoe Fluent Icons / Segoe MDL2
   Assets), and a slider in the Windows shape - rounded track, filled
   portion, round thumb at the fill end. No percentage text: the flyout
   has none, and the thumb position is the reading. Appears whenever
   keyboard brightness changes from outside the window and hides after
   ~1.4s. */
static HWND g_osdHwnd = NULL;
static int  g_osdPct = -1;

/* Windows draws its own flyout in the app theme, so a hardcoded dark body on
   a light desktop is the single most obvious tell that this is not Windows.
   AppsUseLightTheme is the same value UWP and Win32 apps read; missing or
   unreadable means assume light, which is what a default install is. */
static int OsdLightTheme(void) {
    DWORD val = 0, sz = sizeof(val);
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                              L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &val, &sz);
    if (st != ERROR_SUCCESS) return 1;
    return val != 0;
}

static void OsdDrawSunFallback(HDC mem, int cx, int cy, int r, COLORREF col) {
    HPEN sunPen = CreatePen(PS_SOLID, r / 3 + 1, col);
    HBRUSH sunBr = CreateSolidBrush(col);
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

        int light = OsdLightTheme();
        COLORREF body = light ? RGB(249, 249, 249) : RGB(32, 32, 32);
        COLORREF fg   = light ? RGB(26, 26, 26)    : RGB(243, 243, 243);
        COLORREF trk  = light ? RGB(199, 199, 199) : RGB(85, 85, 85);

        HBRUSH bg = CreateSolidBrush(body);
        HPEN bgPen = CreatePen(PS_SOLID, 1, body);
        HGDIOBJ oldBr = SelectObject(mem, bg);
        HGDIOBJ oldPen = SelectObject(mem, bgPen);
        RoundRect(mem, 0, 0, W, H, H / 6, H / 6);
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
            SetTextColor(mem, fg);
            SetRect(&ir, iconCx - H * 30 / 100, 0, iconCx + H * 30 / 100, H);
            DrawTextW(mem, L"\xE706", -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(mem, of);
        } else {
            OsdDrawSunFallback(mem, iconCx, H / 2, H * 11 / 100, fg);
        }
        DeleteObject(f1);
        DeleteObject(f2);

        /* Slider in the Windows shape. The track is thin and rounded, the
           filled portion runs to the current level, and a round thumb sits at
           the fill end proud of the bar - the part the old version was
           missing, and the part the eye reads as "this is a Windows slider". */
        int barH = H * 12 / 100;
        if (barH < 4) barH = 4;
        int barY = (H - barH) / 2;
        int barX = iconCx + H * 38 / 100;
        int barW = W - barX - pad;
        if (barW < 40) barW = 40;
        HBRUSH trBr = CreateSolidBrush(trk);
        HPEN trPen = CreatePen(PS_SOLID, 1, trk);
        SelectObject(mem, trBr);
        SelectObject(mem, trPen);
        RoundRect(mem, barX, barY, barX + barW, barY + barH, barH, barH);
        int pct = g_osdPct;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        int fillW = barW * pct / 100;
        HBRUSH fBr = CreateSolidBrush(fg);
        HPEN fPen = CreatePen(PS_SOLID, 1, fg);
        SelectObject(mem, fBr);
        SelectObject(mem, fPen);
        if (fillW > barH)
            RoundRect(mem, barX, barY, barX + fillW, barY + barH, barH, barH);
        int tx = barX + fillW;
        int ty = barY + barH / 2;
        int tr2 = barH;
        /* Body-coloured ring first, so the thumb stays readable where it
           overlaps the filled part of the track. */
        HBRUSH ringBr = CreateSolidBrush(body);
        HPEN ringPen = CreatePen(PS_SOLID, 1, body);
        SelectObject(mem, ringBr);
        SelectObject(mem, ringPen);
        Ellipse(mem, tx - tr2 - 2, ty - tr2 - 2, tx + tr2 + 2, ty + tr2 + 2);
        Ellipse(mem, tx - tr2, ty - tr2, tx + tr2, ty + tr2);

        BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBr);
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBm);
        DeleteObject(bg); DeleteObject(bgPen);
        DeleteObject(trBr); DeleteObject(trPen);
        DeleteObject(fBr); DeleteObject(fPen);
        DeleteObject(ringBr); DeleteObject(ringPen);
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

