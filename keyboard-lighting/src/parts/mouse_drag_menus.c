/* ============================================================================
 *  Mouse Drag, Color Picker, Menus & Window Procedure
 * ============================================================================ */

static void ApplySliderDrag(int kind, int idx, RECT r, int mouseX) {
    int w = r.right - r.left;
    if (w <= 2) return;
    double t = (double)(mouseX - r.left) / (double)w;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    GroupCfg *cg = CurGroup();
    if (kind == HIT_SLIDER_MASTER) {
        g_cfg.masterBrightness = (int)floor(t * 100.0 + 0.5);
        WriteLiveBrightness();
        ScheduleLiveApply();
    } else if (kind == HIT_SLIDER_SPEED) {
        cg->speed = 1 + (int)floor(t * 49.0 + 0.5);
        SyncLinkedZone();
        ScheduleLiveApply();
    } else if (kind == HIT_SLIDER_BRIGHT) {
        cg->brightness = (int)floor(t * 100.0 + 0.5);
        SyncLinkedZone();
        ScheduleLiveApply();
    } else if (kind >= HIT_WIDTH_0 && kind < HIT_WIDTH_0 + 8 && idx >= 0 && idx < cg->swatchCount) {
        cg->widths[idx] = 0.25 * pow(16.0, t);
        SyncLinkedZone();
        ScheduleLiveApply();
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void OpenEffectPopup(HWND hwnd, RECT anchor) {
    HMENU hMenu = CreatePopupMenu();
    GroupCfg *cg = CurGroup();
    for (int i = 0; i < EFFECT_COUNT; i++) {
        wchar_t lbl[64];
        _snwprintf(lbl, 63, L"%S", kEffects[i].label);
        AppendMenuW(hMenu, MF_STRING | (cg->effectIdx == i ? MF_CHECKED : 0),
                    (UINT_PTR)(IDM_EFF_BASE + i), lbl);
    }
    POINT pt = { anchor.left, anchor.bottom + 2 };
    ClientToScreen(hwnd, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

