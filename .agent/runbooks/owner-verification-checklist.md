# Owner verification checklist — retained 18-item plan

**Nothing below is owner-accepted yet.** Automation, prepared tools and
cross-builds are separate from desktop/hardware/host/game/print acceptance.
After reviewing your observations together, mark only the demonstrated parts
complete; retain failed, blocked, not-run and parked items. This is the known
list, not a claim that every earlier request has been recovered.

This execution batch prepares Weather/Orca review tooling and repairs stale HUD
handoff guidance. It does not change the seven shipped EXEs, plugin algorithms,
lighting files or HUD renderer. No new app install is needed just for the tools.

## Record once for each test session

- Selected Hub branch and its `.agent/RELEASED` commit; installed Hub version
  (expected product version **2.8.70**), Windows version, screen/DPI/theme.
- Installed app/plugin/JAR identity, not just current Git tip. Undahang's public
  version is **0.0.52**; Infill **0.4.10**. Orca intentionally still calls the
  underlying package/capabilities **Wave Overhangs**.
- For each item: **PASS / FAIL / BLOCKED / NOT RUN**, exact action, expected vs
  observed result, build/environment and a reviewed screenshot/log if relevant.
  Keep precise locations, security history, user paths and server data private;
  raw owner evidence must not enter public Git/Drive.

## Priority 0 — preserve the gates

### APP-01: collect evidence; do not retry blocked Guard

The existing screenshot already establishes Windows error 225 for Bluetooth
Guard and no UAC prompt. **Do not send the same screenshot again.** What is
still missing is the actual blocking provider, detection/rule, action/time,
affected file and installed-file provenance. A 225 error is not a malware or
false-positive verdict.

Use the actual antivirus/Windows Protection history, or optionally the published
read-only `tools/guard_security_report.ps1` under the existing ordinary,
unelevated script policy. Follow
[`test-hub-open-to-front.md`](test-hub-open-to-front.md) for the collector and
private report review. Its frozen Hub baseline intentionally predates 2.8.70:
a changed Hub hash is not automatically suspicious or a clean verdict.

If policy/security refuses collection, stop and use the History UI. **No
Allow/Restore, exclusions, protection disable, policy bypass, elevation changes,
renaming/repackaging/rebuilding to evade detection, or Guard launch/update for
testing.** Resolve evidence/provenance and the legitimate vendor-review path
first. Guard-dependent BT-01 and Open-to-front tests wait on that prerequisite.
You can still do the independent Hub/Weather checks below.

### HUD-01: don't test the wrong methods/build

The original **Classic Affine / Captured Mesh / World-Space Texture** request is
retained. Current source instead forces **Purple Horizontal Panel**; the old
choices cannot be tested as requested. No methods were restored in this batch.
A purple-panel smoke pass cannot close the original request. Identify the
actual branch JAR and reconcile this scope mismatch before marking HUD-01 off;
see [`test-spatial-hud-f5w.md`](test-spatial-hud-f5w.md).

## Priority 1 — useful checks that do not require Guard

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

## Priority 2 — conditional hardware/host/game checks

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

## Priority 3 — monitoring, missing evidence and parked work

- **OPS-01:** if UAC recurs on Hub open/app open/update, record the prompting
  EXE/path/action. Earlier UAC reports were uncertain; Guard's later 225 case
  had no prompt. Do not infer a shared cause or change elevation speculatively.
- **KBL-01:** provide a **fresh `engine.log`** for the current lighting issue
  before lighting work/own-app Prism branding. No lighting code changed.
- **PLAN-01:** review the known list for missing earlier requests; add omissions
  rather than assuming this audit recovered unsaved conversations.
- **ORCA-04 / ORCA-05:** wake/seam blending and smoother curved-boundary
  research stay parked. No new coding approval is inferred.
- **PLAN-02:** first optimization is shipped; further caches/workers stay
  deferred. No new owner performance acceptance is requested for this batch.

## Signoff ledger — eight categories, all 18 IDs retained

| Category | ID | Owner acceptance / next evidence |
| --- | --- | --- |
| Bluetooth | BT-01 | Basic connection confirmed; remaining checks OPEN after APP-01 clearance |
| Hub/Windows | APP-01 | **NEXT safety evidence**: provider/detection/action/installed provenance, then foreground |
| Hub/Windows | APP-02 | OPEN: longevity and original-expiry soak |
| Hub/Windows | APP-03 | **NEXT unblocked desktop check**: header/icons/layout |
| Hub/Windows | OPS-01 | OPEN: separate UAC monitoring |
| Weather | WX-01 | OPEN: installed planner review; component kit prepared, not natively run |
| Weather | WX-02 | OPEN: actual live-map/place alignment; component kit prepared, not natively run |
| Weather | WX-03 | OPEN: Windows permission/provider and actual-position acceptance |
| Lighting | KBL-01 | PARKED: fresh owner engine.log required |
| Spatial HUD | HUD-01 | OPEN: original three-method scope mismatch; current-panel F5W evidence separate |
| Orca | ORCA-01 | OPEN: actual effective configuration and fresh curved-wall control |
| Orca | ORCA-02 | OPEN: fresh exports and physical print |
| Orca | ORCA-03 | OPEN: independent real-host plugin execution |
| Orca | ORCA-04 | PARKED: wake/seam research, no coding approval |
| Orca | ORCA-05 | PARKED: curved-boundary research, no coding approval |
| Naming | NAME-01 | Rollout shipped; OPEN native acceptance / lighting log gate |
| Planning | PLAN-01 | Known-list audit done; OPEN owner completeness review |
| Planning | PLAN-02 | First optimization shipped; further caching/workers DEFERRED |

Suggested reply: `APP-03 — PASS/FAIL/BLOCKED/NOT RUN — build + DPI + action + evidence`.
Continue by ID; we mark acceptance only after reviewing what was actually tested.
