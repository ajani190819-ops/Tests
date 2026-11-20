# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-16. Git is canonical for code, tests and releases;
Drive holds extended plans/history.

## Released work

- **Hub:** main 2.8.56; Arena 2.8.64 (implementation in RELEASED). APP-02 fixes
  catalogue icon-handle growth with draw-locked replacement and rechecks/restores
  lost shell icons; failed registration releases tray ownership. Build/checks/
  213 mutations pass; native soak/original expiry unconfirmed. APP-01 (2.8.63,
  `47b00a3a3c29a17c9c6aaca575f190b5bb543206`) reveals/restores windows, preserves
  maximized/modal state and prevents duplicate starts; native focus review pending.
  asInvoker, child elevation, user-opened Notes and stop/update gates unchanged.
- **Agent Notes:** `ad9232009b9f38c287727123ddd404b204ce26e1`, marker `5343e54`.
  Saved=receiver flush; Read=later checkpoint. Native visual wait, opt-in sound;
  no Run key/task/service.
- **Tri-Spoke:** `shared/win32/suite_emblem.c` is the sole native renderer.
  Raw DIBs use `SuiteEmblemDibPixel`; each consumer releases its cache.
- **Radar rain watch:** `1be38f40f8612835ac539b9958528231da1c854b`, marker
  `59794dc`. Alert at 66%+ exact-point ensemble odds in 3h only; persisted
  episode/cooldown suppresses repeats. Regional coverage never alerts.
- **Radar planner:** `d483a5629db77a4428d57c1c870489a3e9eee5d2`, Hub 2.8.62.
  Five date-first rows, labeled hourly detail; point odds distinct from regional
  PoP. Native review pending.
- **Radar alignment:** `36c55bda541b22fc34048e5fb87f7dba7aa27c99`, marker
  `d97ba63`. Reject old-place tiles; lock the published view through its blit.
  No prior city under a new marker. Build/checks/156 mutations passed;
  native/live review pending.

## Bluetooth approval release

`83e54d5ed8648856f7dbbfa5786096d36ab8ce8e`, Hub 2.8.62. Paired rows offer
Accept/Deny/Auto-Connect. Undecided pairs stay reviewable without trust; Deny
keeps pairing. Actions/drops recheck addresses. Build/checks/164 mutations pass
(0 surviving/stale); real-earbud Windows check pending. Legacy PS unchanged.

## Agreed to-do list

Full plan: **Tests — Master To-Do List** in `.agent/DRIVE_GUIDE.md`.
Repeat pending/parked items in replies; owner-reported omissions remain OPEN.

- **Weather Radar:** WX-03 one-shot Windows location implemented, permission,
  accuracy/errors, no tracking. Build/checks/199 mutations pass; actual-position
  Windows acceptance pending. WX-01/02 native planner/live-map review unrun;
  workflow-write permission blocks the parked review draft.
- **Hub / Windows:** APP-02 two reproduced lifetime defects fixed in 2.8.64;
  Windows soak/original expiry still open. APP-01 native focus pending.
  OPS-01 monitor: latest open fine; identify UAC program/path/action if repeated
  (reports: Hub open, app open, app update). APP-03 next queued build: General apps /
  Orca plugins / Minecraft mods, with separate Undahang and Unlayered Infill entries.
- **Bluetooth Guard:** BT-01 released; first owner hardware check: earbuds
  discovery, choices, persistence/reconnect. Scans never grant trust.
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
  preserve internal IDs/paths and fresh-log gate. No renames applied.
- **Planning:** PLAN-01 categorized master; missing-request recovery open.

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
- Default context: AGENTS, STATE, execution, needed topic; four files <=15,000
  chars. CODEMAP/Drive/history on demand. HUD needs JDK 25 (available in CI).
- Keep Drive plan/Guide synced from verified Git; no source/raw-output mirrors.
