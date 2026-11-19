# Repository Current State

**Active branch:** read with `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-16. Git is authoritative for implementation, tests,
release records, and compact offline safety facts. Drive holds extended narrative.

## Released work

- **Main: Project Hub 2.8.56:** Tri-Spoke tray, alerts, and default-on branch
  discovery remain. Remote probes pin releases; updates appear only for
  different/newer content.
- **Arena Project Hub 2.8.62** (`81eb2c1a1822fcb7fe76118df5dccaef676c3495`):
  Hub runs asInvoker; children keep their own elevation. Agent Notes is
  user-opened; its elevated `--stop` and the managed-copy gate precede updates.
  Soft Slate tray/Settings, the popup reentry guard, and the offline Weather
  Radar card are verified; owner confirmed tray hover and Radar visibility.
  Main remains 2.8.56.
- **Agent Notes waiting receipts:** `ad9232009b9f38c287727123ddd404b204ce26e1`
  (marker `5343e54`). Local outbox distinguishes Saved (receiver flush) from
  Read (later agent checkpoint). Native visual wait alert and opt-in sound; no
  sign-in Run key, task, or service.
- **Shared Tri-Spoke engine:** `shared/win32/suite_emblem.c` is the sole native
  renderer. Consumers use `SuiteEmblemDibPixel` for raw DIBs, own placement,
  and release their cache.
- **Weather Radar rain watch:** `1be38f40f8612835ac539b9958528231da1c854b`,
  marker `59794dc`. Tray alerts require **66%+ exact-point ensemble odds in 3h**;
  persisted episode state and cooldown suppress repeats. Regional odds never alert.
- **Weather Radar planner:** `d483a5629db77a4428d57c1c870489a3e9eee5d2`, Hub
  2.8.62. Five date-first aligned rows open a labeled hourly chart on selection.
  Exact-point ensemble odds stay distinct from regional PoP; build/checks and
  153 mutations pass. Native Windows review remains.
- **Weather Radar place alignment:** implementation
  `36c55bda541b22fc34048e5fb87f7dba7aa27c99`, Hub 2.8.62. Source requests
  remain capped at level 7 and local zoom crops use the selected place. Tile
  workers now discard late results from an old place generation, and paint
  holds the published view lock through the GDI blit; the previous city is not
  shown under the newly selected marker while matching imagery loads. Build,
  checks, and all 156 mutations pass; native Windows/live-tile verification
  remains outstanding.
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

Native Windows review of the planner and live radar tiles/alignment remains.
The reported Hub UAC recurrence is active: source and built PE declare
`asInvoker`; startup only starts the remote status check. The only explicit Hub
`runas` path is the Agent Notes `--stop` helper during a Notes update; child
apps keep their own elevation manifests. The exact action behind the owner's
prompt is not yet known. Keyboard Lighting M4 stays parked; the latest test
still shows learning and no key events in `engine-5.log`.

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
  Radar tile replies and forecast/ensemble replies are generation-gated by
  place; keep the published view buffer locked through its UI blit.

## Git / Drive boundary

Git retains code, build/runtime inputs, tests, safety/release facts, current
state, and concise public entry points. Drive is extensive readable orientation,
history, product narrative, and the documentation archive. Update Drive only
from verified Git; it never overrides Git.
