/* All native brightness inputs share this controller. live.txt is the
   canonical 0..1000 master, never a rounded UI echo. Native-owned engines
   mirror it; only real Fn/legacy external actions may write a new level. */
#include "brightness_controller.h"
static BrightnessController g_brightness;
static BOOL g_brightnessWritePending;
static BOOL g_brightnessChannelReady;
static ULONGLONG g_brightnessCfgSaveAt;
static BOOL g_panelSaveFailed;
static void SavePanelCfg(void);
static void OsdShow(int pct);
static void WriteLiveBrightness(void);
static void PublishMasterBrightness(int before, BOOL writeLive);
static void SyncMasterBrightness(void);
static void SetMasterBrightnessLevel(int level);
static void PollFnBrightness(void);

static void WriteLiveBrightness(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\r\n", g_brightness.level);
    g_brightnessWritePending = !WriteAtomicUtf8(g_livePath, buf);
    if (!g_brightnessWritePending) g_brightnessChannelReady = TRUE;
}

static void PublishMasterBrightness(int before, BOOL writeLive) {
    g_cfg.masterBrightness = (g_brightness.level + 5) / 10; /* display only */
    if (before == g_brightness.level) return;
    if (writeLive) WriteLiveBrightness();
    int pct = g_cfg.masterBrightness;
    if (pct <= 0) _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  Off");
    else _snwprintf(g_toastText, 95, L"Keyboard Lights  \x00B7  %d%%", pct);
    ULONGLONG now = GetTickCount64();
    g_toastAt = now + 1400ULL;
    g_brightnessCfgSaveAt = now + 300ULL;
    if (g_hwnd) {
        OsdShow(pct);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void SetMasterBrightnessLevel(int level) {
    int before = g_brightness.level;
    BrightnessAbsolute(&g_brightness, level);
    PublishMasterBrightness(before, TRUE);
}

/* Reconcile external Fn/legacy writes before a new input, not after it. A
   failed publish stays pending and is retried; stale disk cannot undo it. */
static void SyncMasterBrightness(void) {
    if (g_brightnessWritePending) {
        WriteLiveBrightness();
        return;
    }
    if (!g_brightnessChannelReady) return; /* old run's live.txt is not a new input */
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_livePath, &len);
    if (!txt) return;
    char *end = NULL;
    long parsed = strtol(txt, &end, 10);
    int parseOk = end != txt;
    while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end++;
    if (!parseOk || !end || *end != '\0' || end != txt + len || parsed < 0 || parsed > 1000) {
        free(txt);
        return;
    }
    free(txt);
    if ((int)parsed == g_brightness.level) return;
    int before = g_brightness.level;
    BrightnessAbsolute(&g_brightness, (int)parsed);
    PublishMasterBrightness(before, FALSE); /* observe; never echo/quantize */
}

static void FlushPendingBrightnessCfg(void) {
    if (!g_brightnessCfgSaveAt) return;
    g_brightnessCfgSaveAt = 0;
    SavePanelCfg();
    if (g_panelSaveFailed) g_brightnessCfgSaveAt = GetTickCount64() + 1000ULL;
}

static void PollFnBrightness(void) {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastFnPoll < 35ULL) return;
    g_lastFnPoll = now;
    SyncMasterBrightness();
    if (g_brightnessCfgSaveAt && now >= g_brightnessCfgSaveAt)
        FlushPendingBrightnessCfg();
}
