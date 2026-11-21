/* Paired discovery is a review queue, not automatic trust. Keep these decisions
   independent of Win32 calls so the real policy can also run in regression tests. */
static BOOL DeviceNeedsReview(const DeviceEntry *d) {
    return (d->paired || d->remembered) && !d->visible && !d->forgotten && !d->denied;
}

static BOOL DeviceIsListed(const DeviceEntry *d) {
    return !d->forgotten && (d->visible || d->denied || DeviceNeedsReview(d));
}

static BOOL DeviceShouldBlock(const DeviceEntry *d, BOOL guardOn) {
    if (!d->connected) return FALSE;
    if (d->forgotten || d->denied) return TRUE;
    /* Windows already paired this device. Wait for the owner's decision, even
       on the pre-window startup scan; a foreground-window heuristic is not
       proof that the owner meant to deny it. Unpaired strangers remain blocked. */
    /* A remembered record is reviewable but is not authenticated. Do not let
       cached discovery history exempt an unpaired stranger from blocking. */
    if (!d->visible) return !d->paired;
    return guardOn && !d->allowAuto && !d->sessionAllowed;
}

static BOOL DeviceIsAudio(const DeviceEntry *d) {
    return _wcsicmp(d->kind, L"Audio") == 0 || d->audioEndpointFound;
}

static BOOL DeviceAudioReady(const DeviceEntry *d) {
    return d->audioStatusValid && d->audioConnected;
}

static ULONGLONG ConnectRetryDelayMs(unsigned attempts) {
    static const ULONGLONG delays[] = {5000ULL, 10000ULL, 20000ULL, 40000ULL, 60000ULL};
    return delays[attempts == 0 ? 0 : (attempts > 5 ? 4 : attempts - 1)];
}

static BOOL DeviceAutoConnectDue(const DeviceEntry *d, ULONGLONG now, BOOL radioOn) {
    /* Phone/HID links are managed by Windows, not by the audio one-shot API.
       An established ACL alone must not suppress an earbuds audio reconnect. */
    return radioOn && d->address && d->visible && !d->forgotten && !d->denied &&
           d->allowAuto && !d->autoPaused && DeviceIsAudio(d) && !DeviceAudioReady(d) &&
           (d->nextConnectTick == 0 || now >= d->nextConnectTick);
}

static void ResetConnectRequest(DeviceEntry *d) {
    d->connectState = BT_CONNECT_IDLE;
    d->connectError = S_OK;
    d->connectAttempts = 0;
    d->connectRequestTick = 0;
    d->nextConnectTick = 0;
}

static void SetManualConnectionChoice(DeviceEntry *d, BOOL connect, ULONGLONG now) {
    d->autoPaused = !connect;
    d->sessionAllowed = connect;
    d->seenConnected = FALSE;
    d->sessionStartTick = now;
    d->blockPending = FALSE;
    ResetConnectRequest(d);
    /* Do not invent a disconnected/connected observation when a button is
       clicked. The next Windows link/audio snapshot confirms the result. */
}

static void ApproveDevice(DeviceEntry *d, BOOL allowAuto, ULONGLONG now) {
    d->visible = TRUE;
    d->forgotten = FALSE;
    d->denied = FALSE;
    d->transient = FALSE;
    d->allowAuto = allowAuto;
    d->sessionAllowed = !allowAuto;
    d->seenConnected = d->connected;
    d->sessionStartTick = now;
    d->blockPending = FALSE;
    d->autoPaused = FALSE;
    ResetConnectRequest(d);
}

static void DenyDevice(DeviceEntry *d) {
    d->visible = FALSE;
    d->denied = TRUE;
    d->transient = FALSE;
    d->allowAuto = FALSE;
    d->sessionAllowed = FALSE;
    d->seenConnected = FALSE;
    d->blockPending = FALSE;
    d->autoPaused = FALSE;
    ResetConnectRequest(d);
}

/* Caller owns g_lock. UI rows and queued actions identify devices by address,
   never an array index that a background scan can compact under a painted row. */
static int FindDeviceIndexByAddress(ULONGLONG address) {
    if (!address) return -1;
    for (int i = 0; i < g_devCount; i++) {
        if (g_devices[i].address == address) return i;
    }
    return -1;
}
