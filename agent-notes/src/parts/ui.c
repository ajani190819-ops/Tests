/* ---------------- UI ---------------- */

/* RichEdit controls ignore WM_CTLCOLOREDIT entirely - the character format
   is the text colour. Without this the editor kept Windows' default black
   text on a dark surface. */
static void RichTextColor(HWND h, COLORREF c) {
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = c;
    DWORD selA = 0, selB = 0;
    SendMessageW(h, EM_GETSEL, (WPARAM)&selA, (LPARAM)&selB);
    SendMessageW(h, EM_SETSEL, 0, -1);
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(h, EM_SETSEL, (WPARAM)selA, (LPARAM)selB);
    SendMessageW(h, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
}

static void EmbRect(RECT *r) {
    SetRect(r, S(20), S(22), S(20) + S(56), S(22) + S(56));
}

static void BtnRect(int i, RECT *r) {
    (void)i;
    SetRect(r, S(20), S(410), S(600), S(454));
}

static void DrawRoundedSurface(HDC dc, const RECT *r, int radius,
                               COLORREF fill, COLORREF edge) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    RoundRect(dc, r->left, r->top, r->right, r->bottom, S(radius), S(radius));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : COL_INPUT;
    COLORREF edge = (g_hoverBtn == idx) ? COL_HOT : COL_DIV;
    DrawRoundedSurface(dc, &r, 10, fill, edge);
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

static const wchar_t *DeliverySummary(void) {
    if (g_sending) return L"Delivering queued notes now.";
    if (g_lastOk == 1) return L"Recent delivery confirmed. New notes send automatically.";
    if (g_link[0]) return L"Checking the saved agent session automatically.";
    return L"Looking for the agent session. Notes queue safely in the meantime.";
}

static void PaintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    /* Spin fast-path: only the emblem square is dirty. */
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
            for (int i = 0; i < sz * sz; i++) bits[i] = COL_BG;
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

    SelectObject(mem, g_fontTitle);
    SetTextColor(mem, COL_TXT);
    RECT title = { S(92), S(20), W - S(20), S(50) };
    DrawTextW(mem, L"Agent Notes", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTED);
    RECT subtitle = { S(92), S(51), W - S(20), S(78) };
    DrawTextW(mem, L"Send a note directly to the agent. Delivery is automatic.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    /* No link box or setup action: session discovery is background plumbing. */
    RECT delivery = { S(17), S(98), W - S(17), S(148) };
    DrawRoundedSurface(mem, &delivery, 14, COL_CARD, COL_CARD_BD);
    COLORREF deliveryCol = g_lastOk == 0 ? COL_WARN : (g_lastOk == 1 ? COL_GOOD : COL_ACCENT);
    DrawDot(mem, S(34), S(119), S(4), deliveryCol);
    SetTextColor(mem, COL_TXT);
    RECT dTitle = { S(48), S(104), W - S(28), S(122) };
    DrawTextW(mem, L"Automatic delivery", -1, &dTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(mem, COL_MUTED);
    RECT dText = { S(48), S(122), W - S(28), S(142) };
    DrawTextW(mem, DeliverySummary(), -1, &dText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetTextColor(mem, COL_MUTED);
    RECT noteLabel = { S(20), S(166), W - S(20), S(187) };
    DrawTextW(mem, L"New note", -1, &noteLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT noteSurface = { S(17), S(188), W - S(17), S(400) };
    DrawRoundedSurface(mem, &noteSurface, 14, COL_INPUT, COL_DIV);
    DrawBtn(mem, BTN_SEND, L"Send to Agent", 1);

    RECT recentLabel = { S(20), S(470), W - S(20), S(491) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Recent activity", -1, &recentLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT history = { S(17), S(494), W - S(17), S(590) };
    DrawRoundedSurface(mem, &history, 14, COL_CARD, COL_CARD_BD);

    SelectObject(mem, g_fontUI);
    for (int i = 0; i < HIST_SHOW && i < g_histN; i++) {
        int y = S(498) + i * S(28);
        wchar_t qp[MAX_PATH + 96];
        _snwprintf(qp, MAX_PATH + 95, L"%s\\%s", g_outboxDir, g_hist[i].name);
        int queued = FileExistsW(qp);
        DrawDot(mem, S(30), y + S(10), S(3), queued ? COL_WARN : COL_GOOD);
        RECT stamp = { S(44), y, S(44) + S(78), y + S(22) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, g_hist[i].stamp, -1, &stamp, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT chip = { S(126), y + S(1), S(126) + S(62), y + S(21) };
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
        RECT snip = { S(200), y, W - S(24), y + S(22) };
        SetTextColor(mem, COL_TXT);
        DrawTextW(mem, g_hist[i].snip, -1, &snip,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (g_histN == 0) {
        RECT empty = { S(42), S(526), W - S(24), S(550) };
        SetTextColor(mem, COL_MUTED);
        DrawTextW(mem, L"Your sent and queued notes will appear here.", -1, &empty,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    int statusY = H - S(38);
    DrawDot(mem, S(26), statusY + S(14), S(4),
            g_lastOk == 0 ? COL_WARN : (g_lastOk == 1 ? COL_GOOD : COL_MUTED));
    RECT status = { S(42), statusY, W - S(20), statusY + S(28) };
    SetTextColor(mem, g_lastOk == 0 ? COL_WARN : (g_lastOk == 1 ? COL_GOOD : COL_MUTED));
    DrawTextW(mem, g_status, -1, &status,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (bits) {
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
    SetWindowTextW(g_hNote, on ? HINT_TEXT : L"");
    g_noteHint = on;
    RichTextColor(g_hNote, on ? COL_MUTED : COL_TXT);
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
        if (b == BTN_SEND) {
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
    case WM_COMMAND:
        if (LOWORD(wp) == ID_EDIT_NOTE) {
            int code = HIWORD(wp);
            if (code == EN_SETFOCUS && g_noteHint) {
                NoteSetHint(0);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (code == EN_KILLFOCUS && GetWindowTextLengthW(g_hNote) <= 0) {
                NoteSetHint(1);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC h = (HDC)wp;
        SetTextColor(h, g_noteHint ? COL_MUTED : COL_TXT);
        SetBkColor(h, COL_INPUT);
        return (LRESULT)g_bgBrush;
    }
    case WM_TIMER:
        if (wp == TIMER_SPIN) {
            double rate = g_sending ? (6.283185307179586 / 8.0)
                                    : (6.283185307179586 / 30.0);
            g_spinAngle += rate * 0.05;
            if (g_spinAngle > 6.283185307179586) g_spinAngle -= 6.283185307179586;
            RECT er;
            EmbRect(&er);
            InvalidateRect(hwnd, &er, FALSE);
        } else if (wp == TIMER_FLUSH) {
            if (LinkDiscoveryTick()) {
                wcsncpy(g_status, L"Agent session found - queued notes will deliver automatically.", 255);
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
