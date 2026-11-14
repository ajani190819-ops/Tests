/* ---------------- UI ---------------- */

static void EmbRect(RECT *r) {
    SetRect(r, S(20), S(22), S(20) + S(56), S(22) + S(56));
}

static void BtnRect(int i, RECT *r) {
    if (i == BTN_SAVE) SetRect(r, S(470), S(98), S(470) + S(120), S(98) + S(32));
    else SetRect(r, S(20), S(380), S(20) + S(190), S(380) + S(38));
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : COL_INPUT;
    COLORREF edge = (g_hoverBtn == idx) ? COL_HOT : COL_DIV;
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
    SetTextColor(dc, primary ? COL_BG : COL_TXT);
    SelectObject(dc, g_fontBtn);
    DrawTextW(dc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

static void DrawDot(HDC dc, int cx, int cy, int rad, COLORREF col) {
    HBRUSH b = CreateSolidBrush(col);
    HPEN p = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    Ellipse(dc, cx - rad, cy - rad, cx + rad + 1, cy + rad + 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void PaintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    /* Spin fast-path: only the emblem square is dirty - repaint just it. */
    RECT er;
    EmbRect(&er);
    if (ps.rcPaint.left >= er.left && ps.rcPaint.top >= er.top &&
        ps.rcPaint.right <= er.right && ps.rcPaint.bottom <= er.bottom) {
        int sz = er.right - er.left;
        uint32_t *bits = NULL;
        BITMAPINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = sz;
        bi.bmiHeader.biHeight = -sz;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        if (dib && bits) {
            for (int i = 0; i < sz * sz; i++) bits[i] = RGB(24, 27, 36);
            EmbSpinDraw(bits, sz, sz, 0, 0, sz, g_spinAngle);
            HDC mdc = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mdc, dib);
            BitBlt(dc, er.left, er.top, sz, sz, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            DeleteDC(mdc);
            DeleteObject(dib);
        }
        EndPaint(hwnd, &ps);
        return;
    }

    /* Full paint into a top-down 32-bit DIB (GDI text + emblem pixels). */
    uint32_t *bits = NULL;
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = NULL;
    if (dib && bits) old = SelectObject(mem, dib);

    RECT full = { 0, 0, W, H };
    FillRect(mem, &full, g_bgBrush);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, COL_TXT);
    SelectObject(mem, g_fontTitle);
    RECT t = { S(92), S(20), W - S(20), S(52) };
    DrawTextW(mem, L"Agent Notes", -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTED);
    RECT s = { S(92), S(52), W - S(20), S(84) };
    DrawTextW(mem,
              L"Type a note and press Send to Agent - it lands in the agent's session inbox.",
              -1, &s, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT lbl = { S(20), S(98), S(126), S(130) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Session link", -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT nlbl = { S(20), S(152), W - S(20), S(178) };
    DrawTextW(mem, L"New note", -1, &nlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Soft-slate rounded control surfaces. The RichEdit children paint on
       top of these; the rounded fill shows at the corners. */
    {
        HBRUSH b = CreateSolidBrush(COL_INPUT);
        HPEN p = CreatePen(PS_SOLID, 1, COL_DIV);
        HGDIOBJ ob = SelectObject(mem, b);
        HGDIOBJ op = SelectObject(mem, p);
        RECT fr1 = { S(127), S(95), S(463), S(133) };
        RoundRect(mem, fr1.left, fr1.top, fr1.right, fr1.bottom, S(10), S(10));
        RECT fr2 = { S(17), S(175), S(603), S(371) };
        RoundRect(mem, fr2.left, fr2.top, fr2.right, fr2.bottom, S(14), S(14));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(b);
        DeleteObject(p);
    }

    DrawBtn(mem, BTN_SAVE, L"Save Link", 0);
    DrawBtn(mem, BTN_SEND, L"Send to Agent", 1);

    /* Recent notes: live Sent/Queued state (outbox file exists = queued). */
    RECT rl = { S(20), S(438), W - S(20), S(462) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Recent notes", -1, &rl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    {
        HBRUSH b = CreateSolidBrush(COL_CARD);
        HPEN p = CreatePen(PS_SOLID, 1, COL_CARD_BD);
        HGDIOBJ ob = SelectObject(mem, b);
        HGDIOBJ op = SelectObject(mem, p);
        RECT cardRc = { S(17), S(462), W - S(17), S(550) };
        RoundRect(mem, cardRc.left, cardRc.top, cardRc.right, cardRc.bottom, S(14), S(14));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(b);
        DeleteObject(p);
    }
    SelectObject(mem, g_fontUI);
    for (int i = 0; i < HIST_SHOW && i < g_histN; i++) {
        int y = S(466) + i * S(26);
        wchar_t qp[MAX_PATH + 96];
        _snwprintf(qp, MAX_PATH + 95, L"%s\\%s", g_outboxDir, g_hist[i].name);
        int queued = FileExistsW(qp);
        DrawDot(mem, S(28), y + S(10), S(3), queued ? COL_WARN : COL_GOOD);
        RECT st = { S(42), y, S(42) + S(78), y + S(21) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, g_hist[i].stamp, -1, &st, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT chip = { S(124), y + S(1), S(124) + S(62), y + S(20) };
        HPEN p = CreatePen(PS_SOLID, 1, queued ? COL_WARN : COL_GOOD);
        HGDIOBJ op = SelectObject(mem, p);
        HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
        RoundRect(mem, chip.left, chip.top, chip.right, chip.bottom, S(10), S(10));
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(p);
        SetTextColor(mem, queued ? COL_WARN : COL_GOOD);
        DrawTextW(mem, queued ? L"Queued" : L"Sent", -1, &chip,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT sn = { S(198), y, W - S(24), y + S(21) };
        SetTextColor(mem, COL_TXT);
        DrawTextW(mem, g_hist[i].snip, -1, &sn,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (g_histN == 0) {
        RECT e = { S(42), S(466), W - S(24), S(487) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, L"Notes you send will show up here.", -1, &e,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* Status strip: dot + what is going on right now. */
    int sy = H - S(36);
    DrawDot(mem, S(26), sy + S(14), S(4),
            g_lastOk == 0 ? COL_WARN : (g_lastOk == 1 ? COL_GOOD : COL_MUTED));
    RECT fr = { S(42), sy, W - S(20), sy + S(28) };
    SetTextColor(mem, (g_lastOk == 0) ? COL_WARN : ((g_lastOk == 1) ? COL_GOOD : COL_MUTED));
    DrawTextW(mem, g_status, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (bits) {
        /* GDI batches its draws - flush before touching the DIB pixels, or
           the batched fills can land on top of the emblem as a ghost square. */
        GdiFlush();
        EmbSpinDraw(bits, W, H, er.left, er.top, er.right - er.left, g_spinAngle);
    }

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    if (dib) {
        SelectObject(mem, old);
        DeleteObject(dib);
    }
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void NoteSetHint(int on) {
    if (!g_hNote) return;
    if (on) SetWindowTextW(g_hNote, HINT_TEXT);
    else SetWindowTextW(g_hNote, L"");
    g_noteHint = on;
}

static void NowStamp(wchar_t *out, int cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(out, cap - 1, L"%02d-%02d %02d:%02d",
               st.wMonth, st.wDay, st.wHour, st.wMinute);
    out[cap - 1] = L'\0';
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
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
        int b = HitBtn(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (b == BTN_SAVE) {
            GetWindowTextW(g_hLink, g_link, 255);
            SaveConfig();
            if (g_link[0]) {
                wcsncpy(g_status, L"Link saved - testing the connection...", 255);
                StartFlush();
            } else {
                wcsncpy(g_status, L"Link cleared.", 255);
            }
            g_status[255] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (b == BTN_SEND) {
            int empty = (g_noteHint || GetWindowTextLengthW(g_hNote) <= 0);
            if (empty) {
                wcsncpy(g_status, L"Nothing to send - type a note first.", 255);
                g_status[255] = L'\0';
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            int n = GetWindowTextLengthW(g_hNote);
            wchar_t *text = (wchar_t *)malloc(((size_t)n + 1) * sizeof(wchar_t));
            if (!text) return 0;
            GetWindowTextW(g_hNote, text, n + 1);
            wchar_t name[80], stamp[16], snip[80];
            if (OutboxAdd(text, name, 80)) {
                NowStamp(stamp, 16);
                HistSnip(text, snip, 80);
                HistAdd(name, stamp, snip);
            }
            free(text);
            NoteSetHint(1);
            SetFocus(g_hNote);
            wcsncpy(g_status, L"Sending...", 255);
            g_status[255] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
            StartFlush();
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == ID_EDIT_NOTE) {
            if (code == EN_SETFOCUS && g_noteHint) {
                NoteSetHint(0);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (code == EN_KILLFOCUS && GetWindowTextLengthW(g_hNote) <= 0) {
                NoteSetHint(1);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC h = (HDC)wp;
        if ((HWND)lp == g_hNote && g_noteHint) {
            SetTextColor(h, COL_MUTED);
        } else {
            SetTextColor(h, COL_TXT);
        }
        SetBkColor(h, COL_INPUT);
        return (LRESULT)g_bgBrush;
    }
    case WM_TIMER:
        if (wp == TIMER_SPIN) {
            /* One turn per 30 s idle; per 8 s while a send is in flight. */
            double rate = g_sending ? (6.283185307179586 / 8.0)
                                    : (6.283185307179586 / 30.0);
            g_spinAngle += rate * 0.05;
            if (g_spinAngle > 6.283185307179586) g_spinAngle -= 6.283185307179586;
            RECT er;
            EmbRect(&er);
            InvalidateRect(hwnd, &er, FALSE);
        } else if (wp == TIMER_FLUSH) {
            if (LinkDiscoveryTick()) {
                SetWindowTextW(g_hLink, g_link);
                wcsncpy(g_status, L"Found the agent's live session - link updated.", 255);
                g_status[255] = L'\0';
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (OutboxCount() > 0 && !g_sending) StartFlush();
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_FLUSH);
        KillTimer(hwnd, TIMER_SPIN);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

