/* Pure policy shared by the native provider and executable regression tests.
   A Windows fix can be approximate. Never treat missing accuracy, an old fix,
   a future timestamp or unsupported Mercator latitude as an exact position. */
enum { LOCATION_PROVIDER_PENDING = 0, LOCATION_PROVIDER_READY,
       LOCATION_PROVIDER_DENIED, LOCATION_PROVIDER_UNAVAILABLE, LOCATION_PROVIDER_CANCELED };
typedef struct { double lat, lon, accuracyM; uint64_t fixEpoch; } LocationSample;

#define LOCATION_MAX_AGE_SEC 120ULL
#define LOCATION_FUTURE_SEC 15ULL
#define LOCATION_MAP_LAT 85.05112878

enum { LOCATION_FIX_OK = 0, LOCATION_FIX_COORDS, LOCATION_FIX_ACCURACY,
       LOCATION_FIX_STALE };

static int LocationFixProblem(double lat, double lon, double accuracyM,
                               uint64_t fixEpoch, uint64_t now) {
    if (!isfinite(lat) || !isfinite(lon) ||
        lat < -LOCATION_MAP_LAT || lat > LOCATION_MAP_LAT ||
        lon < -180.0 || lon > 180.0) return LOCATION_FIX_COORDS;
    if (!isfinite(accuracyM) || accuracyM <= 0.0) return LOCATION_FIX_ACCURACY;
    if (!fixEpoch || (fixEpoch > now && fixEpoch - now > LOCATION_FUTURE_SEC) ||
        (now >= fixEpoch && now - fixEpoch > LOCATION_MAX_AGE_SEC))
        return LOCATION_FIX_STALE;
    return LOCATION_FIX_OK;
}

static int LocationTicketCurrent(uint32_t ticket, uint32_t current, int pending) {
    return pending && ticket != 0 && ticket == current;
}
