# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Updated:** 2026-11-16. Git: code/tests/releases; Drive: extended plans/history.

## Released work / verified tooling

- **Hub:** main 2.8.56; Arena 2.8.70. APP-02 lifetime repairs released;
  original expiry/native soak open. Retain modal/max/no-duplicate, asInvoker,
  child-elevation, user-opened Notes and stop/update gates.
- **Agent Notes:** Saved=receiver flush; Read=later checkpoint. Opt-in sound;
  no startup persistence.
- **Tri-Spoke:** `shared/win32/suite_emblem.c` is the sole native renderer.
  Raw DIBs use `SuiteEmblemDibPixel`; each consumer releases its cache.
- **Review batch:** Weather kit cross-build, 24 portable tests, seven-EXE build,
  full uncached checks and 377 mutations pass; 208-test clean gates, restored,
  151.364s. Seven EXEs/app/plugin/HUD source unchanged; no native acceptance.

## Bluetooth Guard / BT-01

Basic connection owner-confirmed after `9c91afd`/`7740af8` (2.8.66).
2.8.67 + Guard: own-UI restore, show-only UIPI message, fresh scalar ticks;
elevation/async/duplicate/update gates retained. Green=confirmed connected,
blue=paired/Auto/link-only/request; separate Auto, amber approval, red denied.
Keep K/D/F, address-only actions, profiles, 5..60s Guard-independent retries.
Focus/DPI, choices/persistence/manual/lifecycle/LE open. Original Guard OFF +
Auto ON earbuds/Earpods failure versus working phone under identical settings
is separate from missing rows/pre-decision drops/no Auto row. Guard tests wait
for APP-01 clearance. `test-bluetooth-reconnect.md` / `test-hub-open-to-front.md`;
legacy PS unchanged.

## Agreed to-do list

Full plan: **Tests — Master To-Do List**, `.agent/DRIVE_GUIDE.md`.
Owner sequence: `.agent/runbooks/owner-verification-checklist.md`.
All 18 IDs retained; acceptance after joint review, omissions OPEN.

- **Weather:** WX-03 one-shot Windows permission/accuracy/errors, no tracking;
  actual-position acceptance OPEN. WX-01/02 native/live/desktop unrun.
  Standalone kit: HTTP-refused fixtures, opt-in live, not CI-integrated.
  `test-weather-radar-windows.md` / `test-weather-location.md`.
- **Hub / Windows:** APP-01 Guard 225 virus/PUA block, no UAC. Read-only
  `tools/guard_security_report.ps1`: provider/detection/action + installed hash
  next, not another screenshot. No safety/false-positive verdict, Guard retry,
  allow/restore/exclusion/AV disable, repackaging, policy bypass or elevation
  change. APP-02 2.8.64 repairs shipped; original expiry/soak open. APP-03
  grouping, independent plugins/approved icons and 2.8.68 fitting header/wrap
  shipped; native layout open (`test-hub-grouped-entries.md`). OPS-01 monitor
  UAC program/path/action on Hub open/app open/update; do not guess.
- **Bluetooth:** BT-01 basic success; rest above. Reviews never grant trust.
- **Lighting:** KBL-01 parked; fresh owner `engine.log` required.
- **Spatial HUD:** HUD-01 original Classic Affine/Captured Mesh/World-Space
  Texture request OPEN. Current source forces Purple Horizontal Panel; no old
  three-method selectors/fixed 30-degree gate. Owner scope reconciliation needed;
  current-panel smoke separate, not completion (`test-spatial-hud-f5w.md`).
- **Orca:** ORCA-01 actual-config curved walls, ORCA-02 fresh export/physical
  print, ORCA-03 real-host independent plugins OPEN. T2 `clean.gcode` and T3
  already contain Infill 0.4.8 rewrites, not untouched controls; effective plugin
  settings missing. Read-only inspector/procedure: `test-orca-real-host.md`.
  ORCA-04/05 wake/seam and smoother curved-boundary research PARKED, no approval.
- **Naming:** NAME-01 shipped: Hub 2.8.70 Weather Report/Prism Lighting,
  Undahang 0.0.52. Wave package/capability/preset APIs, paths, shortcut aliases
  and Infill/Guard names stay stable. Native review OPEN (`test-public-names.md`);
  lighting own files unchanged/log-gated.
- **Planning:** PLAN-01 known-list audit done; owner completeness review OPEN.
  PLAN-02 first slice shipped: 335 detected/restored, matched 8.14x speedup,
  identical verdicts; six old gaps fixed, no verdict cache/app changes.
  Further reuse/workers deferred (`mutation-runner.md`).

## Offline safety / efficiency

- Move `.agent/RELEASED` only in a second commit blessing verified finished work;
  download that SHA, not unfinished tip. Current apps never offer Update.
- Close and prove managed targets stopped before replacing EXEs.
- Preserve Weather attribution/cache, generation-gated staging, source zoom <=7,
  local display 8-10 crops and lock through published-view blit.
- Behavior edits need regressions/mutations; build/check before every commit;
  regenerate CODEMAP after mapped C/Python layout changes.
- Default context <=15,000 chars; map/Drive on demand. HUD build needs JDK 25.
- Keep Drive plan/Guide synced; no source/raw-owner-evidence mirrors.
