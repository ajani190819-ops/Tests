/* Presentation only: connection observations, approval and saved Auto choice
   stay separate. A request/ACL alone must never make an audio row green. */
typedef struct {
    const wchar_t *status;
    COLORREF statusFg, statusBg;
    const wchar_t *preference;
    COLORREF preferenceFg;
    wchar_t detail[128];
} BtDeviceVisual;

static void DescribeDevice(const DeviceEntry *d, BOOL radioOn, ULONGLONG now,
                           BtDeviceVisual *view) {
    BOOL audio = DeviceIsAudio(d);
    BOOL ready = DeviceAudioReady(d) || (!audio && d->connected);
    BOOL requested = d->connectState == BT_CONNECT_REQUESTED &&
                     now - d->connectRequestTick < 10000ULL;
    view->status = L"Saved device";
    view->statusFg = COL_MUTED;
    view->statusBg = COL_PILL_MUTED;
    if (DeviceNeedsReview(d)) {
        view->status = L"Needs approval";
        view->statusFg = COL_WARN; view->statusBg = COL_WARN_BG;
    } else if (d->denied) {
        view->status = L"Denied";
        view->statusFg = COL_DANGER; view->statusBg = COL_DANGER_BG;
    } else if (!radioOn) {
        view->status = L"Radio unavailable";
    } else if (ready) {
        view->status = audio ? L"Audio connected" : L"Connected";
        view->statusFg = COL_GOOD; view->statusBg = COL_GOOD_BG;
    } else if (requested || (audio && d->connected) || d->paired || d->allowAuto) {
        view->status = requested ? L"Connecting..." :
                       (audio && d->connected) ? L"Link only" :
                       d->paired ? L"Paired" : L"Auto configured";
        view->statusFg = COL_ACCENT; view->statusBg = COL_ACCENT_BG;
    }

    view->preference = L"Auto-Connect off";
    view->preferenceFg = COL_MUTED;
    if (DeviceNeedsReview(d)) {
        view->preference = L"Approval required";
        view->preferenceFg = COL_WARN;
    } else if (d->denied) {
        view->preference = L"Auto-Connect disabled";
        view->preferenceFg = COL_DANGER;
    } else if (d->allowAuto) {
        view->preference = d->autoPaused ? L"Auto-Connect paused" : L"Auto-Connect on";
        view->preferenceFg = d->autoPaused ? COL_WARN : COL_ACCENT;
    }

    const wchar_t *detail;
    if (DeviceNeedsReview(d))
        detail = d->paired ? L"Choose Accept, Deny or Auto-Connect" :
                             L"Windows saved record; pairing unconfirmed";
    else if (d->denied)
        detail = L"Connection denied; Windows pairing kept";
    else if (!radioOn)
        detail = L"Bluetooth radio unavailable; saved choices kept";
    else if (ready)
        detail = audio ? L"Windows audio endpoint connected" :
                         L"Bluetooth link connected; Windows manages this device";
    else if (d->autoPaused)
        detail = d->allowAuto ? L"Automatic retry paused; Connect resumes it" :
                                L"Not connected; click Connect for one session";
    else if (d->connectState == BT_CONNECT_FAILED) {
        _snwprintf(view->detail, 127, L"Connect failed (0x%08lX); %ls",
                   (unsigned long)(DWORD)d->connectError,
                   d->allowAuto ? L"retry pending" : L"try Windows Connect");
        view->detail[127] = L'\0';
        return;
    } else if (d->connectState == BT_CONNECT_NO_ENDPOINT)
        detail = L"No address-matched audio endpoint";
    else if (requested)
        detail = L"Request sent; waiting for Windows audio";
    else if (audio && d->connected)
        detail = L"Bluetooth link only; audio not connected";
    else if (!audio)
        detail = L"Not connected; Windows manages this device";
    else if (!d->allowAuto)
        detail = L"Not connected; click Connect for one session";
    else
        detail = L"Waiting to retry; up to 60s between attempts";
    wcsncpy(view->detail, detail, 127);
    view->detail[127] = L'\0';
}
