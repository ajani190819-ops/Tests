/* ---- M4/M5 hold-to-repeat ------------------------------
   G-Helper's Custom action fires ONCE per press (no key-up, no repeat).
   Three independent signals can reveal the rest of the physical gesture:
     1. The key may ALSO report as a normal keyboard key: Windows then
        sends typematic REPEAT keydowns and a key-up while it is held.
     2. RawInput keyboard reports (catches key usages that never become
        WM_KEYDOWN - VKey 255 vendor keys).
     3. The ASUS input HID stream (VID 0x0B05, report id 0x5A - the very
        stream G-Helper reads): its press frames may REPEAT while held,
        and some models send a RELEASE frame (byte1 == 0, like the ROG
        Ally). Its byte1 is a full eight-bit ACTION ID, not a scan code
        with a break bit. High-bit IDs such as 0xB5 are presses too.
        Only an observed zero on the same device establishes HID release.
   Raw events supply hold/release evidence only. Genuine Custom commands
   enter the native master controller once; timers never rewrite their flags.
   The native-owned engine consumes absolute levels, not a second gesture
   state machine. Latch/burst still need only Custom commands.
   Learned bindings persist in presets.ini [Keys] M4Scan=/M5Scan=/M4Vk=/
   M5Vk=/M4Hid=/M5Hid=/M4HidRelease=/M5HidRelease=. Without any hold signal,
   behavior stays a clean tap per press. */
typedef struct KeyRec { ULONGLONG tick; DWORD scan; WORD vk; BYTE down; BYTE injected; } KeyRec;
static KeyRec    g_keyRing[256];
static int       g_keyRingHead = 0;
typedef struct HidRec { ULONGLONG tick; HANDLE device; DWORD code; BYTE rel; } HidRec;
static HidRec    g_hidRing[256];
static int       g_hidRingHead = 0;
static int       g_scanBound[2] = { 0, 0 };
static DWORD     g_scanVk[2] = { 0, 0 };
static DWORD     g_hidCode[2] = { 0, 0 };
static HANDLE    g_hidDevice[2];
enum { HOLD_SCAN = 1, HOLD_HID = 2, HID_RELEASE_ZERO = 2 };
static BYTE      g_signalSource[2];
static BYTE      g_scanArmed[2] = { 1, 1 };
static BYTE      g_hidDown[2] = { 0, 0 };
static BYTE      g_holdOn[2] = { 0, 0 };
static BYTE      g_hidRelSeen[2] = { 0, 0 };
static ULONGLONG g_sigAt[2] = { 0, 0 };
static ULONGLONG g_scanNext[2] = { 0, 0 };
static ULONGLONG g_gestureAt[2], g_gestureReleasedAt[2], g_customGestureAt[2];
static ULONGLONG g_customAt[2];
static BYTE g_customAwaitRaw[2];
static int       g_learnCand[2] = { 0, 0 };
static int       g_learnN[2] = { 0, 0 };
static DWORD     g_learnHid[2] = { 0, 0 };
static int       g_learnHidN[2] = { 0, 0 };
static char      g_seenTok[2][32];
static ULONGLONG g_pendingLearnAt[2] = { 0, 0 };
static ULONGLONG g_pendingLearnUntil[2] = { 0, 0 };

/* Aggregate receiver health only: never persist typed keys, scan codes, HID
   payloads, device identities or profile contents in the diagnostic snapshot. */
typedef struct HoldInputDiagnostics {
    DWORD enumerateError;
    DWORD registrationError;
    UINT requestedUsages;
    UINT vendorUsages;
    BOOL keyboardReady;
    ULONGLONG keyboardReports;
    ULONGLONG asusReports;
    ULONGLONG tokenObservations[2];
    ULONGLONG holdSteps[2];
} HoldInputDiagnostics;
static HoldInputDiagnostics g_holdInputDiag;
static ULONGLONG g_holdDiagAt;

static void ReleaseSignal(int dir, ULONGLONG now);
static void PressSignal(int dir, int repeat, int source, ULONGLONG now);
static void PersistKeyValue(const wchar_t *key, DWORD value);
static void MarkHidReleaseSeen(int dir);
static int HoldModeRead(void);

static int HasReleaseSrc(int dir) {
    /* Old value 1 also included invented press|0x80 releases. Preserve that
       saved metadata, but only value 2 proves the zero-frame format. */
    return g_scanBound[dir] != 0 ||
           (g_hidCode[dir] != 0 && g_hidRelSeen[dir] == HID_RELEASE_ZERO);
}

static int SignalHasRelease(int dir) {
    /* A saved keyboard binding is not evidence that this HID-only gesture
       came from that keyboard. Likewise, its stale VK cannot veto HID hold. */
    return g_signalSource[dir] == HOLD_SCAN ||
           (g_signalSource[dir] == HOLD_HID && g_hidRelSeen[dir] == HID_RELEASE_ZERO);
}

static void ExpirePressOnlySignal(int dir, ULONGLONG now) {
    if (g_signalSource[dir] == HOLD_HID && !SignalHasRelease(dir) &&
        g_gestureAt[dir] && !g_gestureReleasedAt[dir] &&
        (now < g_sigAt[dir] || now - g_sigAt[dir] > 300ULL)) {
        /* Retire silence; it is NOT a physical release or release capability.
           Otherwise a later genuine press is swallowed as an old duplicate. */
        ReleaseSignal(dir, now < g_sigAt[dir] ? now : g_sigAt[dir] + 300ULL);
    }
}

static ULONGLONG TickDistance(ULONGLONG a, ULONGLONG b) {
    return a >= b ? a - b : b - a;
}

static int ScanCurrentlyDown(DWORD scan, DWORD vk) {
    if (vk > 0 && vk < 0xFF)
        return (GetAsyncKeyState((int)vk) & 0x8000) != 0;
    for (int i = 0; i < 256; i++) {
        int idx = (g_keyRingHead + 255 - i) & 255;
        KeyRec *r = &g_keyRing[idx];
        if (r->tick == 0) break;
        if (r->scan == scan) return r->down != 0;
    }
    return 0;
}

static int HidCurrentlyDown(DWORD code, HANDLE device) {
    for (int i = 0; i < 256; i++) {
        int idx = (g_hidRingHead + 255 - i) & 255;
        HidRec *r = &g_hidRing[idx];
        if (r->tick == 0) break;
        if (r->device == device) return r->code == code && !r->rel;
    }
    return 0;
}

static int HidReleaseSeen(DWORD code, HANDLE device) {
    for (int i = 0; i < 256; i++) {
        int idx = (g_hidRingHead + 255 - i) & 255;
        HidRec *r = &g_hidRing[idx];
        if (r->tick == 0) break;
        if (r->code == code && r->device == device && r->rel) return 1;
    }
    return 0;
}

/* Native repeats are evidence, NEVER another Custom press. Every input
   shares the master controller and its independent, bounded 30ms clock. */
static void HoldPump(void) {
    ULONGLONG now = GetTickCount64();
    SyncMasterBrightness();
    int mode = HoldModeRead();
    BrightnessMode(&g_brightness, mode);
    if (mode != 0) {
        g_holdOn[0] = g_holdOn[1] = 0;
    } else for (int i = 0; i < 2; i++) {
        ExpirePressOnlySignal(i, now);
        if (!g_holdOn[i]) continue;
        if (g_signalSource[i] == HOLD_SCAN && g_scanBound[i] &&
            g_scanVk[i] > 0 && g_scanVk[i] < 0xFF &&
            (GetAsyncKeyState((int)g_scanVk[i]) & 0x8000) == 0) {
            ReleaseSignal(i, now);
            continue;
        }
        if (now < g_gestureAt[i] || now - g_gestureAt[i] >= 10000ULL) {
            /* Never re-arm a stuck raw key at the ceiling. Only its real
               release or a fresh Custom action may begin another gesture. */
            g_holdOn[i] = 0;
            BrightnessRelease(&g_brightness, i);
            continue;
        }
        if (now >= g_scanNext[i]) {
            g_scanNext[i] = now + 30;
            if (g_customGestureAt[i] == g_gestureAt[i])
                BrightnessHeld(&g_brightness, i, now);
        }
    }
    int before = g_brightness.level, direction = g_brightness.direction;
    if (BrightnessTick(&g_brightness, now)) {
        g_holdInputDiag.holdSteps[direction > 0]++;
        PublishMasterBrightness(before, TRUE);
    }
}

static void PressSignal(int dir, int repeat, int source, ULONGLONG now) {
    if (!repeat && (!g_gestureAt[dir] || g_gestureReleasedAt[dir])) {
        g_gestureAt[dir] = now;
        g_gestureReleasedAt[dir] = 0;
        if (g_customAwaitRaw[dir] && now >= g_customAt[dir] && now - g_customAt[dir] <= 750ULL) {
            g_customGestureAt[dir] = now;
            g_customAwaitRaw[dir] = 0;
        }
    }
    /* Prefer an actually down keyboard source over its duplicate HID report,
       never a merely remembered scan/VK. The source owns release polling. */
    if (source == HOLD_SCAN || g_signalSource[dir] != HOLD_SCAN || g_scanArmed[dir])
        g_signalSource[dir] = (BYTE)source;
    g_sigAt[dir] = now;
    if (repeat || SignalHasRelease(dir)) {
        g_holdOn[dir] = 1;
        if (!repeat) g_scanNext[dir] = now + 30;
    }
}

static void ReleaseSignal(int dir, ULONGLONG now) {
    g_gestureReleasedAt[dir] = now;
    g_holdOn[dir] = 0;
    g_scanArmed[dir] = 1;
    BrightnessRelease(&g_brightness, dir);
    g_hidDown[dir] = 0;
    g_signalSource[dir] = 0;
    /* Keyboard up, silence and the ceiling must not certify a HID format. */
}

static void OnScanEvent(DWORD scan, int down, int injected, ULONGLONG now) {
    if (injected) return;
    for (int i = 0; i < 2; i++) {
        if (g_scanBound[i] == 0 || (int)scan != g_scanBound[i]) continue;
        if (!down) {
            ReleaseSignal(i, now);
        } else if (g_scanArmed[i]) {
            /* Fresh press: G-Helper's Custom action writes this tap's
               token already. Gate briefly so a duplicate raw-input copy of
               this same down cannot double-step. */
            g_scanArmed[i] = 0;
            g_scanNext[i] = now + 30;
            PressSignal(i, 0, HOLD_SCAN, now);
        } else {
            PressSignal(i, 1, HOLD_SCAN, now);
        }
    }
}

static void PersistKeyValue(const wchar_t *key, DWORD value) {
    wchar_t num[16], path[MAX_PATH];
    _snwprintf(num, 15, L"%lu", (unsigned long)value);
    num[15] = L'\0';
    _snwprintf(path, MAX_PATH - 1, L"%s\\presets.ini", g_cfgDir);
    path[MAX_PATH - 1] = L'\0';
    WritePrivateProfileStringW(L"Keys", key, num, path);
}

static void MarkHidReleaseSeen(int dir) {
    if (g_hidRelSeen[dir] != HID_RELEASE_ZERO) {
        g_hidRelSeen[dir] = HID_RELEASE_ZERO;
        PersistKeyValue(dir == 0 ? L"M4HidRelease" : L"M5HidRelease", HID_RELEASE_ZERO);
    }
}

static void BindScan(int dir, DWORD scan, DWORD vk) {
    if (!scan || (int)scan == g_scanBound[1 - dir]) return;
    g_scanBound[dir] = (int)scan;
    g_scanVk[dir] = vk;
    g_scanArmed[dir] = 1;
    PersistKeyValue(dir == 0 ? L"M4Scan" : L"M5Scan", scan);
    PersistKeyValue(dir == 0 ? L"M4Vk" : L"M5Vk", vk);
}

static void BindHid(int dir, DWORD code) {
    if (!code || code == g_hidCode[1 - dir]) return;
    g_hidCode[dir] = code;
    PersistKeyValue(dir == 0 ? L"M4Hid" : L"M5Hid", code);
}

static void OnRawHid(HANDLE device, const BYTE *b, int len, ULONGLONG now) {
    if (!device || len < 2 || b[0] != 0x5A) return;
    g_holdInputDiag.asusReports++;
    DWORD code = b[1];
    int rel = code == 0, mapped = -1;
    if (rel) {
        /* Zero belongs to the most recent report on THIS raw device, not the
           last key/device observed globally. Unpaired zeros prove nothing. */
        HidRec *previous = NULL;
        for (int i = 0; i < 256; i++) {
            HidRec *r = &g_hidRing[(g_hidRingHead + 255 - i) & 255];
            if (!r->tick) break;
            if (r->device == device) { previous = r; break; }
        }
        if (!previous || previous->rel || !previous->code) return;
        code = previous->code;
    }
    /* G-Helper ignores 0xEC. Keep a non-action barrier in the ring so a later
       zero cannot be paired with an earlier, unrelated M-key report. */
    if (code == 0xEC) { code = 0; rel = 1; }
    HidRec *r = &g_hidRing[g_hidRingHead];
    r->tick = now; r->device = device; r->code = code; r->rel = (BYTE)rel;
    g_hidRingHead = (g_hidRingHead + 1) & 255;
    for (int i = 0; i < 2; i++)
        if (code != 0 && code == g_hidCode[i]) mapped = i;
    if (mapped >= 0) {
        ExpirePressOnlySignal(mapped, now);
        if (rel) {
            if (g_hidDevice[mapped] == device) {
                MarkHidReleaseSeen(mapped);
                ReleaseSignal(mapped, now);
            }
        } else {
            int repeat = g_hidDown[mapped] && g_hidDevice[mapped] == device;
            g_hidDevice[mapped] = device;
            g_hidDown[mapped] = 1;
            PressSignal(mapped, repeat, HOLD_HID, now);
        }
    }
}

static int ParseBrightTokenTick(const char *token, ULONGLONG now, ULONGLONG *tick) {
    char *end = NULL;
    if (!token || token[0] < '0' || token[0] > '9' || !tick) return 0;
    unsigned long long value = strtoull(token, &end, 10);
    if (end == token || (ULONGLONG)value > now) return 0;
    /* New transient writers add their PID, so distinct launches in one
       millisecond remain distinct. Numeric legacy tokens still work. */
    if (*end == ':') {
        char *pidEnd = NULL;
        if (end[1] < '0' || end[1] > '9') return 0;
        unsigned long pid = strtoul(end + 1, &pidEnd, 10);
        if (!pid || !pidEnd || *pidEnd) return 0;
    } else if (*end) return 0;
    *tick = (ULONGLONG)value;
    return 1;
}

static void LearnFromRing(int dir, ULONGLONG tokenAt, int newToken) {
    ULONGLONG now = GetTickCount64();
    if (!tokenAt) tokenAt = now;

    /* Learn each family only until bound. Genuine Custom tokens alone may
       correlate inputs; held timer ticks can never rebind either key. */
    if (g_scanBound[dir] == 0) {
        /* G-Helper writes its command token just after the physical key event.
           Correlate timestamps so the first held gesture can bind and repeat. */
        KeyRec *bestKey = NULL;
        ULONGLONG bestKeyDelta = (ULONGLONG)-1;
        for (int i = 0; i < 256; i++) {
            int idx = (g_keyRingHead + 255 - i) & 255;
            KeyRec *r = &g_keyRing[idx];
            if (r->tick == 0) break;
            if (!r->down || r->injected || r->scan == 0 ||
                (int)r->scan == g_scanBound[1 - dir]) continue;
            if (r->scan == 0x1D || r->scan == 0x2A || r->scan == 0x36 || r->scan == 0x38) continue;
            ULONGLONG delta = TickDistance(r->tick, tokenAt);
            if (delta <= 750ULL && delta < bestKeyDelta) {
                bestKey = r;
                bestKeyDelta = delta;
            }
        }
        if (bestKey) {
            BindScan(dir, bestKey->scan, bestKey->vk);
            g_learnCand[dir] = (int)bestKey->scan;
            g_learnN[dir] = 2;
            if (!g_gestureAt[dir] || g_gestureReleasedAt[dir]) g_gestureAt[dir] = bestKey->tick;
            g_gestureReleasedAt[dir] = ScanCurrentlyDown(bestKey->scan, bestKey->vk) ? 0 : now;
            if (ScanCurrentlyDown(bestKey->scan, bestKey->vk)) {
                g_scanArmed[dir] = 0;
                PressSignal(dir, 0, HOLD_SCAN, now);
            }
        } else if (newToken) {
            /* If a delayed token still finds the key down, bind it now; for a
               released tap, retain the two-distinct-token learning fallback. */
            int cand = 0;
            DWORD candVk = 0;
            for (int i = 0; i < 256; i++) {
                int idx = (g_keyRingHead + 255 - i) & 255;
                KeyRec *r = &g_keyRing[idx];
                if (r->tick == 0) break;
                if (now < r->tick || now - r->tick > 1500ULL) break;
                if (!r->down || r->injected || !r->scan ||
                    (int)r->scan == g_scanBound[1 - dir]) continue;
                if (r->scan == 0x1D || r->scan == 0x2A || r->scan == 0x36 || r->scan == 0x38) continue;
                cand = (int)r->scan;
                candVk = r->vk;
                break;
            }
            if (cand && ScanCurrentlyDown((DWORD)cand, candVk)) {
                /* The poll can lag the launch token while the UI thread is
                   busy. If the candidate is still physically down, this live
                   gesture is stronger evidence than a second-token heuristic. */
                BindScan(dir, (DWORD)cand, candVk);
                g_learnCand[dir] = cand;
                g_learnN[dir] = 2;
                g_scanArmed[dir] = 0;
                PressSignal(dir, 0, HOLD_SCAN, now);
            } else if (cand) {
                if (g_learnCand[dir] == cand) {
                    g_learnN[dir]++;
                    if (g_learnN[dir] >= 2)
                        BindScan(dir, (DWORD)cand, candVk);
                } else {
                    g_learnCand[dir] = cand;
                    g_learnN[dir] = 1;
                }
            }
        }
    }

    if (g_hidCode[dir] == 0) {
        HidRec *bestHid = NULL;
        ULONGLONG bestHidDelta = (ULONGLONG)-1;
        for (int i = 0; i < 256; i++) {
            int idx = (g_hidRingHead + 255 - i) & 255;
            HidRec *r = &g_hidRing[idx];
            if (r->tick == 0) break;
            if (r->rel || r->code == 0 || r->code == g_hidCode[1 - dir]) continue;
            ULONGLONG delta = TickDistance(r->tick, tokenAt);
            if (delta <= 750ULL && delta < bestHidDelta) {
                bestHid = r;
                bestHidDelta = delta;
            }
        }
        if (bestHid) {
            DWORD hc = bestHid->code;
            BindHid(dir, hc);
            g_learnHid[dir] = hc;
            g_learnHidN[dir] = 2;
            g_hidDevice[dir] = bestHid->device;
            if (HidReleaseSeen(hc, bestHid->device)) MarkHidReleaseSeen(dir);
            if (!g_gestureAt[dir] || g_gestureReleasedAt[dir]) g_gestureAt[dir] = bestHid->tick;
            g_gestureReleasedAt[dir] = HidCurrentlyDown(hc, bestHid->device) ? 0 : now;
            if (HidCurrentlyDown(hc, bestHid->device)) {
                g_hidDown[dir] = 1;
                if (!g_holdOn[dir]) {
                    /* Adopt the first press before its report code was known;
                       the matching release can now stop this same gesture. */
                    g_scanNext[dir] = now + 30;
                    PressSignal(dir, 0, HOLD_HID, now);
                }
            }
        } else if (newToken) {
            DWORD hc = 0;
            for (int i = 0; i < 256; i++) {
                int idx = (g_hidRingHead + 255 - i) & 255;
                HidRec *r = &g_hidRing[idx];
                if (r->tick == 0) break;
                if (now < r->tick || now - r->tick > 1500ULL) break;
                if (r->rel || r->code == 0 || r->code == g_hidCode[1 - dir]) continue;
                hc = r->code;
                break;
            }
            if (hc) {
                if (g_learnHid[dir] == hc) {
                    g_learnHidN[dir]++;
                    if (g_learnHidN[dir] >= 2)
                        BindHid(dir, hc);
                } else {
                    g_learnHid[dir] = hc;
                    g_learnHidN[dir] = 1;
                }
            }
        }
    }
}

static void SeedBrightTokenWatch(void) {
    ULONGLONG now = GetTickCount64();
    for (int dir = 0; dir < 2; dir++) {
        char cur[32] = { 0 };
        DWORD len = 0;
        char *txt = ReadFileUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, &len);
        if (txt) { strncpy(cur, txt, 31); free(txt); }
        cur[31] = '\0';
        cur[strcspn(cur, "\r\n")] = '\0';
        if (cur[0]) {
            ULONGLONG tokenAt = 0;
            strncpy(g_seenTok[dir], cur, 31);
            if (ParseBrightTokenTick(cur, now, &tokenAt) && now - tokenAt <= 1500ULL) {
                /* The app may start just after G-Helper's first launch token.
                   Keep that fresh token pending until raw input is dispatched. */
                g_pendingLearnAt[dir] = tokenAt;
                g_pendingLearnUntil[dir] = now + 1500ULL;
            }
        } else {
            strncpy(g_seenTok[dir], "seeded", 31);
        }
        g_seenTok[dir][31] = '\0';
    }
}

static int TokenMatchesGesture(int dir, ULONGLONG tokenAt) {
    if (!g_gestureAt[dir]) return 0;
    if (tokenAt < g_gestureAt[dir]) return g_gestureAt[dir] - tokenAt <= 750ULL;
    ULONGLONG finish = g_gestureReleasedAt[dir];
    return finish ? tokenAt <= finish + 750ULL : tokenAt - g_gestureAt[dir] <= 10000ULL;
}

static void WatchBrightTokens(void) {
    ULONGLONG now = GetTickCount64();
    for (int dir = 0; dir < 2; dir++) {
        ExpirePressOnlySignal(dir, now);
        char cur[32] = { 0 };
        DWORD len = 0;
        char *txt = ReadFileUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, &len);
        if (txt) { if (len < sizeof(cur)) memcpy(cur, txt, len); free(txt); }
        cur[strcspn(cur, "\r\n")] = '\0';
        if (cur[0] && strcmp(cur, g_seenTok[dir]) != 0) {
            ULONGLONG tokenAt = 0;
            strncpy(g_seenTok[dir], cur, 31);
            g_seenTok[dir][31] = '\0';
            if (!ParseBrightTokenTick(cur, now, &tokenAt) || now - tokenAt > 1500ULL) continue;
            g_holdInputDiag.tokenObservations[dir]++;
            g_pendingLearnAt[dir] = tokenAt;
            g_pendingLearnUntil[dir] = now + 1500ULL;
            LearnFromRing(dir, tokenAt, 1);
            int associated = TokenMatchesGesture(dir, tokenAt);
            if (associated && g_customGestureAt[dir] == g_gestureAt[dir]) {
                if (!g_gestureReleasedAt[dir] || tokenAt <= g_gestureReleasedAt[dir]) continue;
                /* A new launch after an observed release may arrive before
                   its new raw event. Do not attach it to the consumed hold. */
                associated = 0;
            }
            g_customAt[dir] = tokenAt;
            g_customAwaitRaw[dir] = (BYTE)!associated;
            if (associated) g_customGestureAt[dir] = g_gestureAt[dir];
            SyncMasterBrightness();
            int before = g_brightness.level;
            BrightnessPress(&g_brightness, dir, HoldModeRead(), now);
            PublishMasterBrightness(before, TRUE);
        }
        if (g_pendingLearnAt[dir] && now <= g_pendingLearnUntil[dir] &&
            (g_scanBound[dir] == 0 || g_hidCode[dir] == 0)) {
            LearnFromRing(dir, g_pendingLearnAt[dir], 0);
            if (g_customAwaitRaw[dir] && g_customGestureAt[dir] != g_gestureAt[dir] &&
                TokenMatchesGesture(dir, g_customAt[dir])) {
                /* Do not re-attach a new launch to the consumed gesture that
                   the new-token path just rejected. Its raw event is pending. */
                g_customGestureAt[dir] = g_gestureAt[dir];
                g_customAwaitRaw[dir] = 0;
            }
        } else if (now > g_pendingLearnUntil[dir]) g_pendingLearnAt[dir] = 0;
    }
}

static void InitRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid[16];
    UINT n = 0;
    g_holdInputDiag.enumerateError = 0;
    g_holdInputDiag.registrationError = 0;
    g_holdInputDiag.keyboardReady = FALSE;
    g_holdInputDiag.vendorUsages = 0;
    g_holdInputDiag.requestedUsages = 0;
    if (!hwnd) {
        g_holdInputDiag.registrationError = ERROR_INVALID_WINDOW_HANDLE;
        return;
    }
    rid[n].usUsagePage = 0x01; rid[n].usUsage = 0x06;
    rid[n].dwFlags = RIDEV_INPUTSINK; rid[n].hwndTarget = hwnd;
    n++;
    UINT num = 0;
    UINT queried = GetRawInputDeviceList(NULL, &num, sizeof(RAWINPUTDEVICELIST));
    if (queried == (UINT)-1) {
        g_holdInputDiag.enumerateError = GetLastError();
    } else if (num > 64) {
        g_holdInputDiag.enumerateError = ERROR_INSUFFICIENT_BUFFER;
    } else if (num > 0) {
        RAWINPUTDEVICELIST list[64] = {0};
        UINT capacity = 64;
        UINT found = GetRawInputDeviceList(list, &capacity, sizeof(RAWINPUTDEVICELIST));
        /* Devices can change between the size query and fill. UINT_MAX is an
           error, never a count: the old loop could walk past list[64]. Keep
           keyboard fallback usable and report the actual enumeration failure. */
        if (found != (UINT)-1 && found <= 64) {
            for (UINT i = 0; i < found && n < 16; i++) {
                RID_DEVICE_INFO info = {0};
                UINT cb = sizeof(info);
                info.cbSize = sizeof(info);
                if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &cb) != (UINT)-1 &&
                    info.dwType == RIM_TYPEHID && info.hid.dwVendorId == 0x0B05) {
                    UINT j; int dup = 0;
                    for (j = 0; j < n; j++)
                        if (rid[j].usUsagePage == info.hid.usUsagePage && rid[j].usUsage == info.hid.usUsage) dup = 1;
                    if (!dup) {
                        rid[n].usUsagePage = info.hid.usUsagePage;
                        rid[n].usUsage = info.hid.usUsage;
                        rid[n].dwFlags = RIDEV_INPUTSINK;
                        rid[n].hwndTarget = hwnd;
                        n++;
                    }
                }
            }
        } else {
            g_holdInputDiag.enumerateError = found == (UINT)-1
                ? GetLastError() : ERROR_INVALID_DATA;
        }
    }
    g_holdInputDiag.requestedUsages = n;
    if (!RegisterRawInputDevices(rid, n, sizeof(RAWINPUTDEVICE))) {
        /* A rejected ASUS usage must not also disable the generic keyboard
           registration that can still report the M4/M5 scan codes. */
        g_holdInputDiag.registrationError = GetLastError();
        g_holdInputDiag.keyboardReady = RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE));
        if (!g_holdInputDiag.keyboardReady) {
            DWORD error = GetLastError();
            if (error) g_holdInputDiag.registrationError = error;
        }
    } else {
        g_holdInputDiag.keyboardReady = TRUE;
        g_holdInputDiag.vendorUsages = n - 1;
    }
}

static void OnRawInputMsg(HWND hwnd, LPARAM lp) {
    (void)hwnd;
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    if (dwSize == 0 || dwSize >= 4096) return;
    BYTE buf[4096];
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) return;
    const RAWINPUT *ri = (const RAWINPUT *)buf;
    ULONGLONG now = GetTickCount64();
    if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        g_holdInputDiag.keyboardReports++;
        int down = (ri->data.keyboard.Flags & RI_KEY_BREAK) == 0;
        KeyRec *r = &g_keyRing[g_keyRingHead];
        r->tick = now; r->scan = ri->data.keyboard.MakeCode;
        r->vk = ri->data.keyboard.VKey;
        r->down = (BYTE)down; r->injected = 0;
        g_keyRingHead = (g_keyRingHead + 1) & 255;
        OnScanEvent(ri->data.keyboard.MakeCode, down, 0, now);
    } else if (ri->header.dwType == RIM_TYPEHID) {
        const RAWHID *hid = &ri->data.hid;
        size_t rawOffset = (size_t)(hid->bRawData - buf);
        size_t available = rawOffset <= dwSize ? (size_t)dwSize - rawOffset : 0;
        if (hid->dwSizeHid == 0 || hid->dwCount == 0 ||
            (size_t)hid->dwSizeHid > available / (size_t)hid->dwCount) return;
        /* One WM_INPUT can contain several reports. Preserve their order so a
           press and release batched together still form a complete gesture. */
        for (DWORD i = 0; i < hid->dwCount; i++) {
            OnRawHid(ri->header.hDevice, hid->bRawData + (size_t)i * (size_t)hid->dwSizeHid,
                     (int)hid->dwSizeHid, now);
        }
    }
}

/* ---- hold behaviour, reachable from the tray -------------------------
   The engine has supported three hold behaviours all along and re-reads
   hold_mode.txt on every tick, so switching needs no restart. The only way to
   reach them used to be the -HoldMode command line, which left the two that do
   NOT depend on learning the keys effectively unreachable. That matters
   because "tap" degrades silently: its hold-to-sweep needs this panel to
   recognise the M-keys, and on a machine where they produce neither a
   typematic repeat nor a release frame there is nothing to recognise, so
   holding does nothing at all and every press is a single 5% step, with no
   explanation anywhere. */
static const char *kHoldModes[3] = { "tap", "latch", "burst" };
static const wchar_t *kHoldLabels[3] = {
    L"Sweep While Held",
    L"Press To Glide, Press To Stop",
    L"Press For A 20% Glide"
};

/* Read from the file rather than tracked in a global, so the tray menu stays
   correct when the mode was set out of process by -HoldMode. */
static int HoldModeRead(void) {
    DWORD len = 0;
    char *txt = ReadFileUtf8(g_holdModePath, &len);
    int mode = 0;
    if (txt) {
        if (strncmp(txt, "latch", 5) == 0) mode = 1;
        else if (strncmp(txt, "burst", 5) == 0) mode = 2;
        free(txt);
    }
    return mode;
}

static void HoldModeWrite(int mode) {
    if (mode < 0 || mode > 2) mode = 0;
    WriteAtomicUtf8(g_holdModePath, kHoldModes[mode]);
}

/* What the tray reports about whether hold-to-sweep can work at all. This
   reflects HasReleaseSrc rather than mere learning, because that is the actual
   gate: a fresh press only starts the stream when a release is observable, so
   a key whose HID code was learned but which has never been seen to release is
   not ready, however many times it has been pressed. */
static const wchar_t *HoldReadyLabel(void) {
    int known = 0;
    for (int i = 0; i < 2; i++)
        if (g_scanBound[i] != 0 || g_hidCode[i] != 0) known++;
    if (HasReleaseSrc(0) && HasReleaseSrc(1)) return L"M4/M5 Keys: Hold Ready";
    if (known == 0) return L"M4/M5 Keys: Not Recognised";
    return L"M4/M5 Keys: Still Learning";
}


/* One small local snapshot per minute, plus the initial receiver setup. The
   dedicated filename never replaces settings or engine.log. A failed write
   does not stop input, change preferences or start another app/engine. */
static void WriteHoldDiagnostics(int force) {
    ULONGLONG now = GetTickCount64();
    if (!force && now - g_holdDiagAt < 60000ULL) return;
    g_holdDiagAt = now;
    wchar_t path[MAX_PATH];
    /* Leave room for the atomic writer's .tmp suffix too. */
    int plen = _snwprintf(path, MAX_PATH - 5, L"%ls\\input-status.txt", g_cfgDir);
    if (plen < 0 || plen >= MAX_PATH - 5) return;
    path[plen] = L'\0';
    char text[2048];
    int length = _snprintf(text, sizeof(text),
        "schema=tests.lighting-input.v2\r\n"
        "panel_pid=%lu\r\nsample_tick_ms=%llu\r\n"
        "keyboard_registered=%d\r\nrequested_usages=%u\r\nvendor_usages_registered=%u\r\n"
        "enumeration_error=%lu\r\nregistration_error=%lu\r\n"
        "keyboard_reports=%llu\r\nasus_reports=%llu\r\n"
        "token_observations_down=%llu\r\ntoken_observations_up=%llu\r\n"
        "hold_steps_down=%llu\r\nhold_steps_up=%llu\r\n"
        "scan_bound_down=%d\r\nscan_bound_up=%d\r\n"
        "hid_bound_down=%d\r\nhid_bound_up=%d\r\n"
        "release_source_down=%d\r\nrelease_source_up=%d\r\n"
        "hold_active_down=%d\r\nhold_active_up=%d\r\n",
        (unsigned long)GetCurrentProcessId(), (unsigned long long)now,
        g_holdInputDiag.keyboardReady != 0, g_holdInputDiag.requestedUsages,
        g_holdInputDiag.vendorUsages, (unsigned long)g_holdInputDiag.enumerateError,
        (unsigned long)g_holdInputDiag.registrationError,
        (unsigned long long)g_holdInputDiag.keyboardReports,
        (unsigned long long)g_holdInputDiag.asusReports,
        (unsigned long long)g_holdInputDiag.tokenObservations[0],
        (unsigned long long)g_holdInputDiag.tokenObservations[1],
        (unsigned long long)g_holdInputDiag.holdSteps[0],
        (unsigned long long)g_holdInputDiag.holdSteps[1],
        g_scanBound[0] != 0, g_scanBound[1] != 0,
        g_hidCode[0] != 0, g_hidCode[1] != 0,
        HasReleaseSrc(0) != 0, HasReleaseSrc(1) != 0,
        g_holdOn[0] != 0, g_holdOn[1] != 0);
    if (length < 0 || (size_t)length >= sizeof(text)) return;
    text[length] = '\0';
    WriteAtomicUtf8(path, text);
}
