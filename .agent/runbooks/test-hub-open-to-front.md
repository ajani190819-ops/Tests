# Hub Open-to-front — native Windows check

Status: pending owner/native Windows execution. A cross-build and mocked Win32
regression are not a Windows foreground-lock or cross-integrity result.

## Setup

1. In Project Hub choose the current active Arena branch, refresh, and update
   Hub to 2.8.69 or later, plus the Bluetooth Guard panel/status update. The original route shipped
   in 2.8.63; 2.8.67 adds Guard-owned cross-integrity reveal. Keep existing
   elevation settings; do not change UAC policy or run everything elevated
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

## Bluetooth Guard follow-up

The owner reports basic Bluetooth connection now works, but Hub/tray Open did
not show its panel. The new route adds a single per-window registered show-only
message to the elevated Guard, with scalar ticks, freshness checks and no policy
commands. The legacy asynchronous Hub route remains for old builds/other apps.
Automated mocks reproduce blocked foreign show/foreground calls and execute the
production Guard handler; they do not prove this was the owner's exact cause.

Prioritize the normal unelevated Hub -> elevated Guard case from card and tray,
then Guard's fallback tray (Hub exited), duplicate explicit launch, hidden,
minimized/maximized and any active modal window. Keep PID unchanged and do not
accept merely visible-but-behind as success. Move the pointer after clicking
Open; bounded retries should still work while Hub owns foreground. Switching to
another app must cancel deferred focus; a busy child's stale queued request must
not pull you back later. Record display/focus and any UAC program/path separately.

## Guard launch failure / exact Windows error — APP-01 active

The owner reports Bluetooth Guard Open/Start shows the generic launch-failure
footer with **no UAC prompt**. This is a new-launch failure, not proof that the
2.8.67 reveal protocol failed or that the owner cancelled consent. The old Hub
suppressed generic Shell error UI and discarded GetLastError. Its NO_UI flag
does not suppress security prompts; do not infer an elevation-policy cause.

Hub 2.8.69 captures the Windows code immediately, before handle/timer cleanup,
and shows one local Hub-owned error dialog with executable, working folder,
code and Windows description. The footer retains the app filename and code.
Missing/unknown descriptions or absent codes are explicit, never called success.
Nothing is logged/uploaded, no automatic retry/download is added, and existing
child manifests/elevation, installed-image lookup, reveal and update gates stay.

Next owner check: update **Hub** on the active branch, click Guard Open/Start
once, and retain the exact error dialog (code, description and target path).
Also record whether a UAC prompt appeared. Diagnose that actual error before
changing policy or replacing files. This release repairs error reporting only;
the cause of the owner's Guard launch failure remains unconfirmed.

## Failures / boundaries

- A failed/cancelled launch shows its actual Windows failure without
  installing/updating a replacement or claiming success. A running but
  inaccessible app must not be blindly launched again. A late failed shell
  must not show a stale error over a newer successful Open.
- A window that never becomes available stops retrying after the bounded
  startup interval (10 seconds after the shell returns). The Hub stays usable.
- Explicit Quit, update stop/verify gates, and hidden/visible update restoration
  are unchanged. Test updates separately; do not turn this check into an
  elevation or startup-policy change.

Record Hub version/released SHA, app, card versus tray, starting state, resulting
visibility/focus, whether its PID changed, and any prompting program/path.
Do not mark APP-01 hardware/native verification complete from automated results.
