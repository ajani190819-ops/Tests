/* ---------------- workers: the one background thread ----------------

   Every byte this app reads arrives on this thread. The UI thread only ever
   sees results, handed over through WM_APP_DATA after the worker has
   finished writing them into staging under g_lock - so a slow tile host can
   never freeze the window, and a paint can never read a half-filled struct. */

static HANDLE g_workerThread = NULL;
static HANDLE g_jobEvent = NULL;
static int    g_jobs[8];
static int    g_jobHead = 0, g_jobTail = 0;
static Place  g_reqPlace;
static int    g_reqHas = 0;
static wchar_t g_reqQuery[96];

static void StartJob(int kind) {
    EnterCriticalSection(&g_lock);
    for (int i = g_jobHead; i != g_jobTail; i = (i + 1) & 7) {
        if (g_jobs[i] == kind) { LeaveCriticalSection(&g_lock); return; }
    }
    if (((g_jobTail + 1) & 7) != g_jobHead) {
        g_jobs[g_jobTail] = kind;
        g_jobTail = (g_jobTail + 1) & 7;
    }
    LeaveCriticalSection(&g_lock);
    SetEvent(g_jobEvent);
}

static int PopJob(void) {
    int j = JOB_NONE;
    EnterCriticalSection(&g_lock);
    if (g_jobHead != g_jobTail) {
        j = g_jobs[g_jobHead];
        g_jobHead = (g_jobHead + 1) & 7;
    }
    LeaveCriticalSection(&g_lock);
    return j;
}

static DWORD WINAPI WorkerMain(LPVOID arg) {
    (void)arg;
    for (;;) {
        WaitForSingleObject(g_jobEvent, INFINITE);
        int j;
        while ((j = PopJob()) != JOB_NONE) {
            switch (j) {
            case JOB_GEOCODE:       GeocodeSearch(g_reqQuery); break;
            case JOB_FORECAST:      if (g_reqHas) FetchForecast(&g_reqPlace); break;
            case JOB_ENSEMBLE:      if (g_reqHas) FetchEnsemble(&g_reqPlace); break;
            case JOB_RADAR_CATALOG: RadarCatalogFetch(); break;
            case JOB_TILES: {
                int fi;
                EnterCriticalSection(&g_lock);
                fi = g_frameSel;
                LeaveCriticalSection(&g_lock);
                if (g_reqHas) RadarViewBuild(&g_reqPlace, fi);
                break;
            }
            default: break;
            }
        }
    }
}

static void WorkerStart(void) {
    g_jobEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_workerThread = CreateThread(NULL, 0, WorkerMain, NULL, 0, NULL);
}

/* Remember which place the pending jobs refer to. Copied under the lock so
   a click on another place mid-fetch can never mix two places' data. */
static void RequestPlaceData(const Place *p) {
    EnterCriticalSection(&g_lock);
    g_reqPlace = *p;
    g_reqHas = 1;
    LeaveCriticalSection(&g_lock);
    g_fc.have = 0;
    g_ens.have = 0;
    g_viewReady = 0;
    StartJob(JOB_FORECAST);
    StartJob(JOB_ENSEMBLE);
    StartJob(JOB_RADAR_CATALOG);
}

static void RequestTiles(void) {
    StartJob(JOB_TILES);
}

static void RequestSearch(const wchar_t *q) {
    EnterCriticalSection(&g_lock);
    wcsncpy(g_reqQuery, q, 95);
    g_reqQuery[95] = L'\0';
    LeaveCriticalSection(&g_lock);
    StartJob(JOB_GEOCODE);
}
