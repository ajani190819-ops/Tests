/* Weather Radar adapter for the shared transparent Tri-Spoke engine.
   The weather canvas can follow the Windows light theme, so color selection
   belongs to the shared engine rather than to a forked renderer. */

static SuiteEmblemEngine g_weatherRadarEmblem = { 0 };
static double g_radarSpinAngle = 0.0;

static void DrawWeatherRadarEmblem(uint32_t *pixels, int width, int height,
                                   int x, int y, int size, double angle) {
    COLORREF nearColor, farColor;
    SuiteEmblemResolvePalette(COL_BG, COL_ACCENT, COL_TXT, &nearColor, &farColor);
    SuiteEmblemDraw(&g_weatherRadarEmblem, pixels, width, height, x, y, size,
                    angle, nearColor, farColor);
}

static void ReleaseWeatherRadarEmblem(void) {
    SuiteEmblemRelease(&g_weatherRadarEmblem);
}
