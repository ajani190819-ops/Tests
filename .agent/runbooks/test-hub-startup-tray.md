# APP-05 — intermittent startup tray consolidation

## Owner report and intended behavior

The owner now reports startup seems fixed right now, alongside Lighting and
UAC. APP-05 is **owner-reported good / MONITOR**, not permanently certified.
The earlier one-icon working observation is retained; the original report was
that startup sometimes showed every app's icon instead of one Hub icon. No
startup/tray change was made in the Open follow-up, so the exact cause remains
unproven. Do not turn this current-use approval into another acceptance matrix.

Expected while Hub is running with a usable tray: one Hub icon, with apps and
controls reachable from it. If Hub is unavailable, independent icons are an
intentional accessibility fallback. Never hide them merely to mask a missing
host. Preserve existing startup preferences; this report is not permission to
create a Run key/task/service or silently start Hub/Agent Notes at sign-in.

## Source audit — no cause or fix claimed yet

- `project-hub/src/parts/tray_unified.c`: Hub publishes the named host event only
  after shell registration succeeds. Its two-second housekeeping verifies the
  icon, releases unconfirmed ownership and retries registration/marker creation.
- `shared/win32/hub_tray_host.h`: companions probe and close the event on each
  check rather than keeping the host artificially alive. Consumers reconcile
  their icons on timers and handle `TaskbarCreated`.
- All six native companion sync bodies were inspected. Their hide branch clears
  the local visible flag even when `NIM_DELETE` fails; a later healthy-host tick
  then has no flagged icon to retry. This is a concrete retry-gap lead, not a
  reproduction of the owner's startup symptom. Any fix must also cover the
  ambiguous case where Explorer already lost the icon, so retaining a stale
  visible flag must not prevent fallback restoration if Hub later exits.
- Full Hub `wWinMain` was inspected: mutex/show-event coordination and window
  setup do not enroll Hub at sign-in. Existing child timers do not launch it.
- The present design coordinates an **already-running** Hub; it does not add a
  Hub sign-in registration. Historical Hub presence during the multi-icon
  condition remains unknown; that is not a new owner observation request.
- `project-hub/src/parts/app_activation.c`: card and unified-tray Open share
  bounded asynchronous show/launch handling. The 2.8.71 Open follow-up extends
  Guard's show-only reveal to Lighting and keeps driver enumeration outside
  Guard's UI lock. These are separate source repairs, not a startup consolidation
  fix or native UAC/foreground acceptance. The prior silent Lighting failure
  preceded the 2.8.72 offscreen correction; Lighting, UAC and startup are now
  owner-reported good. Their exact causes are not proven. Hub stays asInvoker.

## Next observations / future regression boundary

There is no current owner reproduction/log request while one icon is working.
Historical Hub presence during the multi-icon condition remains unestablished.
The separate Open apps are now known: Guard and Lighting. Do not ask which apps
again or request another copy of the old Windows 225 screenshot.
The fresh lighting log has now arrived and was scanned; its separate input
receiver follow-up is in [`test-lighting-input.md`](test-lighting-input.md). The
shorthand `2/3. kindarandom/ all` does not clearly establish Hub presence or the
prompting executable. Those historical unknowns do not negate the latest
current-use approval or make further collection the next task.

A later fix needs controlled startup-order, delayed/missing Explorer, Hub marker
failure/recovery, Explorer-restart, Hub-exit/crash and duplicate-free Open
regressions. Native sign-in evidence is distinct from mocked harness results.
Do not change manifests, UAC/security policy, startup registrations, child
settings or lighting files simply to make the symptoms disappear.

This Hub/tray investigation remains source inspection only: no Hub/companion
tray change, startup registration or elevation-policy change is claimed. The
separate lighting receiver guard/diagnostics do not fix APP-05 or constitute
Windows sign-in/Open evidence.
