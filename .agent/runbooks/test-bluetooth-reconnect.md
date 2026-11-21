# BT-01 — Windows earbuds visibility, audio status and automatic reconnect

**Status:** owner reports the original Guard OFF + Auto ON connection problem is
working. New panel-reveal/status visuals and the remaining matrix below are not
native-verified. Production-code mocks/cross-builds are not Windows focus, DPI,
earbud-driver, persistence or lifecycle acceptance.

## Next owner check — panel reveal and readable status

1. On the active Arena branch update **Project Hub to 2.8.67 (or later) and
   Bluetooth Guard**. Keep existing pairing/rules and normal elevation settings;
   no reset, unpairing or all-elevated Hub workaround is required. The Hub still
   closes/proves the old instance stopped before replacing an EXE.
2. Open Guard from its **Hub card**, then close it to the tray. Use **unified-tray
   Open**. Repeat behind another window, minimized, maximized, and maximized then
   minimized. The same PID/panel must appear in front, with maximized placement
   retained. With Hub exited, repeat using Guard's own fallback tray Open/click.
   Also test a direct second explicit launch. No second normal instance should
   remain. An active modal dialog, if available, should receive focus.
3. With the already-working earbuds connected, check **green Audio connected**
   against Windows Sound. The separate Auto-Connect line should say on/off/paused
   as appropriate; turning Auto off or an independent Windows reconnect during a
   current-run pause must not hide a real connection. Green means an ACTIVE
   Windows endpoint, not proof of audible playback.
4. Disconnect normally. A confirmed pair should be **blue Paired**; a pending
   request is blue **Connecting...**, and an ACL without audio is blue **Link
   only**. None should appear green until audio readiness is actually observed.
   Auto remains separately labeled; errors/HRESULTs have a full-width details
   row. Check long names/details and clipped card buttons at 100/150/200% DPI.
5. Existing review/denied rows stay amber/red. A saved-only record must say
   pairing is unconfirmed, not falsely Paired. An unavailable radio is neutral
   and keeps saved choices. Do not unpair or deny the working earbuds just to
   manufacture a visual state.
6. A queued/slow Open must not steal focus after newer activity in another app.
   See [the full Open matrix](test-hub-open-to-front.md) for other native apps.

The earlier connection check remains useful **if it regresses**: Guard OFF + Auto
ON, paired earbuds available for a full 60-second retry interval, compared with
Windows Connect. Record exact status/HRESULT, model, Windows version and Windows
Connect outcome if failing. Classic discovery still excludes LE-only devices;
no address-matched audio adapter is different from a request/driver error. Do
not infer an earbuds outcome from a phone's Windows-managed connection.

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
