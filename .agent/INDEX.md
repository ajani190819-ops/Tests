# Agent context index

Read only the files needed for the current task, and always read them in **one parallel tool batch**. Do not load historical notes or unrelated docs by default.

## Required at the start of every task (read in parallel)

1. `AGENTS.md` — the always-read core: turn protocol, hard rules, communication standards.
2. `.agent/rules/execution.md` — Fast Execution Protocol + owner standing preferences (how to work each turn).
3. `.agent/STATE.md` — current work, verified status, versions, and next action.
4. `.agent/CODEMAP.md` — generated function/line index (from `tools/agent_map.py`) for fast lookup.
5. Only the single topic file from the table below that matches the user's request.

## Topic map

| Task | Read | One-shot build / test command |
|---|---|---|
| `ProjectHub.exe`, `BluetoothGuard.exe`, `Keyboard Lighting`, or suite icons | `.agent/facts/windows-suite.md` + `docs/UI_STYLE_GUIDE.md` | `python3 tools/build_windows.py && python3 tools/check_all.py` |
| Spatial HUD rendering, F5W, or its updater | `.agent/facts/spatial-hud.md` | `(cd spatial-hud-template-26.3 && ./gradlew build)` |
| Testing Spatial HUD in F5W | `.agent/runbooks/test-spatial-hud-f5w.md` | — |
| Any change that might capture or affect other GUIs | `.agent/decisions/0001-selected-hud-only.md` | — |
| OrcaSlicer plugin behavior | `docs/ORCA-PLUGIN-FACTS.md` | `python3 tools/check_all.py` |
| Orca plugin plans or open work | `docs/ROADMAP.md` | — |
| Committing, building, or an Orca plugin release | `.agent/rules/release.md` (+ the relevant plugin changelog) | `python3 tools/build_windows.py && python3 tools/check_all.py` |
| Repo layout, where a file/symbol lives, or the `parts/` modularization | `.agent/rules/repo-map.md` | — |

## Maintenance rules

- Keep `.agent/STATE.md` current and scannable (no hard length cap — the old 120-line budget is retired).
- Put stable technical invariants in `.agent/facts/` so no future session ever wastes time re-discovering them.
- Never duplicate prose across multiple docs — link to the single source of truth.
