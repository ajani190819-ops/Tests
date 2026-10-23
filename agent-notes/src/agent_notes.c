/* AgentNotes - quick notes from the owner straight to the agent workspace.
 *
 * No Google middleman: this little suite app POSTs notes directly to the
 * agent session's notes server (tools/notes_server.py behind the Arena
 * preview link). While the agent is offline the notes queue locally in
 * %LOCALAPPDATA%\AgentNotes\outbox\ and are delivered automatically as
 * soon as the session link answers again.
 */
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define APP_TITLE_W   L"Agent Notes"
#define MUTEX_NAME    L"Local\\Tests_AgentNotes_SingleInstance_v1"
#define SHOW_EVENT    L"Local\\Tests_AgentNotes_Show_v1"

#define COL_BG        RGB(24, 27, 36)
#define COL_CARD      RGB(34, 39, 52)
#define COL_TXT       RGB(232, 236, 245)
#define COL_MUTED     RGB(139, 147, 168)
#define COL_ACCENT    RGB(78, 161, 255)
#define COL_WARN      RGB(245, 181, 68)
#define COL_GOOD      RGB(52, 199, 120)
#define COL_DIV       RGB(48, 55, 72)

#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR (WM_USER + 67)   /* rich edit background color */
#endif

enum { BTN_SAVE = 0, BTN_SEND, BTN_COUNT };

static HINSTANCE g_hInst;
static HWND      g_hwnd, g_hNote, g_hLink;
static HFONT     g_fontUI, g_fontTitle, g_fontBtn;
static HBRUSH    g_bgBrush;
static UINT      g_dpi = 96;
static int       g_hoverBtn = -1;
static HANDLE    g_showEvent = NULL;

static wchar_t   g_link[256] = L"";
static wchar_t   g_cfgDir[MAX_PATH] = L"";
static wchar_t   g_cfgPath[MAX_PATH] = L"";
static wchar_t   g_outboxDir[MAX_PATH] = L"";
static wchar_t   g_status[256] = L"Paste the agent's session link above, then type a note.";
static int       g_lastOk = -1;          /* -1 unknown, 0 failed, 1 delivered */
static int       g_sending = 0;

static int S(int px) { return MulDiv(px, (int)g_dpi, 96); }

static UINT GetWinDpi(HWND h) {
    UINT (WINAPI *pfn)(HWND) = (UINT (WINAPI *)(HWND))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return (pfn && h) ? pfn(h) : 96;
}

/* ---------------- config + outbox ---------------- */

static void InitPaths(void) {
    wchar_t appdata[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, MAX_PATH);
    _snwprintf(g_cfgDir, MAX_PATH - 1, L"%s\\AgentNotes", appdata);
    CreateDirectoryW(g_cfgDir, NULL);
    _snwprintf(g_cfgPath, MAX_PATH - 1, L"%s\\config.ini", g_cfgDir);
    _snwprintf(g_outboxDir, MAX_PATH - 1, L"%s\\outbox", g_cfgDir);
    CreateDirectoryW(g_outboxDir, NULL);
}

static void LoadConfig(void) {
    GetPrivateProfileStringW(L"AgentNotes", L"Link", L"", g_link, 255, g_cfgPath);
}

static void SaveConfig(void) {
    WritePrivateProfileStringW(L"AgentNotes", L"Link", g_link, g_cfgPath);
}

static int OutboxCount(void) {
    int n = 0;
    wchar_t pat[MAX_PATH + 16];
    WIN32_FIND_DATAW fd;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return n;
}

static void OutboxAdd(const wchar_t *text) {
    wchar_t path[MAX_PATH + 32];
    static LONG seq = 0;
    LONG i = InterlockedIncrement(&seq);
    _snwprintf(path, MAX_PATH + 31, L"%s\\n-%lu-%lu.txt", g_outboxDir,
               (unsigned long)GetTickCount(), (unsigned long)i);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *u = (char *)malloc((size_t)n);
    if (u) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, u, n, NULL, NULL);
        DWORD wrote = 0;
        WriteFile(h, u, (DWORD)(n - 1), &wrote, NULL);
        free(u);
    }
    CloseHandle(h);
}

/* ---------------- HTTP to the agent session ---------------- */

static int HttpPostNote(const wchar_t *link, const wchar_t *path,
                        const char *body, int bodyLen) {
    URL_COMPONENTS uc;
    wchar_t host[128] = L"", urlPath[256] = L"";
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 127;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 255;
    if (!WinHttpCrackUrl(link, 0, 0, &uc)) return 0;

    HINTERNET ses = WinHttpOpen(L"AgentNotes/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 6000, 6000, 10000, 15000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return 0; }
    HINTERNET req = WinHttpOpenRequest(con, L"POST", path, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       (uc.nScheme == INTERNET_SCHEME_HTTPS)
                                           ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return 0; }
    int ok = 0;
    if (WinHttpSendRequest(req, L"Content-Type: text/plain; charset=utf-8", (DWORD)-1L,
                           (LPVOID)body, (DWORD)bodyLen, (DWORD)bodyLen, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(req,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
                            WINHTTP_NO_HEADER_INDEX);
        ok = (code == 200);
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static int FlushOutbox(int *delivered) {
    wchar_t pat[MAX_PATH + 16], path[MAX_PATH + 32];
    WIN32_FIND_DATAW fd;
    int sent = 0, failed = 0;
    _snwprintf(pat, MAX_PATH + 15, L"%s\\*.txt", g_outboxDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        _snwprintf(path, MAX_PATH + 31, L"%s\\%s", g_outboxDir, fd.cFileName);
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) { failed++; continue; }
        DWORD sz = GetFileSize(f, NULL), rd = 0;
        if (sz > 60000) sz = 60000;
        char *buf = (char *)malloc((size_t)sz + 1);
        if (!buf) { CloseHandle(f); failed++; continue; }
        ReadFile(f, buf, sz, &rd, NULL);
        buf[rd] = '\0';
        CloseHandle(f);
        if (rd > 0 && g_link[0] && HttpPostNote(g_link, L"/note", buf, (int)rd)) {
            DeleteFileW(path);
            sent++;
        } else {
            failed++;
        }
        free(buf);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (delivered) *delivered = sent;
    return failed;
}

static DWORD WINAPI FlushThread(LPVOID unused) {
    (void)unused;
    int sent = 0;
    int failed = FlushOutbox(&sent);
    int left = OutboxCount();
    g_sending = 0;
    if (sent > 0 && failed == 0) {
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d note%s to the agent  |  inbox is clear.",
                   sent, sent == 1 ? L"" : L"s");
    } else if (sent > 0) {
        g_lastOk = 1;
        _snwprintf(g_status, 255, L"Delivered %d  |  %d still queued - will retry automatically.",
                   sent, left);
    } else if (failed > 0) {
        g_lastOk = 0;
        _snwprintf(g_status, 255,
                   L"Agent not reachable  |  %d queued - delivers when the session is back.",
                   left);
    } else {
        g_lastOk = 1;
        wcsncpy(g_status, L"Nothing to send.", 255);
    }
    g_status[255] = L'\0';
    InvalidateRect(g_hwnd, NULL, FALSE);
    return 0;
}

static void StartFlush(void) {
    if (g_sending) return;
    g_sending = 1;
    HANDLE h = CreateThread(NULL, 0, FlushThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    else g_sending = 0;
}

/* ---------------- UI ---------------- */

static void BtnRect(int i, RECT *r) {
    int y = S(378), h = S(38);
    if (i == BTN_SAVE) SetRect(r, S(430), S(96), S(430) + S(120), S(96) + S(32));
    else SetRect(r, S(20), y, S(20) + S(190), y + h);
}

static void DrawBtn(HDC dc, int idx, const wchar_t *label, int primary) {
    RECT r;
    BtnRect(idx, &r);
    COLORREF fill = primary ? COL_ACCENT : COL_CARD;
    COLORREF edge = (g_hoverBtn == idx) ? COL_ACCENT : COL_DIV;
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
    SetTextColor(dc, COL_TXT);
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

static void PaintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);

    RECT full = { 0, 0, W, H };
    FillRect(mem, &full, g_bgBrush);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, COL_TXT);
    SelectObject(mem, g_fontTitle);
    RECT t = { S(20), S(18), W - S(20), S(52) };
    DrawTextW(mem, L"Agent Notes", -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem, g_fontUI);
    SetTextColor(mem, COL_MUTED);
    RECT s = { S(20), S(52), W - S(20), S(84) };
    DrawTextW(mem, L"Notes go straight to the agent's workspace while a session is running.",
              -1, &s, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT lbl = { S(20), S(96), S(120), S(128) };
    SetTextColor(mem, COL_MUTED);
    DrawTextW(mem, L"Session link", -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT nlbl = { S(20), S(150), W - S(20), S(176) };
    DrawTextW(mem, L"New note", -1, &nlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawBtn(mem, BTN_SAVE, L"Save Link", 0);
    DrawBtn(mem, BTN_SEND, L"Send to Agent", 1);

    RECT fr = { S(20), H - S(44), W - S(20), H - S(12) };
    SetTextColor(mem, (g_lastOk == 0) ? COL_WARN : ((g_lastOk == 1) ? COL_GOOD : COL_MUTED));
    DrawTextW(mem, g_status, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
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
            int n = GetWindowTextLengthW(g_hNote);
            if (n <= 0) {
                wcsncpy(g_status, L"Nothing to send - type a note first.", 255);
                g_status[255] = L'\0';
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            wchar_t *text = (wchar_t *)malloc(((size_t)n + 1) * sizeof(wchar_t));
            if (!text) return 0;
            GetWindowTextW(g_hNote, text, n + 1);
            OutboxAdd(text);
            free(text);
            SetWindowTextW(g_hNote, L"");
            SetFocus(g_hNote);
            wcsncpy(g_status, L"Sending...", 255);
            g_status[255] = L'\0';
            InvalidateRect(hwnd, NULL, FALSE);
            StartFlush();
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC h = (HDC)wp;
        SetTextColor(h, COL_TXT);
        SetBkColor(h, COL_CARD);
        return (LRESULT)g_bgBrush;
    }
    case WM_TIMER:
        if (OutboxCount() > 0 && !g_sending) StartFlush();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---------------- entry ---------------- */

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine; (void)nShow;
    g_hInst = hInst;

    CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        g_showEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, SHOW_EVENT);
        if (g_showEvent) SetEvent(g_showEvent);
        return 0;
    }
    g_showEvent = CreateEventW(NULL, FALSE, FALSE, SHOW_EVENT);

    InitPaths();
    LoadConfig();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"AgentNotesWnd";
    RegisterClassExW(&wc);

    RECT wrc = { 0, 0, S(580), S(470) };
    AdjustWindowRectEx(&wrc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_TITLE_W, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             wrc.right - wrc.left, wrc.bottom - wrc.top,
                             NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    g_dpi = GetWinDpi(g_hwnd);
    g_bgBrush = CreateSolidBrush(COL_BG);
    g_fontUI = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(22), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
    g_fontBtn = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

    /* rich-edits give us dark, standard multiline/single-line inputs */
    LoadLibraryW(L"msftedit.dll");
    g_hLink = CreateWindowExW(0, L"RichEdit50W", g_link,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              S(130), S(96), S(290), S(32), g_hwnd, NULL, hInst, NULL);
    g_hNote = CreateWindowExW(0, L"RichEdit50W", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                              ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                              S(20), S(176), S(540), S(186), g_hwnd, NULL, hInst, NULL);
    if (g_hLink) {
        SendMessageW(g_hLink, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_CARD);
        SendMessageW(g_hLink, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hLink, EM_SETLIMITTEXT, 0, 255);
    }
    if (g_hNote) {
        SendMessageW(g_hNote, EM_SETBKGNDCOLOR, 0, (LPARAM)COL_CARD);
        SendMessageW(g_hNote, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
        SendMessageW(g_hNote, EM_SETLIMITTEXT, 0, 4000);
    }

    if (OutboxCount() > 0)
        _snwprintf(g_status, 255, L"%d queued note(s) found - delivering when reachable.",
                   OutboxCount());
    else if (g_link[0])
        wcsncpy(g_status, L"Ready - type a note and press Send to Agent.", 255);

    SetTimer(g_hwnd, 1, 60000, NULL);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    for (;;) {
        DWORD w = MsgWaitForMultipleObjects(1, &g_showEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (w == WAIT_OBJECT_0) {
            /* second-launch nudge: bring the window forward */
            if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
            SetForegroundWindow(g_hwnd);
            continue;
        }
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            if (!IsDialogMessageW(g_hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
}
