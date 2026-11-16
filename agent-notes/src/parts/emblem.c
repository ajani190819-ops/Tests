/* Agent Notes adapter for the shared transparent Tri-Spoke engine.
   The engine selects the canonical pale ice mark on dark slate and uses this
   app's accessible accent/text pair on a light Windows theme. */

static SuiteEmblemEngine g_agentNotesEmblem = { 0 };
static double g_spinAngle = 0.0;

static void DrawAgentNotesEmblem(uint32_t *pixels, int width, int height,
                                 int x, int y, int size, double angle) {
    COLORREF nearColor, farColor;
    SuiteEmblemResolvePalette(COL_BG, COL_ACCENT, COL_TXT, &nearColor, &farColor);
    SuiteEmblemDraw(&g_agentNotesEmblem, pixels, width, height, x, y, size,
                    angle, nearColor, farColor);
}

static void ReleaseAgentNotesEmblem(void) {
    SuiteEmblemRelease(&g_agentNotesEmblem);
}
