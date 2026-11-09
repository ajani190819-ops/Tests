/* ---------------------------------------------------------------------------
 *  UI reconciliation
 *
 *  The old refresh_ui() destroyed and recreated all 32 endpoint and 128 session
 *  controls on every call -- and WM_TIMER called it every two seconds. Dragging
 *  a volume slider therefore had the slider destroyed underneath the mouse,
 *  focus was lost, and the panel flickered twice a second forever.
 *
 *  Now the control tree is rebuilt ONLY when the set of devices or apps
 *  actually changes (detected by a hash over their identities). Otherwise the
 *  existing controls are updated in place, and a slider the user is currently
 *  dragging is left alone: a trackbar holds the mouse capture while its thumb
 *  is down, so GetCapture() is a reliable "hands off" signal.
 * ------------------------------------------------------------------------- */

static uint64_t g_ui_endpoint_sig = 0;
static uint64_t g_ui_session_sig  = 0;

/* Layout captured while building, so WM_PAINT can draw section headers and the
 * footer status line at the same positions the controls actually occupy. */
static int g_y_outputs_header = 0;
static int g_y_apps_header    = 0;
static int g_y_footer         = 0;
static int g_truncated_endpoints = 0;
static int g_truncated_sessions  = 0;

/* Card rectangles, captured at build time so WM_PAINT can draw the card
 * background behind each row at exactly the geometry the controls use. */
static RECT g_ep_card[32];
static RECT g_ss_card[128];
static int  g_ep_rows = 0;
static int  g_ss_rows = 0;

/* Scroll state, all in logical ROWS. The column is the endpoint list followed
   by the session list; g_scroll_offset is how many rows have scrolled off the
   top. Endpoints exhaust first (g_ep_scroll_max of them), then sessions
   (g_ss_scroll_max), so g_scroll_range is their sum. g_ep_max / g_ss_max are how
   many rows of each section fit on screen. Control slots are created once and
   then re-aimed at a different logical row on scroll, so scrolling never
   destroys a control the user is dragging. */
static int  g_scroll_offset = 0;
static int  g_scroll_range  = 0;
static int  g_viewport_h    = 0;
static int  g_ep_max        = 0;
static int  g_ss_max        = 0;
static int  g_ep_scroll_max = 0;
static int  g_ss_scroll_max = 0;

static uint64_t ui_hash_wstr(uint64_t h, const wchar_t *s) {
    for (; s && *s; s++) { h ^= (uint64_t)*s; h *= 1099511628211ULL; }
    return h;
}

static uint64_t ui_mix(uint64_t h, uint64_t v) {
    return (h ^ v) * 1099511628211ULL;
}

/* Identity only -- deliberately excludes volume and mute, which change
 * constantly and must be updated in place rather than trigger a rebuild. */
static uint64_t endpoints_signature(void) {
    uint64_t h = 1469598103934665603ULL;
    h = ui_mix(h, (uint64_t)g_endpoint_count);
    for (int i = 0; i < g_endpoint_count; i++) {
        h = ui_hash_wstr(h, g_endpoints[i].id);
        h = ui_mix(h, (uint64_t)(g_endpoints[i].is_default ? 1 : 0));
        h = ui_mix(h, (uint64_t)(g_endpoints[i].vol ? 1 : 0));
    }
    return h;
}

static uint64_t sessions_signature(void) {
    uint64_t h = 1469598103934665603ULL;
    h = ui_mix(h, (uint64_t)g_session_count);
    for (int i = 0; i < g_session_count; i++) {
        h = ui_mix(h, (uint64_t)g_sessions[i].process_id);
        h = ui_hash_wstr(h, g_sessions[i].key);
        h = ui_hash_wstr(h, g_sessions[i].name);
        h = ui_mix(h, (uint64_t)(g_sessions[i].svol ? 1 : 0));
        h = ui_mix(h, (uint64_t)(g_sessions[i].on_other_device ? 1 : 0));
    }
    return h;
}

static void ui_destroy_all(void) {
    /* A hovered button that is about to be destroyed would otherwise leave
     * g_hover_btn pointing at a handle Windows may hand to the next button,
     * making an unrelated button draw itself as hovered. */
    g_hover_btn = NULL;

    for (int i = 0; i < 32; i++) {
        if (g_endpoint_ctrls[i].hwnd_btn)    DestroyWindow(g_endpoint_ctrls[i].hwnd_btn);
        if (g_endpoint_ctrls[i].hwnd_vol)    DestroyWindow(g_endpoint_ctrls[i].hwnd_vol);
        if (g_endpoint_ctrls[i].hwnd_mute)   DestroyWindow(g_endpoint_ctrls[i].hwnd_mute);
        if (g_endpoint_ctrls[i].hwnd_label)  DestroyWindow(g_endpoint_ctrls[i].hwnd_label);
        if (g_endpoint_ctrls[i].hwnd_voltxt) DestroyWindow(g_endpoint_ctrls[i].hwnd_voltxt);
        memset(&g_endpoint_ctrls[i], 0, sizeof(g_endpoint_ctrls[i]));
        g_endpoint_ctrls[i].endpoint_idx = -1;
    }
    for (int i = 0; i < 128; i++) {
        if (g_session_ctrls[i].hwnd_label)  DestroyWindow(g_session_ctrls[i].hwnd_label);
        if (g_session_ctrls[i].hwnd_vol)    DestroyWindow(g_session_ctrls[i].hwnd_vol);
        if (g_session_ctrls[i].hwnd_mute)   DestroyWindow(g_session_ctrls[i].hwnd_mute);
        if (g_session_ctrls[i].hwnd_voltxt) DestroyWindow(g_session_ctrls[i].hwnd_voltxt);
        memset(&g_session_ctrls[i], 0, sizeof(g_session_ctrls[i]));
        g_session_ctrls[i].session_idx = -1;
    }
}

/* Layout is computed against a fixed budget, NOT against the current client
 * height, and the window is then sized to the result. Deriving rows from the
 * live client height would feed the resize back into the next layout and
 * oscillate. What the budget cannot show is scrolled to instead of dropped:
 * the old code simply never created those rows, which is how the bottom of the
 * app list ended up under the taskbar. */
#define UI_MAX_CLIENT   940
#define UI_MIN_APP_ROWS 5
#define UI_EP_ROW       72
#define UI_SS_ROW       56
#define UI_HDR_ROW      34
#define UI_GAP_ROW      18
#define UI_FOOTER_ROW   40

static int g_row_ep_h = 0, g_row_ss_h = 0, g_row_gap = 0, g_row_hdr = 0;

/* One scrollable column: endpoint rows first, then the APP VOLUME header, then
   session rows. The bands sit at fixed screen positions; scrolling re-aims each
   slot at a different logical row (ep_start / ss_start) rather than sliding the
   band, so a slider the user is dragging never moves under the cursor. */
static void ui_row_geometry(int logical_ep_row, int logical_ss_row,
                            int *out_ep_y, int *out_apps_y, int *out_ss_y) {
    int top = S(PANEL_Y) + g_row_hdr;
    *out_ep_y = top + logical_ep_row * g_row_ep_h;
    *out_apps_y = top + g_ep_max * g_row_ep_h + g_row_gap;
    *out_ss_y = *out_apps_y + g_row_hdr + logical_ss_row * g_row_ss_h;
}

/* How many rows fit and how far the scrollbar travels. APP VOLUME keeps a
   guaranteed minimum: a long device list must never squeeze it to nothing,
   because zero visible app rows is painted as "no apps are playing audio". */
static void ui_layout_budget(int client_h) {
    g_row_ep_h = S(UI_EP_ROW);
    g_row_ss_h = S(UI_SS_ROW);
    g_row_gap  = S(UI_GAP_ROW);
    g_row_hdr  = S(UI_HDR_ROW);

    int footer = S(UI_FOOTER_ROW);
    int viewport = client_h - footer - (S(PANEL_Y) + g_row_hdr);
    if (viewport < g_row_ss_h * 2) viewport = g_row_ss_h * 2;
    g_viewport_h = viewport;

    g_ep_max = viewport / g_row_ep_h;
    if (g_ep_max > g_endpoint_count) g_ep_max = g_endpoint_count;
    if (g_ep_max > 32) g_ep_max = 32;

    int ss_room = (viewport - (g_ep_max * g_row_ep_h + g_row_gap + g_row_hdr)) / g_row_ss_h;
    int ss_min = g_session_count < UI_MIN_APP_ROWS ? g_session_count : UI_MIN_APP_ROWS;
    g_ss_max = ss_room > ss_min ? ss_room : ss_min;
    if (g_ss_max > g_session_count) g_ss_max = g_session_count;
    if (g_ss_max > 128) g_ss_max = 128;

    /* Rows nobody could ever reach because the control arrays end here. */
    g_truncated_endpoints = g_endpoint_count > 32 ? g_endpoint_count - 32 : 0;
    g_truncated_sessions  = g_session_count > 128 ? g_session_count - 128 : 0;

    /* Travel is counted in ROWS to match the re-aim model: the top of the
       column shows g_ep_max endpoints and g_ss_max sessions, and whatever does
       not fit scrolls. Endpoints exhaust first, then sessions. */
    g_ep_scroll_max = g_endpoint_count > g_ep_max ? g_endpoint_count - g_ep_max : 0;
    g_ss_scroll_max = g_session_count  > g_ss_max ? g_session_count  - g_ss_max : 0;
    g_scroll_range  = g_ep_scroll_max + g_ss_scroll_max;
}

static void ui_apply_viewport(void);
static void ui_rebuild(void);

/* Re-aim the existing controls at the rows the current scroll offset shows.
   The handles survive, so focus and an in-flight slider drag survive too. */
static void ui_apply_viewport(void) {
    int ep_y0 = 0, apps_y = 0, ss_y0 = 0;
    ui_row_geometry(0, 0, &ep_y0, &apps_y, &ss_y0);
    int ep_start = g_scroll_offset < g_ep_scroll_max ? g_scroll_offset : g_ep_scroll_max;
    int ss_start = g_scroll_offset - ep_start;

    g_y_outputs_header = ep_y0 - g_row_hdr;
    g_y_apps_header    = apps_y;

    int right  = S(PANEL_X) + S(PANEL_W) - S(14);
    int def_w = S(104), mute_w = S(72), pct_w = S(46), g10 = S(10);
    int def_x  = right - def_w;
    int mute_x = def_x - g10 - mute_w;
    int pct_x  = mute_x - g10 - pct_w;
    int sl_x   = S(PANEL_X) + S(16);
    int sl_w   = pct_x - S(10) - sl_x;
    UINT sw = SWP_NOZORDER | SWP_NOACTIVATE;

    for (int i = 0; i < g_ep_rows; i++) {
        EndpointCtrl *c = &g_endpoint_ctrls[i];
        int logical = ep_start + i;
        int y = ep_y0 + i * g_row_ep_h;
        c->endpoint_idx = (logical < g_endpoint_count) ? logical : -1;
        SetRect(&g_ep_card[i], S(PANEL_X), y, S(PANEL_X) + S(PANEL_W),
                y + g_row_ep_h - S(8));
        BOOL vis = (c->endpoint_idx >= 0);
        if (c->hwnd_label) {
            SetWindowPos(c->hwnd_label, NULL, S(PANEL_X) + S(16), y + S(9),
                         S(PANEL_W) - S(32), S(20), sw);
            ShowWindow(c->hwnd_label, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_vol) {
            SetWindowPos(c->hwnd_vol, NULL, sl_x, y + S(36), sl_w, S(26), sw);
            ShowWindow(c->hwnd_vol, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_voltxt) {
            SetWindowPos(c->hwnd_voltxt, NULL, pct_x, y + S(39), pct_w, S(18), sw);
            ShowWindow(c->hwnd_voltxt, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_mute) {
            SetWindowPos(c->hwnd_mute, NULL, mute_x, y + S(34), mute_w, S(26), sw);
            ShowWindow(c->hwnd_mute, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_btn) {
            SetWindowPos(c->hwnd_btn, NULL, def_x, y + S(34), def_w, S(26), sw);
            ShowWindow(c->hwnd_btn, vis ? SW_SHOW : SW_HIDE);
        }
    }

    for (int i = 0; i < g_ss_rows; i++) {
        SessionCtrl *c = &g_session_ctrls[i];
        int logical = ss_start + i;
        int y = ss_y0 + i * g_row_ss_h;
        c->session_idx = (logical < g_session_count) ? logical : -1;
        SetRect(&g_ss_card[i], S(PANEL_X), y, S(PANEL_X) + S(PANEL_W),
                y + g_row_ss_h - S(6));
        BOOL vis = (c->session_idx >= 0);
        if (c->hwnd_label) {
            SetWindowPos(c->hwnd_label, NULL, S(PANEL_X) + S(16), y + S(7),
                         S(PANEL_W) - S(32), S(18), sw);
            ShowWindow(c->hwnd_label, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_vol) {
            SetWindowPos(c->hwnd_vol, NULL, sl_x, y + S(27), sl_w, S(24), sw);
            ShowWindow(c->hwnd_vol, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_voltxt) {
            SetWindowPos(c->hwnd_voltxt, NULL, pct_x, y + S(30), pct_w, S(18), sw);
            ShowWindow(c->hwnd_voltxt, vis ? SW_SHOW : SW_HIDE);
        }
        if (c->hwnd_mute) {
            SetWindowPos(c->hwnd_mute, NULL, mute_x, y + S(25), mute_w, S(24), sw);
            ShowWindow(c->hwnd_mute, vis ? SW_SHOW : SW_HIDE);
        }
    }

    /* The footer is pinned to the bottom of the client area, never scrolled. */
    RECT cr;
    GetClientRect(g_hwnd, &cr);
    g_y_footer = cr.bottom - S(UI_FOOTER_ROW);

    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = g_scroll_range;
    si.nPage  = (UINT)(g_ep_max + g_ss_max > 0 ? g_ep_max + g_ss_max : 1);
    si.nPos   = g_scroll_offset;
    SetScrollInfo(g_hwnd, SB_VERT, &si, TRUE);
    ShowScrollBar(g_hwnd, SB_VERT, g_scroll_range > 0 ? TRUE : FALSE);
}

/* Full rebuild: budget the rows, recreate the control slots, publish the
   scrollbar, then size the window to the content AND to the screen. */
static void ui_rebuild(void) {
    if (!g_hwnd) return;
    ui_destroy_all();

    RECT wr, cr;
    GetWindowRect(g_hwnd, &wr);
    GetClientRect(g_hwnd, &cr);
    int frame_h = (wr.bottom - wr.top) - cr.bottom;
    int frame_w = (wr.right - wr.left) - cr.right;
    if (frame_h <= 0 || frame_w <= 0) {
        frame_h = GetSystemMetrics(SM_CYCAPTION) + 2 * GetSystemMetrics(SM_CYFRAME);
        frame_w = 2 * GetSystemMetrics(SM_CXFRAME);
    }

    int client_h = cr.bottom > 0 ? cr.bottom : S(UI_MAX_CLIENT);
    if (client_h > S(UI_MAX_CLIENT)) client_h = S(UI_MAX_CLIENT);
    ui_layout_budget(client_h);
    if (g_scroll_offset > g_scroll_range) g_scroll_offset = g_scroll_range;
    if (g_scroll_offset < 0) g_scroll_offset = 0;

    g_ep_rows = g_ep_max;
    g_ss_rows = g_ss_max;

    /* Content height is what the window wants to be; the screen decides how
       much of it is visible and the scrollbar covers the rest. */
    int content = S(PANEL_Y) + g_row_hdr
                + g_ep_rows * g_row_ep_h + g_row_gap + g_row_hdr
                + g_ss_rows * g_row_ss_h + S(10) + S(UI_FOOTER_ROW);
    if (content < S(460)) content = S(460);
    if (content > S(UI_MAX_CLIENT)) content = S(UI_MAX_CLIENT);

    int want_w = S(PANEL_X) * 2 + S(PANEL_W) + frame_w;
    HMONITOR mon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
    /* MONITORINFO, not MONITORINFOW: mingw's winuser.h only typedefs the
       unsuffixed struct, and it has no string member to be wide about. */
    MONITORINFO mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    BOOL haveMon = GetMonitorInfoW(mon, &mi) ? TRUE : FALSE;
    int avail = S(UI_MAX_CLIENT);
    if (haveMon) {
        avail = (mi.rcWork.bottom - mi.rcWork.top) - frame_h - S(8);
        if (avail < S(320)) avail = S(320);
    }
    int want_h = content + frame_h;
    if (want_h > avail) want_h = avail;

    /* Claim the scrollbar's width only when there is something to scroll. */
    ui_layout_budget(want_h - frame_h);
    if (g_scroll_range > 0) want_w += GetSystemMetrics(SM_CXVSCROLL);

    /* Keep the whole window inside the work area: a window taller than the
       screen is exactly how the bottom of the app list became invisible. */
    int x = wr.left, y = wr.top;
    if (haveMon) {
        if (want_w > mi.rcWork.right - mi.rcWork.left)
            want_w = mi.rcWork.right - mi.rcWork.left;
        if (want_h > mi.rcWork.bottom - mi.rcWork.top)
            want_h = mi.rcWork.bottom - mi.rcWork.top;
        if (x + want_w > mi.rcWork.right)  x = mi.rcWork.right - want_w;
        if (y + want_h > mi.rcWork.bottom) y = mi.rcWork.bottom - want_h;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top)  y = mi.rcWork.top;
    }
    SetWindowPos(g_hwnd, NULL, x, y, want_w, want_h,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    GetClientRect(g_hwnd, &cr);
    ui_layout_budget(cr.bottom);
    if (g_scroll_offset > g_scroll_range) g_scroll_offset = g_scroll_range;
    g_ep_rows = g_ep_max;
    g_ss_rows = g_ss_max;

    int ep_y0 = 0, apps_y = 0, ss_y0 = 0;
    ui_row_geometry(0, 0, &ep_y0, &apps_y, &ss_y0);
    int ep_start = g_scroll_offset < g_ep_scroll_max ? g_scroll_offset : g_ep_scroll_max;
    int ss_start = g_scroll_offset - ep_start;

    int right  = S(PANEL_X) + S(PANEL_W) - S(14);
    int def_w = S(104), mute_w = S(72), pct_w = S(46), g10 = S(10);
    int def_x  = right - def_w;
    int mute_x = def_x - g10 - mute_w;
    int pct_x  = mute_x - g10 - pct_w;
    int sl_x   = S(PANEL_X) + S(16);
    int sl_w   = pct_x - S(10) - sl_x;

    for (int i = 0; i < g_ep_rows; i++) {
        EndpointCtrl *c = &g_endpoint_ctrls[i];
        int logical = ep_start + i;
        int y = ep_y0 + i * g_row_ep_h;
        BOOL has = (logical < g_endpoint_count);
        c->endpoint_idx = has ? logical : -1;
        SetRect(&g_ep_card[i], S(PANEL_X), y, S(PANEL_X) + S(PANEL_W),
                y + g_row_ep_h - S(8));

        c->hwnd_label = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | (has ? WS_VISIBLE : 0),
            S(PANEL_X) + S(16), y + S(9), S(PANEL_W) - S(32), S(20),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_label, WM_SETFONT, (WPARAM)g_font_head, TRUE);

        c->hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | (has ? WS_VISIBLE : 0)
                | (has && g_endpoints[logical].vol ? 0 : WS_DISABLED),
            sl_x, y + S(36), sl_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(1000 + i), g_hInst, NULL);
        SendMessageW(c->hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        SetWindowSubclass(c->hwnd_vol, TrackbarSkinProc, 0, (DWORD_PTR)1);

        c->hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_RIGHT | (has ? WS_VISIBLE : 0),
            pct_x, y + S(39), pct_w, S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_voltxt, WM_SETFONT, (WPARAM)g_font_small, TRUE);

        c->hwnd_mute = CreateWindowExW(
            0, L"BUTTON", L"Mute",
            WS_CHILD | BS_OWNERDRAW | (has ? WS_VISIBLE : 0),
            mute_x, y + S(34), mute_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(2000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_mute, BtnHoverProc, 0, 0);

        c->hwnd_btn = CreateWindowExW(
            0, L"BUTTON", L"Set Default",
            WS_CHILD | BS_OWNERDRAW | (has ? WS_VISIBLE : 0),
            def_x, y + S(34), def_w, S(26),
            g_hwnd, (HMENU)(INT_PTR)(3000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_btn, BtnHoverProc, 0, 0);
    }

    for (int i = 0; i < g_ss_rows; i++) {
        SessionCtrl *c = &g_session_ctrls[i];
        int logical = ss_start + i;
        int y = ss_y0 + i * g_row_ss_h;
        BOOL has = (logical < g_session_count);
        c->session_idx = has ? logical : -1;
        SetRect(&g_ss_card[i], S(PANEL_X), y, S(PANEL_X) + S(PANEL_W),
                y + g_row_ss_h - S(6));

        c->hwnd_label = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | (has ? WS_VISIBLE : 0),
            S(PANEL_X) + S(16), y + S(7), S(PANEL_W) - S(32), S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_label, WM_SETFONT, (WPARAM)g_font_ui, TRUE);

        c->hwnd_vol = CreateWindowExW(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | (has ? WS_VISIBLE : 0),
            sl_x, y + S(27), sl_w, S(24),
            g_hwnd, (HMENU)(INT_PTR)(4000 + i), g_hInst, NULL);
        SendMessageW(c->hwnd_vol, TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        SetWindowSubclass(c->hwnd_vol, TrackbarSkinProc, 0, (DWORD_PTR)1);

        c->hwnd_voltxt = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_RIGHT | (has ? WS_VISIBLE : 0),
            pct_x, y + S(30), pct_w, S(18),
            g_hwnd, NULL, g_hInst, NULL);
        SendMessageW(c->hwnd_voltxt, WM_SETFONT, (WPARAM)g_font_small, TRUE);

        c->hwnd_mute = CreateWindowExW(
            0, L"BUTTON", L"Mute",
            WS_CHILD | BS_OWNERDRAW | (has ? WS_VISIBLE : 0),
            mute_x, y + S(25), mute_w, S(24),
            g_hwnd, (HMENU)(INT_PTR)(5000 + i), g_hInst, NULL);
        SetWindowSubclass(c->hwnd_mute, BtnHoverProc, 0, 0);
    }

    ui_apply_viewport();

    /* This rebuild reflects the current sets, so the next refresh_ui() updates
       values in place instead of tearing the tree down a second time. */
    g_ui_endpoint_sig = endpoints_signature();
    g_ui_session_sig  = sessions_signature();
}


static void ui_update_values(void) {
    wchar_t label[320];

    for (int i = 0; i < 32; i++) {
        EndpointCtrl *c = &g_endpoint_ctrls[i];
        if (!c->hwnd_vol) continue;
        int idx = c->endpoint_idx;
        if (idx < 0 || idx >= g_endpoint_count) continue;

        BOOL is_def = (idx == g_default_endpoint_idx);
        _snwprintf(label, 320, L"%s%s", g_endpoints[idx].name,
                   is_def ? L"   \u25CF current output" : L"");
        label[319] = L'\0';
        if (c->hwnd_label) SetWindowTextW(c->hwnd_label, label);

        /* Leave a slider alone while the user is dragging it. */
        if (GetCapture() != c->hwnd_vol) {
            if (g_endpoints[idx].vol) {
                float v = endpoint_get_volume(idx);
                int pct = (int)(v * 100.0f + 0.5f);
                SendMessageW(c->hwnd_vol, TBM_SETPOS, TRUE, pct);
                if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, v);
            } else if (c->hwnd_voltxt) {
                /* No IAudioEndpointVolume means we genuinely cannot read this
                 * device's level. Showing 0% would be a lie the user can hear. */
                SetWindowTextW(c->hwnd_voltxt, L"\u2014");
            }
        }
        if (c->hwnd_mute)
            SetWindowTextW(c->hwnd_mute, endpoint_get_mute(idx) ? L"Unmute" : L"Mute");
        if (c->hwnd_btn)
            SetWindowTextW(c->hwnd_btn, is_def ? L"Default" : L"Set Default");
    }

    for (int i = 0; i < 128; i++) {
        SessionCtrl *c = &g_session_ctrls[i];
        if (!c->hwnd_vol) continue;
        int idx = c->session_idx;
        if (idx < 0 || idx >= g_session_count) continue;

        if (c->hwnd_label) SetWindowTextW(c->hwnd_label, g_sessions[idx].name);
        if (GetCapture() != c->hwnd_vol) {
            SendMessageW(c->hwnd_vol, TBM_SETPOS, TRUE,
                         (int)(g_sessions[idx].volume * 100.0f + 0.5f));
            if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, g_sessions[idx].volume);
        }
        if (c->hwnd_mute)
            SetWindowTextW(c->hwnd_mute, g_sessions[idx].mute ? L"Unmute" : L"Mute");
    }
}

static void refresh_ui(void) {
    if (!g_hwnd) return;

    uint64_t esig = endpoints_signature();
    uint64_t ssig = sessions_signature();

    if (esig != g_ui_endpoint_sig || ssig != g_ui_session_sig) {
        ui_rebuild();
        g_ui_endpoint_sig = esig;
        g_ui_session_sig  = ssig;
    }

    ui_update_values();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* Footer status line. Reports what actually happened rather than implying
 * success: a switch that Windows refused must say so, because the old code
 * repainted a "Default" badge over a device it had never switched to. */
static void ui_status_text(wchar_t *out, size_t cap) {
    wchar_t detail[256];

    if (g_last_switch_idx >= 0 && FAILED(g_last_switch_hr)) {
        _snwprintf(detail, 256,
                   L"Switch FAILED (0x%08lX) \u2014 audio is still on the previous device",
                   (unsigned long)g_last_switch_hr);
    } else if (g_policy_variant == 0) {
        /* Probed at startup, so this is Windows' actual refusal code, not a
         * guess. The code is what makes it actionable. */
        _snwprintf(detail, 256,
                   L"Device switching unavailable: IPolicyConfig 0x%08lX (run --diag)",
                   (unsigned long)g_policy_hr);
    } else if (g_last_switch_idx >= 0 && g_last_switch_idx < g_endpoint_count) {
        _snwprintf(detail, 256, L"Switched to %s", g_endpoints[g_last_switch_idx].name);
    } else {
        _snwprintf(detail, 256, L"%d output(s)  \u00B7  %d app(s)  \u00B7  %d remembered level(s)",
                   g_endpoint_count, g_session_count, g_remembered_count);
    }
    detail[255] = L'\0';

    int n = _snwprintf(out, cap, L"%s", detail);
    if (n < 0) n = 0;
    if (g_truncated_endpoints > 0 || g_truncated_sessions > 0) {
        _snwprintf(out + n, cap - (size_t)n, L"   \u00B7  %d row(s) past the list limit",
                   g_truncated_endpoints + g_truncated_sessions);
    }
    out[cap - 1] = L'\0';
}


static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_app_icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_ICON_APP));
        if (!g_app_icon) g_app_icon = LoadIconW(NULL, IDI_APPLICATION);

        ZeroMemory(&g_nid, sizeof(g_nid));
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_app_icon;
        wcscpy(g_nid.szTip, L"Audio Switcher - per-app volume & output switching");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        ui_fonts_create();

        g_btn_refresh = CreateWindowExW(
            0, L"BUTTON", L"\u21BB  Refresh",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            S(PANEL_X), S(PANEL_Y) - S(38), S(104), S(28),
            hwnd, (HMENU)IDM_REFRESH, g_hInst, NULL);
        SetWindowSubclass(g_btn_refresh, BtnHoverProc, 0, 0);

        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        com_create_enumerator();
        levels_path_init();
        levels_load();
        endpoints_refresh();
        sessions_refresh();

        /* Probe IPolicyConfig once at startup, not lazily on first click. The
         * status line reports g_policy_variant, so leaving it 0 until someone
         * pressed a button made the footer claim "switching unavailable" on
         * machines where it had simply never been attempted -- a false alarm
         * that reads as a broken app. Attempting it here means the warning, if
         * it still appears, is a genuine HRESULT from Windows. */
        policy_activate();

        /* First real layout: sizes the window to the content and to the
           screen, and publishes the scrollbar the lists need. */
        ui_rebuild();

        SetTimer(hwnd, 1, 2000, NULL);
        /* Emblem spin timer - 50ms for smooth rotation */
        SetTimer(hwnd, 2, 50, NULL);
        return 0;
    }

    case WM_DESTROY:
        g_quitting = TRUE;
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        sessions_cleanup();
        endpoints_cleanup();
        com_release_enumerator();
        ui_fonts_destroy();
        CoUninitialize();
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open Audio Switcher");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_QUIT, L"Quit Audio Switcher");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                           pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lp == WM_LBUTTONDBLCLK) {
            if (!IsWindowVisible(hwnd))
                ShowWindow(hwnd, SW_SHOWNORMAL);
            else
                ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    }

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) { ShowWindow(hwnd, SW_HIDE); return 0; }
        /* WM_SIZE arrives before WM_CREATE, so g_hwnd may still be NULL. */
        if (!g_hwnd) return 0;
        {
            /* A resized window fits a different number of rows. Rebuild only
               when that number changes, so dragging the frame does not destroy
               the control tree on every pixel of the drag. */
            RECT cr;
            GetClientRect(hwnd, &cr);
            int before = g_ep_max * 1000 + g_ss_max;
            ui_layout_budget(cr.bottom);
            if (before != g_ep_max * 1000 + g_ss_max) ui_rebuild();
            else ui_apply_viewport();
        }
        return 0;

    case WM_DPICHANGED: {
        /* Per-monitor DPI: moving to another screen changes every scaled size,
           so the fonts and the whole layout have to follow. */
        g_dpi = (int)LOWORD(wp);
        if (g_dpi < 48) g_dpi = 96;
        ui_fonts_destroy();
        ui_fonts_create();
        ui_rebuild();
        const RECT *sug = (const RECT *)lp;
        if (sug) {
            SetWindowPos(hwnd, NULL, sug->left, sug->top,
                         sug->right - sug->left, sug->bottom - sug->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_VSCROLL: {
        if ((HWND)lp != hwnd) return 0;      /* a child control, not the panel */
        int pos = g_scroll_offset;
        int page = (g_ep_max + g_ss_max) > 1 ? (g_ep_max + g_ss_max) - 1 : 1;
        switch (LOWORD(wp)) {
        case SB_LINEUP:   pos -= 1; break;
        case SB_LINEDOWN: pos += 1; break;
        case SB_PAGEUP:   pos -= page; break;
        case SB_PAGEDOWN: pos += page; break;
        case SB_TOP:      pos = 0; break;
        case SB_BOTTOM:   pos = g_scroll_range; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: pos = (int)HIWORD(wp); break;
        default: return 0;
        }
        if (pos < 0) pos = 0;
        if (pos > g_scroll_range) pos = g_scroll_range;
        if (pos == g_scroll_offset) return 0;
        /* Never slide a trackbar out from under a thumb the user is holding. */
        for (int i = 0; i < 32; i++)
            if (g_endpoint_ctrls[i].hwnd_vol && GetCapture() == g_endpoint_ctrls[i].hwnd_vol)
                return 0;
        for (int i = 0; i < 128; i++)
            if (g_session_ctrls[i].hwnd_vol && GetCapture() == g_session_ctrls[i].hwnd_vol)
                return 0;
        g_scroll_offset = pos;
        ui_apply_viewport();
        ui_update_values();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (g_scroll_range <= 0) return 0;
        UINT cmd = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN;
        for (int i = 0; i < 3; i++)
            SendMessageW(hwnd, WM_VSCROLL, MAKELONG(cmd, 0), 0);
        return 0;
    }

    case WM_ASW_SHOW: {
        /* Another launch asked us to come forward (the Hub's Open button, a
           desktop shortcut, a second double-click). Restore if minimized,
           show, and take the foreground so the user actually sees the panel
           instead of only the tray icon it is already running in. */
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDM_REFRESH) {
            sessions_refresh();
            refresh_ui();
            return 0;
        }
        if (id == IDM_TRAY_OPEN) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (id == IDM_TRAY_QUIT) {
            g_quitting = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id >= 3000 && id < 3100) {
            int slot = id - 3000;
            if (slot < 0 || slot >= 32) return 0;
            int idx = g_endpoint_ctrls[slot].endpoint_idx;
            if (idx < 0 || idx >= g_endpoint_count) return 0;
            /* Actually switch the device, then re-read the truth from Windows
             * instead of trusting our own bookkeeping. The old handler only set
             * g_default_endpoint_idx and repainted, so the button said "Default"
             * while audio kept playing through the previous device. */
            HRESULT hr = endpoint_make_default(idx);
            if (SUCCEEDED(hr)) {
                endpoints_refresh();
                sessions_refresh();   /* sessions live on the new default device */
            }
            refresh_ui();
            return 0;
        }
        if (id >= 2000 && id < 2100) {
            int slot = id - 2000;
            if (slot < 0 || slot >= 32) return 0;
            int idx = g_endpoint_ctrls[slot].endpoint_idx;
            if (idx < 0 || idx >= g_endpoint_count) return 0;
            endpoint_set_mute(idx, !endpoint_get_mute(idx));
            refresh_ui();
            return 0;
        }
        if (id >= 5000 && id < 5200) {
            int slot = id - 5000;
            /* Bounds-check twice: the slot has to exist, and the 2-second poll
             * can shrink the session list (or a scroll can re-aim the slot)
             * while a stale button id is still in flight. */
            if (slot < 0 || slot >= 128) return 0;
            int idx = g_session_ctrls[slot].session_idx;
            if (idx >= 0 && idx < g_session_count) {
                BOOL now_muted = !g_sessions[idx].mute;
                session_set_mute(idx, now_muted);
                levels_remember(g_sessions[idx].key, g_sessions[idx].volume, now_muted);
                refresh_ui();
            }
            return 0;
        }
        return 0;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lp;
        int pos = SendMessageW(slider, TBM_GETPOS, 0, 0);
        float level = pos / 100.0f;

        /* Resolve the row through the slot that owns this slider, not through
           its control id: rows are re-aimed at different logical entries when
           the panel scrolls, and only the slot knows which one it shows now.
           An id-based lookup also overflowed its range once the list got long
           enough to scroll, which silently dropped the volume change. */
        for (int i = 0; i < 32; i++) {
            EndpointCtrl *c = &g_endpoint_ctrls[i];
            if (c->hwnd_vol != slider) continue;
            int idx = c->endpoint_idx;
            if (idx < 0 || idx >= g_endpoint_count) return 0;
            endpoint_set_volume(idx, level);
            if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, level);
            return 0;
        }
        for (int i = 0; i < 128; i++) {
            SessionCtrl *c = &g_session_ctrls[i];
            if (c->hwnd_vol != slider) continue;
            int idx = c->session_idx;
            if (idx < 0 || idx >= g_session_count) return 0;
            session_set_volume(idx, level);
            levels_remember(g_sessions[idx].key, level, g_sessions[idx].mute);
            if (c->hwnd_voltxt) update_volume_text(c->hwnd_voltxt, level);
            return 0;
        }
        return 0;
    }

    case WM_TIMER:
        if (wp == 1) {
            /* Re-read endpoints too, not just sessions: plugging in headphones
             * must appear without a manual Refresh, and if the user switches
             * the default device in Windows' own settings the "current output"
             * marker has to follow. Safe to poll now that both refresh paths
             * release their previous COM references instead of leaking them. */
            endpoints_refresh();
            sessions_refresh();
            refresh_ui();
        } else if (wp == 2) {
            /* Emblem spin only - invalidate just the emblem square */
            if (g_rcEmblem.right > g_rcEmblem.left)
                InvalidateRect(hwnd, &g_rcEmblem, FALSE);
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT);
        return (INT_PTR)ui_brush(COL_CARD);
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lp;
        if (!di || di->CtlType != ODT_BUTTON) return FALSE;

        BOOL disabled = !IsWindowEnabled(di->hwndItem);
        BOOL pressed  = (di->itemState & ODS_SELECTED) != 0;
        BOOL hovered  = (g_hover_btn == di->hwndItem) && !disabled;

        wchar_t cap[128] = L"";
        GetWindowTextW(di->hwndItem, cap, 128);
        BOOL is_current = (wcscmp(cap, L"Default") == 0);

        COLORREF fill = disabled          ? COL_SEC_BTN
                      : pressed           ? COL_ACCENT_STRONG
                      : (hovered || is_current) ? COL_SEC_HOV
                      : COL_SEC_BTN;
        COLORREF edge = (hovered || pressed) ? COL_HOVER_RIM : COL_BTN_BORDER;
        COLORREF txt  = disabled ? COL_TELEMETRY : COL_TEXT;

        FillRect(di->hDC, &di->rcItem, ui_brush(fill));
        FrameRect(di->hDC, &di->rcItem, ui_brush(edge));

        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, txt);
        HFONT old = (HFONT)SelectObject(di->hDC, g_font_ui);
        DrawTextW(di->hDC, cap, -1, &di->rcItem,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(di->hDC, old);

        if (di->itemState & ODS_FOCUS) {
            RECT ring = di->rcItem;
            InflateRect(&ring, -3, -3);
            DrawFocusRect(di->hDC, &ring);
        }
        return TRUE;
    }

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR*)lp;
        if (!nm || nm->code != NM_CUSTOMDRAW) return 0;
        NMCUSTOMDRAW *cd = (NMCUSTOMDRAW*)lp;
        HWND tb = (HWND)cd->hdr.hwndFrom;

        if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->dwDrawStage != CDDS_ITEMPREPAINT) return 0;

        COLORREF acc = IsWindowEnabled(tb) ? COL_ACCENT : COL_TELEMETRY;

        if (cd->dwItemSpec == TBCD_CHANNEL) {
            RECT r = cd->rc;
            int mid = r.top + (r.bottom - r.top) / 2;
            RECT groove = { r.left, mid - S(2), r.right, mid + S(2) };
            FillRect(cd->hdc, &groove, ui_brush(COL_DIVIDER));
            LONG pos = SendMessageW(tb, TBM_GETPOS, 0, 0);
            int fw = MulDiv(groove.right - groove.left, pos, 100);
            if (fw > 0) {
                RECT filled = { groove.left, groove.top, groove.left + fw, groove.bottom };
                FillRect(cd->hdc, &filled, ui_brush(acc));
            }
            return CDRF_SKIPDEFAULT;
        }
        if (cd->dwItemSpec == TBCD_THUMB) {
            RECT r = cd->rc;
            HPEN pen = CreatePen(PS_SOLID, 1, COL_BG);
            HPEN oldp = (HPEN)SelectObject(cd->hdc, pen);
            HBRUSH oldb = (HBRUSH)SelectObject(cd->hdc, ui_brush(acc));
            Ellipse(cd->hdc, r.left, r.top, r.right + 1, r.bottom + 1);
            SelectObject(cd->hdc, oldb);
            SelectObject(cd->hdc, oldp);
            DeleteObject(pen);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_SKIPDEFAULT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, ui_brush(COL_BG));

        /* Header band */
        RECT hdr = { 0, 0, rc.right, S(60) };
        FillRect(hdc, &hdr, ui_brush(COL_FOOTER_BG));

        int embSz = S(44);
        int embX = rc.right - embSz - S(12);
        int embY = S(8);
        SetRect(&g_rcEmblem, embX, embY, embX + embSz, embY + embSz);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COL_TEXT);
        HFONT saved = (HFONT)SelectObject(hdc, g_font_head);
        DrawTextW(hdc, APP_TITLE, -1, &(RECT){ S(20), S(7), embX - S(12), S(29) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, saved);

        SetTextColor(hdc, COL_MUTED);
        saved = (HFONT)SelectObject(hdc, g_font_small);
        DrawTextW(hdc, L"Output devices and per-app volume", -1,
                  &(RECT){ S(20), S(31), embX - S(12), S(49) },
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, saved);
        PaintEmblemBlit(hdc, embX, embY, embSz);

        RECT div = { 0, S(60), rc.right, S(61) };
        FillRect(hdc, &div, ui_brush(COL_DIVIDER));

        /* Card backgrounds behind each row. The current output gets an accent
         * border so the default device reads at a glance instead of relying on
         * a text suffix alone. */
        for (int i = 0; i < g_ep_rows; i++) {
            int idx = g_endpoint_ctrls[i].endpoint_idx;
            if (idx < 0 || idx >= g_endpoint_count) continue;   /* spare slot */
            FillRect(hdc, &g_ep_card[i], ui_brush(COL_CARD));
            FrameRect(hdc, &g_ep_card[i],
                      ui_brush(idx == g_default_endpoint_idx ? COL_ACCENT : COL_BORDER));
        }
        for (int i = 0; i < g_ss_rows; i++) {
            int idx = g_session_ctrls[i].session_idx;
            if (idx < 0 || idx >= g_session_count) continue;    /* spare slot */
            FillRect(hdc, &g_ss_card[i], ui_brush(COL_CARD));
            FrameRect(hdc, &g_ss_card[i], ui_brush(COL_BORDER));
        }

        /* Section headers at the positions the layout actually used. */
        SetBkMode(hdc, TRANSPARENT);
        if (g_y_outputs_header > 0) {
            SetTextColor(hdc, COL_MUTED);
            saved = (HFONT)SelectObject(hdc, g_font_head);
            DrawTextW(hdc, L"OUTPUT DEVICES", -1,
                      &(RECT){ S(PANEL_X), g_y_outputs_header, S(PANEL_X) + S(PANEL_W),
                               g_y_outputs_header + S(26) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, saved);
        }
        if (g_y_apps_header > 0) {
            SetTextColor(hdc, COL_MUTED);
            saved = (HFONT)SelectObject(hdc, g_font_head);
            DrawTextW(hdc, L"APP VOLUME", -1,
                      &(RECT){ S(PANEL_X), g_y_apps_header, S(PANEL_X) + S(PANEL_W),
                               g_y_apps_header + S(26) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, saved);

            /* An empty app list must say why. A blank panel reads as a broken
             * app; "nothing is playing" and "enumeration failed with
             * 0x80070005" are two very different empty panels. */
            if (g_session_count == 0) {
                wchar_t why[256];
                if (FAILED(g_sessions_hr))
                    _snwprintf(why, 256,
                               L"Could not list running apps (0x%08lX). Run "
                               L"AudioSwitcher.exe --diag and send the report.",
                               (unsigned long)g_sessions_hr);
                else
                    wcscpy(why, L"No apps are playing audio on any output right "
                                L"now. Start something and it appears here.");
                why[255] = L'\0';
                SetTextColor(hdc, COL_TELEMETRY);
                saved = (HFONT)SelectObject(hdc, g_font_ui);
                DrawTextW(hdc, why, -1,
                          &(RECT){ S(PANEL_X), g_y_apps_header + S(28),
                                   S(PANEL_X) + S(PANEL_W), g_y_apps_header + S(72) },
                          DT_LEFT | DT_TOP | DT_WORDBREAK);
                SelectObject(hdc, saved);
            }
        }

        /* Footer status line: the honest report of what the last action did. */
        if (g_y_footer > 0) {
            RECT fband = { 0, g_y_footer - S(8), rc.right, rc.bottom };
            FillRect(hdc, &fband, ui_brush(COL_FOOTER_BG));
            RECT fdiv = { 0, g_y_footer - S(8), rc.right, g_y_footer - S(7) };
            FillRect(hdc, &fdiv, ui_brush(COL_DIVIDER));

            wchar_t status[320];
            ui_status_text(status, 320);

            COLORREF col = COL_MUTED;
            if (g_last_switch_idx >= 0 && FAILED(g_last_switch_hr)) col = COL_DANGER;
            else if (g_policy_variant == 0)                         col = COL_WARN;
            else if (g_last_switch_idx >= 0)                        col = COL_GOOD;

            /* state dot */
            int dy = g_y_footer + S(10);
            RECT dot = { S(PANEL_X), dy - S(4), S(PANEL_X) + S(8), dy + S(4) };
            FillRect(hdc, &dot, ui_brush(col));

            SetTextColor(hdc, col);
            saved = (HFONT)SelectObject(hdc, g_font_small);
            DrawTextW(hdc, status, -1,
                      &(RECT){ S(PANEL_X) + S(16), g_y_footer, S(PANEL_X) + S(PANEL_W),
                               g_y_footer + S(22) },
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, saved);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

