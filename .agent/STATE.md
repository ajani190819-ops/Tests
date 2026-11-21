# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Updated:** 2026-11-16. Git: code/tests/releases; Drive: extended plans/history.

## Released work

- **Hub:** main 2.8.56; Arena 2.8.67. APP-02 (2.8.64) repairs draw-locked
  icon ownership and shell recovery; original expiry/native soak unconfirmed.
  APP-01 Open reported failing again; Guard follow-up below. Retain modal/max
  placement and duplicate prevention. asInvoker, child elevation,
  user-opened Notes and stop/update gates unchanged.
- **Agent Notes:** `ad92320`, marker `5343e54`. Saved=receiver flush;
  Read=later checkpoint. Visual wait/opt-in sound; no startup persistence.
- **Tri-Spoke:** `shared/win32/suite_emblem.c` is the sole native renderer.
  Raw DIBs use `SuiteEmblemDibPixel`; each consumer releases its cache.
- **Radar:** rain watch `1be38f4`/`59794dc`, 66%+ point odds in 3h, persisted
  episode/cooldown; regional coverage never alerts. Planner: Hub 2.8.62.
  Alignment `36c55bd`/`d97ba63` rejects old-place tiles, locks through blit.
  Native/live review pending; release history in Drive.

## Bluetooth Guard / BT-01

Owner reports basic Bluetooth connection now works after `9c91afd` (marker
`7740af8`, Hub 2.8.66). Remaining persistence/manual/lifecycle/LE checks stay open.
Released `1839e1e`, Hub 2.8.67 + Guard: own-UI Hub/tray reveal, one per-window
show-only UIPI message, fresh scalar ticks, no elevation change;
legacy async/duplicate/update gates retained. Green=confirmed connected,
blue=paired/Auto-configured/link-only/request; separate Auto on/off/paused and
full-width details. Approval amber, denied red; no request=audio success.
Preserve K/D/F, address-only actions, enabled profiles and 5..60s Guard-independent
retries. Build/checks, 136 tests/320 mutations and ASan/UBSan pass; focus/DPI unrun:
`test-bluetooth-reconnect.md` and `test-hub-open-to-front.md`. Legacy PS unchanged.

## Agreed to-do list

Full plan: **Tests — Master To-Do List**, `.agent/DRIVE_GUIDE.md`.
Repeat pending/parked items; omissions remain OPEN.

- **Weather Radar:** WX-03 one-shot Windows location implemented, permission,
  accuracy/errors, no tracking. Build/checks/199 mutations pass; actual-position
  Windows acceptance pending. WX-01/02 native planner/live-map review unrun;
  workflow-write permission blocks the parked review draft.
- **Hub / Windows:** APP-02 two reproduced lifetime defects fixed in 2.8.64;
  Windows soak/original expiry still open. APP-01 Open-to-front active.
  OPS-01 monitor: latest open fine; identify UAC program/path/action if repeated
  (reports: Hub open, app open, app update). APP-03 released 2.8.65 (`b16380c`):
  General apps / Orca plugins / Minecraft mods; independent Undahang / Unlayered
  Infill, settings/legacy paths preserved. Approved abstract Undahang icon in
  2.8.66 (`fd27fa3`); Infill art unchanged.
  Native fitting/icons unrun (`test-hub-grouped-entries.md`).
- **Bluetooth Guard:** BT-01 active/next: panel reveal and status visuals. Basic
  connection owner-confirmed; choices/persistence/manual/lifecycle checks remain.
  Scans never grant trust.
- **Keyboard Lighting:** KBL-01 parked for fresh owner `engine.log`;
  `engine-5.log` showed learning/no key events. No engine changes without logs.
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
- **Planning:** PLAN-01 missing-request recovery open.

## Offline safety / efficiency

- Move `.agent/RELEASED` only in a second commit blessing verified finished work;
  download that SHA, not an unfinished tip. Current apps never offer Update.
- Close and prove managed targets stopped before replacing EXEs.
- Keep Bluetooth profiles enabled; K/D/F distinguish trusted/denied/forgotten.
  Review candidates are not persisted as trust.
- Preserve Radar attribution/cache, generation-gated staging, source zoom <=7,
  local display 8-10 crops and a lock through the published-view blit.
- Behavior edits need regressions/mutations. Build/check before commits;
  regenerate CODEMAP after mapped C/Python layout changes.
- Context: AGENTS/STATE/execution/topic <=15,000 chars; map/Drive on demand.
  HUD needs JDK 25 (in CI).
- Keep Drive plan/Guide synced from verified Git; no source/raw-output mirrors.
