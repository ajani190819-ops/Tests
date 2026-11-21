# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Updated:** 2026-11-16. Git: code/tests/releases; Drive: extended plans/history.

## Released work

- **Hub:** main 2.8.56; Arena 2.8.69. APP-02 lifetime repairs released;
  expiry/native soak open. APP-01 Guard Start fails without UAC; cause unknown.
  Retain modal/max/no-duplicate, asInvoker, child-elevation, user-opened Notes
  and stop/update gates.
- **Agent Notes:** Saved=receiver flush; Read=later checkpoint. Visual wait/
  opt-in sound; no startup persistence.
- **Tri-Spoke:** `shared/win32/suite_emblem.c` is the sole native renderer.
  Raw DIBs use `SuiteEmblemDibPixel`; each consumer releases its cache.
- **Radar:** rain watch `1be38f4`/`59794dc`, 66%+ point odds in 3h, persisted
  episode/cooldown; regional coverage never alerts. Planner: Hub 2.8.62.
  Alignment `36c55bd`/`d97ba63` rejects old-place tiles, locks through blit.
  Native/live review pending.

## Bluetooth Guard / BT-01

Owner reports basic connection now works after `9c91afd`/`7740af8` (Hub 2.8.66).
Hub 2.8.67 + Guard: own-UI restore, show-only per-window UIPI message,
fresh scalar ticks; elevation/legacy async/duplicate/update gates retained. Green=confirmed connected, blue=paired/Auto/
link-only/request; separate Auto on/off/paused/details, approval amber, denied red.
Preserve K/D/F, address-only actions, profiles and 5..60s Guard-independent retries.
Native focus/DPI and persistence/manual/lifecycle/LE checks remain open:
`test-bluetooth-reconnect.md` / `test-hub-open-to-front.md`. Legacy PS unchanged.

## Agreed to-do list

Full plan: **Tests — Master To-Do List**, `.agent/DRIVE_GUIDE.md`.
Repeat pending/parked items; omissions remain OPEN.

- **Weather Radar:** WX-03 one-shot Windows location implemented, permission,
  accuracy/errors, no tracking. Build/checks/199 mutations pass; actual-position
  Windows acceptance pending. WX-01/02 native planner/live-map review unrun;
  workflow-write permission blocks the parked review draft.
- **Hub / Windows:** APP-02 two reproduced lifetime defects fixed in 2.8.64;
  Windows soak/original expiry open. APP-01 active: 2.8.69 reports
  app/Windows launch code/path; no retry/elevation change. Cause unknown.
  Original 137-test/335-mutation release gate passed; PLAN-02 later corrected
  six false-positive mutation kills. Exact owner error is next.
  OPS-01 monitor: identify UAC program/path/action if repeated (Hub open,
  app open/update). APP-03: General apps / Orca plugins / Minecraft mods;
  independent Undahang / Unlayered Infill, stable settings/paths. Approved
  Undahang icon in 2.8.66; Infill art unchanged. Header 2.8.68 aligns controls,
  wraps when narrow. Native header/icons/fitting pending
  (`test-hub-grouped-entries.md`).
- **Bluetooth Guard:** BT-01 pending: panel reveal/status confirmation. Basic
  connection owner-confirmed; choices/persistence/manual/lifecycle checks remain.
  Scans/review rows never grant persisted trust.
- **Keyboard Lighting:** KBL-01 parked; fresh owner `engine.log` required.
- **Spatial HUD:** HUD-01 three-method F5W runtime/compatibility checks pending.
- **Orca:** ORCA-01/02/03 historical follow-ups to confirm: actual-config curved
  walls, fresh export/physical print, real-host dual-plugin flow. ORCA-04/05
  parked research: wake/seam blending without coverage/fragment regressions,
  smoother curved-wall perimeter-following rung; not new coding approval.
- **Naming:** NAME-01 confirmed/queued: Weather Radar -> Weather Report;
  Wave Overhangs -> Undahang; Keyboard Lighting -> Prism Lighting. Unlayered
  Infill/Bluetooth Guard stay unchanged; not Bluetooth Bouncer. Public labels only;
  preserve internal IDs/paths and fresh-log gate. APP-03 applies only the Undahang
  Hub label; wider public-name rollout is still queued.
- **Planning:** PLAN-01 missing-request recovery open. PLAN-02 first slice
  verified: timing/fail-fast/335 explicit routes, clean 169-test gates, exact
  restoration; 335 detected, 0 survived/stale/errors in 132.622s. Same-scope
  benchmark 1199.910 -> 147.385s (8.14x), all 335 verdicts equal. Six old
  coverage gaps repaired; no verdict cache/app changes. Artifact reuse/isolated
  workers deferred; `.agent/runbooks/mutation-runner.md`.

## Offline safety / efficiency

- Move `.agent/RELEASED` only in a second commit blessing verified finished work;
  download that SHA, not an unfinished tip. Current apps never offer Update.
- Close and prove managed targets stopped before replacing EXEs.
- Preserve Radar attribution/cache, generation-gated staging, source zoom <=7,
  local display 8-10 crops and a lock through the published-view blit.
- Behavior edits need regressions/mutations. Build/check before commits;
  regenerate CODEMAP after mapped C/Python layout changes.
- Default context <=15,000 chars; map/Drive on demand. HUD: JDK 25 (CI).
- Keep Drive plan/Guide synced; no source/raw-output mirrors.
