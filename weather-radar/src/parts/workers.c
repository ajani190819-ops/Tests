/* ---------------- workers: the one background thread ----------------

   Every byte this app reads arrives on this thread. The UI thread sees staged
   results only after publication through WM_APP_DATA. Place-dependent replies
   carry a generation so slow, obsolete work cannot replace the selected place;
   radar paint holds the published-view lock while GDI reads its reusable buffer. */

static HANDLE g_workerThread = NULL;
static HANDLE g_jobEvent = NULL;
static int    g_jobs[8];
static int    g_jobHead = 0, g_jobTail = 0;
static Place  g_reqPlace;
static int    g_reqHas = 0;
static wchar_t g_reqQuery[96];
static DWORD   g_reqSearchTicket = 0;

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
            case JOB_GEOCODE: {
                wchar_t query[96];
                DWORD ticket;
                EnterCriticalSection(&g_lock);
                wcsncpy(query, g_reqQuery, 95);
                query[95] = L'\0';
                ticket = g_reqSearchTicket;
                LeaveCriticalSection(&g_lock);
                GeocodeSearch(query, ticket);
                break;
            }
            case JOB_FORECAST: {
                Place p; int have; DWORD ticket;
                EnterCriticalSection(&g_lock);
                p = g_reqPlace; have = g_reqHas; ticket = g_placeTicket;
                LeaveCriticalSection(&g_lock);
                if (have) FetchForecast(&p, ticket);
                break;
            }
            case JOB_ENSEMBLE: {
                Place p; int have; DWORD ticket;
                EnterCriticalSection(&g_lock);
                p = g_reqPlace; have = g_reqHas; ticket = g_placeTicket;
                LeaveCriticalSection(&g_lock);
                if (have) FetchEnsemble(&p, ticket);
                break;
            }
            case JOB_RADAR_CATALOG: RadarCatalogFetch(); break;
            case JOB_TILES: {
                Place p; int have, fi; DWORD ticket;
                EnterCriticalSection(&g_lock);
                p = g_reqPlace; have = g_reqHas; fi = g_frameSel;
                ticket = g_placeTicket;
                LeaveCriticalSection(&g_lock);
                if (have) RadarViewBuild(&p, fi, ticket);
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
    g_placeTicket++;
    if (g_placeTicket == 0) g_placeTicket = 1;
    g_viewReady = 0;
    LeaveCriticalSection(&g_lock);
    g_fc.have = 0;
    g_ens.have = 0;
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
    g_searchTicket++;
    if (g_searchTicket == 0) g_searchTicket = 1;
    g_reqSearchTicket = g_searchTicket;
    LeaveCriticalSection(&g_lock);
    StartJob(JOB_GEOCODE);
}
