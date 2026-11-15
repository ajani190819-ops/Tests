# Repo map & code navigation

> Split from `AGENTS.md`. Read this to find where things live and how the
> modularized C sources are organized.

## Repo map

```
Orca-Plugins.bat              ONLY top-level .bat in repo (CRLF): Orca plugin installer
plugins.json                  Orca plugin catalogue (kept in lockstep with Orca-Plugins.bat)
project-hub/                  Native Win32 Suite Control Center (ProjectHub.exe + self-update)
bluetooth-guard/              High-priority native Win32 Bluetooth auto-connect blocker (BluetoothGuard.exe)
keyboard-lighting/            ASUS ROG Strix G16 RGB tray & engine (strict 7-bit ASCII, C# 5.0)
dns-pilot/                    DNS benchmark & auto-pilot (DnsPilot.exe, tray autostart)
agent-notes/                  Quick-notes Windows app (AgentNotes.exe) -> session .agent/INBOX.md
audio-switcher/               Per-app volume & audio-output switcher (AudioSwitcher.exe, tray)
suite-apps.json               App catalog: one entry adds a hub card (Install + icon), no hub update needed
spatial-hud-template-26.3/    Minecraft 26.3 Fabric client mod + updater
docs/
  UI_STYLE_GUIDE.md           Soft Slate UI palette, rounded geometry & icon specs
  brand/                      Master Tri-Spoke Core emblem & unified suite .ico/.png assets
tools/
  build_windows.py            Parallel Zig compiler for all six EXEs
  agent_map.py                Regenerates .agent/CODEMAP.md (fast lookup index)
  generate_suite_icons.py     Analytic SDF emblem & multi-size .ico/.png generator
  check_all.py                Concurrent runner for all Python, sync, Windows, & Node tests
  bump_version.py             Locks plugin versions across code, .bat, JSON, & changelogs
  sync_engine.py              Syncs shared nonplanar_core into unlayered-infill plugin
  sync_changelog.py           Syncs recent changelog bullets into plugin Check setup
  dump_default_config.py      Refreshes docs/config-reference/*.json
tests/                        Automated regression & contract tests
.agent/CODEMAP.md             GENERATED function/line index — regenerate after moving code
```

---

## Fast lookup — the code map

* **`.agent/CODEMAP.md`** lists every function, section banner, and `g_` global of the six C sources and their `parts/` files, plus `tools/` and `tests/` Python files, with exact line numbers. Read it before exploring; jump straight to the line.
* **App C sources are modularized (unity build).** Each `<app>.c` is a thin main (headers, defines, globals) that textually `#include`s its `src/parts/*.c` sections IN ORIGINAL ORDER, so it stays ONE translation unit and `build_windows.py` (a fixed target list) needs no change when a part is added. Edit the part files, not the thin main. Tests read sources through `read_c_inlined()`; CODEMAP indexes every part.
* Regenerate after adding/moving code: `python3 tools/agent_map.py` (its `--check` runs in `check_all.py`, so a stale map fails the suite).
