/* ============================================================================
 *  Sub-Pixel Anti-Aliased Smooth Rounded UI Renderer (32-bit DIBSection)
 * ============================================================================ */

static inline uint32_t PackRgb(COLORREF c) {
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static inline uint32_t BlendRgb(uint32_t dst, uint32_t src, int a256) {
    if (a256 <= 0) return dst;
    if (a256 >= 256) return src;
    int inv = 256 - a256;
    uint32_t rb = (((dst & 0x00FF00FFu) * (uint32_t)inv) + ((src & 0x00FF00FFu) * (uint32_t)a256)) >> 8;
    uint32_t g  = (((dst & 0x0000FF00u) * (uint32_t)inv) + ((src & 0x0000FF00u) * (uint32_t)a256)) >> 8;
    return (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void FillCanvasRect(uint32_t *px, int W, int H, int x0, int y0, int x1, int y1, COLORREF col) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t p = PackRgb(col);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; x++) row[x] = p;
    }
}

static void SmoothRoundRect(uint32_t *px, int W, int H, const RECT *rc, float radius,
                            COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    float x0 = (float)rc->left, y0 = (float)rc->top;
    float x1 = (float)rc->right, y1 = (float)rc->bottom;
    float rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0.5f || rh <= 0.5f) return;
    if (radius * 2.0f > rw) radius = rw * 0.5f;
    if (radius * 2.0f > rh) radius = rh * 0.5f;

    float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
    float hx = 0.5f * rw - radius, hy = 0.5f * rh - radius;

    int iy0 = (int)floorf(y0 - 1.0f), iy1 = (int)ceilf(y1 + 1.0f);
    int ix0 = (int)floorf(x0 - 1.0f), ix1 = (int)ceilf(x1 + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP = PackRgb(fillCol), borderP = PackRgb(borderCol);
    BOOL sameBorder = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float qy = fabsf(((float)y + 0.5f) - cy) - hy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float qx = fabsf(((float)x + 0.5f) - cx) - hx;
            float sd = (qx > 0.0f && qy > 0.0f) ? (sqrtf(qx * qx + qy * qy) - radius)
                                                 : ((qx > qy ? qx : qy) - radius);
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) row[x] = c;
            else row[x] = BlendRgb(row[x], c, (int)((0.5f - sd) * 256.0f));
        }
    }
}

static void SmoothCircle(uint32_t *px, int W, int H, float cx, float cy, float radius,
                         COLORREF fillCol, COLORREF borderCol, int clipTop, int clipBottom) {
    if (radius <= 0.5f) return;
    int iy0 = (int)floorf(cy - radius - 1.0f), iy1 = (int)ceilf(cy + radius + 1.0f);
    int ix0 = (int)floorf(cx - radius - 1.0f), ix1 = (int)ceilf(cx + radius + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t fillP = PackRgb(fillCol), borderP = PackRgb(borderCol);
    BOOL sameBorder = (fillP == borderP);

    for (int y = iy0; y < iy1; y++) {
        float dy = ((float)y + 0.5f) - cy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float sd = sqrtf(dx * dx + dy * dy) - radius;
            if (sd >= 0.5f) continue;

            uint32_t c = fillP;
            if (!sameBorder && sd > -1.6f) {
                float ba = 1.0f - fabsf(sd + 0.5f);
                if (ba > 0.0f) {
                    int b256 = (int)(ba * 256.0f);
                    c = BlendRgb(fillP, borderP, b256 > 256 ? 256 : b256);
                }
            }
            if (sd <= -0.5f) row[x] = c;
            else row[x] = BlendRgb(row[x], c, (int)((0.5f - sd) * 256.0f));
        }
    }
}

/* Thin annulus: the track that makes the Light Bar read as one closed loop. */
static void SmoothRingTrack(uint32_t *px, int W, int H, float cx, float cy,
                            float radius, float thick, COLORREF col,
                            int clipTop, int clipBottom) {
    if (radius <= 0.5f || thick <= 0.5f) return;
    float outer = radius + thick * 0.5f;
    int iy0 = (int)floorf(cy - outer - 1.0f), iy1 = (int)ceilf(cy + outer + 1.0f);
    int ix0 = (int)floorf(cx - outer - 1.0f), ix1 = (int)ceilf(cx + outer + 1.0f);
    if (iy0 < clipTop) iy0 = clipTop;
    if (iy1 > clipBottom) iy1 = clipBottom;
    if (iy0 < 0) iy0 = 0;
    if (iy1 > H) iy1 = H;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > W) ix1 = W;

    uint32_t p = PackRgb(col);
    for (int y = iy0; y < iy1; y++) {
        float dy = ((float)y + 0.5f) - cy;
        uint32_t *row = px + (size_t)y * (size_t)W;
        for (int x = ix0; x < ix1; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float sd = fabsf(sqrtf(dx * dx + dy * dy) - radius) - thick * 0.5f;
            if (sd >= 0.5f) continue;
            if (sd <= -0.5f) row[x] = p;
            else row[x] = BlendRgb(row[x], p, (int)((0.5f - sd) * 256.0f));
        }
    }
}

