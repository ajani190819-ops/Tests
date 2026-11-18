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
  renderer for Project Hub, Agent Notes, Audio Switcher, and Weather Radar.
  Consumers use `SuiteEmblemDibPixel` for raw DIB backgrounds, own placement,
  and release their shared cache at shutdown.
- **Weather Radar rain watch:** implementation `1be38f40f8612835ac539b9958528231da1c854b`,
  released by marker `59794dc`. A running Weather Radar hides to its notification
  area on close and sends a native Windows shell alert only at **66%+ exact-point
  ensemble odds in 3 hours**. The three-hour cooldown and persisted episode latch
  suppress duplicates while rain is under way. Regional coverage odds never alert.
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

1. Weather Radar five-day planner: equal rain, temperature, cloud, and wind
   daily categories; selected day/category gives hourly detail, compact trend,
   and plain-English reading. Rain remains primary; exact ensemble odds stay
   distinct from regional coverage; humidity stays supporting.
2. Keyboard Lighting M4 hold remains unresolved: the owner test after `9cb8d8d`
   still shows M4 learning; `engine-5.log` has no key events. Keep it parked
   until the owner reopens it; preserve exclusive-lock, pacing, and
   single-engine safety.

## Offline safety facts

- `.agent/RELEASED` blesses one finished implementation SHA. Move it only in a
  second release-marker commit after build/check success; remote probes/downloads
  resolve that commit, not an unfinished tip.
- Never show a current app's Update action or an unnecessary Update Hub control.
  Close and verify an app stopped before replacing its EXE.
- Behavioral changes need focused tests and a deliberate-break mutation; run
  build/check before commit and refresh CODEMAP after mapped layout changes.
- Preserve Weather Radar attribution, local cache, async worker/UI handoff, and
  zoom-8-to-7 tile fallback. Late forecast/ensemble replies are generation-gated
  and must never overwrite a newly selected place.

## Git / Drive boundary

Git retains code, build/runtime inputs, tests, safety/release facts, current
state, and concise public entry points. Drive is extensive readable orientation,
history, product narrative, and the documentation archive. Update Drive only
from verified Git; it never overrides Git.
