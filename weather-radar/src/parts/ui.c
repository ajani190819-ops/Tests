/* ---------------- ui: paint, hit testing, WndProc ----------------

   Everything is owner-drawn onto one double-buffered DIB (the same trick the
   rest of the suite uses to keep resizing flicker-free) except the search
   box, which is a real EDIT so keyboard, IME and clipboard behave. The one
   RichEdit lesson from Agent Notes applies in reverse here: an EDIT DOES
   honour WM_CTLCOLOREDIT, so the hint text colour lives in that handler. */

#define WIN_LW 1120
#define WIN_LH 960
#define LEFT_W 252
/* The radar host's imagery ends at level 8. Draw a centred local focus from
   our three-tile fetch rather than shrinking the entire 768px region. Higher
   owner-selected display levels crop this available source progressively;
   no unsupported closer tile request is made or misrepresented as radar data. */
#define RADAR_FOCUS_PX 512
#define RADAR_FOCUS_MIN_PX 96

enum { PLAN_RAIN = 0, PLAN_TEMP, PLAN_CLOUD, PLAN_WIND, PLAN_CATEGORY_COUNT };
static int g_planDay = 0;
static int g_planCategory = PLAN_RAIN;

static void RadarEmblemRect(RECT *r) {
    SetRect(r, S(14), S(10), S(56), S(52));
}

static void BtnRect(int i, RECT *r) {
    switch (i) {
    case BTN_SEARCH:  SetRect(r, S(16), S(110), S(120), S(140)); break;
    case BTN_KEEP:    SetRect(r, S(126), S(110), S(182), S(140)); break;
    case BTN_DROP:    SetRect(r, S(188), S(110), S(244), S(140)); break;
    case BTN_REFRESH: SetRect(r, S(WIN_LW - 16 - 104), S(16), S(WIN_LW - 16), S(48)); break;
    case BTN_PREV:    SetRect(r, S(272), S(520), S(308), S(548)); break;
    case BTN_PLAY:    SetRect(r, S(314), S(520), S(372), S(548)); break;
    case BTN_NEXT:    SetRect(r, S(378), S(520), S(414), S(548)); break;
    case BTN_ZOOM:    SetRect(r, S(420), S(520), S(510), S(548)); break;
    default:          SetRect(r, 0, 0, 0, 0); break;
    }
}

static void PlannerRect(RECT *r) {
    SetRect(r, S(260), S(290), S(WIN_LW - 16), S(508));
}

static void RadarImageRect(RECT *r) {
    /* Keep a square local-radar canvas below its playback controls and the
       five-day decision planner. */
    SetRect(r, S(272), S(556), S(WIN_LW - 28), S(WIN_LH - 94));
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
    RECT inner = { card->left + S(18), card->top + S(8), card->right - S(18), card->bottom - S(8) };
    if (!g_fc.have) {
        DrawTextAt(mem, L"Fetching the current conditions...", &inner, g_fontUI,
                   COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    wchar_t big[24];
    _snwprintf(big, 23, L"%d°", (int)llround(g_fc.tempF));
    /* A clear at-a-glance block: the temperature remains prominent while the
       decision details have four separate lines instead of one crammed run. */
    RECT tr = { inner.left, inner.top, inner.left + S(132), inner.bottom };
    DrawTextAt(mem, big, &tr, g_fontBig, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int detailLeft = inner.left + S(138);
    RECT cr = { detailLeft, inner.top, inner.right, inner.top + S(22) };
    DrawTextAt(mem, WxCodeText(g_fc.wcode), &cr, g_fontTitle, COL_TXT,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    wchar_t line[192];
    _snwprintf(line, 191, L"Feels %d°  ·  Humidity %d%%  ·  Cloud %d%%",
               (int)llround(g_fc.feelsF), (int)llround(g_fc.rh),
               (int)llround(g_fc.cloudCover));
    line[191] = L'\0';
    RECT lr = { detailLeft, inner.top + S(22), inner.right, inner.top + S(40) };
    DrawTextAt(mem, line, &lr, g_fontSmall, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    _snwprintf(line, 191, L"Wind %d mph  ·  Gusts %d mph  ·  Rain now %.2f in",
               (int)llround(g_fc.windMph), (int)llround(g_fc.windGustMph), g_fc.precipIn);
    line[191] = L'\0';
    RECT wr = { detailLeft, inner.top + S(40), inner.right, inner.top + S(58) };
    DrawTextAt(mem, line, &wr, g_fontSmall, WxCodeIsWet(g_fc.wcode) ? COL_RAIN : COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (!isnan(g_fc.dMaxF) && !isnan(g_fc.dMinF) &&
        !isnan(g_fc.dPoP) && !isnan(g_fc.dSumIn))
        _snwprintf(line, 191, L"Today: %d° / %d°  ·  rain up to %d%%  ·  total %.2f in",
                   (int)llround(g_fc.dMaxF), (int)llround(g_fc.dMinF),
                   (int)llround(g_fc.dPoP), g_fc.dSumIn);
    else
        wcscpy(line, L"Today's daily outlook is unavailable.");
    line[191] = L'\0';
    RECT dr = { detailLeft, inner.top + S(58), inner.right, inner.bottom };
    DrawTextAt(mem, line, &dr, g_fontSmall, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* Give the selected place a useful onset estimate without implying minute-level
   precision. Prefer the first hour when a majority of reporting ensemble
   members forecast measurable rain; if there is no majority, show the peak
   hourly ensemble signal as possible. The single-run hourly forecast is only
   a clearly labelled fallback when the ensemble has no rain signal. */
static void RainEtaText(const Forecast *f, const Ensemble *e, wchar_t *out, int cap) {
    if (!out || cap < 2) return;
    out[0] = L'\0';
    if (!f || !f->have) {
        _snwprintf(out, cap - 1, L"Rain ETA: waiting for the location forecast");
        out[cap - 1] = L'\0';
        return;
    }
    if (ForecastIsRainingNow(f)) {
        _snwprintf(out, cap - 1, L"Rain ETA: rainfall is already underway");
        out[cap - 1] = L'\0';
        return;
    }
    if (f->hN <= 0) {
        _snwprintf(out, cap - 1, L"Rain ETA: hourly forecast unavailable");
        out[cap - 1] = L'\0';
        return;
    }

    int start = f->nowIdx;
    if (start < 0) start = 0;
    int ensembleHorizon = 0;
    if (e && e->have) {
        ensembleHorizon = e->hours - start;
        if (ensembleHorizon < 0) ensembleHorizon = 0;
        if (ensembleHorizon > f->hN) ensembleHorizon = f->hN;
    }

    int likelyK = -1, peakK = -1;
    double peakProb = 0.0;
    for (int k = 0; k < ensembleHorizon; k++) {
        int h = start + k;
        double prob = e->prob[h];
        if (e->reported[h] <= 0 || isnan(prob)) continue;
        if (peakK < 0 || prob > peakProb) {
            peakK = k;
            peakProb = prob;
        }
        if (prob >= RAIN_ETA_LIKELY_PROB && likelyK < 0) likelyK = k;
    }

    int k = likelyK;
    int mode = likelyK >= 0 ? 1 : 0; /* 1 likely, 2 possible peak, 3 single-run fallback */
    double odds = NAN;
    if (k >= 0) {
        odds = e->prob[start + k];
    } else if (peakK >= 0 && peakProb > 0.0) {
        k = peakK;
        odds = peakProb;
        mode = 2;
    } else {
        for (int i = 0; i < f->hN; i++) {
            if (!isnan(f->hP[i]) && f->hP[i] >= RAIN_ALERT_TRACE_IN) {
                k = i;
                mode = 3;
                int h = start + k;
                if (e && e->have && h >= 0 && h < e->hours &&
                    e->reported[h] > 0 && !isnan(e->prob[h])) {
                    odds = e->prob[h];
                }
                break;
            }
        }
    }

    if (k >= 0) {
        wchar_t time[16], offset[24];
        if (f->hLabel[k][0]) {
            wcsncpy(time, f->hLabel[k], 15);
            time[15] = L'\0';
        } else {
            wcscpy(time, L"forecast time");
        }
        if (k == 0) wcscpy(offset, L"within ~1h");
        else _snwprintf(offset, 23, L"in ~%dh", k);
        offset[23] = L'\0';

        int pct = isnan(odds) ? -1 : (int)llround(odds * 100.0);
        if (mode == 1) {
            _snwprintf(out, cap - 1,
                       L"Rain ETA: likely around %s local (%s) · %d%% ensemble",
                       time, offset, pct);
        } else if (mode == 2) {
            _snwprintf(out, cap - 1,
                       L"Rain ETA: possible peak around %s local (%s) · %d%% ensemble",
                       time, offset, pct);
        } else if (pct >= 0) {
            _snwprintf(out, cap - 1,
                       L"Rain ETA: hourly-model signal around %s local (%s) · %d%% ensemble",
                       time, offset, pct);
        } else {
            _snwprintf(out, cap - 1,
                       L"Rain ETA: hourly-model signal around %s local (%s) · ensemble unavailable",
                       time, offset);
        }
        out[cap - 1] = L'\0';
        return;
    }

    if (e && e->have && ensembleHorizon > 0)
        _snwprintf(out, cap - 1,
                   L"Rain ETA: no measurable rain signal in the next %d forecast hours",
                   ensembleHorizon);
    else
        _snwprintf(out, cap - 1,
                   L"Rain ETA: no measurable rain modeled in the next %d forecast hours",
                   f->hN);
    out[cap - 1] = L'\0';
}

/* The headline: the ensemble fraction, in words and as a bar, next to the
   coverage-blended figure so the two can be compared at a glance. */
static void PaintHonestCard(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT lbl = { card->left + S(18), card->top + S(8), card->right - S(18), card->top + S(25) };
    DrawTextAt(mem, L"Rain at your exact location · next 6 hours", &lbl, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int start = g_fc.have ? g_fc.nowIdx : 0;
    double honest = EnsWindowProb(&g_ens, start, 6);
    double blended = g_fc.have ? PoPWindowMax(&g_fc, 0, 6) : NAN;

    RECT big = { card->left + S(18), card->top + S(24), card->left + S(140), card->top + S(76) };
    wchar_t pct[16];
    if (isnan(honest)) wcscpy(pct, L"--");
    else _snwprintf(pct, 15, L"%d%%", (int)llround(honest * 100.0));
    DrawTextAt(mem, pct, &big, g_fontBig, COL_RAIN, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* The fill is the member share at this point.  The gold tick is retained
       as the regional/coverage number, clearly labelled but never promoted to
       the headline. */
    RECT bar = { card->left + S(150), card->top + S(37), card->right - S(22), card->top + S(55) };
    DrawRoundedBox(mem, &bar, S(9), COL_INPUT, COL_DIV);
    if (!isnan(honest)) {
        RECT fill = bar;
        int w = (int)((bar.right - bar.left) * honest);
        fill.right = bar.left + (w > S(6) ? w : S(6));
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

    wchar_t detail[192];
    if (g_ens.have && !isnan(honest)) {
        _snwprintf(detail, 191, L"Blue: %d ensemble members here  ·  Gold tick: %d%% regional coverage-blended forecast",
                   g_ens.members, isnan(blended) ? 0 : (int)llround(blended));
    } else {
        wcscpy(detail, L"Ensemble unavailable — no borrowed probability is shown as yours.");
    }
    detail[191] = L'\0';
    wchar_t eta[192];
    RainEtaText(&g_fc, &g_ens, eta, 192);
    RECT er = { card->left + S(150), card->top + S(57), card->right - S(18), card->top + S(75) };
    DrawTextAt(mem, eta, &er, g_fontSmall, COL_TXT,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT dr = { card->left + S(150), card->top + S(75), card->right - S(18), card->bottom - S(4) };
    DrawTextAt(mem, detail, &dr, g_fontSmall, COL_MUTED,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static const wchar_t *PlannerCategoryName(int category) {
    static const wchar_t *names[PLAN_CATEGORY_COUNT] = {
        L"Rain", L"Temperature", L"Cloud cover", L"Wind"
    };
    return (category >= 0 && category < PLAN_CATEGORY_COUNT) ? names[category] : L"Weather";
}

static int PlannerCellRect(int day, int category, RECT *r) {
    if (!r || day < 0 || day >= FC_DAYS || category < 0 ||
        category >= PLAN_CATEGORY_COUNT) return 0;
    RECT card;
    PlannerRect(&card);
    int firstLeft = card.left + S(116);
    int lastRight = card.right - S(14);
    int gap = S(4);
    int colWidth = (lastRight - firstLeft - gap * (FC_DAYS - 1)) / FC_DAYS;
    if (colWidth <= 0) return 0;
    int left = firstLeft + day * (colWidth + gap);
    int top = card.top + S(48 + category * 19);
    SetRect(r, left, top, left + colWidth, top + S(17));
    return 1;
}

static int PlannerDayRect(int day, RECT *r) {
    if (!r || day < 0 || day >= FC_DAYS) return 0;
    RECT card;
    PlannerRect(&card);
    int firstLeft = card.left + S(116);
    int lastRight = card.right - S(14);
    int gap = S(4);
    int colWidth = (lastRight - firstLeft - gap * (FC_DAYS - 1)) / FC_DAYS;
    if (colWidth <= 0) return 0;
    int left = firstLeft + day * (colWidth + gap);
    int top = card.top + S(27);
    SetRect(r, left, top, left + colWidth, top + S(18));
    return 1;
}

static int HitPlannerCell(int x, int y, int *dayOut, int *categoryOut) {
    if (!g_fc.have || g_fc.dayN <= 0) return 0;
    POINT pt = { x, y };
    int days = g_fc.dayN < FC_DAYS ? g_fc.dayN : FC_DAYS;
    for (int day = 0; day < days; day++) {
        for (int category = 0; category < PLAN_CATEGORY_COUNT; category++) {
            RECT r;
            if (PlannerCellRect(day, category, &r) && PtInRect(&r, pt)) {
                if (dayOut) *dayOut = day;
                if (categoryOut) *categoryOut = category;
                return 1;
            }
        }
    }
    return 0;
}

static double PlannerPointRainChance(const Forecast *f, const Ensemble *e,
                                     const ForecastDay *day) {
    if (!f || !e || !e->have || !day || day->firstHour < 0 || day->hourCount <= 0)
        return NAN;
    int start = f->nowIdx + day->firstHour;
    if (start < 0 || start >= e->hours) return NAN;
    return EnsCompleteWindowProb(e, start, day->hourCount);
}

static void PlannerCellText(int dayIndex, int category, wchar_t *out, int cap) {
    if (!out || cap < 2) return;
    out[0] = L'\0';
    if (dayIndex < 0 || dayIndex >= g_fc.dayN || dayIndex >= FC_DAYS) {
        wcscpy(out, L"--");
        return;
    }
    const ForecastDay *day = &g_fc.days[dayIndex];
    if (category == PLAN_RAIN) {
        double point = PlannerPointRainChance(&g_fc, &g_ens, day);
        double regional = day->regionalPoP;
        if (!isnan(point) && !isnan(regional))
            _snwprintf(out, cap - 1, L"P %d%% · R %d%%",
                       (int)llround(point * 100.0), (int)llround(regional));
        else if (!isnan(point))
            _snwprintf(out, cap - 1, L"P %d%% · R --", (int)llround(point * 100.0));
        else if (!isnan(regional))
            _snwprintf(out, cap - 1, L"P -- · R %d%%", (int)llround(regional));
        else
            wcscpy(out, L"--");
    } else if (category == PLAN_TEMP && !isnan(day->maxF) && !isnan(day->minF)) {
        _snwprintf(out, cap - 1, L"H %d° · L %d°",
                   (int)llround(day->maxF), (int)llround(day->minF));
    } else if (category == PLAN_CLOUD && !isnan(day->cloudMean)) {
        _snwprintf(out, cap - 1, L"Avg %d%%", (int)llround(day->cloudMean));
    } else if (category == PLAN_WIND && !isnan(day->windMaxMph)) {
        _snwprintf(out, cap - 1, L"Peak %d mph", (int)llround(day->windMaxMph));
    } else {
        wcscpy(out, L"--");
    }
    out[cap - 1] = L'\0';
}

static double PlannerHourlyValue(int category, int hour, int regionalRain) {
    if (hour < 0 || hour >= g_fc.hN) return NAN;
    switch (category) {
    case PLAN_RAIN:
        if (regionalRain) return g_fc.hPoP[hour];
        if (!g_ens.have) return NAN;
        {
            int ensembleHour = g_fc.nowIdx + hour;
            if (ensembleHour < 0 || ensembleHour >= g_ens.hours) return NAN;
            return isnan(g_ens.prob[ensembleHour]) ? NAN : g_ens.prob[ensembleHour] * 100.0;
        }
    case PLAN_TEMP: return g_fc.hT[hour];
    case PLAN_CLOUD: return g_fc.hCloud[hour];
    case PLAN_WIND: return g_fc.hWindMph[hour];
    default: return NAN;
    }
}

static void PlannerTrendRange(const ForecastDay *day, int category,
                              double *lowOut, double *highOut) {
    if (category == PLAN_RAIN || category == PLAN_CLOUD) {
        *lowOut = 0.0;
        *highOut = 100.0;
        return;
    }
    double low = NAN, high = NAN;
    for (int i = 0; i < day->hourCount; i++) {
        int h = day->firstHour + i;
        double value = PlannerHourlyValue(category, h, 0);
        if (isnan(value)) continue;
        if (isnan(low) || value < low) low = value;
        if (isnan(high) || value > high) high = value;
    }
    if (category == PLAN_TEMP) {
        if (!isnan(day->minF) && (isnan(low) || day->minF < low)) low = day->minF;
        if (!isnan(day->maxF) && (isnan(high) || day->maxF > high)) high = day->maxF;
        if (isnan(low) || isnan(high)) { low = 0.0; high = 100.0; }
        if (high - low < 1.0) { low -= 5.0; high += 5.0; }
        double pad = (high - low) * 0.08;
        *lowOut = low - pad;
        *highOut = high + pad;
        return;
    }
    if (isnan(high) || high < 5.0) high = 5.0;
    *lowOut = 0.0;
    *highOut = high * 1.12;
}

static void PaintPlannerSeries(HDC dc, const ForecastDay *day, int category,
                               int regionalRain, const RECT *plot,
                               double low, double high, COLORREF color) {
    if (!day || day->firstHour < 0 || day->hourCount <= 0 ||
        plot->right <= plot->left || plot->bottom <= plot->top || high <= low) return;
    HPEN pen = CreatePen(PS_SOLID, S(2), color);
    if (!pen) return;
    HGDIOBJ old = SelectObject(dc, pen);
    if (!old || old == HGDI_ERROR) { DeleteObject(pen); return; }
    int prev = 0;
    int width = plot->right - plot->left;
    int height = plot->bottom - plot->top;
    for (int i = 0; i < day->hourCount; i++) {
        int h = day->firstHour + i;
        double value = PlannerHourlyValue(category, h, regionalRain);
        if (isnan(value)) { prev = 0; continue; }
        double fraction = (value - low) / (high - low);
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        int x = plot->left + (day->hourCount > 1
            ? i * (width - 1) / (day->hourCount - 1) : width / 2);
        int y = plot->bottom - 1 - (int)(fraction * (height - 1));
        if (!prev) MoveToEx(dc, x, y, NULL);
        else LineTo(dc, x, y);
        prev = 1;
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

static int PlannerPeakHour(const ForecastDay *day, int category) {
    if (!day || day->firstHour < 0) return -1;
    int peak = -1;
    double max = NAN;
    for (int i = 0; i < day->hourCount; i++) {
        int h = day->firstHour + i;
        double value = PlannerHourlyValue(category, h, 0);
        if (!isnan(value) && (isnan(max) || value > max)) {
            max = value;
            peak = h;
        }
    }
    return peak;
}

static void PlannerDetailHeading(const ForecastDay *day, int category,
                                 wchar_t *out, int cap) {
    if (!day || !out || cap < 2) return;
    if (category == PLAN_RAIN) {
        double point = PlannerPointRainChance(&g_fc, &g_ens, day);
        if (!isnan(point) && !isnan(day->regionalPoP) && !isnan(day->rainSumIn))
            _snwprintf(out, cap - 1,
                       L"%s · Rain — point %d%% / regional %d%% · %.2f in total",
                       day->label, (int)llround(point * 100.0),
                       (int)llround(day->regionalPoP), day->rainSumIn);
        else if (!isnan(point) && !isnan(day->regionalPoP))
            _snwprintf(out, cap - 1, L"%s · Rain — point %d%% / regional %d%%",
                       day->label, (int)llround(point * 100.0), (int)llround(day->regionalPoP));
        else if (!isnan(day->regionalPoP))
            _snwprintf(out, cap - 1, L"%s · Rain — point odds unavailable / regional %d%%",
                       day->label, (int)llround(day->regionalPoP));
        else
            _snwprintf(out, cap - 1, L"%s · Rain — odds unavailable", day->label);
    } else if (category == PLAN_TEMP && !isnan(day->maxF) && !isnan(day->minF)) {
        _snwprintf(out, cap - 1, L"%s · Temperature — high %d° / low %d°",
                   day->label, (int)llround(day->maxF), (int)llround(day->minF));
    } else if (category == PLAN_CLOUD && !isnan(day->cloudMean)) {
        _snwprintf(out, cap - 1, L"%s · Cloud cover — average %d%%",
                   day->label, (int)llround(day->cloudMean));
    } else if (category == PLAN_WIND && !isnan(day->windMaxMph)) {
        _snwprintf(out, cap - 1, L"%s · Wind — peak %d mph",
                   day->label, (int)llround(day->windMaxMph));
    } else {
        _snwprintf(out, cap - 1, L"%s · %s — detail unavailable",
                   day->label, PlannerCategoryName(category));
    }
    out[cap - 1] = L'\0';
}

static void PlannerReading(const ForecastDay *day, int category,
                           wchar_t *out, int cap) {
    if (!day || !out || cap < 2) return;
    if (category == PLAN_RAIN) {
        double point = PlannerPointRainChance(&g_fc, &g_ens, day);
        if (!isnan(point) && !isnan(day->regionalPoP)) {
            if (point >= 0.50)
                _snwprintf(out, cap - 1,
                           L"The point ensemble favors measurable rain; regional coverage PoP is separate at %d%%.",
                           (int)llround(day->regionalPoP));
            else if (point >= 0.20)
                _snwprintf(out, cap - 1,
                           L"Rain is uncertain at this point; regional coverage PoP separately peaks at %d%%.",
                           (int)llround(day->regionalPoP));
            else
                _snwprintf(out, cap - 1,
                           L"The point-ensemble rain signal is low; the separate regional PoP peaks at %d%%.",
                           (int)llround(day->regionalPoP));
        } else if (!isnan(day->regionalPoP)) {
            _snwprintf(out, cap - 1,
                       L"Regional coverage PoP peaks at %d%%; exact-point ensemble odds are unavailable.",
                       (int)llround(day->regionalPoP));
        } else {
            wcscpy(out, L"Rain details are unavailable; point and regional odds remain separate.");
        }
    } else if (category == PLAN_TEMP && !isnan(day->maxF) && !isnan(day->minF)) {
        int peak = PlannerPeakHour(day, PLAN_TEMP);
        if (peak >= 0 && g_fc.hLabel[peak][0])
            _snwprintf(out, cap - 1,
                       L"Warmest forecast hour near %s; daily high %d° and low %d°.",
                       g_fc.hLabel[peak], (int)llround(day->maxF), (int)llround(day->minF));
        else
            _snwprintf(out, cap - 1, L"Expect a high of %d° and a low of %d°.",
                       (int)llround(day->maxF), (int)llround(day->minF));
    } else if (category == PLAN_CLOUD && !isnan(day->cloudMean)) {
        if (day->cloudMean >= 70.0)
            _snwprintf(out, cap - 1, L"Mostly cloudy, averaging %d%% cloud cover.",
                       (int)llround(day->cloudMean));
        else if (day->cloudMean >= 40.0)
            _snwprintf(out, cap - 1, L"A mix of sun and clouds, averaging %d%% cloud cover.",
                       (int)llround(day->cloudMean));
        else
            _snwprintf(out, cap - 1, L"Mostly clear, averaging %d%% cloud cover.",
                       (int)llround(day->cloudMean));
    } else if (category == PLAN_WIND && !isnan(day->windMaxMph)) {
        if (day->windMaxMph < 8.0)
            _snwprintf(out, cap - 1, L"Light winds, with an hourly peak near %d mph.",
                       (int)llround(day->windMaxMph));
        else if (day->windMaxMph < 18.0)
            _snwprintf(out, cap - 1, L"A gentle breeze, peaking near %d mph.",
                       (int)llround(day->windMaxMph));
        else if (day->windMaxMph < 28.0)
            _snwprintf(out, cap - 1, L"Breezy, with an hourly peak near %d mph.",
                       (int)llround(day->windMaxMph));
        else
            _snwprintf(out, cap - 1, L"Windy, with an hourly peak near %d mph.",
                       (int)llround(day->windMaxMph));
    } else {
        _snwprintf(out, cap - 1, L"Hourly %s details are unavailable for this day.",
                   PlannerCategoryName(category));
    }
    out[cap - 1] = L'\0';
}

static void PaintPlannerSamples(HDC dc, const ForecastDay *day, int category,
                                const RECT *row) {
    if (!day || day->firstHour < 0 || day->hourCount <= 0) return;
    int count = day->hourCount < 5 ? day->hourCount : 5;
    int width = row->right - row->left;
    for (int i = 0; i < count; i++) {
        int sample = count > 1 ? i * (day->hourCount - 1) / (count - 1) : 0;
        int h = day->firstHour + sample;
        const wchar_t *time = g_fc.hLabel[h][0] ? g_fc.hLabel[h] : L"--:--";
        wchar_t text[80];
        if (category == PLAN_RAIN) {
            double point = PlannerHourlyValue(PLAN_RAIN, h, 0);
            double regional = PlannerHourlyValue(PLAN_RAIN, h, 1);
            if (!isnan(point) && !isnan(regional))
                _snwprintf(text, 79, L"%s  P %d%% / R %d%%", time,
                           (int)llround(point), (int)llround(regional));
            else if (!isnan(point))
                _snwprintf(text, 79, L"%s  P %d%% / R --", time, (int)llround(point));
            else if (!isnan(regional))
                _snwprintf(text, 79, L"%s  P -- / R %d%%", time, (int)llround(regional));
            else
                _snwprintf(text, 79, L"%s  --", time);
        } else if (category == PLAN_TEMP && !isnan(g_fc.hT[h])) {
            _snwprintf(text, 79, L"%s  %d°", time, (int)llround(g_fc.hT[h]));
        } else if (category == PLAN_CLOUD && !isnan(g_fc.hCloud[h])) {
            _snwprintf(text, 79, L"%s  %d%%", time, (int)llround(g_fc.hCloud[h]));
        } else if (category == PLAN_WIND && !isnan(g_fc.hWindMph[h])) {
            _snwprintf(text, 79, L"%s  %d mph", time, (int)llround(g_fc.hWindMph[h]));
        } else {
            _snwprintf(text, 79, L"%s  --", time);
        }
        text[79] = L'\0';
        int left = row->left + i * width / count;
        int right = row->left + (i + 1) * width / count;
        RECT sampleRect = { left, row->top, right, row->bottom };
        DrawTextAt(dc, text, &sampleRect, g_fontSmall, COL_MUTED,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void PaintHourlyCard(HDC mem, const RECT *card, int dayIndex, int category) {
    DrawRoundedBox(mem, card, S(10), COL_INPUT, COL_DIV);
    if (dayIndex < 0 || dayIndex >= g_fc.dayN || dayIndex >= FC_DAYS) {
        RECT waiting = { card->left + S(8), card->top, card->right - S(8), card->bottom };
        DrawTextAt(mem, L"Waiting for the five-day forecast...", &waiting,
                   g_fontSmall, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    const ForecastDay *day = &g_fc.days[dayIndex];
    wchar_t heading[224], reading[224];
    PlannerDetailHeading(day, category, heading, 224);
    PlannerReading(day, category, reading, 224);
    RECT title = { card->left + S(8), card->top + S(2), card->right - S(8), card->top + S(18) };
    DrawTextAt(mem, heading, &title, g_fontSmall, COL_TXT,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (day->firstHour < 0 || day->hourCount <= 0) {
        RECT unavailable = { card->left + S(8), card->top + S(20), card->right - S(8), card->top + S(39) };
        DrawTextAt(mem, L"Hourly detail unavailable for this day.", &unavailable,
                   g_fontSmall, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT read = { card->left + S(8), card->top + S(54), card->right - S(8), card->bottom - S(3) };
        DrawTextAt(mem, reading, &read, g_fontSmall, COL_TXT,
                   DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        return;
    }
    RECT samples = { card->left + S(8), card->top + S(18), card->right - S(8), card->top + S(33) };
    PaintPlannerSamples(mem, day, category, &samples);
    RECT plot = { card->left + S(9), card->top + S(35), card->right - S(9), card->top + S(55) };
    HPEN grid = CreatePen(PS_SOLID, 1, COL_DIV);
    if (grid) {
        HGDIOBJ old = SelectObject(mem, grid);
        if (old && old != HGDI_ERROR) {
            int mid = plot.top + (plot.bottom - plot.top) / 2;
            MoveToEx(mem, plot.left, mid, NULL);
            LineTo(mem, plot.right, mid);
            SelectObject(mem, old);
        }
        DeleteObject(grid);
    }
    double low, high;
    PlannerTrendRange(day, category, &low, &high);
    if (category == PLAN_RAIN) {
        PaintPlannerSeries(mem, day, PLAN_RAIN, 0, &plot, low, high, COL_RAIN);
        PaintPlannerSeries(mem, day, PLAN_RAIN, 1, &plot, low, high, COL_WARN);
    } else {
        PaintPlannerSeries(mem, day, category, 0, &plot, low, high, COL_ACCENT);
    }
    RECT read = { card->left + S(8), card->top + S(56), card->right - S(8), card->bottom - S(3) };
    DrawTextAt(mem, reading, &read, g_fontSmall, COL_TXT,
               DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
}

static void PaintPlannerCard(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT title = { card->left + S(14), card->top + S(4), card->left + S(404), card->top + S(23) };
    DrawTextAt(mem, L"Five-day planner · select a day/category cell", &title,
               g_fontSmall, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT legend = { card->left + S(408), card->top + S(4), card->right - S(14), card->top + S(23) };
    DrawTextAt(mem, L"P = point ensemble (blue)  ·  R = regional PoP (gold)", &legend,
               g_fontSmall, COL_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (!g_fc.have || g_fc.dayN <= 0) {
        RECT waiting = { card->left + S(16), card->top + S(34), card->right - S(16), card->bottom - S(8) };
        DrawTextAt(mem, L"Waiting for the five-day forecast...", &waiting,
                   g_fontUI, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    static const int categoryOrder[PLAN_CATEGORY_COUNT] = {
        PLAN_RAIN, PLAN_TEMP, PLAN_CLOUD, PLAN_WIND
    };
    for (int day = 0; day < FC_DAYS; day++) {
        RECT header;
        if (PlannerDayRect(day, &header)) {
            const wchar_t *label = day < g_fc.dayN ? g_fc.days[day].label : L"—";
            DrawTextAt(mem, label, &header, g_fontSmall,
                       day == g_planDay ? COL_ACCENT : COL_MUTED,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        for (int row = 0; row < PLAN_CATEGORY_COUNT; row++) {
            int category = categoryOrder[row];
            RECT cell, rowLabel;
            if (!PlannerCellRect(day, category, &cell)) continue;
            rowLabel.left = card->left + S(14);
            rowLabel.right = card->left + S(110);
            rowLabel.top = cell.top;
            rowLabel.bottom = cell.bottom;
            if (day == 0)
                DrawTextAt(mem, PlannerCategoryName(category), &rowLabel,
                           g_fontSmall, COL_MUTED,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            wchar_t value[64];
            PlannerCellText(day, category, value, 64);
            int selected = day == g_planDay && category == g_planCategory;
            DrawRoundedBox(mem, &cell, S(5),
                           selected ? MixCol(COL_CARD, COL_ACCENT, 24) : COL_CARD,
                           selected ? COL_ACCENT : COL_CARD_BD);
            DrawTextAt(mem, value, &cell, g_fontSmall, COL_TXT,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    RECT detail = { card->left + S(12), card->top + S(130),
                    card->right - S(12), card->bottom - S(6) };
    PaintHourlyCard(mem, &detail, g_planDay, g_planCategory);
}

static void PaintRadarPane(HDC mem, const RECT *card) {
    DrawRoundedBox(mem, card, S(14), COL_CARD, COL_CARD_BD);
    RECT img;
    RadarImageRect(&img);

    EnterCriticalSection(&g_lock);
    uint32_t *view = g_view;
    int ready = g_viewReady;
    int note = g_radarNote;
    int displayZoom = g_zoom;
    int sourceZoom = g_baseKey[0];
    int sourceTx = g_baseKey[1];
    int sourceTy = g_baseKey[2];
    int frames = g_frameN;
    int fsel = g_frameSel;
    long long ftime = (frames > 0 && fsel < frames) ? g_frames[fsel].time : 0;
    int fkind = (frames > 0 && fsel < frames) ? g_frames[fsel].kind : 0;
    int forecastN = 0, forecastPos = 0;
    for (int i = 0; i < frames; i++) {
        if (g_frames[i].kind == 1) {
            forecastN++;
            if (i == fsel) forecastPos = forecastN;
        }
    }
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
        /* A centred local focus makes weather around the selected place large
           enough to read. Every increment beyond the actual available host
           level halves this crop, so zoom 9/10 visibly go further while still
           being an honest enlargement of the level-8/7 source. */
        int focusPx = RADAR_FOCUS_PX;
        int localSteps = displayZoom - sourceZoom;
        while (localSteps > 0 && focusPx > RADAR_FOCUS_MIN_PX) {
            focusPx /= 2;
            localSteps--;
        }
        if (focusPx < RADAR_FOCUS_MIN_PX) focusPx = RADAR_FOCUS_MIN_PX;
        int srcLeft = (GRID_PX - focusPx) / 2;
        int srcTop = (GRID_PX - focusPx) / 2;
        EnterCriticalSection(&g_lock);
        StretchDIBits(mem, d.left, d.top, side, side, srcLeft, srcTop,
                      focusPx, focusPx, g_view, &bi,
                      DIB_RGB_COLORS, SRCCOPY);
        LeaveCriticalSection(&g_lock);
        /* Crosshair on the exact place pixel, translated through the same
           focused crop used for the map so it cannot drift away from home. */
        if (hasPlace) {
            int tx = 0, ty = 0;
            int crosshairZoom = sourceZoom > 0 ? sourceZoom : RadarTileZoomForDisplay(displayZoom);
            GridFor(crosshairZoom, pl.lat, pl.lon, &tx, &ty);
            if (sourceZoom > 0) { tx = sourceTx; ty = sourceTy; }
            double px = 0, py = 0;
            PlacePixelInGrid(crosshairZoom, pl.lat, pl.lon, tx, ty, &px, &py);
            int cx = d.left + (int)((px - srcLeft) / (double)focusPx * (double)side);
            int cy = d.top + (int)((py - srcTop) / (double)focusPx * (double)side);
            if (cx >= d.left && cx < d.right && cy >= d.top && cy < d.bottom) {
                /* Two contrasting rings remain visible over both pale roads
                   and the strongest red/yellow precipitation echoes. */
                HPEN outer = CreatePen(PS_SOLID, S(3), RGB(20, 24, 34));
                HPEN inner = CreatePen(PS_SOLID, S(2), RGB(255, 255, 255));
                HGDIOBJ op = SelectObject(mem, outer);
                Ellipse(mem, cx - S(10), cy - S(10), cx + S(10), cy + S(10));
                SelectObject(mem, inner);
                MoveToEx(mem, cx - S(14), cy, NULL); LineTo(mem, cx - S(4), cy);
                MoveToEx(mem, cx + S(4), cy, NULL); LineTo(mem, cx + S(14), cy);
                MoveToEx(mem, cx, cy - S(14), NULL); LineTo(mem, cx, cy - S(4));
                MoveToEx(mem, cx, cy + S(4), NULL); LineTo(mem, cx, cy + S(14));
                Ellipse(mem, cx - S(6), cy - S(6), cx + S(6), cy + S(6));
                SelectObject(mem, op);
                DeleteObject(inner);
                DeleteObject(outer);
            }
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
        wchar_t step[32];
        if (forecastN > 0 && fkind && forecastPos > 0)
            _snwprintf(step, 31, L"Forecast %d/%d", forecastPos, forecastN);
        else if (forecastN > 0)
            _snwprintf(step, 31, L"Forecast · %d steps", forecastN);
        else
            wcscpy(step, L"No forecast frames");
        step[31] = L'\0';
        RECT fr = { img.left + S(340), img.top + S(6), img.left + S(510), img.top + S(28) };
        DrawRoundedBox(mem, &fr, S(8), COL_CARD, COL_CARD_BD);
        DrawTextAt(mem, step, &fr, g_fontSmall, forecastN > 0 ? COL_ACCENT : COL_MUTED,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (ready == 1 && view) {
        RECT nr = { img.left + S(8), img.top + S(32), img.left + S(330), img.top + S(54) };
        DrawTextAt(mem, L"No rain echoes in this frame.", &nr, g_fontSmall, COL_MUTED,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (ready == 2 && view) {
        /* RainViewer uses colour to communicate intensity.  Name that scale
           on the map so the coloured cells become an actionable answer rather
           than an unexplained overlay. */
        RECT legend = { img.left + S(8), img.bottom - S(30), img.left + S(316), img.bottom - S(8) };
        DrawRoundedBox(mem, &legend, S(8), COL_CARD, COL_CARD_BD);
        RECT sw = { legend.left + S(7), legend.top + S(6), legend.left + S(19), legend.bottom - S(6) };
        DrawRoundedBox(mem, &sw, S(3), RGB(70, 140, 245), RGB(70, 140, 245));
        sw.left += S(66); sw.right += S(66);
        DrawRoundedBox(mem, &sw, S(3), RGB(52, 190, 110), RGB(52, 190, 110));
        sw.left += S(66); sw.right += S(66);
        DrawRoundedBox(mem, &sw, S(3), RGB(251, 191, 36), RGB(251, 191, 36));
        sw.left += S(66); sw.right += S(66);
        DrawRoundedBox(mem, &sw, S(3), RGB(240, 82, 82), RGB(240, 82, 82));
        RECT lt = { legend.left + S(23), legend.top, legend.right - S(4), legend.bottom };
        DrawTextAt(mem, L"light     moderate     heavy     intense", &lt, g_fontSmall, COL_TXT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (note) {
        RECT zr = { img.left + S(520), img.top + S(6), img.right - S(8), img.top + S(28) };
        const wchar_t *zoomNote = note == 1
            ? L"radar source limited — using available imagery"
            : L"detail zoom — sourced from host level 8";
        DrawTextAt(mem, zoomNote, &zr, g_fontSmall, COL_WARN,
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

    /* The 50ms emblem timer invalidates only this small square. Avoid
       rebuilding the large radar DIB just to turn the suite mark a fraction. */
    RECT emblem;
    RadarEmblemRect(&emblem);
    if (ps.rcPaint.left >= emblem.left && ps.rcPaint.top >= emblem.top &&
        ps.rcPaint.right <= emblem.right && ps.rcPaint.bottom <= emblem.bottom) {
        int side = emblem.right - emblem.left;
        uint32_t *spinBits = NULL;
        BITMAPINFO spinBi;
        ZeroMemory(&spinBi, sizeof(spinBi));
        spinBi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        spinBi.bmiHeader.biWidth = side;
        spinBi.bmiHeader.biHeight = -side;
        spinBi.bmiHeader.biPlanes = 1;
        spinBi.bmiHeader.biBitCount = 32;
        spinBi.bmiHeader.biCompression = BI_RGB;
        HBITMAP spinDib = CreateDIBSection(dc, &spinBi, DIB_RGB_COLORS,
                                            (void **)&spinBits, NULL, 0);
        if (spinDib && spinBits) {
            for (int i = 0; i < side * side; i++) spinBits[i] = SuiteEmblemDibPixel(COL_BG);
            DrawWeatherRadarEmblem(spinBits, side, side, 0, 0, side, g_radarSpinAngle);
            HDC spinDc = CreateCompatibleDC(dc);
            HGDIOBJ spinOld = SelectObject(spinDc, spinDib);
            BitBlt(dc, emblem.left, emblem.top, side, side, spinDc, 0, 0, SRCCOPY);
            SelectObject(spinDc, spinOld);
            DeleteDC(spinDc);
            DeleteObject(spinDib);
        }
        EndPaint(hwnd, &ps);
        return;
    }

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

    /* header: the suite emblem is alive here too, not a dead app-icon copy. */
    RECT headerEmblem;
    RadarEmblemRect(&headerEmblem);
    RECT tt = { S(68), S(12), S(560), S(38) };
    DrawTextAt(mem, APP_TITLE_W, &tt, g_fontTitle, COL_TXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT stt = { S(68), S(36), S(560), S(56) };
    DrawTextAt(mem, L"Point rain odds + ETA from the ensemble, plus live radar", &stt,
               g_fontSmall, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawBtn(mem, BTN_REFRESH, L"Refresh", 0);
    if (g_placeSel >= 0 && g_placeSel < g_placeN) {
        RECT pn = { S(580), S(16), S(WIN_LW - 16 - 112), S(48) };
        DrawTextAt(mem, g_places[g_placeSel].name, &pn, g_fontTitle, COL_ACCENT,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    PaintLeftColumn(mem);

    /* Current conditions and exact-point rain remain at the top. The planner
       gives all four daily categories equal rows, with selected hourly detail;
       radar keeps its own lower pane and playback controls. */
    RECT nowCard = { S(260), S(70), S(WIN_LW - 16), S(172) };
    PaintNowCard(mem, &nowCard);
    RECT honestCard = { S(260), S(180), S(WIN_LW - 16), S(282) };
    PaintHonestCard(mem, &honestCard);
    RECT plannerCard = { S(260), S(290), S(WIN_LW - 16), S(508) };
    PaintPlannerCard(mem, &plannerCard);
    RECT radarCard = { S(260), S(512), S(WIN_LW - 16), S(WIN_LH - 56) };
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

    if (bits) {
        GdiFlush();
        DrawWeatherRadarEmblem(bits, W, H, headerEmblem.left, headerEmblem.top,
                               headerEmblem.right - headerEmblem.left, g_radarSpinAngle);
    }
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
    if (msg == SuiteTaskbarCreatedMessage()) {
        g_trayVisible = 0;
        WeatherTraySync();
        return 0;
    }
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
        if (id == ID_TRAY_OPEN) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (id == ID_TRAY_EXIT) {
            g_quitting = 1;
            DestroyWindow(hwnd);
            return 0;
        }
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
    case WM_TRAYICON: {
        UINT event = LOWORD((DWORD)lp);
        if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            ShowWeatherTrayMenu(hwnd);
        }
        return 0;
    }
    case WM_CLOSE:
        if (!g_quitting) {
            ShowWindow(hwnd, SW_HIDE);
            WeatherTrayHintOnce();
            return 0;
        }
        DestroyWindow(hwnd);
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
            case BTN_ZOOM: {
                int oldTileZoom, newTileZoom;
                EnterCriticalSection(&g_lock);
                oldTileZoom = RadarTileZoomForDisplay(g_zoom);
                g_zoom = (g_zoom >= RADAR_DISPLAY_ZOOM_MAX) ? RADAR_TILE_ZOOM_MIN : g_zoom + 1;
                newTileZoom = RadarTileZoomForDisplay(g_zoom);
                /* Levels 9 and 10 intentionally reuse level-8 imagery as a
                   local detail crop. They update instantly instead of issuing
                   an unsupported host request. */
                if (newTileZoom == oldTileZoom && g_baseKey[0] == newTileZoom)
                    g_radarNote = g_zoom > newTileZoom ? 2 : 0;
                LeaveCriticalSection(&g_lock);
                if (oldTileZoom != newTileZoom) RequestTiles();
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            }
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
        int planDay, planCategory;
        if (HitPlannerCell(x, y, &planDay, &planCategory)) {
            g_planDay = planDay;
            g_planCategory = planCategory;
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
            /* Calm idle motion: one complete turn every thirty seconds. */
            g_radarSpinAngle += 6.283185307179586 / 600.0;
            if (g_radarSpinAngle > 6.283185307179586)
                g_radarSpinAngle -= 6.283185307179586;
            RECT emblem;
            RadarEmblemRect(&emblem);
            InvalidateRect(hwnd, &emblem, FALSE);
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
        if (wp == TIMER_TRAY_SYNC) {
            WeatherTraySync();
            return 0;
        }
        return 0;
    case WM_APP_DATA: {
        int kind = (int)wp;
        DWORD ticket = (DWORD)(uintptr_t)lp;
        EnterCriticalSection(&g_lock);
        if (kind == JOB_FORECAST) {
            int current = (ticket == g_placeTicket);
            if (current) {
                g_fc = g_fcStage;
                if (g_fc.dayN > 0 && g_planDay >= g_fc.dayN) g_planDay = 0;
            }
            wchar_t m[160];
            _snwprintf(m, 159, L"Forecast for %s updated at %s.",
                       (g_placeSel >= 0 && g_placeSel < g_placeN) ? g_places[g_placeSel].name : L"the place",
                       g_fc.fetched);
            m[159] = L'\0';
            LeaveCriticalSection(&g_lock);
            if (!current) return 0; /* an old location finished after a new selection */
            StatusSet(m, 1);
            RainAlertConsider(CurrentPlace());
        } else if (kind == JOB_ENSEMBLE) {
            int current = (ticket == g_placeTicket);
            if (current) g_ens = g_ensStage;
            LeaveCriticalSection(&g_lock);
            if (!current) return 0; /* never mix ensemble odds between places */
            RainAlertConsider(CurrentPlace());
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
        KillTimer(hwnd, TIMER_TRAY_SYNC);
        WeatherTrayRemove();
        if (g_inputBrush) { DeleteObject(g_inputBrush); g_inputBrush = NULL; }
        if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = NULL; }
        ReleaseWeatherRadarEmblem();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
