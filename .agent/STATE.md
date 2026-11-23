# Repository Current State

**Active branch:** use `git rev-parse --abbrev-ref HEAD`; never hardcode it.
Git: implementation/tests/releases; Drive: extended plans/history.

## Current owner status / next work

Owner reports current use good except lighting, UAC, intermittent Hub Open and
startup tray consolidation. Do not demand the old full matrix again. This is
broad owner approval, not new fixture/security/print evidence, implementation
of missing HUD methods, parked-work approval or recovery of unsaved requests.
Shorthand `2/3. kindarandom/ all` leaves UAC identity/Hub presence unresolved.

**Four active problems (20 retained IDs total):**
- **APP-01 NEXT:** intermittent Hub Open, not just the old Guard error. Need
  affected app/result. Shared card/tray repair must preserve no duplicates,
  focus freshness and elevation. `test-hub-open-to-front.md`.
- **APP-05 NEXT (new):** startup exposes all app icons instead of one Hub icon.
  Is Hub running then? Only a usable Hub may suppress fallbacks; preserve their
  recovery and startup choices, no new Run/task/service/auto-start.
  `test-hub-startup-tray.md`: source audit and unknowns.
- **OPS-01:** UAC remains an issue; prompting EXE/path/action unknown. Keep Hub
  asInvoker and child manifests; no speculative elevation/security change.
- **KBL-01 NEXT:** fresh `engine-6.log` received/scanned: shared-write/timing
  evidence, no input diagnostics/cause proof. Receiver enumeration guard and
  bounded local `input-status.txt`; hardware hold unverified. `test-lighting-input.md`.
  Keep ASCII/C# 5.0, pacing, exclusive-lock and single-engine safeguards.

## Released / owner-reported good

- **Hub:** Arena 2.8.70 (main 2.8.56). APP-02 longevity and APP-03 grouping,
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
  **PLAN-02:** first optimization shipped (matched 8.14x); further caches/workers
  DEFERRED. No new owner benchmark requested.

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
  Default context <=15,000 chars; map/Drive on demand.
