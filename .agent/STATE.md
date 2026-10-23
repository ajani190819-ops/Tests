# Repository Current State (`STATE.md`)

**Active Working Branch:** `arena/e895b94f-tests`
**Last Updated:** 2026-10-23
**PR:** #10 open — every commit+push updates it.

## Turn protocol (do this literally)

1. **Live notes first** (before any other action): `.agent/INBOX.md` - the
   AgentNotes app / browser capture write here via `tools/notes_server.py`
   (start it at turn start if down: `python3 tools/notes_server.py 8765`).
   Re-read every 2-3 commits and before the final push; answer notes in the
   final message. Transition fallback: Google Drive doc id
   `1XRzQ4yhq2slvH1E2r2t9c7uK-vGcI1EBAxQOmSZArN8` until retired.
2. **Git re-align:** `bash tools/sync_branch.sh` (safe: installs `.githooks/` commit guards, fetch + re-aligns the pointer; working files never touched; good unpushed commits left alone). The pre-commit guard refuses stale-base commits. Never force-push.
3. One parallel read batch: this file + `.agent/CODEMAP.md` + only the task's topic doc (`.agent/INDEX.md`).
4. One logical change per commit (own build+check), push immediately.
5. Update this file whenever project state changes (it survives resets).

## Active Component Status

| Component | Path | Version | Status |
| :--- | :--- | :--- | :--- |
| **Project Hub** | `project-hub/src/project_hub.c` + `ProjectHub.exe` | 2.8.21 | **GREEN** — update nag only for genuinely newer GitHub builds (numeric `VerNewer`), buttons amber only while update ready, reopens after self-update (mutex handover + retry), quiet 60s ETag checks, cached-frame spin paint, Arena Link removed (5 cards), requireAdministrator |
| **Bluetooth Guard** | `bluetooth-guard/src/bluetooth_guard.c` + `BluetoothGuard.exe` | 2.8.7 | **GREEN** — rules-ghost device list; FastBoot task now `HighestAvailable` (elevated logon), requireAdministrator |
| **Keyboard Lighting** | `keyboard-lighting/src/keyboard_lighting.c` + `KeyboardLighting.exe` + `Aura-Background.ps1` | 2.8.7 | **GREEN** — M4/M5 hold modes (`-HoldMode latch\|burst\|tap`, latch default); logon task `/RL HIGHEST`; requireAdministrator |
| **DNS Pilot** | `dns-pilot/src/dns_pilot.c` + `DnsPilot.exe` | size-tracked | **GREEN** — parallel rank test (seconds); benchmarks on EVERY start incl. tray; autostart = `TestsSuite-DnsPilot` ONLOGON task `/RL HIGHEST` (Run key migrated away); requireAdministrator |
| **Agent Notes** | `agent-notes/src/agent_notes.c` + `AgentNotes.exe` | size-tracked | **GREEN** — direct quick-notes channel (no Google middleman): POSTs to `tools/notes_server.py` (live preview port 8765), queues in `%LOCALAPPDATA%\AgentNotes\outbox\` while the agent is offline, auto-delivers on reconnect; hub card #6 with Install; requireAdministrator |
| **Agent tooling** | `AGENTS.md`, `.agent/CODEMAP.md`, `tools/` | 2026-10-23 | **GREEN** — `agent_map.py` code map (`--check` in suite), one-shot build (~2s cached) / full suite (~4s cached) |

## Fast Commands

- Build all four EXEs: `python3 tools/build_windows.py`
- All verification suites: `python3 tools/check_all.py`
- Code map regen: `python3 tools/agent_map.py`
- Icons + spin: `/home/user/.venv/bin/python3 tools/generate_suite_icons.py` (only when brand files change)

## Open Threads / Next Actions

1. **User validation round (2026-10-23 evening):** update buttons stop glowing when up to date; hub reopens after self-update (2.8.21); false "vX available" nag gone; DNS Pilot tests from tray startup; all apps elevated with no UAC at login.
2. If the owner still sees a UAC prompt on manual double-click launches (UAC-on machines), offer routing shortcuts through a silent task launcher (`schtasks /Run`).
