/* Project Hub adapter for the shared transparent Tri-Spoke engine.
   Geometry, cache ownership, alpha compositing, and the DIB channel contract
   live in shared/win32/suite_emblem.c.  Do not copy renderer math here. */

static SuiteEmblemEngine g_hubEmblem = { 0 };

static void DrawWayfinderEmblem(uint32_t *pixels, int width, int height,
                                int x, int y, int size) {
    COLORREF nearColor, farColor;
    SuiteEmblemResolvePalette(COL_BG, COL_SKY, COL_TEXT_PRIMARY, &nearColor, &farColor);
    SuiteEmblemDraw(&g_hubEmblem, pixels, width, height, x, y, size,
                    SuiteEmblemAngleNow(GetTickCount64(), 30000ULL),
                    nearColor, farColor);
}

static void ReleaseWayfinderEmblem(void) {
    SuiteEmblemRelease(&g_hubEmblem);
}

static BOOL FileExistsW(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static DWORD LocalSizeOf(const wchar_t *p) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(p, GetFileExInfoStandard, &fad))
        return fad.nFileSizeLow;
    return 0;
}

/* Installed means usable: a 0-byte exe (interrupted copy, AV quarantine
   remnant, ...) must not count as installed, or the update check stays
   silent while the app cannot even open. */
static BOOL FileHealthyW(const wchar_t *p) {
    return FileExistsW(p) && LocalSizeOf(p) > 0;
}
