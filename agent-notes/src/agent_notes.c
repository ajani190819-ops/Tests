/* AgentNotes - quick notes from the owner straight to the agent workspace.
 *
 * No Google middleman: this little suite app POSTs notes directly to the
 * agent session's notes server (tools/notes_server.py behind the Arena
 * preview link). While the agent is offline the notes queue locally in
 * %LOCALAPPDATA%\AgentNotes\outbox\ and are delivered automatically as
 * soon as the session link answers again.
 *
 * UI: always-spinning Tri-Spoke Core emblem (the hub's SDF renderer, one
 * turn per 30 s idle / 8 s while sending), guided note box with a live
 * hint, a recent-notes list whose Sent/Queued state is derived from the
 * outbox, and a connection status strip.
 */
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define APP_TITLE_W   L"Agent Notes"
#define MUTEX_NAME    L"Local\\Tests_AgentNotes_SingleInstance_v1"
#define SHOW_EVENT    L"Local\\Tests_AgentNotes_Show_v1"

/* Soft Slate palette - exact tokens from docs/UI_STYLE_GUIDE.md */
#define COL_BG        RGB(24, 27, 36)     /* Canvas Slate   #181B24 */
#define COL_CARD      RGB(34, 39, 52)     /* Card Surface   #222734 */
#define COL_INPUT     RGB(43, 49, 66)     /* Control Surface #2B3142 */
#define COL_TXT       RGB(236, 239, 246)  /* Primary Text   #ECEFF6 */
#define COL_MUTED     RGB(154, 163, 184)  /* Secondary Text #9AA3B8 */
#define COL_ACCENT    RGB(56, 189, 248)   /* Primary Sky    #38BDF8 */
#define COL_WARN      RGB(251, 191, 36)   /* Warm Amber     #FBBF24 */
#define COL_GOOD      RGB(52, 211, 153)   /* Active Mint    #34D399 */
#define COL_DIV       RGB(58, 66, 86)     /* Soft Hairline  #3A4256 */
#define COL_HOT       RGB(82, 94, 122)    /* Hover Border   #525E7A */
#define COL_CARD_BD   RGB(56, 64, 84)     /* Card border    #384054 */

#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR (WM_USER + 67)   /* rich edit background color */
#endif

enum { BTN_SAVE = 0, BTN_SEND, BTN_COUNT };
enum { TIMER_FLUSH = 1, TIMER_SPIN = 2 };
#define ID_EDIT_LINK   101
#define ID_EDIT_NOTE   102

#define HINT_TEXT  L"Type your note here, then press Send to Agent - it lands in the agent's inbox."
#define HIST_MAX   20
#define HIST_SHOW  3

static HINSTANCE g_hInst;
static HWND      g_hwnd, g_hNote, g_hLink;
static HFONT     g_fontUI, g_fontTitle, g_fontBtn;
static HBRUSH    g_bgBrush;
static UINT      g_dpi = 96;
static int       g_fit = 100;      /* uniform shrink so the window always fits the screen */
static int       g_hoverBtn = -1;
static HANDLE    g_showEvent = NULL;

static wchar_t   g_link[256] = L"";
static wchar_t   g_cfgDir[MAX_PATH] = L"";
static wchar_t   g_cfgPath[MAX_PATH] = L"";
static wchar_t   g_histPath[MAX_PATH] = L"";
static wchar_t   g_outboxDir[MAX_PATH] = L"";
static wchar_t   g_status[256] = L"No live session yet - notes queue safely here and deliver on the agent's next turn.";
static int       g_lastOk = -1;          /* -1 unknown, 0 failed, 1 delivered */
static int       g_sending = 0;
static int       g_noteHint = 1;         /* note box is showing the hint */
static wchar_t   g_lastSent[16] = L"";

static int S(int px) {
    return MulDiv(MulDiv(px, (int)g_dpi, 96), g_fit, 100);
}

/* Dark title bar + full Windows 11 rounded corners (same as the hub). */
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

static void ApplyModernWindowFrame(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    PFN_DwmSetWindowAttribute setAttr =
        (PFN_DwmSetWindowAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttr) {
        BOOL dark = TRUE;
        DWORD cornerPref = 2;   /* DWMWCP_ROUND */
        COLORREF captionCol = COL_BG;
        COLORREF borderCol  = COL_DIV;
        setAttr(hwnd, 20, &dark, sizeof(dark));
        setAttr(hwnd, 19, &dark, sizeof(dark));
        setAttr(hwnd, 33, &cornerPref, sizeof(cornerPref));
        setAttr(hwnd, 35, &captionCol, sizeof(captionCol));
        setAttr(hwnd, 34, &borderCol, sizeof(borderCol));
    }
    FreeLibrary(dwm);
}

static UINT GetWinDpi(HWND h) {
    UINT (WINAPI *pfn)(HWND) = (UINT (WINAPI *)(HWND))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return (pfn && h) ? pfn(h) : 96;
}


/* ---- modularized (unity build): each section lives in its own file under
      parts/ and is textually included here IN ORIGINAL ORDER, so agent_notes.c
      is still ONE translation unit. Edit the part files; the build compiles
      this one file. ---- */
#include "parts/config_outbox.c"
#include "parts/http_agent.c"
#include "parts/emblem.c"
#include "parts/ui.c"
#include "parts/entry.c"
