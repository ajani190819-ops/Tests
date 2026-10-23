# AGENTS.md — read this before doing anything in this repo

This is the rulebook for any AI assistant (or human contributor) working here.
Everything in this file is an instruction from the repository owner and outranks
default assistant habits wherever the two disagree.

**Turn start — in this exact order:**

1. **Live notes first, before any other action.** Read the Google Drive doc
   "Agent Notes - Tests repo" (Drive connector `read_file_text`, id
   `1XRzQ4yhq2slvH1E2r2t9c7uK-vGcI1EBAxQOmSZArN8`; newest entries at the
   bottom). During long multi-commit turns RE-READ at checkpoints: after
   every 2-3 commits and once before the final push. Answer every note in
   the turn's final message (simple answer first). If the doc has an
   attached image, export the doc as DOCX and extract `word/media/*` to see it.
2. **Re-align git history if needed** (the sandbox rebuilds `.git` between
   turns while working files survive) — see §0b.4.
3. **One parallel read batch:** `.agent/STATE.md`, `.agent/CODEMAP.md`, and
   ONLY the topic doc the task needs (map in `.agent/INDEX.md`). Never load
   historical or unrelated docs. Update `.agent/STATE.md` before finishing
   when project state changes.

Companion documents (read only when relevant):

* `.agent/INDEX.md` — topic map and one-shot build/test table.
* `.agent/CODEMAP.md` — generated function/landmark index (fast lookup).
* `.agent/facts/windows-suite.md` — critical invariants & build commands for `ProjectHub.exe`, `BluetoothGuard.exe`, `Keyboard Lighting`, and suite icons.
* `.agent/facts/spatial-hud.md` — Spatial HUD architecture and F5W constraints.
* `docs/UI_STYLE_GUIDE.md` — shared Soft Slate UI tokens, rounded geometry, Tri-Spoke Core emblem, and unified suite icons.
* `docs/ORCA-PLUGIN-FACTS.md` — binding OrcaSlicer plugin rules.
* `docs/ROADMAP.md` — Orca plugin roadmap and open questions.

---

## 0. Starter step — translate the owner's message internally (mandatory)

The owner often dictates by voice or types quickly. Treat fast/casual phrasing as normal input, never as ambiguity by itself.

1. **Mentally bucket every part of the message into:**
   * **Do now** — concrete changes requested for this turn.
   * **Do next (queued)** — follow-up items ("once we're done...", "after that..."). Record these in `.agent/STATE.md` instead of mixing half-finished work in.
   * **Boundaries / constraints** — anything the owner asked to keep or avoid.
   * **Genuine product choices** — ask only when behaviour genuinely forks.
2. **Translate user symptoms into engineering root causes yourself** (Windows kernel/audio/Bluetooth APIs, WinForms/DWM, Python/Orca, Gradle/Fabric, Electron) without asking the owner technical internals questions.
3. **Keep this translation strictly internal — NEVER echo or repeat a "Translated Instructions" or "What I understood" block in your visible reply.** Go straight to doing the work and reporting the result cleanly.

---

## 0b. Fast Execution Protocol — minimize user wait time without sacrificing quality

Every extra sequential round-trip or redundant rebuild adds seconds of wall-clock wait time for the owner. Follow every rule below on every turn:

1. **Parallelize every independent tool call.** Batch `read_file`, `edit_file`, `write_file`, and independent `bash` calls in **one assistant response**; read whole target files (or whole sections) in one call instead of nibbling.
2. **Use the repo's one-shot scripts instead of hand-crafted serial loops:**
   * **Compile all four EXEs (Hub, BluetoothGuard, KeyboardLighting, DnsPilot), parallel, ~2s cached / ~130s cold:**
     ```bash
     python3 tools/build_windows.py
     ```
   * **Regenerate all multi-DPI `.ico` / `.png` suite icons + spin loops (~80–130s):** only when icon/brand files change.
     ```bash
     /home/user/.venv/bin/python3 tools/generate_suite_icons.py
     ```
   * **Run ALL repository verification suites concurrently (~4s cached):**
     ```bash
     python3 tools/check_all.py
     ```
3. **Keep tool environments & caches strictly outside `/home/user/Tests`.** Virtualenvs in `/home/user/.venv`, Zig/cache data in `/home/user/.cache` — never inside the worktree, so turn-end snapshots stay instantaneous.
4. **Git: small commits, safe recovery, never force-push.**
   * The sandbox can rebuild `.git` between turns: HEAD falls back to the branch base while working files survive, and a commit made on that stale base pushes as non-fast-forward.
   * **Turn start: `bash tools/sync_branch.sh`** — safe by design: fetch + re-align (working files never touched), installs the `.githooks/` commit guards, and leaves good unpushed commits alone.
   * **Crash isolation: ONE logical change per commit**, each with its own `build_windows.py` + `check_all.py` run, pushed immediately. Never bundle unrelated changes.
   * **If a push rejects:** `bash tools/sync_branch.sh` (it detects the stale base and re-surfaces the commit content as working diffs), re-`git add`, re-`commit`, push again. Never `--force`.
5. **Keep internal reasoning tight.** Decide the architecture once, batch edits, build + check once per change, push, answer concisely.

---

## 1. Who you are working for & how to communicate

The owner wants plain, direct, useful answers — zero filler, zero hype, zero AI-style fluff:

* **Simple answer first.** Open every reply with the direct answer / status, then detail.
* **Plain English.** Short sentences and scannable bullets. Explain a technical mechanism briefly in plain words when it helps.
* **Honest status.** Separate `Tested` (automated suite / compiler) from `Needs test on Windows / in-game` whenever OS hardware interaction is involved.

## 1b. Owner standing preferences

* **Ask clarifying questions (`ask_user`) BEFORE creating anything new — be 100% certain of the ask.** Translate symptoms to root causes yourself; only ask when behaviour genuinely forks.
* **Keep new creations simple and clean.**
* **No wasted bandwidth or CPU.** Quiet background checks, cached repaints — never re-render a whole page just to animate one thing.
* **One instance of every suite app;** a second launch brings the existing window forward. All apps run elevated (`requireAdministrator`) and autostart at login through highest-privilege scheduled tasks so the owner never touches UAC.
* **After an update, apps restore exactly as they were** (open → open, tray → tray) and the hub reliably reopens after self-update.
* **Update checks are honest:** "Update Ready" only for genuinely newer remote builds (numeric version / byte compare), and update buttons glow only while an update is really ready.
* **The live-notes Drive doc is the interruption channel** (see Turn start) — notes added mid-turn must be caught at checkpoints before the turn ends.

---

## 2. Repo map

```
Orca-Plugins.bat              ONLY top-level .bat in repo (CRLF): Orca plugin installer
plugins.json                  Orca plugin catalogue (kept in lockstep with Orca-Plugins.bat)
project-hub/                  Native Win32 Suite Control Center (ProjectHub.exe + self-update)
bluetooth-guard/              High-priority native Win32 Bluetooth auto-connect blocker (BluetoothGuard.exe)
keyboard-lighting/            ASUS ROG Strix G16 RGB tray & engine (strict 7-bit ASCII, C# 5.0)
dns-pilot/                    DNS benchmark & auto-pilot (DnsPilot.exe, tray autostart)
spatial-hud-template-26.3/    Minecraft 26.3 Fabric client mod + updater
docs/
  UI_STYLE_GUIDE.md           Soft Slate UI palette, rounded geometry & icon specs
  brand/                      Master Tri-Spoke Core emblem & unified suite .ico/.png assets
tools/
  build_windows.py            Parallel Zig compiler for all four EXEs
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

## 3. Hard rules — breaking these breaks real users

1. **The repo must stay public** (`Orca-Plugins.bat` and `ProjectHub.exe` fetch via `raw.githubusercontent.com`).
2. **All `.bat` files must stay pure CRLF (`\r\n`) on every line**, and **`Orca-Plugins.bat` must remain the ONLY root `.bat`** (`Path.read_bytes`/`write_bytes` only — never text mode). Flat `goto` labels only in setup `.bat` files (no multiline `(` blocks).
3. **Never disable Bluetooth service profiles (`BLUETOOTH_SERVICE_DISABLE`) in `bluetooth-guard/`.** Always keep profiles enabled (`EnsureProfilesEnabled`) and connect/disconnect via CoreAudio `KSPROPERTY_ONESHOT_RECONNECT` / `KSPROPERTY_ONESHOT_DISCONNECT` + `IOCTL_BTH_DISCONNECT_DEVICE` so manual connect always works.
4. **Keep prominent `Update Hub` self-update working in `ProjectHub.exe`**, and bump `HUB_VERSION` **and** `HUB_VERSION_W` (two separate lines, bump both together) whenever shipping a new `ProjectHub.exe` binary.
5. **Strict 7-bit ASCII & C# 5.0 in `keyboard-lighting/`** — no non-ASCII bytes, no C# 6+ syntax, and never `$Var::Member` in PowerShell. Do not touch `keyboard-lighting/` unless asked.
6. **UI & Brand consistency (`docs/UI_STYLE_GUIDE.md`):** Soft rounded corners (`14px` cards, `9-11px` controls, `DWMWCP_ROUND = 2`), **Tri-Spoke Core Emblem** everywhere (**never** the old 8-point compass), and unified squircle `.ico` app icons.
7. **OrcaSlicer plugin invariants (`docs/ORCA-PLUGIN-FACTS.md`):**
   * Version bumps move in lockstep via `tools/bump_version.py`; never put versions inside plugin/capability `name` strings; keep `|`, `^`, `%`, `!` out of `plugins.json`; keep exactly 2 capabilities per plugin.
   * Keep imports at module top level, zero disk I/O at import time, idempotent G-code rewrites, and `nonplanar_core` synced via `python3 tools/sync_engine.py`.
   * Preserve GPL-3.0 attribution in `plugins/unlayered-infill/` and `tools/nonplanar-infill-tool/`.
8. **Git discipline:** Commit and push strictly on the active `arena/<id>-tests` session branch (never push directly to `main`, never force-push).

---

## 4. Verification & Release Playbook

Run before every commit:
```bash
python3 tools/build_windows.py
python3 tools/check_all.py
```

When bumping an Orca plugin version:
1. Prepend `## <version> — YYYY-MM-DD` (starting with `*` bullets) to `plugins/<id>/CHANGELOG.md` and root `CHANGELOG.md`.
2. Run:
   ```bash
   python3 tools/bump_version.py <plugin-id> <version>
   python3 tools/sync_engine.py
   python3 tools/sync_changelog.py
   python3 tools/dump_default_config.py
   python3 tools/check_all.py
   ```

---

## 5. Fast lookup — the code map

* **`.agent/CODEMAP.md`** lists every function, section banner, and `g_` global of the four C sources plus `tools/` and `tests/` Python files with exact line numbers. Read it before exploring; jump straight to the line.
* Regenerate after adding/moving code: `python3 tools/agent_map.py` (its `--check` runs in `check_all.py`, so a stale map fails the suite).
