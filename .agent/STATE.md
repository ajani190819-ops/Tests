# Repository Current State

**Active branch:** read with `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-16. Git is authoritative for implementation, tests,
release records, and compact offline safety facts. Drive holds extended narrative.

## Released work

- **Project Hub 2.8.54:** the live unified tray handles suite Open/Hide/Quit,
  safe toggles, and Agent Notes/Weather alerts; app icons are fallbacks only.
  Alerts retry until Windows accepts them. `Update Hub` requires a verified
  newer build; default-on newest-test-branch following and Hub reveal of
  tray-hidden Agent Notes remain.
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
- **Documentation archive:** full pre-migration text for 29 human-facing files
  is in [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
  Commit `39f71df` replaced them with compact Git stubs while preserving current
  plugin changelog windows, version-sync anchors, installers, tests, config, and
  runtime assets.

## Efficiency system and completed audit

- Default agent orientation is `AGENTS.md`, this state file, and
  `.agent/rules/execution.md`, plus a topic file only when needed. Generated
  CODEMAP, Drive narrative, history, and topic facts are on demand.
- `tools/check_context_budget.py`, run by `tools/check_all.py`, caps the four
  orientation files at 15,000 total characters and forbids a mandatory CODEMAP
  read. This reduces avoidable context without weakening focused review/tests.
- The final size sweep removed six unreferenced byte-identical brand aliases;
  regeneration now removes them too. The canonical spin/web assets and suite
  icons remain unchanged, saving about 26 MiB per clone/download.
- Repository audit: all tracked Python files compile; shell syntax, Windows
  build, check suite, mutation suite, generated artifacts, and Git diff checks
  pass. The Spatial HUD needs JDK 25; its GitHub workflow provisions it, while
  this sandbox has no Java runtime, so an in-sandbox Gradle build is impossible.

## Next approved work

1. Weather Radar five-day planner: equal rain, temperature, cloud, and wind
   daily categories; selected day/category gives hourly detail, compact trend,
   and plain-English reading. Rain remains primary; exact ensemble odds stay
   distinct from regional coverage; humidity stays supporting.
3. Keyboard Lighting only against a fresh owner `engine.log`; preserve its
   exclusive-lock, pacing, and single-engine safeguards.

## Offline safety facts

- `.agent/RELEASED` blesses one finished implementation SHA. Move it only in a
  second release-marker commit after build/check success; remote probes/downloads
  resolve that commit, not an unfinished tip.
- Never show a current app's Update action or an unnecessary Update Hub control.
  Close and verify an app stopped before replacing its EXE.
- Preserve Weather Radar attribution, local cache, async worker/UI handoff, and
  zoom-8-to-7 tile fallback. Late forecast/ensemble replies are generation-gated
  and must never overwrite a newly selected place.
- Every behavioral change needs focused regression coverage and a mutation entry.
  Before commit run `python3 tools/build_windows.py && python3 tools/check_all.py`;
  run `python3 tools/mutate.py` for behavioral work and regenerate the code map
  after mapped C/Python layout changes.

## Git / Drive boundary

Git retains code, build/runtime inputs, tests, safety/release facts, current
state, and concise public entry points. Drive is extensive readable orientation,
history, product narrative, and the documentation archive. Update Drive only
from verified Git; it never overrides Git.
