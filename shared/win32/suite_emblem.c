/* -------------------------------------------------------------------------
   Suite Tri-Spoke header-emblem engine

   Include this file exactly once from a native Win32 unity-build root, before
   the app's small emblem adapter.  An app owns a SuiteEmblemEngine context;
   the reusable engine owns its cached, four-times-supersampled source and
   releases it through SuiteEmblemRelease at shutdown.

   Pixels are AARRGGBB values for a 32-bit BI_RGB DIB.  COLORREF deliberately
   is not written directly to a DIB: COLORREF is 0x00BBGGRR, while a DIB pixel
   in a little-endian DWORD is 0x00RRGGBB.  SuiteEmblemDibPixel is the one
   approved conversion, which prevents opaque brown/orange cache squares.

   Required host headers: windows.h, stdint.h, stdlib.h, math.h.
   ------------------------------------------------------------------------- */

#ifndef SUITE_WIN32_EMBLEM_ENGINE_INCLUDED
#define SUITE_WIN32_EMBLEM_ENGINE_INCLUDED

typedef struct SuiteEmblemEngine {
    int       size;       /* on-screen edge of the cached source */
    int       sourceSize; /* four-times supersampled source edge */
    uint32_t *source;     /* straight-alpha AARRGGBB DIB pixels */
    COLORREF  nearColor;
    COLORREF  farColor;
} SuiteEmblemEngine;

static uint32_t SuiteEmblemDibRgb(int red, int green, int blue) {
    return ((uint32_t)(red & 255) << 16) |
           ((uint32_t)(green & 255) << 8) |
           (uint32_t)(blue & 255);
}

static uint32_t SuiteEmblemDibPixel(COLORREF color) {
    return SuiteEmblemDibRgb(GetRValue(color), GetGValue(color), GetBValue(color));
}

/* One shared timebase for the calm suite default: one turn every 30 seconds. */
static double SuiteEmblemAngleNow(ULONGLONG nowMs, ULONGLONG turnMs) {
    if (!turnMs) turnMs = 30000ULL;
    return ((double)(nowMs % turnMs) / (double)turnMs) * 6.283185307179586;
}

/* The pale ice gradient is the canonical mark on a dark slate header.  Light
   themes use their existing accessible accent-to-text pair, so the exact same
   geometry remains readable instead of becoming a washed-out ghost. */
static void SuiteEmblemResolvePalette(COLORREF surface, COLORREF accent,
                                      COLORREF text, COLORREF *nearColor,
                                      COLORREF *farColor) {
    int luma = (GetRValue(surface) * 54 + GetGValue(surface) * 183 +
                GetBValue(surface) * 19) / 256;
    if (luma >= 160) {
        *nearColor = accent;
        *farColor = text;
    } else {
        *nearColor = RGB(246, 249, 254); /* Platinum Ice */
        *farColor = RGB(196, 212, 234);  /* Soft Sky shadow */
    }
}

static void SuiteEmblemRasterize(uint32_t *dst, int n,
                                 COLORREF nearColor, COLORREF farColor) {
    const double scale = 0.92, boost = 0.012;
    const double pixelSize = 2.0 / (double)n;
    const double antiAlias = (pixelSize * 1.35 > 1e-4) ? pixelSize * 1.35 : 1e-4;
    const double degree = 0.017453292519943295;
    static const double gaps[3] = { -90.0, 30.0, 150.0 };
    static const double normals[3] = { 90.0, -150.0, -30.0 };

    for (int row = 0; row < n; row++) {
        double yUnit = ((double)row + 0.5) / (double)n;
        double y = -1.0 + 2.0 * yUnit;
        for (int column = 0; column < n; column++) {
            double xUnit = ((double)column + 0.5) / (double)n;
            double x = -1.0 + 2.0 * xUnit;
            double radius = sqrt(x * x + y * y) / scale;
            double angle = atan2(y, x) / degree;
            double outer = fabs(radius - 0.895) - (0.022 + boost * 0.45);
            double ring = fabs(radius - 0.742) - (0.056 + boost * 0.5);
            double nearestGap = 180.0;

            for (int arm = 0; arm < 3; arm++) {
                double wrapped = fmod(angle - gaps[arm] + 180.0, 360.0);
                if (wrapped < 0) wrapped += 360.0;
                double difference = fabs(wrapped - 180.0);
                if (difference < nearestGap) nearestGap = difference;
            }
            {
                double gapCut = (12.5 - nearestGap) * degree * 0.742;
                double arcs = (ring > gapCut) ? ring : gapCut;
                double spokes = 10.0;
                double parallelMid = 0.052 + boost * 0.12;
                double parallelHalfWidth = 0.024 + boost * 0.42;
                double spokeMid = 0.5 * (0.258 + 0.722);
                double spokeHalfLength = 0.5 * (0.722 - 0.258);

                for (int arm = 0; arm < 3; arm++) {
                    double radians = gaps[arm] * degree;
                    double ux = cos(radians), uy = sin(radians);
                    double along = fabs((x * ux + y * uy) / scale - spokeMid) - spokeHalfLength;
                    double across = fabs(-x * uy + y * ux) / scale;
                    double bars = fabs(across - parallelMid) - parallelHalfWidth;
                    double spoke = (along > bars) ? along : bars;
                    if (spoke < spokes) spokes = spoke;
                }

                {
                    double hubOutside = radius - (0.224 + boost * 0.25);
                    double triangle = -10.0;
                    for (int arm = 0; arm < 3; arm++) {
                        double radians = normals[arm] * degree;
                        double halfPlane = (x * cos(radians) + y * sin(radians)) / scale - 0.074;
                        if (halfPlane > triangle) triangle = halfPlane;
                    }
                    {
                        double hub = (hubOutside > -triangle) ? hubOutside : -triangle;
                        double shell = (outer < arcs) ? outer : arcs;
                        double mark = (shell < spokes) ? shell : spokes;
                        if (hub < mark) mark = hub;

                        double alpha = 0.5 - (mark * scale) / antiAlias;
                        if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
                        {
                            double gradient = xUnit * 0.3 + yUnit * 0.7;
                            if (gradient < 0) gradient = 0;
                            else if (gradient > 1) gradient = 1;
                            {
                                int red = (int)(GetRValue(nearColor) * (1 - gradient) +
                                                GetRValue(farColor) * gradient + 0.5);
                                int green = (int)(GetGValue(nearColor) * (1 - gradient) +
                                                  GetGValue(farColor) * gradient + 0.5);
                                int blue = (int)(GetBValue(nearColor) * (1 - gradient) +
                                                 GetBValue(farColor) * gradient + 0.5);
                                dst[row * n + column] = SuiteEmblemDibRgb(red, green, blue) |
                                    ((uint32_t)(alpha * 255.0 + 0.5) << 24);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void SuiteEmblemRelease(SuiteEmblemEngine *engine) {
    if (!engine) return;
    free(engine->source);
    engine->source = NULL;
    engine->size = 0;
    engine->sourceSize = 0;
    engine->nearColor = 0;
    engine->farColor = 0;
}

static void SuiteEmblemEnsure(SuiteEmblemEngine *engine, int size,
                              COLORREF nearColor, COLORREF farColor) {
    if (!engine) return;
    if (engine->source && engine->size == size && engine->nearColor == nearColor &&
        engine->farColor == farColor) return;
    SuiteEmblemRelease(engine);
    if (size < 8 || size > 512) return;
    {
        int sourceSize = size * 4;
        uint32_t *source = (uint32_t *)malloc((size_t)sourceSize * sourceSize * sizeof(uint32_t));
        if (!source) return;
        SuiteEmblemRasterize(source, sourceSize, nearColor, farColor);
        engine->size = size;
        engine->sourceSize = sourceSize;
        engine->source = source;
        engine->nearColor = nearColor;
        engine->farColor = farColor;
    }
}

/* Alpha-composite the rotated transparent mark into an AARRGGBB DIB canvas. */
static void SuiteEmblemDraw(SuiteEmblemEngine *engine, uint32_t *pixels,
                            int width, int height, int x, int y, int size,
                            double angle, COLORREF nearColor, COLORREF farColor) {
    if (!pixels || width <= 0 || height <= 0) return;
    SuiteEmblemEnsure(engine, size, nearColor, farColor);
    if (!engine || !engine->source) return;

    {
        const uint32_t *source = engine->source;
        int sourceSize = engine->sourceSize;
        int maxSource = sourceSize - 1;
        double cosine = cos(angle), sine = sin(angle);
        double center = (size - 1) * 0.5;
        double scale = (double)(sourceSize - 1) / (double)(size > 1 ? size : 1);

        for (int dy = 0; dy < size; dy++) {
            int targetY = y + dy;
            if (targetY < 0 || targetY >= height) continue;
            {
                double vertical = dy - center;
                for (int dx = 0; dx < size; dx++) {
                    int targetX = x + dx;
                    if (targetX < 0 || targetX >= width) continue;
                    {
                        double horizontal = dx - center;
                        double sourceX = (cosine * horizontal + sine * vertical + center) * scale;
                        double sourceY = (-sine * horizontal + cosine * vertical + center) * scale;
                        if (sourceX < 0) sourceX = 0; else if (sourceX > maxSource) sourceX = maxSource;
                        if (sourceY < 0) sourceY = 0; else if (sourceY > maxSource) sourceY = maxSource;
                        {
                            int x0 = (int)sourceX, y0 = (int)sourceY;
                            double xFraction = sourceX - x0, yFraction = sourceY - y0;
                            double w00 = (1 - xFraction) * (1 - yFraction);
                            double w10 = xFraction * (1 - yFraction);
                            double w01 = (1 - xFraction) * yFraction;
                            double w11 = xFraction * yFraction;
                            const uint32_t *p00 = &source[y0 * sourceSize + x0];
                            uint32_t c00 = p00[0];
                            uint32_t c10 = (x0 < maxSource) ? p00[1] : c00;
                            const uint32_t *p01 = (y0 < maxSource) ? p00 + sourceSize : p00;
                            uint32_t c01 = p01[0];
                            uint32_t c11 = (x0 < maxSource) ? p01[1] : c01;
                            double red = ((c00 >> 16) & 255) * w00 + ((c10 >> 16) & 255) * w10 +
                                         ((c01 >> 16) & 255) * w01 + ((c11 >> 16) & 255) * w11;
                            double green = ((c00 >> 8) & 255) * w00 + ((c10 >> 8) & 255) * w10 +
                                           ((c01 >> 8) & 255) * w01 + ((c11 >> 8) & 255) * w11;
                            double blue = (c00 & 255) * w00 + (c10 & 255) * w10 +
                                          (c01 & 255) * w01 + (c11 & 255) * w11;
                            double alpha = ((c00 >> 24) & 255) * w00 + ((c10 >> 24) & 255) * w10 +
                                           ((c01 >> 24) & 255) * w01 + ((c11 >> 24) & 255) * w11;
                            int opacity = (int)(alpha + 0.5);
                            if (opacity <= 0) continue;
                            if (opacity >= 255) {
                                pixels[targetY * width + targetX] = SuiteEmblemDibRgb(
                                    (int)(red + 0.5), (int)(green + 0.5), (int)(blue + 0.5));
                            } else {
                                uint32_t destination = pixels[targetY * width + targetX];
                                int dstRed = (destination >> 16) & 255;
                                int dstGreen = (destination >> 8) & 255;
                                int dstBlue = destination & 255;
                                pixels[targetY * width + targetX] = SuiteEmblemDibRgb(
                                    ((int)(red + 0.5) * opacity + dstRed * (255 - opacity) + 127) / 255,
                                    ((int)(green + 0.5) * opacity + dstGreen * (255 - opacity) + 127) / 255,
                                    ((int)(blue + 0.5) * opacity + dstBlue * (255 - opacity) + 127) / 255);
                            }
                        }
                    }
                }
            }
        }
    }
}

#endif /* SUITE_WIN32_EMBLEM_ENGINE_INCLUDED */
