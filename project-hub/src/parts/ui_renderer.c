/* ============================================================================
 * Sub-Pixel Anti-Aliased Smooth Rounded UI Renderer (32-bit DIBSection)
 * ============================================================================ */

static inline uint32_t PackRgb(COLORREF c) {
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static inline uint32_t BlendRgb(uint32_t dst, uint32_t src, int a256) {
    if (a256 <= 0) return dst;
    if (a256 >= 256) return src;
    int inv = 256 - a256;
    uint32_t rb = (((dst & 0x00FF00FFu) * (uint32_t)inv) + ((src & 0x00FF00FFu) * (uint32_t)a256)) >> 8;
    uint32_t g  = (((dst & 0x0000FF00u) * (uint32_t)inv) + ((src & 0x0000FF00u) * (uint32_t)a256)) >> 8;
    return (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void FillCanvasRect(uint32_t *px, int W, int H, int x0, int y0, int x1, int y1, COLORREF col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t p = PackRgb(col);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; x++) row[x] = p;
    }
}

static void SmoothRoundRect(uint32_t *px, int W, int H, const RECT *rc, float radius,
                            COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    float x0 = (float)rc->left, y0 = (float)rc->top;
    float x1 = (float)rc->right, y1 = (float)rc->bottom;
    float rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0.5f || rh <= 0.5f) return;
    if (radius * 2.0f > rw) radius = rw * 0.5f;
    if (radius * 2.0f > rh) radius = rh * 0.5f;

    float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    float hx = 0.5f * rw - radius, hy = 0.5f * rh - radius;

    int iy0 = (int)floorf(y0 - 1.0f);
    int iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f);
    int ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP   = PackRgb(fillCol);
    uint32_t borderP = PackRgb(borderCol);
    BOOL sameBorder  = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd;
            if (qx > 0.0f && qy > 0.0f) {
                sd = sqrtf(qx * qx + qy * qy) - radius;
            } else {
                sd = (qx > qy ? qx : qy) - radius;
            }
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) {
                row[x] = c;
            } else {
                int cov = (int)((0.5f - sd) * 256.0f);
                row[x] = BlendRgb(row[x], c, cov);
            }
        }
    }
}

static void DrawActionBtn(HDC hdc, uint32_t *px, int W, int H, const ActionBtn *btn,
                          int dpi, int clipTop, int clipBottom) {
    BOOL hot = (g_hoverId == btn->id);
    COLORREF bg, border, fg = COL_TEXT_PRIMARY;
    if (btn->style == BTN_PRIMARY) {
        bg     = hot ? COL_PRIMARY_HOVER : COL_PRIMARY_BG;
        border = bg;
    } else if (btn->style == BTN_AMBER) {
        bg     = hot ? COL_AMBER_BTN_HOVER : COL_AMBER_BTN;
        border = COL_AMBER;
        fg     = COL_AMBER;
    } else {
        bg     = hot ? COL_BTN_HOVER : COL_BTN_BG;
        border = hot ? COL_HOVER_BORDER : COL_BTN_BORDER;
    }
    SmoothRoundRect(px, W, H, &btn->rc, (float)S(7, dpi), bg, border, clipTop, clipBottom);
    SelectObject(hdc, g_fontBtn);
    SetTextColor(hdc, fg);
    RECT tr = btn->rc;
    DrawTextW(hdc, btn->label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static void ComputeLayout(HWND hwnd) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int cardH = S(80, dpi);
    int gap = S(11, dpi);

    int embSz = S(52, dpi);
    int embY = (headerH - embSz) / 2;
    SetRect(&g_rcEmblem, pad, embY, pad + embSz, embY + embSz);

    int bx = w - pad;
    int topBtnH = S(35, dpi);
    int topBtnY = (headerH - topBtnH) / 2;
    for (int i = 3; i >= 0; i--) {
        int bw = S(g_topBtns[i].widthPx, dpi);
        bx -= bw;
        SetRect(&g_topBtns[i].rc, bx, topBtnY, bx + bw, topBtnY + topBtnH);
        bx -= S(9, dpi);
    }

    int contentH = g_cardCount * cardH + (g_cardCount > 1 ? (g_cardCount - 1) : 0) * gap + S(24, dpi);
    int viewH = h - headerH - footerH;
    g_maxScrollY = (contentH > viewH) ? (contentH - viewH) : 0;
    if (g_scrollY > g_maxScrollY) g_scrollY = g_maxScrollY;
    if (g_scrollY < 0) g_scrollY = 0;

    int startY = headerH + ((viewH > contentH) ? (viewH - contentH) / 2 + S(12, dpi) : S(12, dpi)) - g_scrollY;
    int btnH = S(30, dpi);
    for (int c = 0; c < g_cardCount; c++) {
        SetRect(&g_cards[c].rcCard, pad, startY, w - pad, startY + cardH);
        int rx = w - pad - S(16, dpi);
        int by = startY + cardH - S(12, dpi) - btnH;
        for (int b = g_cards[c].btnCount - 1; b >= 0; b--) {
            if (g_cards[c].btns[b].hidden) continue;
            int bw = S(g_cards[c].btns[b].widthPx, dpi);
            rx -= bw;
            SetRect(&g_cards[c].btns[b].rc, rx, by, rx + bw, by + btnH);
            rx -= S(7, dpi);
        }
        startY += cardH + gap;
    }
}

static int HitTestButtons(POINT pt, int headerBottom, int footerTop) {
    if (PtInRect(&g_rcEmblem, pt)) return ID_EMBLEM_MENU;
    for (int i = 0; i < 4; i++) {
        if (PtInRect(&g_topBtns[i].rc, pt)) return g_topBtns[i].id;
    }
    if (pt.y < headerBottom || pt.y > footerTop) return 0;
    for (int c = 0; c < g_cardCount; c++) {
        for (int b = 0; b < g_cards[c].btnCount; b++) {
            if (g_cards[c].btns[b].hidden) continue;
            if (PtInRect(&g_cards[c].btns[b].rc, pt)) return g_cards[c].btns[b].id;
        }
    }
    return 0;
}

/* Blit the spinning emblem square straight to a DC (spin fast-path). The
   square is filled with the canvas background first - a raw COLORREF fill
   would swap red/blue in the 32bpp DIB and show as a box. */
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

/* Static-frame cache for the spin fast-path: the 50 ms emblem timer blits
   this snapshot plus the spinning square instead of re-rendering every
   card, pill and row of text 20 times a second. */
static HBITMAP g_frameCache = NULL;
static int    g_fcW = 0, g_fcH = 0;

static void PaintHubWindow(HWND hwnd, HDC targetDc, const RECT *updRc) {
    int dpi = GetWinDpi(hwnd);
    RECT cli;
    GetClientRect(hwnd, &cli);
    int w = cli.right - cli.left;
    int h = cli.bottom - cli.top;
    if (w <= 0 || h <= 0) return;

    ComputeLayout(hwnd);

    /* Spin fast-path: an emblem-only update rect plus a cached frame means
       we blit the snapshot and repaint just the spinning square - no card
       sweeps, no per-pixel roundrects, no text at 20 fps. */
    if (updRc && g_frameCache && g_fcW == w && g_fcH == h &&
        updRc->right > updRc->left && updRc->bottom > updRc->top &&
        g_rcEmblem.right > g_rcEmblem.left &&
        updRc->left >= g_rcEmblem.left && updRc->top >= g_rcEmblem.top &&
        updRc->right <= g_rcEmblem.right && updRc->bottom <= g_rcEmblem.bottom) {
        HDC cacheDc = CreateCompatibleDC(targetDc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(targetDc, 0, 0, w, h, cacheDc, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
        PaintEmblemBlit(targetDc, g_rcEmblem.left, g_rcEmblem.top,
                        g_rcEmblem.right - g_rcEmblem.left);
        return;
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; /* top-down 32bpp */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *px = NULL;
    HDC hdc = CreateCompatibleDC(targetDc);
    HBITMAP bmp = CreateDIBSection(targetDc, &bmi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
    if (!bmp || !px) { DeleteDC(hdc); return; }
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);
    SetBkMode(hdc, TRANSPARENT);

    int pad = S(28, dpi);
    int headerH = S(76, dpi);
    int footerH = S(42, dpi);
    int footerTop = h - footerH;

    /* Soft low-glare slate canvas */
    FillCanvasRect(px, w, h, 0, 0, w, footerTop, COL_BG);
    FillCanvasRect(px, w, h, 0, footerTop, w, h, COL_FOOTER_BG);

    /* Clipped project cards */
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, 0, headerH + 1, w, footerTop - 1);

    int cardIcoSz = S(44, dpi);
    for (int c = 0; c < g_cardCount; c++) {
        const ProjectCard *card = &g_cards[c];
        if (card->rcCard.bottom < headerH || card->rcCard.top > footerTop) continue;

        SmoothRoundRect(px, w, h, &card->rcCard, (float)S(10, dpi),
                        COL_CARD_BG, COL_CARD_BORDER, headerH + 1, footerTop - 1);

        COLORREF tagFg = COL_TEXT_SECONDARY, tagBg = COL_IDLE_BG, barCol = COL_IDLE_SLATE;
        if (card->badgeKind == BADGE_OK) {
            tagFg = COL_MINT; tagBg = COL_MINT_BG; barCol = COL_MINT;
        } else if (card->badgeKind == BADGE_WARN) {
            tagFg = COL_AMBER; tagBg = COL_AMBER_BG; barCol = COL_AMBER;
        } else if (card->badgeKind == BADGE_SKY) {
            tagFg = COL_SKY; tagBg = COL_SKY_BG; barCol = COL_SKY;
        } else if (card->badgeKind == BADGE_DANGER) {
            tagFg = COL_CORAL; tagBg = COL_CORAL_BG; barCol = COL_CORAL;
        }

        /* Soft inset rounded accent capsule on left of card */
        RECT barRc = {
            card->rcCard.left + S(1, dpi),
            card->rcCard.top + S(16, dpi),
            card->rcCard.left + S(5, dpi),
            card->rcCard.bottom - S(16, dpi)
        };
        SmoothRoundRect(px, w, h, &barRc, (float)S(2, dpi), barCol, barCol, headerH + 1, footerTop - 1);

        /* Catalog icon first (download_icons), then embedded, letter tile */
        {
            HICON ico = card->hIcon ? card->hIcon : g_cardIcons[c];
            int ix = card->rcCard.left + S(18, dpi);
            int iy = (card->rcCard.top + card->rcCard.bottom - cardIcoSz) / 2;
            if (ico) {
                DrawIconEx(hdc, ix, iy, ico, cardIcoSz, cardIcoSz, 0, NULL, DI_NORMAL);
            } else if (card->title && card->title[0]) {
                RECT tile = { ix, iy, ix + cardIcoSz, iy + cardIcoSz };
                SmoothRoundRect(px, w, h, &tile, (float)S(10, dpi),
                                COL_SKY_BG, COL_SKY, headerH + 1, footerTop - 1);
                wchar_t lt[2];
                lt[0] = card->title[0];
                lt[1] = L'\0';
                SelectObject(hdc, g_fontBadge);
                SetTextColor(hdc, COL_SKY);
                DrawTextW(hdc, lt, -1, &tile, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        /* Smooth rounded status pill badge on top right */
        SelectObject(hdc, g_fontBadge);
        const wchar_t *badgeTxt = card->badge[0] ? card->badge : L"Checking";
        SIZE bsz = {0, 0};
        GetTextExtentPoint32W(hdc, badgeTxt, (int)wcslen(badgeTxt), &bsz);
        int pillH = S(23, dpi);
        int pillW = bsz.cx + S(28, dpi);
        RECT pillRc = {
            card->rcCard.right - S(16, dpi) - pillW,
            card->rcCard.top + S(11, dpi),
            card->rcCard.right - S(16, dpi),
            card->rcCard.top + S(11, dpi) + pillH
        };
        SmoothRoundRect(px, w, h, &pillRc, (float)pillH * 0.5f, tagBg, tagBg, headerH + 1, footerTop - 1);

        int dotD = S(6, dpi);
        RECT dotRc = {
            pillRc.left + S(9, dpi),
            pillRc.top + (pillH - dotD) / 2,
            pillRc.left + S(9, dpi) + dotD,
            pillRc.top + (pillH - dotD) / 2 + dotD
        };
        SmoothRoundRect(px, w, h, &dotRc, (float)dotD * 0.5f, tagFg, tagFg, headerH + 1, footerTop - 1);

        RECT pillTxtRc = { pillRc.left + S(19, dpi), pillRc.top, pillRc.right - S(8, dpi), pillRc.bottom };
        SetTextColor(hdc, tagFg);
        DrawTextW(hdc, badgeTxt, -1, &pillTxtRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        /* Clean Title Case card title + calm summary */
        int textLeft = card->rcCard.left + S(74, dpi);
        SelectObject(hdc, g_fontTitle);
        SetTextColor(hdc, COL_TEXT_PRIMARY);
        RECT titleRc = { textLeft, card->rcCard.top + S(13, dpi), pillRc.left - S(12, dpi), card->rcCard.top + S(37, dpi) };
        DrawTextW(hdc, card->title, -1, &titleRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        int firstBtnLeft = (card->btnCount > 0) ? card->btns[0].rc.left : (card->rcCard.right - S(16, dpi));
        SelectObject(hdc, g_fontBody);
        SetTextColor(hdc, COL_TEXT_SECONDARY);
        RECT detRc = { textLeft, card->rcCard.top + S(39, dpi), firstBtnLeft - S(14, dpi), card->rcCard.bottom - S(12, dpi) };
        DrawTextW(hdc, card->detail, -1, &detRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        for (int b = 0; b < card->btnCount; b++) {
            if (card->btns[b].hidden) continue;
            DrawActionBtn(hdc, px, w, h, &card->btns[b], dpi, headerH + 1, footerTop - 1);
        }
    }

    RestoreDC(hdc, saved);

    /* Header: Suite Tri-Spoke Core Emblem + natural title + calm subtitle + rounded buttons */
    int embSz = S(52, dpi);
    DrawWayfinderEmblem(px, w, h, g_rcEmblem.left, g_rcEmblem.top, embSz);

    int hdrTextLeft = g_rcEmblem.right + S(14, dpi);
    int hdrRightLimit = g_topBtns[0].rc.left - S(14, dpi);
    SelectObject(hdc, g_fontHero);
    SetTextColor(hdc, COL_TEXT_PRIMARY);
    RECT hTitle = { hdrTextLeft, S(13, dpi), hdrRightLimit, S(40, dpi) };
    DrawTextW(hdc, L"Project Hub", -1, &hTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, COL_TEXT_MUTED);
    RECT hSub = { hdrTextLeft, S(40, dpi), hdrRightLimit, S(60, dpi) };
    {
        wchar_t subBuf[160];
        EnterCriticalSection(&g_cs);
        _snwprintf(subBuf, 159, L"Suite Control  \x00B7  v" HUB_VERSION_W L"  \x00B7  %s",
                   g_activeBranch[0] ? g_activeBranch : L"auto");
        subBuf[159] = L'\0';
        LeaveCriticalSection(&g_cs);
        DrawTextW(hdc, subBuf, -1, &hSub,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    for (int i = 0; i < 4; i++) {
        DrawActionBtn(hdc, px, w, h, &g_topBtns[i], dpi, 0, headerH);
    }

    FillCanvasRect(px, w, h, pad, headerH, w - pad, headerH + 1, COL_DIVIDER);
    FillCanvasRect(px, w, h, 0, footerTop, w, footerTop + 1, COL_DIVIDER);

    wchar_t footMsg[512];
    int pct = 0;
    EnterCriticalSection(&g_cs);
    wcsncpy(footMsg, g_statusMsg, 511); footMsg[511] = L'\0';
    pct = g_progressVal;
    LeaveCriticalSection(&g_cs);

    BOOL checking = InterlockedCompareExchange(&g_checkActive, 0, 0) != 0;

    /* The full-width progress sweep is reserved for real downloads. The
       periodic update check just pulses a small dot so the footer stays
       readable and nothing sweeps across the window. */
    if (!checking && pct > 0 && pct < 100) {
        int fillW = (w * pct) / 100;
        FillCanvasRect(px, w, h, 0, footerTop - 1, fillW, footerTop + S(2, dpi), COL_SKY);
    }

    int fdSz = S(8, dpi);
    COLORREF dotCol;
    if (checking) {
        double ph = (double)(GetTickCount64() % 1400ULL) / 1400.0;
        double tri = (ph < 0.5) ? (ph * 2.0) : (2.0 - ph * 2.0);
        fdSz = S(5, dpi) + (int)(tri * S(5, dpi) + 0.5);
        int tr = (int)(tri * 255.0);
        int dr = 42 + ((GetRValue(COL_SKY) - 42) * tr) / 255;
        int dg = 84 + ((GetGValue(COL_SKY) - 84) * tr) / 255;
        int db = 130 + ((GetBValue(COL_SKY) - 130) * tr) / 255;
        dotCol = RGB(dr, dg, db);
    } else {
        dotCol = g_hubUpdAvail ? COL_AMBER : ((pct > 0 && pct < 100) ? COL_SKY : COL_MINT);
    }
    RECT fDot = { pad, footerTop + (footerH - fdSz) / 2, pad + fdSz, footerTop + (footerH - fdSz) / 2 + fdSz };
    SmoothRoundRect(px, w, h, &fDot, (float)fdSz * 0.5f, dotCol, dotCol, footerTop, h);
    g_rcFooterDot.left = pad - S(4, dpi);
    g_rcFooterDot.top = footerTop;
    g_rcFooterDot.right = pad + S(18, dpi);
    g_rcFooterDot.bottom = h;

    SelectObject(hdc, g_fontSub);
    SetTextColor(hdc, g_hubUpdAvail ? COL_AMBER : COL_TEXT_SECONDARY);
    RECT fTxt = { pad + S(16, dpi), footerTop, w - pad, h };
    DrawTextW(hdc, footMsg, -1, &fTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    /* snapshot the composed frame for the spin fast-path */
    if (!g_frameCache || g_fcW != w || g_fcH != h) {
        if (g_frameCache) DeleteObject(g_frameCache);
        g_frameCache = CreateCompatibleBitmap(targetDc, w, h);
        g_fcW = w;
        g_fcH = h;
    }
    if (g_frameCache) {
        HDC cacheDc = CreateCompatibleDC(targetDc);
        HGDIOBJ oldc = SelectObject(cacheDc, g_frameCache);
        BitBlt(cacheDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        SelectObject(cacheDc, oldc);
        DeleteDC(cacheDc);
    }

    BitBlt(targetDc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

static void ShowUtilityMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SHORTCUT,  L"Pin Project Hub Shortcut to Desktop");
    AppendMenuW(hMenu, MF_STRING, ID_BTN_SELF_UPDATE, L"Self-Update Project Hub Now");
    AppendMenuW(hMenu, MF_STRING, ID_BTN_BRANCH, L"Download Branch...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SUITE_DIR, L"Open Managed Suite Folder");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_HUB_TOSUITE, L"Install Hub into Suite Folder");

    POINT pt = { g_rcEmblem.left, g_rcEmblem.bottom + 4 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

