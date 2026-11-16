/* Shared Project Hub unified-tray capability.
   The named event exists only while a current Project Hub has successfully
   placed its own notification-area icon. Suite apps open and close it for
   each probe; they do not retain a handle, so the event disappears promptly
   when Hub exits or crashes and each app can restore its own fallback icon. */
#ifndef TESTS_SUITE_HUB_TRAY_HOST_H
#define TESTS_SUITE_HUB_TRAY_HOST_H

#define HUB_TRAY_HOST_EVENT L"Local\\Tests_ProjectHub_UnifiedTray_v1"

static inline int SuiteHubOwnsTray(void) {
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, HUB_TRAY_HOST_EVENT);
    if (!event) return 0;
    CloseHandle(event);
    return 1;
}

/* Explorer broadcasts this registered message after it rebuilds the
   notification area. Callers use it to recreate only their own fallback icon;
   the Hub performs the same restoration for the unified icon. */
static inline UINT SuiteTaskbarCreatedMessage(void) {
    static UINT message = 0;
    if (!message) message = RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

#endif
