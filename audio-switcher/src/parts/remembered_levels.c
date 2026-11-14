/* ============================================================================
 *  Remembered per-app levels
 *
 *  Stored as UTF-8 in %LOCALAPPDATA%\AudioSwitcher\levels.txt, matching the
 *  suite convention (ProjectHub uses %LOCALAPPDATA%\ProjectHub). One line per
 *  app, keyed by lowercase exe basename so the level survives restarts and PID
 *  changes:
 *
 *      spotify.exe=40,0        volume 40%, not muted
 *
 *  A line-based format is deliberate: it is human-readable, hand-editable, and
 *  needs no JSON parser or escaping rules in C.
 * ============================================================================ */

#define MAX_REMEMBERED 256

typedef struct {
    wchar_t key[256];
    int     percent;
    BOOL    mute;
} RememberedLevel;

static RememberedLevel g_remembered[MAX_REMEMBERED];
static int             g_remembered_count = 0;
static wchar_t         g_levels_path[MAX_PATH] = L"";
static BOOL            g_levels_enabled = TRUE;

/* Lowercased exe basename: the identity an app keeps across launches. */
static void key_from_path(const wchar_t *full, wchar_t *key, size_t cap) {
    const wchar_t *base = wcsrchr(full, L'\\');
    base = base ? base + 1 : full;
    if (cap == 0) return;
    wcsncpy(key, base, cap - 1);
    key[cap - 1] = L'\0';
    for (wchar_t *p = key; *p; p++)
        if (*p >= L'A' && *p <= L'Z') *p = (wchar_t)(*p - L'A' + L'a');
}

static void levels_path_init(void) {
    wchar_t local[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local)) || !local[0])
        GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (!local[0]) {
        g_levels_enabled = FALSE;   /* nowhere to write; run without persistence */
        return;
    }
    wchar_t dir[MAX_PATH];
    _snwprintf(dir, MAX_PATH, L"%s\\AudioSwitcher", local);
    CreateDirectoryW(dir, NULL);    /* fails harmlessly if it already exists */
    _snwprintf(g_levels_path, MAX_PATH, L"%s\\levels.txt", dir);
}

static RememberedLevel *levels_find(const wchar_t *key) {
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < g_remembered_count; i++)
        if (wcscmp(g_remembered[i].key, key) == 0) return &g_remembered[i];
    return NULL;
}

static void levels_load(void) {
    g_remembered_count = 0;
    if (!g_levels_enabled || !g_levels_path[0]) return;

    FILE *f = _wfopen(g_levels_path, L"rb");
    if (!f) return;                 /* first run: no file yet is normal */

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Strip a UTF-8 BOM if a text editor added one. */
    char *p = buf;
    if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB
               && (unsigned char)p[2] == 0xBF) p += 3;

    wchar_t wide[65536];
    int wn = MultiByteToWideChar(CP_UTF8, 0, p, -1, wide, 65536);
    if (wn <= 0) return;

    wchar_t *line = wide;
    while (line && *line && g_remembered_count < MAX_REMEMBERED) {
        wchar_t *nl = wcschr(line, L'\n');
        if (nl) *nl = L'\0';
        wchar_t *cr = wcschr(line, L'\r');
        if (cr) *cr = L'\0';

        if (line[0] && line[0] != L'#') {
            wchar_t *eq = wcschr(line, L'=');
            if (eq) {
                *eq = L'\0';
                int percent = 0, mute = 0;
                if (swscanf(eq + 1, L"%d,%d", &percent, &mute) >= 1) {
                    if (percent < 0) percent = 0;
                    if (percent > 100) percent = 100;
                    wcsncpy(g_remembered[g_remembered_count].key, line, 255);
                    g_remembered[g_remembered_count].key[255] = L'\0';
                    g_remembered[g_remembered_count].percent = percent;
                    g_remembered[g_remembered_count].mute = mute ? TRUE : FALSE;
                    g_remembered_count++;
                }
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

static void levels_save(void) {
    if (!g_levels_enabled || !g_levels_path[0]) return;

    /* Build the whole file in memory, then write it in one go: a half-written
     * levels file would be worse than none, since it is read at every start. */
    static char out[MAX_REMEMBERED * 320];
    int off = 0;
    off += _snprintf(out + off, sizeof(out) - off,
                     "# Audio Switcher remembered per-app levels\n"
                     "# <exe-name>=<volume percent 0-100>,<muted 0|1>\n"
                     "# Hand-editable. Delete a line to forget that app.\n");
    for (int i = 0; i < g_remembered_count && off < (int)sizeof(out) - 320; i++) {
        char keyu8[512];
        int kn = WideCharToMultiByte(CP_UTF8, 0, g_remembered[i].key, -1,
                                     keyu8, sizeof(keyu8), NULL, NULL);
        if (kn <= 0) continue;
        off += _snprintf(out + off, sizeof(out) - off, "%s=%d,%d\n",
                         keyu8, g_remembered[i].percent,
                         g_remembered[i].mute ? 1 : 0);
    }

    FILE *f = _wfopen(g_levels_path, L"wb");
    if (!f) return;
    fwrite(out, 1, (size_t)off, f);
    fclose(f);
}

/* Record a level the user set, and persist it. */
static void levels_remember(const wchar_t *key, float volume, BOOL mute) {
    if (!g_levels_enabled || !key || !key[0]) return;
    int percent = (int)(volume * 100.0f + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    RememberedLevel *slot = levels_find(key);
    if (!slot) {
        if (g_remembered_count >= MAX_REMEMBERED) return;
        slot = &g_remembered[g_remembered_count++];
        wcsncpy(slot->key, key, 255);
        slot->key[255] = L'\0';
    }
    slot->percent = percent;
    slot->mute = mute;
    levels_save();
}

static BOOL pid_already_applied(DWORD pid) {
    for (int i = 0; i < g_applied_pid_count; i++)
        if (g_applied_pids[i] == pid) return TRUE;
    return FALSE;
}

static void mark_pid_applied(DWORD pid) {
    if (g_applied_pid_count < MAX_APPLIED_PIDS)
        g_applied_pids[g_applied_pid_count++] = pid;
    else
        g_applied_pids[0] = pid;    /* ring over the oldest; bounds, not correctness */
}

/* Collect the audio sessions of ONE render device into g_sessions.
 *
 * `note_device` appends the device name to the row label: it is only set for
 * the fallback sweep, where the app is playing somewhere the user cannot hear
 * it, and an unlabelled row would look like a bug.
 */
static int sessions_collect_from_device(IMMDevice *dev, BOOL note_device,
                                        const wchar_t *device_name) {
    if (!dev) return 0;

    IAudioSessionManager2 *mgr = NULL;
    HRESULT hr = CALL_VTBL4(dev, Activate, &IID_IAudioSessionManager2, CLSCTX_ALL,
                            NULL, (void**)&mgr);
    if (FAILED(hr) || !mgr) { g_sessions_hr = hr; return 0; }

    /* Keep the manager alive: the ISimpleAudioVolume interfaces handed out by
     * its sessions belong to it, and releasing it would leave the sliders
     * pointing at a dead object. */
    if (g_session_mgr_count < 32) g_session_mgrs[g_session_mgr_count++] = mgr;
    if (!g_session_mgr) g_session_mgr = mgr;

    IAudioSessionEnumerator *enum_sessions = NULL;
    hr = CALL_VTBL1(mgr, GetSessionEnumerator, (void**)&enum_sessions);
    if (FAILED(hr) || !enum_sessions) {
        g_sessions_hr = hr;
        return 0;
    }

    /* The enumerator vtable is declared correctly now, so call it by name
     * instead of re-declaring a duplicate struct at the call site. */
    int count = 0;
    CALL_VTBL1(enum_sessions, GetCount, &count);

    int added = 0;
    for (int i = 0; i < count && g_session_count < 128; i++) {
        void *session = NULL;
        CALL_VTBL2(enum_sessions, GetSession, i, &session);
        if (!session) continue;

        IAudioSessionControl2 *ctl2 = NULL;
        hr = CALL_VTBL2((IUnknown*)session, QueryInterface,
                        &IID_IAudioSessionControl2, (void**)&ctl2);
        if (FAILED(hr) || !ctl2) {
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }

        /* Skip System Sounds: not an app, and nothing useful to control.
         * IsSystemSoundsSession returns S_OK when it IS system sounds and
         * S_FALSE otherwise, so compare exactly -- SUCCEEDED() would match
         * both and drop every session. */
        if (CALL_VTBL0(ctl2, IsSystemSoundsSession) == S_OK) {
            CALL_VTBL0(ctl2, Release);
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }

        AudioSession *s = &g_sessions[g_session_count];
        memset(s, 0, sizeof(*s));

        /* GetProcessId is slot 14 of IAudioSessionControl2. It used to be
         * declared at slot 3, which is really GetState, so this "PID" was the
         * session state (0/1/2): OpenProcess then failed and every app row
         * came up blank, while `if (pid == 0)` silently skipped inactive
         * sessions for the wrong reason. */
        CALL_VTBL1(ctl2, GetProcessId, &s->process_id);

        if (s->process_id != 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                       FALSE, s->process_id);
            if (hProc) {
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, s->exe_path, &size)) {
                    const wchar_t *slash = wcsrchr(s->exe_path, L'\\');
                    wcsncpy(s->name, slash ? slash + 1 : s->exe_path, 255);
                    s->name[255] = L'\0';
                    key_from_path(s->exe_path, s->key, 256);
                }
                CloseHandle(hProc);
            }
        }

        /* Fall back to the session's own display name when the process could
         * not be opened (elevated app, or a session with no owning process). */
        if (!s->name[0]) {
            LPWSTR disp = NULL;
            if (SUCCEEDED(CALL_VTBL1(ctl2, GetDisplayName, &disp)) && disp && disp[0]) {
                wcsncpy(s->name, disp, 255);
                s->name[255] = L'\0';
                s->named_from_session = TRUE;
            }
            if (disp) CoTaskMemFree(disp);
        }
        if (!s->name[0])
            _snwprintf(s->name, 256, L"Unknown app (PID %lu)",
                       (unsigned long)s->process_id);

        /* A session on a device that is not the current output is inaudible.
         * Say so on the row instead of letting the user chase a silent slider. */
        if (note_device && device_name && device_name[0]) {
            wchar_t with_dev[256];
            _snwprintf(with_dev, 256, L"%s  \u2014 on %s", s->name, device_name);
            with_dev[255] = L'\0';
            wcsncpy(s->name, with_dev, 255);
            s->name[255] = L'\0';
            s->on_other_device = TRUE;
        }

        ISimpleAudioVolume *svol = NULL;
        hr = CALL_VTBL2(ctl2, QueryInterface, &IID_ISimpleAudioVolume, (void**)&svol);
        if (FAILED(hr) || !svol) {
            /* No volume interface means the row would have a dead slider;
             * better to not show it at all. */
            CALL_VTBL0(ctl2, Release);
            CALL_VTBL0((IUnknown*)session, Release);
            continue;
        }
        s->svol = svol;

        CALL_VTBL1(svol, GetMasterVolume, &s->volume);
        CALL_VTBL1(svol, GetMute, &s->mute);

        /* Restore a remembered level the first time we see this process, and
         * only then: re-applying on every 2-second poll would snap the slider
         * back out from under the user while they dragged it. */
        if (s->key[0] && s->process_id != 0 && !pid_already_applied(s->process_id)) {
            RememberedLevel *mem = levels_find(s->key);
            if (mem) {
                float want = mem->percent / 100.0f;
                CALL_VTBL2(svol, SetMasterVolume, want, NULL);
                CALL_VTBL2(svol, SetMute, mem->mute, NULL);
                s->volume = want;
                s->mute = mem->mute;
            }
            mark_pid_applied(s->process_id);
        }

        CALL_VTBL0(ctl2, Release);
        CALL_VTBL0((IUnknown*)session, Release);
        g_session_count++;
        added++;
    }

    CALL_VTBL0(enum_sessions, Release);
    return added;
}

static HRESULT sessions_refresh(void) {
    /* Release the previous poll's ISimpleAudioVolume references and the session
     * managers that own them. This runs on a two-second timer, so the old code
     * leaked one COM reference per app per poll for as long as the window was
     * open. */
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].svol) { CALL_VTBL0(g_sessions[i].svol, Release); g_sessions[i].svol = NULL; }
    }
    for (int i = 0; i < g_session_mgr_count; i++) {
        CALL_VTBL0(g_session_mgrs[i], Release);
        g_session_mgrs[i] = NULL;
    }
    g_session_mgr_count = 0;
    g_session_mgr = NULL;

    g_session_count = 0;
    g_session_sweep_used = FALSE;

    /* Enumerate EVERY active render endpoint, not just the default one. An app
     * renders to whichever endpoint it opened its stream on, and Windows does
     * not move that stream when the default later changes - so a game or browser
     * that started on (or was pinned to) another output has its session there
     * and nowhere else. Listing only the default device is why the panel said
     * "no apps are playing audio" while a game was plainly making sound; the old
     * fallback only walked the other devices when the default was COMPLETELY
     * empty, so a single background app on the default hid every game on the
     * rest. This is how the Windows mixer and EarTrumpet do it: one session
     * collection per endpoint, aggregated. Each session lives on exactly one
     * endpoint, so walking the endpoint list once counts every session once. */
    for (int i = 0; i < g_endpoint_count && g_session_count < 128; i++) {
        if (!g_endpoints[i].device) continue;
        BOOL is_def = (i == g_default_endpoint_idx) || g_endpoints[i].is_default;
        /* Sessions not on the current output are inaudible until the user
           switches; label them so a silent slider is not mistaken for a bug. */
        int added = sessions_collect_from_device(g_endpoints[i].device, !is_def,
                                                 g_endpoints[i].name);
        if (added > 0 && !is_def) g_session_sweep_used = TRUE;
    }

    /* endpoints_refresh came back with no usable endpoint list, but a default
       may still answer - fall back to it so we are never silently empty when
       there is a working default device. */
    if (g_endpoint_count == 0 && g_enum) {
        IMMDevice *dev = NULL;
        HRESULT dhr = CALL_VTBL3(g_enum, GetDefaultAudioEndpoint, eRender, eConsole,
                                 (void**)&dev);
        if (SUCCEEDED(dhr) && dev) {
            sessions_collect_from_device(dev, FALSE, NULL);
            CALL_VTBL0(dev, Release);
        } else {
            g_sessions_hr = dhr;
        }
    }

    /* A device that refused to enumerate only matters if it left us with
       nothing to show: the empty panel quotes this code, so it must not quote
       a failure that the other endpoints recovered from. */
    if (g_session_count > 0) g_sessions_hr = S_OK;
    return g_sessions_hr;
}

static void sessions_cleanup(void) {
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].svol) { CALL_VTBL0(g_sessions[i].svol, Release); g_sessions[i].svol = NULL; }
    }
    g_session_count = 0;
    for (int i = 0; i < g_session_mgr_count; i++) {
        CALL_VTBL0(g_session_mgrs[i], Release);
        g_session_mgrs[i] = NULL;
    }
    g_session_mgr_count = 0;
    g_session_mgr = NULL;
}

static BOOL endpoint_set_volume(int idx, float level) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_endpoints[idx].vol, SetMasterVolumeLevelScalar, level, NULL);
    return SUCCEEDED(hr);
}

static float endpoint_get_volume(int idx) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return 0.0f;
    float v = 0.0f;
    CALL_VTBL1(g_endpoints[idx].vol, GetMasterVolumeLevelScalar, &v);
    return v;
}

static BOOL endpoint_get_mute(int idx) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    BOOL m = FALSE;
    CALL_VTBL1(g_endpoints[idx].vol, GetMute, &m);
    return m;
}

static BOOL endpoint_set_mute(int idx, BOOL mute) {
    if (idx < 0 || idx >= g_endpoint_count || !g_endpoints[idx].vol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_endpoints[idx].vol, SetMute, mute, NULL);
    return SUCCEEDED(hr);
}

static BOOL session_set_volume(int idx, float level) {
    if (idx < 0 || idx >= g_session_count || !g_sessions[idx].svol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_sessions[idx].svol, SetMasterVolume, level, NULL);
    if (SUCCEEDED(hr)) g_sessions[idx].volume = level;
    return SUCCEEDED(hr);
}

static BOOL session_set_mute(int idx, BOOL mute) {
    if (idx < 0 || idx >= g_session_count || !g_sessions[idx].svol) return FALSE;
    HRESULT hr = CALL_VTBL2(g_sessions[idx].svol, SetMute, mute, NULL);
    if (SUCCEEDED(hr)) g_sessions[idx].mute = mute;
    return SUCCEEDED(hr);
}

