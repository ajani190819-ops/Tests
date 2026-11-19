# Repository Current State

**Active branch:** read with `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-16. Git is authoritative for implementation, tests,
release records, and compact offline safety facts. Drive holds extended narrative.

## Released work

- **Main: Project Hub 2.8.56:** Tri-Spoke tray, alerts, and default-on branch
  discovery remain. Remote probes pin releases and show updates only when app
  contents differ; `Update Hub` requires a newer build.
- **Arena Project Hub 2.8.62** (`81eb2c1a1822fcb7fe76118df5dccaef676c3495`):
  Hub runs asInvoker; child elevation remains app-owned. Agent Notes stays
  user-opened; elevated `--stop` precedes updates and the managed-copy gate
  remains. Tray and Settings use Soft Slate. Tray rows retain full-row repaint
  and GDI isolation; the popup reentry guard remains. The owner now confirms
  hover is fine on Windows. Weather Radar had been appended only after a
  successful remote catalog fetch, so startup/offline/partial-catalog states
  could omit it. It is now seeded at startup and restored after catalog rebuilds,
  with an embedded icon and a reserved catalog slot. Owner confirms the card is
  visible; regression, mutations, build, and repository checks pass. Main stays
  2.8.56.
- **Agent Notes waiting receipts:** implementation `ad9232009b9f38c287727123ddd404b204ce26e1`,
  released by marker `5343e54`. The direct local outbox records **Saved** only
  after the live receiver flushes `.agent/INBOX.md`, then **Read** only after an
  agent checkpoint captured before that inbox read. A running app hides to the
  Windows notification area and shows a native visual "agent is waiting" alert;
  sound is persistent opt-in (default off), and no Run key/task/service is added.
- **Shared Tri-Spoke engine:** `shared/win32/suite_emblem.c` is the sole native
  renderer. Consumers use `SuiteEmblemDibPixel` for raw DIBs, own placement,
  and release their cache.
- **Weather Radar rain watch:** `1be38f40f8612835ac539b9958528231da1c854b`,
  marker `59794dc`. Tray alerts require **66%+ exact-point ensemble odds in 3h**;
  persisted episode state and cooldown suppress repeats. Regional odds never alert.
- **Weather Radar planner presentation:** implementation
  `d483a5629db77a4428d57c1c870489a3e9eee5d2`, Hub 2.8.62. Five date-first
  rows align rain, high/low temperature, clouds, and wind. Selection opens a
  larger hourly chart with time/value labels and a reading. Point odds still
  require complete member-days and stay distinct from regional PoP; humidity
  is supporting. Build/checks and all 153 mutations pass; native Windows visual
  review remains.
- **Weather Radar zoom:** implementation
  `63eae24853648a51a75dc9fe1a66426fc0aee642`, Hub 2.8.62. Source requests cap
  at level 7; local zoom crops center on the selected place. Build/checks and
  all 154 mutations pass.
- **Documentation archive:** 29 human-facing files remain in Drive; Git keeps
  compact stubs, changelogs, tests, configuration, version anchors, and runtime assets.

## Efficiency system and completed audit

- Default agent orientation is `AGENTS.md`, this state file, and
  `.agent/rules/execution.md`, plus a topic file only when needed. Generated
  CODEMAP, Drive narrative, history, and topic facts are on demand.
- `tools/check_context_budget.py`, run by `tools/check_all.py`, caps four
  orientation files at 15,000 characters and forbids a mandatory CODEMAP read.
- Audit passes Python, shell, Windows build, checks, mutations, and artifacts.
  Spatial HUD needs JDK 25, unavailable in sandbox but present in CI.

## Next approved work

Windows review of the planner and place-centered radar zoom/live tiles remains.
UAC is deferred. Keyboard Lighting M4 stays parked; the latest test still shows
learning and no key events in `engine-5.log`.

## Offline safety facts

- `.agent/RELEASED` blesses one finished implementation SHA. Move it only in a
  second release-marker commit after build/check success; remote probes/downloads
  resolve that commit, not an unfinished tip.
- Never show a current app's Update action or an unnecessary Update Hub control.
  Close and verify an app stopped before replacing its EXE.
- Behavioral changes need focused tests and a deliberate-break mutation; run
  build/check before commit and refresh CODEMAP after mapped layout changes.
- Preserve Weather Radar attribution, local cache, and worker/UI handoff. Tile
  requests are capped at source level 7; display levels 8-10 are local crops.
  Late forecast/ensemble replies remain generation-gated by place.

## Git / Drive boundary

Git retains code, build/runtime inputs, tests, safety/release facts, current
state, and concise public entry points. Drive is extensive readable orientation,
history, product narrative, and the documentation archive. Update Drive only
from verified Git; it never overrides Git.
