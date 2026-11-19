# Project Hub

This concise Git entry point preserves the active artifact and its offline role.

The full pre-migration narrative and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.

## Notification-area ownership

When Project Hub is running and its notification-area icon is available, it is
the suite's single tray host. Its menu opens, hides, or explicitly quits active
suite apps and exposes only verified safe toggles. Closing Hub hides its window;
choose **Exit Project Hub** to stop the host. If Hub exits, crashes, or Explorer
rebuilds its notification area, each already-running app restores its own tray
fallback automatically. See the Drive guide for the owner-facing workflow and
troubleshooting narrative.

## Open an app

Card and unified-tray **Open** actions share one show-or-launch route. An
already-running app is revealed/restored and brought forward, including a
tray-hidden window or its active modal dialog, without starting another copy.
Maximized placement is preserved. Native startup mutexes and catalogue image
process checks distinguish a starting app from an absent one; rapid repeated
clicks reuse the pending request.

The Hub retries a slow window on a short, bounded UI timer, not a blocking
wait. Switching to another app cancels deferred focus so a late startup does
not pull you back. Failed/inaccessible launches are reported, not silently
retried as extra instances. Child elevation contracts, update close-and-prove
gates, and Keyboard Lighting's engine are unchanged. Native Windows validation
remains separate from the automated checks; see
`.agent/runbooks/test-hub-open-to-front.md`.

## Suite Settings

The Soft Slate Settings window centralizes Keyboard Lighting hold behavior,
keyboard Balance/Steady Colours, DNS Pilot scheduling, and Hub branch following.
Keyboard Lighting preferences update both `panel.json` (saved state) and
`theme.json` (the live engine input); its tray reloads an external panel change
before it can write stale in-memory values back. DNS Pilot still applies its
saved INI changes after restart.

Project Hub itself runs as the current user, so opening the Hub does not need an
administrator prompt. Apps that declare elevated access continue to request it
when explicitly opened, and update actions retain their stop-and-verify gates;
cancelled elevation or inaccessible targets do not bypass those checks.
