/* One-shot location orchestration, UI-thread only. The Windows provider is
   asynchronous: timer ticks only inspect an already-requested operation.
   Startup, weather refresh and paint never initiate a location lookup.
   Staged completion carries its own generation, independently of weather
   refresh generations. A later manual selection/hide/quit cancels the fix. */

typedef struct { int have; Place place; wchar_t message[192]; } WindowsLocationResult;
static DWORD g_locationTicket, g_locationStageTicket;
static int g_locationPending, g_locationPhase; /* 0 idle, 1 consent, 2 fix, 3 staged */
static ULONGLONG g_locationDeadline;
static WindowsLocationResult g_locationStage;
static wchar_t g_locationNotice[192] = L"One-time Windows fix, never tracking. Search uses city/postcode coordinates.";
static int g_locationNoticeKind;

static void LocationNotice(const wchar_t *message, int kind) {
    wcsncpy(g_locationNotice, message, 191); g_locationNotice[191] = L'\0';
    g_locationNoticeKind = kind;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static int WindowsLocationCurrent(DWORD ticket) {
    return LocationTicketCurrent(ticket, g_locationTicket, g_locationPending);
}

static const wchar_t *LocationFailureText(int state) {
    if (state == LOCATION_PROVIDER_DENIED)
        return L"Location denied. Previous place kept. Check Windows Location settings.";
    if (state == LOCATION_PROVIDER_CANCELED)
        return L"Location request canceled. Previous place kept.";
    return L"Windows location unavailable. Previous place kept. Check Windows Location settings.";
}

static const wchar_t *LocationProblemText(int problem) {
    if (problem == LOCATION_FIX_COORDS)
        return L"Windows returned invalid or unsupported map coordinates. Previous place kept.";
    if (problem == LOCATION_FIX_ACCURACY)
        return L"Windows did not report usable accuracy. Previous place kept; try again.";
    return L"Windows returned an old or invalid fix time. Previous place kept; try again.";
}

static int CancelWindowsLocation(void) {
    int canceled = g_locationPending;
    g_locationPending = 0; g_locationPhase = 0;
    if (++g_locationTicket == 0) g_locationTicket = 1;
    g_locationStageTicket = 0;
    KillTimer(g_hwnd, TIMER_LOCATION);
    LocationProviderClose(1);
    if (canceled) LocationNotice(LocationFailureText(LOCATION_PROVIDER_CANCELED), 0);
    return canceled;
}

static void PublishWindowsLocation(DWORD ticket, const WindowsLocationResult *result) {
    if (!WindowsLocationCurrent(ticket)) return;
    KillTimer(g_hwnd, TIMER_LOCATION);
    LocationProviderClose(1);
    g_locationPhase = 3;
    g_locationStage = *result; g_locationStageTicket = ticket;
    if (!PostMessageW(g_hwnd, WM_APP_DATA, JOB_LOCATION, (LPARAM)(uintptr_t)ticket)) {
        g_locationPending = 0; g_locationPhase = 0;
        LocationNotice(L"Location result could not be delivered. Previous place kept; try again.", 2);
    }
}

/* Completion is taken only by WM_APP_DATA. Re-check time after message queue
   delays as well as on acquisition; a stale result must not become a new fix. */
static int TakeWindowsLocation(DWORD ticket, WindowsLocationResult *result) {
    if (!WindowsLocationCurrent(ticket) || g_locationStageTicket != ticket || g_locationPhase != 3) return 0;
    *result = g_locationStage;
    g_locationPending = 0; g_locationPhase = 0;
    if (result->have) {
        int problem = LocationFixProblem(result->place.lat, result->place.lon,
            result->place.accuracyM, result->place.fixEpoch, UnixNowSeconds());
        if (problem != LOCATION_FIX_OK) {
            result->have = 0;
            wcsncpy(result->message, LocationProblemText(problem), 191);
            result->message[191] = L'\0';
        }
    }
    LocationNotice(result->message, result->have ? (result->place.accuracyM >= 1000.0 ? 2 : 1) : 2);
    return 1;
}

static void FinishWindowsLocation(DWORD ticket, const LocationSample *sample, const wchar_t *error) {
    WindowsLocationResult result;
    ZeroMemory(&result, sizeof(result));
    int problem = sample ? LocationFixProblem(sample->lat, sample->lon, sample->accuracyM,
                        sample->fixEpoch, UnixNowSeconds()) : LOCATION_FIX_OK;
    if (sample && problem == LOCATION_FIX_OK) {
        result.have = 1;
        wcscpy(result.place.name, L"My location"); wcscpy(result.place.region, L"Windows location");
        result.place.lat = sample->lat; result.place.lon = sample->lon;
        result.place.fromWindows = 1; result.place.accuracyM = sample->accuracyM;
        result.place.fixEpoch = sample->fixEpoch;
        wcscpy(result.message, sample->accuracyM >= 1000.0 ?
            L"Approximate Windows fix; see accuracy above. Saved point, not live tracking." :
            L"Windows fix saved, not live tracking. Use my location again after moving.");
    } else {
        wcsncpy(result.message, sample ? LocationProblemText(problem) : error, 191);
        result.message[191] = L'\0';
    }
    PublishWindowsLocation(ticket, &result);
}

static void RequestWindowsLocation(void) {
    if (g_locationPending) { CancelWindowsLocation(); return; } /* second click cancels, not another prompt */
    if (GetForegroundWindow() != g_hwnd) {
        LocationNotice(L"Bring Weather Radar to the front, then use My location.", 2);
        return;
    }
    if (++g_locationTicket == 0) g_locationTicket = 1;
    g_locationPending = 1; g_locationPhase = 1;
    g_locationDeadline = GetTickCount64() + 120000ULL;
    LocationNotice(L"Waiting for Windows location permission. Click Cancel location to stop.", 0);
    if (!SetTimer(g_hwnd, TIMER_LOCATION, 100, NULL)) {
        FinishWindowsLocation(g_locationTicket, NULL, LocationFailureText(LOCATION_PROVIDER_UNAVAILABLE));
        return;
    }
    int state = LocationProviderBeginAccess(); /* only here: foreground, UI-thread, owner click */
    if (state != LOCATION_PROVIDER_PENDING)
        FinishWindowsLocation(g_locationTicket, NULL, LocationFailureText(state));
}

static void PollWindowsLocation(void) {
    if (!g_locationPending || (g_locationPhase != 1 && g_locationPhase != 2)) return;
    DWORD ticket = g_locationTicket;
    if (GetTickCount64() >= g_locationDeadline) {
        FinishWindowsLocation(ticket, NULL, g_locationPhase == 1 ?
            L"Windows permission request timed out. Previous place kept; try again." :
            L"Windows location timed out. Previous place kept; try again when a provider is available.");
        return;
    }
    LocationSample sample;
    ZeroMemory(&sample, sizeof(sample));
    int state = LocationProviderPoll(g_locationPhase == 1, &sample);
    if (!WindowsLocationCurrent(ticket) || state == LOCATION_PROVIDER_PENDING) return;
    if (state != LOCATION_PROVIDER_READY) {
        FinishWindowsLocation(ticket, NULL, LocationFailureText(state));
    } else if (g_locationPhase == 1) {
        LocationProviderClose(0);
        g_locationPhase = 2;
        g_locationDeadline = GetTickCount64() + 35000ULL; /* OS fix timeout 30s plus dispatch margin */
        LocationNotice(L"Finding a one-time Windows position (up to 30 seconds). Click Cancel location to stop.", 0);
        state = LocationProviderBeginFix(); /* only after explicit permission was Allowed */
        if (state != LOCATION_PROVIDER_PENDING)
            FinishWindowsLocation(ticket, NULL, LocationFailureText(state));
    } else FinishWindowsLocation(ticket, &sample, NULL);
}
