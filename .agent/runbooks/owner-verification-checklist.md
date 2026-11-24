# Owner status and verification reference — 20 retained IDs

**Latest owner report:** UAC/account-control prompts, Lighting and startup seem
fixed for current use. Record **owner-reported good**, not permanent/native
certification. This supersedes the prior Lighting report of **nothing visible**
happening; no new Guard outcome or installed-build identity is supplied. The
historical affected Open apps were Bluetooth Guard and Keyboard Lighting.
Kill/reopen and Lighting **Unfreeze** remain workarounds, not intended Open.
No repeated logs/matrix or invented fixture/security/hardware/print evidence.

## Current actions only

- **PLAN-01 CURRENT:** approved settings/setup increment implemented for
  both Orca plugins: every control visible in labelled groups, exact approved
  process diffs only, no automatic preset saves. Printer/filament stay read-only;
  relative E stays advisory by explicit owner choice. Inspector/toolpath work is
  not in that pass. Menus work/look good; owner now reports a pre-wave outer
  strand and curl. Orca 2.5.2/custom modified Ender 3, PLA 205°C, Auto/12 V fan
  confirmed. Undahang 0.0.54 addresses a synthetic minority-loop ordering gap
  and the requested ≤50% primary M106 ceiling (restore included). Curl cause
  and exact owner-file match remain unproven; PWM is not voltage conversion.
  Next: supported processed-preview handoff; native preferred, separate viewer
  acceptable if needed. Series renumbering remains paused.
  ORCA-04/05 remain PARKED. [Scope](../../docs/ORCA-SETTINGS-SETUP.md).
- **APP-01 Owner-reported good:** Lighting Open/current use now fine. The 2.8.72
  offscreen source regression does not establish the owner's cause or installed
  build, and no new Guard-specific result is inferred. No log/test loop.
  See [`test-hub-open-to-front.md`](test-hub-open-to-front.md).
- **APP-05 MONITOR / Owner-reported good:** startup currently working; preserve
  usable fallback icons and choices. The source-only delete/retry lead remains
  in [`test-hub-startup-tray.md`](test-hub-startup-tray.md), not new coding scope.
- **OPS-01 Owner-reported good:** UAC seems fixed; exact historical prompting
  executable/path/action remains unknown, not a new evidence request.
- **KBL-01 Owner-reported good:** Lighting generally fine; no separately executed
  physical-hold test claimed; fresh `engine-6.log` received and fully scanned.
  No new `input-status.txt` or engine-log request; do not repeat that loop.
  See [`test-lighting-input.md`](test-lighting-input.md).

The older shorthand `2/3. kindarandom/ all` did not identify the UAC executable
or historical Hub presence. The affected Open apps are now known; do not ask
which apps again. No more log collection is the next action.

The procedures below are retained **reference**, not a new request to retest
items the owner has just reported good. The ledger at the end is current.

The prior batch prepared Weather/Orca review tooling and repaired stale HUD
handoff guidance. APP-04 adds standalone AppReview.exe for local evidence; it
is not a Hub button or an update to the seven existing apps. Those tooling-only
releases left their EXEs, plugin algorithms, lighting files and HUD renderer
unchanged. The prior receiver release changed Keyboard Lighting only; the current
2.8.71 Open follow-up changed Hub and both affected panels; the 2.8.72 follow-up
changes Hub/Lighting placement only, not engines, input or settings. No installation or settings reset is required to run the review utility.

## Record once for each test session

- Selected Hub branch and its `.agent/RELEASED` commit; installed Hub version
  (placement follow-up **2.8.72**), Windows version, screen/DPI/theme.
- Installed app/plugin/JAR identity, not just current Git tip. This increment
  is Undahang **0.0.54** / Infill **0.4.11**; installed plugin identity remains
  unconfirmed (host is owner-confirmed Orca 2.5.2). Orca still calls the stable package/capabilities **Wave Overhangs**.
- For each item: **PASS / FAIL / BLOCKED / NOT RUN**, exact action, expected vs
  observed result, build/environment and a reviewed screenshot/log if relevant.
  Keep precise locations, security history, user paths and server data private;
  raw owner evidence must not enter public Git/Drive.

## Reference safety gates

### APP-01: historical security-block procedure, if still blocked

An earlier screenshot established Windows error 225 for Bluetooth Guard with
no UAC prompt. **Do not send the same screenshot again.** Its actual provider,
detection/rule, action/time and installed-file provenance were not supplied.
The latest broad owner report is not a malware/false-positive or safe-resolution
verdict. Do not assume this old refusal explains every new intermittent Open.

Use the actual antivirus/Windows Protection history, or optionally the published
read-only `tools/guard_security_report.ps1` under the existing ordinary,
unelevated script policy. Follow
[`test-hub-open-to-front.md`](test-hub-open-to-front.md) for the collector and
private report review. Its frozen Hub baseline intentionally predates 2.8.70:
a changed Hub hash is not automatically suspicious or a clean verdict.

If policy/security refuses collection, stop and use the History UI. **No
Allow/Restore, exclusions, protection disable, policy bypass, elevation changes,
renaming/repackaging/rebuilding to evade detection, or Guard launch/update for
testing.** If the security block is still present, resolve evidence/provenance
and the legitimate vendor-review path first; do not launch a blocked executable
to obtain tests. The independent Hub/Open/tray investigation needs no bypass.

### HUD-01: retained source/scope boundary

The original **Classic Affine / Captured Mesh / World-Space Texture** request is
retained. Current source instead forces **Purple Horizontal Panel**; the old
choices cannot be tested as requested. No methods were restored in this batch.
The owner now reports the current experience good. This is not proof that the
three missing methods were implemented/tested, nor a request to reopen that work.
Keep this scope boundary in [`test-spatial-hud-f5w.md`](test-spatial-hud-f5w.md).

## APP-04 — optional evidence shortcut (current use owner-reported good)

**Portable pilot:** no installer, Hub entry or Install/Update button. Local
build/safety checks pass; the first owner-supplied Hub/Guard images have now been
reviewed at 120 DPI (125%). The owner now reports current use good. That does
not execute the separate synthetic native kit (manual/unrun, not CI-integrated)
or supply a new preference-persistence test. The already-running Guard image
does not establish how the earlier security block was resolved. Preserve intended
settings; no security workaround or further Guard test before APP-01 clearance.
The pause/detail versus button wording conflict is recorded in
[`test-app-review.md`](test-app-review.md#first-supplied-two-window-pilot--reviewed).

Run the released **AppReview.exe**, check already-open Hub/Weather windows, then
click **Capture + read-only checks**. It takes OS-composited window screenshots
without Print Screen/clipboard and saves a local self-contained **report.html**,
PNGs and metadata. No automatic sharing, target clicks/navigation, config access,
launch/update, resizing/restoring or setting changes/rollback. Unknown capture
support and blank/protected/hidden/minimized windows are evidence gaps, not passes.
No fallback bypasses capture restrictions. More active UI tests are not enabled.

Use **Open report** to review private contents, then **Open folder** and manually
attach report.html (its images are embedded), or selected PNGs. The agent can
review visuals; ping/bounds/placement checks do not certify app functions. Keep
normal intended settings. Do not open blocked Guard or weaken security to use
this tool. Full instructions: [`app-review/README.md`](../../app-review/README.md);
verification boundary: [`test-app-review.md`](test-app-review.md).

## Reference: installed desktop checks (not a new retest request)

1. **APP-03 + NAME-01: Hub layout and public names.** Open the Hub without
   opening blocked Guard. Verify General apps / Orca plugins / Minecraft mods;
   separate Undahang/Infill cards with the approved distinct Undahang icon;
   Weather Report / Prism Lighting labels; four visible right-side controls
   aligned with title/icon when they fit, no phantom Update Hub gap, safe
   wrapping/scrolling and hit targets on smaller windows and actual DPI.
   Existing shortcuts/settings stay compatible; old Orca capability names and
   own-app Keyboard Lighting labels are intentional. Do not exercise destructive
   update/remove actions merely for a visual check. Details:
   [`test-public-names.md`](test-public-names.md),
   [`test-hub-grouped-entries.md`](test-hub-grouped-entries.md).
2. **WX-01: planner.** In Weather Report, click all five dates and rain,
   temperature, cloud and wind. Check aligned readable cells, axes/units,
   selection and explanations at actual DPI/small screen/light-dark themes.
   Exact-point ensemble odds must remain primary, regional PoP secondary.
3. **WX-02: live map.** Search/keep two far-apart places. Check marker/basemap
   identity, zooms 7–10, frames/play, then switch places rapidly while loading:
   no old-city map under a new marker. Check attribution/loading/error states
   and responsiveness. Steps 2–3:
   [`test-weather-radar-windows.md`](test-weather-radar-windows.md). The optional
   component harness is not required for this installed-app check and has not
   been natively executed here.
4. **WX-03: actual position.** Click **Use my location** explicitly. Check the
   Windows permission/provider response, displayed accuracy/time and actual
   marker position; record the original offset if it remains. Denied/unavailable
   cases must be honest and must not silently track or elevate. Keep exact
   coordinates private. Details:
   [`test-weather-location.md`](test-weather-location.md).
5. **APP-02: start the longer soak alongside other work.** Record starting
   process/resource state and exact original expiry symptom. Leave Hub running
   through the usual failure interval; include ordinary refresh/open/close,
   sleep/wake and tray recovery observations per
   [`test-hub-longevity.md`](test-hub-longevity.md). A quick successful opening
   is not an expiry/soak pass. Do not restart merely to hide a recurrence.

## Reference: conditional hardware/host/game checks

- **BT-01 / remaining APP-01 foreground checks:** only after the security
  prerequisite is genuinely cleared. Verify card/unified/fallback-tray Open
  brings the same app/PID and active modal panel forward, preserving placement
  with no duplicate process. Then check green confirmed-connected vs blue
  paired/Auto/request states, missing earbuds rows, no pre-Accept/Deny drop,
  Auto-Connect row, Accept/Deny/Forget, persistence/manual/lifecycle/reconnect.
  Preserve the original **Guard OFF + Auto ON earbuds/Earpods failure while
  phone worked under identical settings** as a separate case; later basic
  connection success is already owner-confirmed, not blanket acceptance.
  Do not disable Bluetooth profiles. Runbooks:
  [`test-bluetooth-reconnect.md`](test-bluetooth-reconnect.md),
  [`test-hub-open-to-front.md`](test-hub-open-to-front.md).
- **ORCA-01 / ORCA-03:** capture actual effective capability Config JSON and
  preset overrides before resets; record exact host/plugin versions. Obtain a
  fresh slicer control plus independently exported Wave-only, Infill-only and,
  if the real UI permits, both-selected comparisons. Use Export G-code and
  reopen the outputs; selections alone are not execution. The archived T2
  `clean.gcode` and T3 both already contain Infill 0.4.8 rewrites. They are not
  untouched controls, and they do not supply the effective plugin settings.
  Inspector/procedure: [`test-orca-real-host.md`](test-orca-real-host.md).
- **ORCA-02:** only after reviewing fresh output paths, supervise a small
  representative print with curved walls/holes/multiple overhang layers. Record
  exact output hash/settings and photos/clearance/quality/timing. No automatic
  print is initiated; an offline replay is not physical-print acceptance.
- **HUD-01:** after verifying the correct built branch JAR, optionally record
  the current purple-panel F5W smoke matrix separately: presets/placement,
  captured companion bars, occlusion of both image and frame, GUI boundaries,
  lifecycle/flicker/performance, normal Iris stack and same-run logs. Do not
  substitute these observations for the original three-method acceptance or
  restore retired methods without a scope decision.

## Monitoring, evidence limits and parked work

- **OPS-01:** UAC seems fixed for current use. The historical prompting
  EXE/path/action is unknown, not a new collection request. Guard's earlier
  225 case had no prompt; do not infer a shared cause or change elevation.
- **KBL-01:** Lighting is generally fine for current use; the fresh log was
  already received/scanned. Its write warnings and prior receiver diagnostics
  do not establish a physical-hold result or installed build. No repeated
  `input-status.txt`/engine-log request; preserve settings and other software.
  APP-01's earlier active failure is superseded, not an ongoing source task.
- **PLAN-01:** the known list is owner-reviewed. Retain all IDs and add any
  recovered omissions; do not claim recovery of unsaved conversations.
- **ORCA-04 / ORCA-05:** wake/seam blending and smoother curved-boundary
  research stay parked. No new coding approval is inferred.
- **PLAN-02:** first optimization is shipped; further caches/workers stay
  deferred. No new owner performance acceptance is requested for this batch.

## Current ledger — eight categories, all 20 IDs retained

“Owner-reported good” records the owner's broad current-use report, not a
newly executed individual acceptance matrix or independently verified test.

| Category | ID | Current owner status / next action |
| --- | --- | --- |
| Bluetooth | BT-01 | Owner-reported good; known wording finding not falsely marked fixed |
| Hub/Windows | APP-01 | Owner-reported good: Lighting current use; no new Guard outcome or proof of offscreen cause |
| Hub/Windows | APP-02 | Owner-reported good; no new detailed soak claimed |
| Hub/Windows | APP-03 | Owner-reported good: current layout/icons |
| Hub/Windows | APP-04 | Owner-reported good; separate native kit still manual/unrun |
| Hub/Windows | APP-05 | **MONITOR / Owner-reported good:** startup currently working; earlier one-icon report retained, no lasting startup fix claimed |
| Hub/Windows | OPS-01 | Owner-reported good: UAC seems fixed; historical prompt identity unproven, no new request |
| Weather | WX-01 | Owner-reported good: planner |
| Weather | WX-02 | Owner-reported good: live map/desktop |
| Weather | WX-03 | Owner-reported good: actual-position experience; no new measurement claimed |
| Lighting | KBL-01 | Owner-reported good: Lighting generally fine; no separately executed physical-hold test or repeated log request |
| Spatial HUD | HUD-01 | Current experience owner-reported good; missing original methods not claimed restored |
| Orca | ORCA-01 | Owner-reported good; no new configuration/export artifact claimed |
| Orca | ORCA-02 | Owner-reported good; no independently documented print claimed |
| Orca | ORCA-03 | Owner-reported good; no new host callback run claimed |
| Orca | ORCA-04 | PARKED: wake/seam research, no coding approval |
| Orca | ORCA-05 | PARKED: curved-boundary research, no coding approval |
| Naming | NAME-01 | Rollout owner-reported good; own lighting branding unchanged |
| Planning | PLAN-01 | **CURRENT:** settings/setup menus owner-reported good; prints pending; preview feasibility next, renumbering paused; printer/filament read-only, no autosave; known list owner-reviewed, all old IDs retained |
| Planning | PLAN-02 | First optimization shipped; further caching/workers DEFERRED |

The current task needs no new owner logs or reproduction. Existing procedures
are reference only, not a request to compile tools, reset settings or rerun the
old full list. Native acceptance remains honestly open.
