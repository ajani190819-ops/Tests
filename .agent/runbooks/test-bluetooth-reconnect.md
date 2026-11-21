# BT-01 — Windows earbuds visibility, audio status and automatic reconnect

**Status: not run on Windows hardware.** Cross-builds and portable mock-COM
regressions are not an owner-earbud pass. This checklist preserves the current
report: Guard OFF + Auto-Connect ON earbuds fail to connect or be reflected,
while a phone connects with the same choices. Do not assume the phone exercises
the earbuds' audio API or that missing retries were the only cause.

## First owner check (priority)

1. In Project Hub select the active Arena branch and install its released
   **Bluetooth Guard** update. Existing rules/pairing should remain; do not delete
   rules, unpair, or reset Windows as a prerequisite. Close/prove-stopped EXE
   replacement remains the Hub's responsibility. The Hub binary/version need
   not change for this child-only release.
2. With the earbuds already paired in Windows but disconnected/in their case,
   open Guard. Confirm their named row exists. If undecided, use Auto-Connect to
   approve; merely scanning must not approve or disconnect a paired candidate.
   Saved-only/unconfirmed pairing is explicitly labeled, not treated as trust.
3. Set **Guard OFF + Auto-Connect ON**. Make the earbuds available to the PC.
   Observe for at least one full 60-second retry interval. Expect a transition
   from Connecting/retry to **Audio connected**, consistent with Windows Sound /
   Bluetooth Settings. Test audio playback separately: ACTIVE is not proof of
   an audible stream. A mere Bluetooth link must not claim audio is connected.
4. If the row is absent or connection misses, record the exact row/status and
   any `0x...` error, earbuds model, Windows version and whether Windows' own
   Connect works. No-address-matched-endpoint is different from a driver/API
   error. LE-only discovery and drivers without address-mappable audio adapters
   remain explicit limits; do not connect a same-named different device as a
   workaround. This evidence determines any remaining native repair.

## Reconnect, controls and safety

- Repeat with **Guard ON + Auto ON**: approved audio reconnect must still work.
  Leave the phone's existing choices intact and verify its Windows-managed
  connection remains reflected as a Bluetooth link, not fabricated audio.
- Case/power-off or take the earbuds out of range, then make them available
  again. The saved row must remain, actual audio status must drop/recover, and
  retries must continue without toggling Auto each time. Retry gaps back off
  through 5/10/20/40/60 seconds; do not expect an immediate attempt at each scan.
- Click **Disconnect** on an Auto ON pair. Verify the actual link/audio ends;
  the app must not undo it with its own retry, even after more than 60 seconds.
  Auto stays ON but shows paused. This is an active-retry pause, not a new block
  on independent Windows/peripheral connections. Click **Connect** (or toggle
  Auto off/on) to resume. Repeat via the tray; stale queued Disconnect must not
  undo a later Connect/Auto change.
- Set **Auto OFF**. Guard ON should block a later unsolicited reconnect; Accept
  or Connect permits a current manual session without enabling future retries.
  Guard OFF should pause that manual-only enforcement, not activate retries on
  an Auto OFF row. Do not mark a button/request as connected before Windows
  actually reports a link/audio endpoint.
- Close/reopen and restart Guard. Explicit approval/Auto and Deny/Forget records
  must persist; manual session/retry timing/pause must not become durable trust.
  Sleep/resume should pause requests while suspended and resume eligible Auto
  requests afterwards without clearing a deliberate current-run Disconnect.
- On a separately chosen review device, **Deny** must keep Windows pairing and
  block its links even with Guard OFF; a later Accept cancels a stale policy
  drop. Test **Forget** only if intentional: it really unpairs. An old scan or
  queued connect must not silently re-enroll it.
- Verify manual Connect still works in Windows Settings; no Bluetooth service
  profile may be disabled. If two devices have the same friendly name, actions
  on one address must never connect/disconnect the other.

Record which steps ran and their outcome; keep unrun hardware/LE/driver cases
open rather than inferring success from automated checks or the phone result.
