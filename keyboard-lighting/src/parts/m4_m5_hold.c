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
   All three feed one state machine: press starts a paced token stream
   (the engine turns each token into one 5% step), release stops it.
   Learned bindings persist in presets.ini [Keys] M4Scan=/M5Scan=/M4Hid=/
   M5Hid=. Without any hold signal, behavior stays a clean tap per press. */
typedef struct KeyRec { ULONGLONG tick; DWORD scan; BYTE down; BYTE injected; } KeyRec;
static KeyRec    g_keyRing[256];
static int       g_keyRingHead = 0;
typedef struct HidRec { ULONGLONG tick; DWORD code; BYTE rel; } HidRec;
static HidRec    g_hidRing[256];
static int       g_hidRingHead = 0;
static int       g_scanBound[2] = { 0, 0 };
static DWORD     g_hidCode[2] = { 0, 0 };
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

static int HasReleaseSrc(int dir) {
    return g_scanBound[dir] != 0 || g_hidRelSeen[dir] >= 2;
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
        if (!HasReleaseSrc(i) && now - g_sigAt[i] > 300ULL) {
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
           Ten seconds is deliberately far longer than the sweep it guards: the
           engine steps 5% every 150ms, so 0 to 100% takes about 3s. A real
           hold always reaches the rail first, so this can only ever stop a
           stream that has nothing left to do. */
        if (now - g_sigAt[i] > 10000ULL) {
            g_holdOn[i] = 0;
            g_scanArmed[i] = 1;
            continue;
        }
        if (now >= g_scanNext[i]) {
            g_scanNext[i] = now + 150;
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
        g_scanNext[dir] = now + 150;
    }
}

static void ReleaseSignal(int dir, ULONGLONG now) {
    (void)now;
    g_holdOn[dir] = 0;
    g_scanArmed[dir] = 1;
    if (g_hidDown[dir]) {
        g_hidDown[dir] = 0;
        if (g_hidRelSeen[dir] < 255) g_hidRelSeen[dir]++;
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
            g_scanNext[i] = now + 120;
            PressSignal(i, 0, now);
        } else {
            PressSignal(i, 1, now);
        }
    }
}

static void OnRawHid(const BYTE *b, int len, ULONGLONG now) {
    if (len < 2 || b[0] != 0x5A) return;
    DWORD code = b[1];
    int rel = 0, mapped = -1;
    if (code == 0) {
        rel = 1;
        for (int i = 0; i < 2; i++) if (g_hidDown[i]) mapped = i;
    }
    for (int i = 0; i < 2; i++) {
        if (g_hidCode[i] != 0 && code == g_hidCode[i]) mapped = i;
        if (g_hidCode[i] != 0 && code == (g_hidCode[i] | 0x80)) { mapped = i; rel = 1; }
    }
    HidRec *r = &g_hidRing[g_hidRingHead];
    r->tick = now; r->code = code; r->rel = (BYTE)rel;
    g_hidRingHead = (g_hidRingHead + 1) & 255;
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

static void LearnFromRing(int dir) {
    ULONGLONG now = GetTickCount64();
    int cand = 0;
    for (int i = 0; i < 256; i++) {
        int idx = (g_keyRingHead + 255 - i) & 255;
        KeyRec *r = &g_keyRing[idx];
        if (r->tick == 0) break;
        if (now - r->tick > 1500ULL) break;
        if (!r->down || r->injected) continue;
        if (r->scan == 0x1D || r->scan == 0x2A || r->scan == 0x36 || r->scan == 0x38) continue;
        cand = (int)r->scan;
        break;
    }
    if (cand) {
        if (g_learnCand[dir] == cand) {
            g_learnN[dir]++;
            if (g_learnN[dir] >= 2 && g_scanBound[dir] != cand) {
                wchar_t num[16], path[MAX_PATH];
                g_scanBound[dir] = cand;
                g_scanArmed[dir] = 1;
                _snwprintf(num, 15, L"%d", cand);
                num[15] = L'\0';
                _snwprintf(path, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
                WritePrivateProfileStringW(L"Keys", dir == 0 ? L"M4Scan" : L"M5Scan", num, path);
            }
        } else {
            g_learnCand[dir] = cand;
            g_learnN[dir] = 1;
        }
    }
    {
        DWORD hc = 0;
        for (int i = 0; i < 256; i++) {
            int idx = (g_hidRingHead + 255 - i) & 255;
            HidRec *r = &g_hidRing[idx];
            if (r->tick == 0) break;
            if (now - r->tick > 1500ULL) break;
            if (r->rel || r->code == 0) continue;
            hc = r->code;
            break;
        }
        if (hc) {
            if (g_learnHid[dir] == hc) {
                g_learnHidN[dir]++;
                if (g_learnHidN[dir] >= 2 && g_hidCode[dir] != hc) {
                    wchar_t num[16], path[MAX_PATH];
                    g_hidCode[dir] = hc;
                    _snwprintf(num, 15, L"%lu", (unsigned long)hc);
                    num[15] = L'\0';
                    _snwprintf(path, MAX_PATH, L"%s\\presets.ini", g_cfgDir);
                    WritePrivateProfileStringW(L"Keys", dir == 0 ? L"M4Hid" : L"M5Hid", num, path);
                }
            } else {
                g_learnHid[dir] = hc;
                g_learnHidN[dir] = 1;
            }
        }
    }
}

static void WatchBrightTokens(void) {
    for (int dir = 0; dir < 2; dir++) {
        char cur[32] = { 0 };
        DWORD len = 0;
        char *txt = ReadFileUtf8(dir == 0 ? g_brightDownPath : g_brightUpPath, &len);
        if (txt) { strncpy(cur, txt, 31); free(txt); }
        if (cur[0] && strcmp(cur, g_seenTok[dir]) != 0) {
            int fresh = g_seenTok[dir][0] != '\0';
            strncpy(g_seenTok[dir], cur, 31);
            if (fresh) LearnFromRing(dir);
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

