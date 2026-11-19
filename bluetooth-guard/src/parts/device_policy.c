/* Paired discovery is a review queue, not automatic trust. Keep these decisions
   independent of Win32 calls so the real policy can also run in regression tests. */
static BOOL DeviceNeedsReview(const DeviceEntry *d) {
    return d->paired && !d->visible && !d->forgotten && !d->denied;
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
    if (!d->visible) return !DeviceNeedsReview(d);
    return guardOn && !d->allowAuto && !d->sessionAllowed;
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
}

static void DenyDevice(DeviceEntry *d) {
    d->visible = FALSE;
    d->denied = TRUE;
    d->transient = FALSE;
    d->allowAuto = FALSE;
    d->sessionAllowed = FALSE;
    d->seenConnected = FALSE;
    d->blockPending = FALSE;
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
