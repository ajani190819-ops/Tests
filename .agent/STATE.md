# Repository Current State

**Active branch:** read with `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-16. Git is authoritative for implementation, tests,
release records, and offline safety facts. Drive holds extended narrative/plans.

## Released work

- **Main Hub 2.8.56; Arena Hub 2.8.62** (`81eb2c1a1822fcb7fe76118df5dccaef676c3495`):
  Hub is `asInvoker`; children keep their elevation. Notes is user-opened;
  elevated `--stop` and managed-copy gates precede updates. Owner confirmed
  tray hover/Radar visibility. Remote probes pin released content.
- **Agent Notes receipts:** `ad9232009b9f38c287727123ddd404b204ce26e1`, marker
  `5343e54`. Saved means receiver flush; Read means a later agent checkpoint.
  Native visual wait alert, opt-in sound; no logon Run key, task, or service.
- **Shared Tri-Spoke:** `shared/win32/suite_emblem.c` is the sole native renderer.
  Raw DIBs use `SuiteEmblemDibPixel`; each consumer releases its cache.
- **Radar rain watch:** `1be38f40f8612835ac539b9958528231da1c854b`, marker
  `59794dc`. Alert only at 66%+ exact-point ensemble odds in 3h; persisted
  episode state/cooldown suppress repeats. Regional coverage never alerts.
- **Radar planner:** `d483a5629db77a4428d57c1c870489a3e9eee5d2`, Hub 2.8.62.
  Five aligned date-first rows open a labeled hourly chart. Exact-point odds
  stay distinct from regional PoP. Native review remains.
- **Radar place alignment:** `36c55bda541b22fc34048e5fb87f7dba7aa27c99`, marker
  `d97ba63`. Old-place tiles are discarded; paint locks the published buffer
  through its blit. No prior city under a new marker. Build/checks and 156
  mutations passed; native/live-tile review remains.
- **Documentation:** 29 human-facing archives are in Drive; Git keeps build,
  test, runtime, release inputs, and compact entry points.

## Bluetooth approval release

Implementation `83e54d5ed8648856f7dbbfa5786096d36ab8ce8e`, Hub 2.8.62. Native
paired rows offer Accept/Deny/Auto-Connect; undecided pairs stay reviewable
without automatic trust. Deny keeps pairing; actions/drops recheck addresses.
Build/checks and all 164 mutations pass (0 surviving/stale); real-earbud Windows
verification remains. The legacy PowerShell setup path is unchanged.

## Agreed to-do list

Full human-readable plan: **Tests — Master To-Do List**, linked with its file ID
in `.agent/DRIVE_GUIDE.md`. Repeat this list in replies; retain pending/parked
items. The owner reports missing requests: reconciliation is NOT complete.

1. **BT-01 first:** native approval update released; verify actual earbuds on
   Windows (discovery, choices, reconnect). Scans must not grant trust.
2. **PLAN-01:** master Drive doc created; recover/confirm any further missing
   requests. Historical follow-ups below are not newly approved coding work.
3. **APP-01 next coding:** Open restores/shows and foregrounds the existing app
   instance, including minimized/tray windows, without duplicates.
4. **WX-01/02 pending Windows review:** planner layout and live radar alignment.
5. **OPS-01 monitor only:** latest open worked. If UAC recurs, identify program/
   path and action before changing elevation; reports covered all three actions.
6. **KBL-01 parked:** fresh owner `engine.log` required; latest `engine-5.log`
   showed learning and no key events. Do not touch lighting without fresh logs.
7. **HUD-01 pending:** saved three-method F5W runtime/compatibility validation.
8. **ORCA-01/02/03 recovered, confirm:** curved-wall reproduction with actual
   config, fresh export/physical print evidence, and real-host dual-plugin flow.
9. **ORCA-04/05 historical research, parked:** wake blending without coverage/
   fragment regressions and a smoother curved-wall perimeter-following rung.

## Offline safety / efficiency

- `.agent/RELEASED` blesses one finished implementation SHA. Move it only in a
  second release-marker commit after build/check success; download that commit,
  never an unfinished tip. A current app must not offer Update/Update Hub.
- Close and prove every managed target stopped before replacing its EXE.
- Keep Bluetooth profiles enabled. Native K/D/F rules distinguish trusted,
  denied, and forgotten devices; review candidates are not persisted as trust.
- Preserve Radar attribution/cache and generation-gated worker/UI staging;
  source zoom <=7, local 8-10 crops, and a lock through the published-view blit.
- Behavior edits need regressions and deliberate-break mutations. Build/check
  before commits; refresh CODEMAP after mapped C/Python layout changes.
- Default context: AGENTS, STATE, execution, needed topic. Four files <=15,000
  chars; CODEMAP/Drive/history on demand. HUD needs JDK 25, available in CI.
- Git is canonical for code/safety/releases. Maintain the full Drive plan and
  Project Guide from verified Git; do not mirror source or raw test output.
