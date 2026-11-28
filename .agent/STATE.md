# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
Git: implementation/tests/releases; Drive: extended plans/history.

## Current owner status / next work

Save and the released M4 fix are owner-confirmed working.
CURRENT: residual gradient flicker; brightness/control audit. Open/startup good.

**Current focus / monitor (20 retained IDs):**
- **PLAN-01 CURRENT:** owner confirms Save fixed; no more Config collection.
  Wave0.0.58 default/fallback0.5, independent S127 cap. Old0.0.53 output is
  historical; wall/curl acceptance open. Orca2.5.2/custom Ender3/PLA205C/Auto/
  12V; PWM is not voltage conversion. NEXT after Prism repairs: processed preview
  (native preferred/separate acceptable); renumbering paused. Controls visible,
  process-only approval; printer/filament read-only, E manual/no autosave.
  `docs/ORCA-SETTINGS-SETUP.md`.
- **APP-01 Owner-reported good:** Lighting Open fine; prior "nothing visible"
  is history. Hub 2.8.72 offscreen/458 mutations do not establish that cause.
  `test-hub-open-to-front.md`.
- **APP-05 MONITOR / Owner-reported good:** startup currently working; no lasting
  startup fix claimed. Preserve fallbacks/preferences; `test-hub-startup-tray.md`.
- **OPS-01 Owner-reported good:** UAC seems fixed; prior prompt unidentified,
  not a security-policy diagnosis/change approval.
- **KBL-01 CURRENT:** M4 fix owner-confirmed working. No additional log request
  or old matrix for M4. Logs optional; Steady retained. Armoury research recorded.
  Precision/control audit next; `29e6b5d` unchanged. Balance/Steady and Tray audit open.
  Keep choices, distinct keys, release/lock/pacing/single-engine and ASCII/C#5.
  `test-lighting-input.md`.

## Released / owner-reported good

- **Hub:** Arena 2.8.72 (main 2.8.56). APP-02 longevity/APP-03 grouping/icons
  owner-reported good, not a new soak. Keep modal/maximized placement,
  single-instance and safe update gates.
- **BT-01:** current use good. Keep original Guard OFF + Auto ON
  earbuds/Earpods-versus-phone, approval/manual/persistence references; K/D/F,
  address-only actions, enabled profiles, independent 5..60s retries, honest
  colours. Pause/button wording recorded, not fixed or newly prioritized.
- **APP-04:** AppReview 0.1.0; no target input/config/launch changes. Two
  Hub/Guard images reviewed at 120 DPI; current use good. Separate native
  fixture manual/unrun, CI hook deferred; approval is not fixture execution.
  `test-app-review.md`.
- **WX-01/02/03:** planner/map/position owner-reported good; no new provider
  measurement or native kit run. Keep exact-point odds, attribution/cache/staging.
- **HUD-01:** current experience owner-reported good. Source forces Purple Panel,
  not the original Classic Affine/Captured Mesh/World-Space Texture. Do not claim
  those restored/tested or reopen scope without a decision. `test-spatial-hud-f5w.md`.
- **ORCA-01/02/03:** prior approvals retained; new Wave feedback above does not
  certify callbacks/exact output. T2/T3 have Infill 0.4.8 rewrites, not controls. Keep
  independent plugins/APIs. ORCA-04/05 research stays PARKED, not approved.
- **NAME-01:** naming rollout owner-reported good (Hub Weather Report/Prism
  Lighting, Undahang 0.0.52); own lighting branding unchanged. Keep Wave
  package/capabilities, preset paths, aliases and Infill/Guard names stable.
- **PLAN-01:** known list owner-reviewed; add APP-05, retain all prior IDs.
  **PLAN-02:** first optimization shipped (8.14x); further caches/workers
  DEFERRED. No owner benchmark requested.

## Retained safety / delivery facts

- Historical Guard Windows 225/no-UAC evidence is not a malware/false-positive
  verdict or proof of safe resolution. Do not retry blocked launch, Allow/Restore,
  exclude, disable security, repackage to evade detection, bypass policy or elevate
  to obtain a test. `tools/guard_security_report.ps1` remains read-only/unelevated.
- Agent Notes: Saved=receiver flush; Read=later checkpoint. Opt-in sound;
  user-opened only, no startup persistence. Keep Notes receipt/wait protocol.
- `shared/win32/suite_emblem.c` is the sole native Tri-Spoke renderer;
  raw DIBs use `SuiteEmblemDibPixel`, each consumer releases its cache.
- `.agent/RELEASED` moves only in a second commit blessing verified work;
  download that SHA. Current apps never offer Update. Close/prove stopped
  before replacing EXEs. No main merge/force-push/workflow-permission bypass.
- Behavior edits need regressions/mutations, build/check before every commit;
  regenerate CODEMAP after mapped C/Python changes. HUD build requires JDK 25.
- Keep Drive Guide/Master synced, no source/raw-owner-evidence mirrors. The
  owner checklist's older matrices are reference, not a request to rerun all.
  Context <=15,000 chars; map/Drive on demand.
