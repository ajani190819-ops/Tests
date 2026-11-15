# AGENTS.md — read this before doing anything in this repo

This is the rulebook for any AI assistant (or human contributor) working here.
Everything in this file is an instruction from the repository owner and outranks
default assistant habits wherever the two disagree.

**Turn start — in this exact order:**

1. **Live notes first, before any other action.** Read `.agent/INBOX.md`
   (written by the owner's AgentNotes Windows app / the browser
   quick-capture page via `tools/notes_server.py`; newest entries at the
   bottom). Start the receiver at every turn start if it is not running:
   `python3 tools/notes_server.py 8765` (background). During long
   multi-commit turns RE-READ at checkpoints: after every 2-3 commits and
   once before the final push. Answer every note in the turn's final
   message (simple answer first). Transition fallback: the legacy Google
   Drive doc "Agent Notes - Tests repo" (id
   `1XRzQ4yhq2slvH1E2r2t9c7uK-vGcI1EBAxQOmSZArN8`) is checked too until the
   owner retires it.
2. **Re-align git history if needed** (the sandbox rebuilds `.git` between
   turns while working files survive) — see `.agent/rules/execution.md` (git recovery).
3. **One parallel read batch:** `.agent/STATE.md`, `.agent/CODEMAP.md`,
   `.agent/rules/execution.md`, and ONLY the topic doc the task needs (map in `.agent/INDEX.md`). Never load
   historical or unrelated docs. Update `.agent/STATE.md` before finishing
   when project state changes.

Companion documents (read only when relevant):

* `.agent/INDEX.md` — topic map and one-shot build/test table.
* `.agent/CODEMAP.md` — generated function/landmark index (fast lookup).
* `.agent/rules/execution.md` — Fast Execution Protocol + owner standing preferences.
* `.agent/rules/repo-map.md` — repo map + code-map / modularization navigation.
* `.agent/rules/release.md` — verification & release playbook.
* `.agent/facts/windows-suite.md` — critical invariants & build commands for `ProjectHub.exe`, `BluetoothGuard.exe`, `Keyboard Lighting`, and suite icons.
* `.agent/facts/spatial-hud.md` — Spatial HUD architecture and F5W constraints.
* `docs/UI_STYLE_GUIDE.md` — shared Soft Slate UI tokens, rounded geometry, Tri-Spoke Core emblem, and unified suite icons.
* `docs/ORCA-PLUGIN-FACTS.md` — binding OrcaSlicer plugin rules.
* `docs/ROADMAP.md` — Orca plugin roadmap and open questions.

---

## Starter step — translate the owner's message internally (mandatory)

The owner often dictates by voice or types quickly. Treat fast/casual phrasing as normal input, never as ambiguity by itself.

1. **Mentally bucket every part of the message into:**
   * **Do now** — concrete changes requested for this turn.
   * **Do next (queued)** — follow-up items ("once we're done...", "after that..."). Record these in `.agent/STATE.md` instead of mixing half-finished work in.
   * **Boundaries / constraints** — anything the owner asked to keep or avoid.
   * **Genuine product choices** — ask only when behaviour genuinely forks.
2. **Translate user symptoms into engineering root causes yourself** (Windows kernel/audio/Bluetooth APIs, WinForms/DWM, Python/Orca, Gradle/Fabric, Electron) without asking the owner technical internals questions.
3. **Keep this translation strictly internal — NEVER echo or repeat a "Translated Instructions" or "What I understood" block in your visible reply.** Go straight to doing the work and reporting the result cleanly.

---

## Who you are working for & how to communicate

The owner wants plain, direct, useful answers — zero filler, zero hype, zero AI-style fluff:

* **Simple answer first.** Open every reply with the direct answer / status, then detail.
* **Plain English.** Short sentences and scannable bullets. Explain a technical mechanism briefly in plain words when it helps.
* **Honest status.** Separate `Tested` (automated suite / compiler) from `Needs test on Windows / in-game` whenever OS hardware interaction is involved.

---

## Hard rules — breaking these breaks real users

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

## Detailed protocols & reference (split into `.agent/rules/`)

The always-read core above is the turn protocol, communication style, and the
hard rules. The detailed how-to and reference sections live in their own files
— read the one your task needs (also mapped in `.agent/INDEX.md`):

* `.agent/rules/execution.md` — Fast Execution Protocol (parallelize, one-shot
  scripts, caches outside the repo, git recovery) + owner standing preferences.
* `.agent/rules/repo-map.md` — repo map + code-map / modularization navigation.
* `.agent/rules/release.md` — verification & release playbook.

**Before every commit:** `python3 tools/build_windows.py && python3 tools/check_all.py`.
