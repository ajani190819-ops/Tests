# Repository Current State

**Active branch:** read with `git rev-parse --abbrev-ref HEAD`; never hardcode it.
**Last updated:** 2026-11-15. Detailed operating orientation belongs in the
[Tests Project Guide](https://docs.google.com/document/d/1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ/edit?usp=drivesdk).
Git is authoritative for code, tests, release records, and the compact safety
facts an agent must have to build or recover the branch.

## Current release work

- **Project Hub 2.8.50:** keeps `Update Hub` hidden unless a released remote
  build is truly newer; `Update All` remains present. Settings visibly exposes
  default-on automatic following of the newest `arena/*-tests` branch. A manual
  Branch choice pauses following until the setting is re-enabled. Every EXE
  replacement requires the app to close and its image path to be verified.
- **One native Tri-Spoke engine:** `shared/win32/suite_emblem.c` now owns the
  geometry, four-times source cache, time helper, transparent alpha composition,
  DIB `COLORREF` conversion, theme-aware palette, and release lifecycle. Project
  Hub, Agent Notes, Audio Switcher, and Weather Radar include it once from their
  unity-build roots. Their adapters may place it but cannot fork it. The four
  dependent build-cache keys include the shared source.
- **Visual result:** dark headers use the canonical pale Platinum-Ice/Soft-Sky
  mark; light headers use their accessible accent-to-text colors. Narrow emblem
  paints initialize DIB pixels with `SuiteEmblemDibPixel(COL_BG)`, eliminating
  the opaque brown/gold square regression.
- **Agent Notes:** one automatic-delivery composer; no manually entered session
  link. Durable outbox remains `%LOCALAPPDATA%\AgentNotes\outbox`.
- **Weather Radar:** the released balanced view preserves exact-location
  ensemble odds as the rain headline, regional PoP as a clearly labelled
  comparison, async workers/caches, attribution, and zoom-8-to-7 fallback.

## Next approved work

1. **Weather Radar five-day planner:** Rain, temperature, cloud cover, and wind
   are equal daily categories over five days. Selecting a day/category reveals
   selected-day hourly detail, a compact trend chart, and plain-English reading.
   Rain remains primary; exact-location ensemble odds stay distinct from regional
   odds. Humidity stays supporting detail.
2. **Keyboard Lighting:** change only against a fresh owner `engine.log`; keep
   the existing exclusive-lock, pacing, and single-engine safeguards intact.

## Non-negotiable current facts

- `.agent/RELEASED` blesses one finished implementation commit. Remote probes
  and downloads resolve that commit, not unfinished branch-tip work. Update it
  only in the second, release-marker commit after build and checks pass.
- Project Hub automatic branch following ranks candidate branch-tip commit dates;
  GitHub branch-list order is not a date signal. Its six-hour cache and manual
  override behavior are intentional.
- Suite Update controls appear only for missing or verified different/newer remote
  builds. Do not show a current app's Update action. Do not expose `Update Hub`
  when it is current.
- The native header engine is a source dependency outside app `src/`; adding a
  consumer requires the root include, a thin adapter, a `TARGET_SHARED_SOURCES`
  entry in `tools/build_windows.py`, regression coverage, and `SuiteEmblemRelease`
  at shutdown. Never write a raw `COLORREF` to its 32-bit emblem DIB.
- Preserve Weather Radar’s RainViewer/OpenStreetMap attribution, local tile cache,
  worker-to-UI staging, and no-blocking paint behavior.
- Before a commit: `python3 tools/build_windows.py && python3 tools/check_all.py`.
  Run `python3 tools/mutate.py` for registered mutation coverage. Regenerate the
  code map with `python3 tools/agent_map.py` when mapped C/Python layout changes.

## Git and Drive boundary

- Git retains only implementation, tests, release/safety rules, current state,
  and generated navigation needed to work safely offline.
- The Drive Project Guide holds the extensive human-facing product map, operating
  narrative, release background, and historical context. Synchronize it only
  from verified Git state; Drive never overrides Git.
- Agent Notes is a live inbox, not an architecture document. Read its local
  `.agent/INBOX.md` first at the start of each turn.
