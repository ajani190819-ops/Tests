/* Show-only cooperation across the Hub/child integrity boundary. No pointers,
   paths, policy commands, process launch or device actions cross this channel.
   Include after windows.h; the receiving window opts in to this one message. */
#ifndef TESTS_SUITE_WINDOW_REVEAL_H
#define TESTS_SUITE_WINDOW_REVEAL_H

#define SUITE_WINDOW_REVEAL_NAME L"Tests.Suite.RevealWindow.v1"
#define SUITE_WINDOW_REVEAL_MAX_AGE_MS 1000UL

static inline UINT SuiteWindowRevealMessage(void) {
    static UINT message = 0;
    if (!message) message = RegisterWindowMessageW(SUITE_WINDOW_REVEAL_NAME);
    return message;
}

static inline BOOL SuitePostWindowReveal(HWND target) {
    UINT message = SuiteWindowRevealMessage();
    LASTINPUTINFO input = { sizeof(input), 0 };
    return message && GetLastInputInfo(&input) &&
           PostMessageW(target, message, (WPARAM)(DWORD)GetTickCount64(), (LPARAM)input.dwTime);
}

/* A busy child's queued reveal must not outlive newer user activity. Ticks are
   DWORD scalars, with wrap-safe elapsed subtraction, never foreign pointers. */
static inline BOOL SuiteWindowRevealIsCurrent(WPARAM sentTick, LPARAM inputTick) {
    LASTINPUTINFO input = { sizeof(input), 0 };
    return (DWORD)((DWORD)GetTickCount64() - (DWORD)sentTick) <= SUITE_WINDOW_REVEAL_MAX_AGE_MS &&
           GetLastInputInfo(&input) && input.dwTime == (DWORD)inputTick;
}

#endif
