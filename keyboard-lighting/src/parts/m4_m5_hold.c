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
        Ally) or press|0x80. A press+release pair gives TRUE hold with
        no repeats needed at all.
   All three feed one state machine: the first token takes an immediate 5%
   step, then the engine glides down 1% or up 1.5% every 30 ms while fresh
   tokens confirm the hold; release stops it.
   Learned bindings persist in presets.ini [Keys] M4Scan=/M5Scan=/M4Vk=/
   M5Vk=/M4Hid=/M5Hid=/M4HidRelease=/M5HidRelease=. Without any hold signal,
   behavior stays a clean tap per press. */
typedef struct KeyRec { ULONGLONG tick; DWORD scan; WORD vk; BYTE down; BYTE injected; } KeyRec;
static KeyRec    g_keyRing[256];
static int       g_keyRingHead = 0;
typedef struct HidRec { ULONGLONG tick; DWORD code; BYTE rel; } HidRec;
static HidRec    g_hidRing[256];
static int       g_hidRingHead = 0;
static int       g_scanBound[2] = { 0, 0 };
static DWORD     g_scanVk[2] = { 0, 0 };
static DWORD     g_hidCode[2] = { 0, 0 };
static DWORD     g_hidLastCode = 0;
static BYTE      g_scanArmed[2] = { 1, 1 };
static BYTE      g_hidDown[2] = { 0, 0 };
static BYTE      g_holdOn[2] = { 0, 0 };
static BYTE      g_hidRelSeen[2] = { 0, 0 };
static ULONGLONG g_sigAt[2] = { 0, 0 };
static ULONGLONG g_scanNext[2] = { 0, 0 };
static int       g_learnCand[2] = { 0, 0 };
static int       g_learnN[2] = { 0, 0 };
static DWORD     g_learnHid[2] = { 0, 0 };
static int       g_learnHidN[2] = { 0, 0 };
static char      g_seenTok[2][32];
static ULONGLONG g_pendingLearnAt[2] = { 0, 0 };
static ULONGLONG g_pendingLearnUntil[2] = { 0, 0 };

static void ReleaseSignal(int dir, ULONGLONG now);
static void PressSignal(int dir, int repeat, ULONGLONG now);
static void PersistKeyValue(const wchar_t *key, DWORD value);
static void MarkHidReleaseSeen(int dir);

static int HasReleaseSrc(int dir) {
    /* One observed HID press/release pair is a real release source. Requiring
       two pairs left a new M4 binding in single-step mode for two extra presses. */
    return g_scanBound[dir] != 0 ||
           (g_hidCode[dir] != 0 && g_hidRelSeen[dir] >= 1);
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

static int HidCurrentlyDown(DWORD code) {
    for (int i = 0; i < 256; i++) {
        int idx = (g_hidRingHead + 255 - i) & 255;
        HidRec *r = &g_hidRing[idx];
        if (r->tick == 0) break;
        if (r->code == code) return !r->rel;
    }
    return 0;
}

static int HidReleaseSeen(DWORD code) {
    for (int i = 0; i < 256; i++) {
        int idx = (g_hidRingHead + 255 - i) & 255;
        HidRec *r = &g_hidRing[idx];
        if (r->tick == 0) break;
        if (r->code == code && r->rel) return 1;
    }
    return 0;
}

static void HoldTokenWrite(int dir) {
    char tok[32];
    _snprintf(tok, 32, "%llu", (unsigned long long)GetTickCount64());
    tok[31] = '\0';
    WriteAtomicUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, tok);
}

/* One paced token per interval while the gesture is live. */
static void HoldPump(void) {
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < 2; i++) {
        if (!g_holdOn[i]) continue;
        /* Raw key-up can be lost while the UI thread is busy. For ordinary
           virtual keys, poll the physical state too; vendor VKey 255 stays on
           the raw-input/HID release path. */
        if (g_scanBound[i] && g_scanVk[i] > 0 && g_scanVk[i] < 0xFF &&
            (GetAsyncKeyState((int)g_scanVk[i]) & 0x8000) == 0) {
            ReleaseSignal(i, now);
            continue;
        }
        if (!HasReleaseSrc(i) && !g_hidDown[i] && now - g_sigAt[i] > 300ULL) {
            g_holdOn[i] = 0;
            continue;
        }
        /* Ceiling on the whole stream, and the only one that still applies
           once a release source is known. HasReleaseSrc is monotonic - nothing
           ever unsets g_scanBound or lowers g_hidRelSeen - so the silence
           timeout above is switched off for good the moment learning succeeds.
           A release that then fails to arrive (key let go while this thread
           was starved, or a device that only reports release sometimes) would
           keep the token stream running forever, with the brightness pinned at
           the rail and nothing anywhere to explain it.
           Ten seconds is deliberately far longer than either fine sweep: M4
           takes about 3s and M5 about 2s from 0 to 100%. A real hold reaches
           the rail first, so this only stops a stream that has nothing left to do. */
        if (now - g_sigAt[i] > 10000ULL) {
            g_holdOn[i] = 0;
            g_scanArmed[i] = 1;
            g_hidDown[i] = 0;
            continue;
        }
        if (now >= g_scanNext[i]) {
            /* The dedicated WM_TIMER runs every 15 ms. Pump once per tick
               so the 30 ms repeat target is reached without being quantized
               by the slower 45 ms preview timer. This keeps fresh tokens well
               inside the engine's 120 ms hold window if one UI tick is late. */
            g_scanNext[i] = now + 30;
            HoldTokenWrite(i);
        }
    }
}

static void PressSignal(int dir, int repeat, ULONGLONG now) {
    g_sigAt[dir] = now;
    if (repeat) {
        g_holdOn[dir] = 1;
    } else if (HasReleaseSrc(dir)) {
        /* Release is observable: stream for the whole hold. The press
           itself already stepped once via G-Helper's Custom token. */
        g_holdOn[dir] = 1;
        g_scanNext[dir] = now + 30;
    }
}

static void ReleaseSignal(int dir, ULONGLONG now) {
    (void)now;
    g_holdOn[dir] = 0;
    g_scanArmed[dir] = 1;
    if (g_hidDown[dir]) {
        g_hidDown[dir] = 0;
        MarkHidReleaseSeen(dir);
    }
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
            PressSignal(i, 0, now);
        } else {
            PressSignal(i, 1, now);
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
    if (g_hidRelSeen[dir] == 0) {
        g_hidRelSeen[dir] = 1;
        PersistKeyValue(dir == 0 ? L"M4HidRelease" : L"M5HidRelease", 1);
    }
}

static void BindScan(int dir, DWORD scan, DWORD vk) {
    if (!scan) return;
    g_scanBound[dir] = (int)scan;
    g_scanVk[dir] = vk;
    g_scanArmed[dir] = 1;
    PersistKeyValue(dir == 0 ? L"M4Scan" : L"M5Scan", scan);
    PersistKeyValue(dir == 0 ? L"M4Vk" : L"M5Vk", vk);
}

static void BindHid(int dir, DWORD code) {
    if (!code) return;
    g_hidCode[dir] = code;
    PersistKeyValue(dir == 0 ? L"M4Hid" : L"M5Hid", code);
}

static void OnRawHid(const BYTE *b, int len, ULONGLONG now) {
    if (len < 2 || b[0] != 0x5A) return;
    DWORD rawCode = b[1];
    DWORD code = rawCode;
    int rel = 0, mapped = -1;
    if (rawCode == 0) {
        rel = 1;
        for (int i = 0; i < 2; i++) {
            if (g_hidDown[i]) {
                mapped = i;
                code = g_hidCode[i];
            }
        }
        if (mapped < 0) code = g_hidLastCode;
    } else if ((rawCode & 0x80) != 0) {
        /* Some ASUS reports encode release as press|0x80. Until a learned
           binding identifies it, retain the base code in the learning ring. */
        rel = 1;
        code = rawCode & 0x7F;
    }
    for (int i = 0; i < 2; i++) {
        if (g_hidCode[i] != 0 && rawCode == g_hidCode[i]) {
            mapped = i;
            code = g_hidCode[i];
            rel = 0;
        }
        if (g_hidCode[i] != 0 && rawCode == (g_hidCode[i] | 0x80)) {
            mapped = i;
            code = g_hidCode[i];
            rel = 1;
        }
    }
    if (!rel && rawCode != 0) g_hidLastCode = code;
    HidRec *r = &g_hidRing[g_hidRingHead];
    r->tick = now; r->code = code; r->rel = (BYTE)rel;
    g_hidRingHead = (g_hidRingHead + 1) & 255;
    if (rel && code != 0 && code == g_hidLastCode) g_hidLastCode = 0;
    if (mapped >= 0) {
        if (rel) {
            ReleaseSignal(mapped, now);
        } else {
            int repeat = g_hidDown[mapped];
            g_hidDown[mapped] = 1;
            PressSignal(mapped, repeat, now);
        }
    }
}

static int ParseBrightTokenTick(const char *token, ULONGLONG now, ULONGLONG *tick) {
    char *end = NULL;
    unsigned long long value;
    if (!token || !token[0] || !tick) return 0;
    value = strtoull(token, &end, 10);
    if (end == token || *end != '\0' || (ULONGLONG)value > now) return 0;
    *tick = (ULONGLONG)value;
    return 1;
}

static void LearnFromRing(int dir, ULONGLONG tokenAt, int newToken) {
    ULONGLONG now = GetTickCount64();
    if (!tokenAt) tokenAt = now;

    /* Learn each input family only until it has a binding. The token file also
       receives our own repeat tokens; those must not rebind M4 to a nearby key
       pressed while the brightness key is held. */
    if (g_scanBound[dir] == 0) {
        /* G-Helper writes its command token just after the physical key event.
           Correlate timestamps so the first held gesture can bind and repeat. */
        KeyRec *bestKey = NULL;
        ULONGLONG bestKeyDelta = (ULONGLONG)-1;
        for (int i = 0; i < 256; i++) {
            int idx = (g_keyRingHead + 255 - i) & 255;
            KeyRec *r = &g_keyRing[idx];
            if (r->tick == 0) break;
            if (!r->down || r->injected || r->scan == 0) continue;
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
            if (ScanCurrentlyDown(bestKey->scan, bestKey->vk)) {
                g_scanArmed[dir] = 0;
                PressSignal(dir, 0, now);
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
                if (!r->down || r->injected) continue;
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
                PressSignal(dir, 0, now);
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
            if (r->rel || r->code == 0) continue;
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
            if (HidReleaseSeen(hc)) MarkHidReleaseSeen(dir);
            if (HidCurrentlyDown(hc)) {
                g_hidDown[dir] = 1;
                if (!g_holdOn[dir]) {
                    /* Adopt the first press before its report code was known;
                       the matching release can now stop this same gesture. */
                    g_scanArmed[dir] = 0;
                    g_scanNext[dir] = now + 30;
                    PressSignal(dir, 1, now);
                }
            }
        } else if (newToken) {
            DWORD hc = 0;
            for (int i = 0; i < 256; i++) {
                int idx = (g_hidRingHead + 255 - i) & 255;
                HidRec *r = &g_hidRing[idx];
                if (r->tick == 0) break;
                if (now < r->tick || now - r->tick > 1500ULL) break;
                if (r->rel || r->code == 0) continue;
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

static void WatchBrightTokens(void) {
    ULONGLONG now = GetTickCount64();
    for (int dir = 0; dir < 2; dir++) {
        char cur[32] = { 0 };
        DWORD len = 0;
        char *txt = ReadFileUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, &len);
        if (txt) { strncpy(cur, txt, 31); free(txt); }
        cur[31] = '\0';
        cur[strcspn(cur, "\r\n")] = '\0';
        if (cur[0] && strcmp(cur, g_seenTok[dir]) != 0) {
            ULONGLONG tokenAt = now;
            ParseBrightTokenTick(cur, now, &tokenAt);
            strncpy(g_seenTok[dir], cur, 31);
            g_seenTok[dir][31] = '\0';
            if (!g_holdOn[dir]) {
                g_pendingLearnAt[dir] = tokenAt;
                g_pendingLearnUntil[dir] = now + 1500ULL;
                LearnFromRing(dir, tokenAt, 1);
            } else {
                g_pendingLearnAt[dir] = 0;
            }
        }
        if (!g_holdOn[dir] && g_pendingLearnAt[dir] &&
            now <= g_pendingLearnUntil[dir] &&
            (g_scanBound[dir] == 0 || g_hidCode[dir] == 0)) {
            LearnFromRing(dir, g_pendingLearnAt[dir], 0);
        } else if (now > g_pendingLearnUntil[dir]) {
            g_pendingLearnAt[dir] = 0;
        }
    }
}

static void InitRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid[16];
    UINT n = 0;
    rid[n].usUsagePage = 0x01; rid[n].usUsage = 0x06;
    rid[n].dwFlags = RIDEV_INPUTSINK; rid[n].hwndTarget = hwnd;
    n++;
    UINT num = 0;
    GetRawInputDeviceList(NULL, &num, sizeof(RAWINPUTDEVICELIST));
    if (num > 0 && num < 64) {
        RAWINPUTDEVICELIST list[64];
        num = GetRawInputDeviceList(list, &num, sizeof(RAWINPUTDEVICELIST));
        for (UINT i = 0; i < num && n < 16; i++) {
            RID_DEVICE_INFO info;
            UINT cb = sizeof(info);
            info.cbSize = sizeof(info);
            if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &cb) != (UINT)-1 &&
                info.dwType == RIM_TYPEHID && info.hid.dwVendorId == 0x0B05) {
                UINT j; int dup = 0;
                for (j = 1; j < n; j++)
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
    }
    RegisterRawInputDevices(rid, n, sizeof(RAWINPUTDEVICE));
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
        int down = (ri->data.keyboard.Flags & RI_KEY_BREAK) == 0;
        KeyRec *r = &g_keyRing[g_keyRingHead];
        r->tick = now; r->scan = ri->data.keyboard.MakeCode;
        r->vk = ri->data.keyboard.VKey;
        r->down = (BYTE)down; r->injected = 0;
        g_keyRingHead = (g_keyRingHead + 1) & 255;
        OnScanEvent(ri->data.keyboard.MakeCode, down, 0, now);
    } else if (ri->header.dwType == RIM_TYPEHID) {
        OnRawHid(ri->data.hid.bRawData, (int)ri->data.hid.dwSizeHid, now);
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

