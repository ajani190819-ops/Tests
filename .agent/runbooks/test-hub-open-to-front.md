# Hub Open-to-front — native Windows check

Status: pending owner/native Windows execution. A cross-build and mocked Win32
regression are not a Windows foreground-lock or cross-integrity result.

## Setup

1. In Project Hub choose the current active Arena branch, refresh, and update
   Hub. The implementation introduces Hub 2.8.63. Keep the existing app installs
   and elevation settings; do not change UAC policy or run everything elevated
   to mask a failure.
2. Cover Bluetooth Guard, Agent Notes, DNS Pilot, Audio Switcher, Weather Radar,
   and the existing Keyboard Lighting window. Do not change lighting settings,
   keys, or engine files: M4/M5 work is still parked for a fresh `engine.log`.
3. If another catalogue EXE is installed, cover its normal main window too.
   Open targets must refer to that installed image, not an unrelated same-name
   executable. Folder/browser actions are outside this native-app matrix.

## Matrix — both the Hub card and unified-tray Open

For each installed app:

- **Visible, behind Hub:** Open puts the app in front and makes it usable.
- **Minimized:** Open restores the existing window, not a second instance.
- **Tray-hidden:** close/hide normally, then Open reveals the same instance.
- **Maximized:** Open preserves maximized placement; also minimize the maximized
  window and check it restores correctly.
- **Modal dialog:** if the app has one, leave it active. Open brings its dialog
  forward rather than stranding it behind a disabled parent.
- **Repeated click/startup:** from a genuinely stopped app, click Open rapidly
  twice. One normal instance appears; a late window is focused when ready.
- **Already running from a registered stray native copy:** Open reveals that
  instance instead of launching the suite-folder copy on top of it.
- **Newer activity:** open a stopped app and switch to another application
  before startup finishes. The Hub's deferred retry must not steal focus back.

For an existing instance, the process ID should stay the same across Open.
Native single-instance protection must remain intact. Verify ordinary/elevated
child windows from the normal unelevated Hub; an already-running window should
not require a new launch merely to show it. A genuinely new elevated child
retains its existing manifest and consent behavior.

## Failures / boundaries

- A failed/cancelled launch reports failure without installing/updating a
  replacement or claiming success. A running but inaccessible app must not be
  blindly launched again.
- A window that never becomes available stops retrying after the bounded
  startup interval (10 seconds after the shell returns). The Hub stays usable.
- Explicit Quit, update stop/verify gates, and hidden/visible update restoration
  are unchanged. Test updates separately; do not turn this check into an
  elevation or startup-policy change.

Record Hub version/released SHA, app, card versus tray, starting state, resulting
visibility/focus, whether its PID changed, and any prompting program/path.
Do not mark APP-01 hardware/native verification complete from automated results.
