/* ---------------- forecast: the honest probability math ----------------

   Two numbers, two meanings, both shown:

   * The HONEST one. The ensemble API serves every GFS member as its own
     hourly precipitation series (precipitation_member01..NN). For any hour,
     the share of members that put >= RAIN_MM at these exact coordinates is
     the probability that it rains here - no areal term, because every
     member is a complete forecast for this single point. The denominator is
     the members that REPORTED that hour: past a member's own horizon its
     value is null, and counting silence as "dry" would deflate every
     longer-range number toward zero.

   * The one everyone else shows. Open-Meteo's precipitation_probability is
     PoP = confidence x areal coverage, the "50% chance" that means "I am
     sure half the region gets wet". It is drawn as a hollow tick beside the
     honest bar so the gap is visible, with the wording that says what it is.
*/

static void BuildForecastUrl(const Place *p, wchar_t *url, int cap) {
    _snwprintf(url, cap - 1,
               L"https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
               L"&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
               L"weather_code,wind_speed_10m,precipitation"
               L"&hourly=temperature_2m,precipitation_probability,precipitation,weather_code"
               L"&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
               L"precipitation_sum,weather_code"
               L"&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch"
               L"&forecast_days=2&timezone=auto",
               p->lat, p->lon);
    url[cap - 1] = L'\0';
}

static void BuildEnsembleUrl(const Place *p, wchar_t *url, int cap) {
    _snwprintf(url, cap - 1,
               L"https://ensemble-api.open-meteo.com/v1/ensemble?latitude=%.5f&longitude=%.5f"
               L"&hourly=precipitation&models=gfs_seamless&forecast_days=2&timezone=auto",
               p->lat, p->lon);
    url[cap - 1] = L'\0';
}

/* Parse the hourly ISO timestamps down to a short local label ("14:00").
   The API already shifted times to the place's timezone (timezone=auto), so
   the string's hour IS local wall-clock there. */
static void HourLabelFromIso(const char *iso, wchar_t *out, int cap) {
    /* yyyy-MM-ddTHH:mm */
    if (strlen(iso) >= 16) {
        Utf8ToWide(iso + 11, out, cap);
        out[5] = L'\0';
        return;
    }
    wcscpy(out, L"--:--");
}

static void FetchForecast(const Place *p) {
    wchar_t url[1400];
    BuildForecastUrl(p, url, 1400);
    HttpBuf b;
    if (!HttpGetBuf(url, &b)) return;
    JNode *root = JsonParse(b.data);
    Forecast *f = &g_fcStage;
    ZeroMemory(f, sizeof(*f));
    if (!root) { HttpBufFree(&b); return; }

    JNode *cur = JGet(root, "current");
    if (cur) {
        f->tempF   = JNum(JGet(cur, "temperature_2m"));
        f->feelsF  = JNum(JGet(cur, "apparent_temperature"));
        f->rh      = JNum(JGet(cur, "relative_humidity_2m"));
        f->windMph = JNum(JGet(cur, "wind_speed_10m"));
        f->precipIn = JNum(JGet(cur, "precipitation"));
        f->wcode   = (int)JNum(JGet(cur, "weather_code"));
        f->have = 1;
    }
    JNode *tz = JGet(root, "timezone");
    if (tz && tz->str) Utf8ToWide(tz->str, f->tz, 48);

    JNode *daily = JGet(root, "daily");
    if (daily) {
        f->dMaxF = JNum(JAt(JGet(daily, "temperature_2m_max"), 0));
        f->dMinF = JNum(JAt(JGet(daily, "temperature_2m_min"), 0));
        f->dPoP  = JNum(JAt(JGet(daily, "precipitation_probability_max"), 0));
        f->dSumIn = JNum(JAt(JGet(daily, "precipitation_sum"), 0));
    }

    JNode *hourly = JGet(root, "hourly");
    if (hourly) {
        JNode *times = JGet(hourly, "time");
        JNode *t = JGet(hourly, "temperature_2m");
        JNode *pop = JGet(hourly, "precipitation_probability");
        JNode *pr = JGet(hourly, "precipitation");
        JNode *wc = JGet(hourly, "weather_code");
        int n = JLen(times);
        if (n > FC_HOURS) n = FC_HOURS;
        /* "now" is the first hourly row at or after the current hour, which
           the API's own `current` block agrees with; the strip starts there. */
        JNode *curTime = cur ? JGet(cur, "time") : NULL;
        int nowIdx = 0;
        if (curTime && curTime->str && times) {
            for (int i = 0; i < n; i++) {
                JNode *ti = JAt(times, i);
                if (ti && ti->str && strcmp(ti->str, curTime->str) >= 0) { nowIdx = i; break; }
            }
        }
        f->nowIdx = nowIdx;
        int k = 0;
        for (int i = nowIdx; i < n && k < FC_HOURS; i++, k++) {
            JNode *ti = JAt(times, i);
            if (ti && ti->str) HourLabelFromIso(ti->str, f->hLabel[k], 8);
            f->hT[k]   = JNum(JAt(t, i));
            f->hPoP[k] = JNum(JAt(pop, i));
            f->hP[k]   = JNum(JAt(pr, i));
            f->hCode[k] = (int)JNum(JAt(wc, i));
            f->hN = k + 1;
        }
        if (f->hN > 0) f->have = 2;
    }
    NowLocalStamp(f->fetched, 24);
    HttpBufFree(&b);
    PostMessageW(g_hwnd, WM_APP_DATA, JOB_FORECAST, 0);
}

/* Fill the member matrix and derive the per-hour honest probability.
   Members are discovered by key prefix rather than by a hard-coded count,
   because the GFS ensemble member count is a property of the model, not of
   this app: whatever the API serves gets counted. */
static void FetchEnsemble(const Place *p) {
    wchar_t url[900];
    BuildEnsembleUrl(p, url, 900);
    HttpBuf b;
    if (!HttpGetBuf(url, &b)) return;
    JNode *root = JsonParse(b.data);
    Ensemble *e = &g_ensStage;
    ZeroMemory(e, sizeof(*e));
    for (int m = 0; m < MAX_MEMBERS; m++)
        for (int h = 0; h < FC_HOURS; h++) e->mm[m][h] = NAN;
    if (!root) { HttpBufFree(&b); return; }

    JNode *hourly = JGet(root, "hourly");
    JNode *times = hourly ? JGet(hourly, "time") : NULL;
    if (!hourly || !times) { HttpBufFree(&b); return; }
    int n = JLen(times);
    if (n > FC_HOURS) n = FC_HOURS;
    e->hours = n;

    int member = 0;
    for (JNode *c = hourly->child; c; c = c->next) {
        if (!c->key || strncmp(c->key, "precipitation_member", 20) != 0) continue;
        if (member >= MAX_MEMBERS) break;
        int i = 0;
        for (JNode *v = c->child; v && i < n; v = v->next, i++)
            e->mm[member][i] = JNum(v);
        member++;
    }
    e->members = member;
    wcscpy(e->model, L"GFS ensemble");

    for (int h = 0; h < n; h++) {
        int wet = 0, reported = 0;
        for (int m = 0; m < member; m++) {
            double v = e->mm[m][h];
            if (isnan(v)) continue;      /* this member's horizon ended: absent, not dry */
            reported++;
            if (v >= RAIN_MM) wet++;
        }
        e->reported[h] = reported;
        e->prob[h] = reported > 0 ? (double)wet / (double)reported : NAN;
    }
    e->have = member > 0;
    HttpBufFree(&b);
    PostMessageW(g_hwnd, WM_APP_DATA, JOB_ENSEMBLE, 0);
}

/* Honest probability that it rains HERE at least once inside
   [startHour, startHour+hours): a member counts as "wet" if any of its
   reported hours in the window reach the threshold. */
static double EnsWindowProb(const Ensemble *e, int startHour, int hours) {
    if (!e || !e->have) return NAN;
    int wet = 0, reporting = 0;
    for (int m = 0; m < e->members; m++) {
        int saw = 0, any = 0;
        for (int h = startHour; h < startHour + hours && h < e->hours; h++) {
            double v = e->mm[m][h];
            if (isnan(v)) continue;
            any = 1;
            if (v >= RAIN_MM) saw = 1;
        }
        if (!any) continue;
        reporting++;
        if (saw) wet++;
    }
    return reporting > 0 ? (double)wet / (double)reporting : NAN;
}

/* The contrast line: the coverage-blended figure most apps would show for
   the same window, straight from the forecast API's hourly PoP (its daily
   max is the headline version of the same blend). */
static double PoPWindowMax(const Forecast *f, int startHour, int hours) {
    if (!f || f->hN <= 0) return NAN;
    double mx = NAN;
    for (int k = 0; k < hours && startHour + k < f->hN; k++) {
        double v = f->hPoP[startHour + k];
        if (isnan(v)) continue;
        if (isnan(mx) || v > mx) mx = v;
    }
    return mx;
}
