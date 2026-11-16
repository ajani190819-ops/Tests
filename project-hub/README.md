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
