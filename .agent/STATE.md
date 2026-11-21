# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Updated:** 2026-11-16. Git: code/tests/releases; Drive: extended plans/history.

## Released work

- **Hub:** main 2.8.56; Arena 2.8.66. APP-02 (2.8.64) repairs draw-locked
  icon ownership and shell recovery; original expiry/native soak unconfirmed.
  APP-01 (2.8.63, `47b00a3`) restores/foregrounds windows, preserves modal/max
  state and prevents duplicates; native focus pending. asInvoker, child elevation,
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

Prior approval release `83e54d5`, Hub 2.8.62; native earbuds acceptance open.
Owner: Guard OFF / Auto ON earbuds fail connection/status while phone connects.
Released correction `9c91afd` (Hub still 2.8.66): saved review without trust,
retain missing metadata, adapter
IMMDevice activation of IKsControl, 5..60s audio retries independent of Guard,
explicit-Disconnect pause, actual audio status separate from ACL. Preserve K/D/F,
enabled profiles and address-only routing. Windows hardware/LE-only unproven;
checklist: `test-bluetooth-reconnect.md`. Legacy PS unchanged.
Cross-build/checks, 129 tests/291 mutations pass; 4 ASan/UBSan cases pass.

## Agreed to-do list

Full plan: **Tests — Master To-Do List**, `.agent/DRIVE_GUIDE.md`.
Repeat pending/parked items; omissions remain OPEN.

- **Weather Radar:** WX-03 one-shot Windows location implemented, permission,
  accuracy/errors, no tracking. Build/checks/199 mutations pass; actual-position
  Windows acceptance pending. WX-01/02 native planner/live-map review unrun;
  workflow-write permission blocks the parked review draft.
- **Hub / Windows:** APP-02 two reproduced lifetime defects fixed in 2.8.64;
  Windows soak/original expiry still open. APP-01 native focus pending.
  OPS-01 monitor: latest open fine; identify UAC program/path/action if repeated
  (reports: Hub open, app open, app update). APP-03 released 2.8.65 (`b16380c`):
  General apps / Orca plugins / Minecraft mods; independent Undahang / Unlayered
  Infill, settings/legacy paths preserved. Approved abstract Undahang icon in
  2.8.66 (`fd27fa3`); Infill art unchanged. Build/checks, 124 tests/254 mutations pass.
  Native fitting/icons unrun (`test-hub-grouped-entries.md`).
- **Bluetooth Guard:** BT-01 active/next: Guard OFF / Auto ON earbuds discovery,
  status/reconnect vs working phone; choices/persistence and first hardware check.
  Prior repairs are not acceptance. Scans never grant trust.
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
