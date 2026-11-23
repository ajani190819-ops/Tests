# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
Git: implementation/tests/releases; Drive: extended plans/history.

## Current owner status / next work

Owner reports UAC prompts, Lighting and startup seem fixed for current use.
The prior silent Lighting failure is history, not cause/build proof. No new
Guard-specific outcome, repeated logs or full matrix; no permanent/native safety
certification is inferred.

**Current focus / monitor (20 retained IDs):**
- **PLAN-01 CURRENT:** menus owner-reported good; prints pending. Review preview
  without manual export/reopen; renumbering paused. All controls visible; approved
  process diffs only, printer/filament read-only, relative E advisory, no autosave.
  `docs/ORCA-SETTINGS-SETUP.md`; host version unknown.
- **APP-01 Owner-reported good:** Lighting Open/current use now fine; prior
  "nothing visible" is history. Hub 2.8.72 offscreen correction/458 mutations
  do not establish that cause. No new Guard outcome. `test-hub-open-to-front.md`.
- **APP-05 MONITOR / Owner-reported good:** startup currently working; no lasting
  startup fix claimed. Preserve fallbacks/preferences; `test-hub-startup-tray.md`.
- **OPS-01 Owner-reported good:** UAC now seems fixed; prior prompt identity
  remains unknown, not a security-policy diagnosis or permission to change it.
- **KBL-01 Owner-reported good:** Lighting generally fine; physical M4/M5 hold
  not separately retested. Fresh `engine-6.log` received/scanned.
  No additional log request. Keep ASCII/C# 5.0, pacing, exclusive-lock/single-engine.
  `test-lighting-input.md`.

## Released / owner-reported good

- **Hub:** Arena 2.8.72 (main 2.8.56). APP-02 longevity and APP-03 grouping,
  icons/header are now owner-reported good; no detailed new soak is claimed.
  Preserve modal/maximized placement, single-instance and safe update gates.
- **BT-01:** current use owner-reported good. Original Guard OFF + Auto ON
  earbuds/Earpods-versus-phone and approval/manual/persistence cases are retained
  regression references. Keep K/D/F, address-only actions, enabled profiles,
  independent 5..60s retries and honest status colours. Pause/button wording
  finding remains recorded, not fixed or newly prioritized.
- **APP-04:** standalone AppReview 0.1.0, no target input/config/launch changes;
  two Hub/Guard images inspected at 120 DPI. Owner reports current use good.
  Separate native fixture remains manual/unrun; CI hook deferred. No claim
  that broad approval executes fixtures. `test-app-review.md`.
- **WX-01/02/03:** planner/map/position owner-reported good; no new provider
  measurement or native kit run. Keep exact-point odds, attribution/cache/staging.
- **HUD-01:** current experience owner-reported good. Source forces Purple Panel,
  not the original Classic Affine/Captured Mesh/World-Space Texture. Do not claim
  those restored/tested or reopen scope without a decision. `test-spatial-hud-f5w.md`.
- **ORCA-01/02/03:** owner-reported good; no new exports/host callbacks/print
  claimed. T2/T3 contain Infill 0.4.8 rewrites, not untouched controls. Keep
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
