# Agent context index

Read only the files needed for the current task, and always read them in **one parallel tool batch**. Do not load historical notes or unrelated docs by default.

## Required at the start of every task (read in parallel)

1. `AGENTS.md` — permanent rules, **Fast Execution Protocol (§0b)**, and communication standards.
2. `.agent/STATE.md` — current work, verified status, versions, and next action.
3. Only the single topic file from the table below that matches the user's request.

## Topic map

| Task | Read | One-shot build / test command |
|---|---|---|
| `ProjectHub.exe`, `BluetoothGuard.exe`, `Keyboard Lighting`, or suite icons | `.agent/facts/windows-suite.md` + `docs/UI_STYLE_GUIDE.md` | `python3 tools/build_windows.py && python3 tools/check_all.py` |
| Spatial HUD rendering, F5W, or its updater | `.agent/facts/spatial-hud.md` | `(cd spatial-hud-template-26.3 && ./gradlew build)` |
| Testing Spatial HUD in F5W | `.agent/runbooks/test-spatial-hud-f5w.md` | — |
| Any change that might capture or affect other GUIs | `.agent/decisions/0001-selected-hud-only.md` | — |
| OrcaSlicer plugin behavior | `docs/ORCA-PLUGIN-FACTS.md` | `python3 tools/check_all.py` |
| Orca plugin plans or open work | `docs/ROADMAP.md` | — |
| Orca plugin release | `AGENTS.md` §5 and §5a, then the relevant plugin changelog | `python3 tools/check_all.py` |
| Arena Link Electron companion | `arena-link-windows/README.md` | `(cd arena-link-windows && npm test)` |

## Maintenance rules

- Keep `.agent/STATE.md` current and under 120 lines.
- Put stable technical invariants in `.agent/facts/` so no future session ever wastes time re-discovering them.
- Never duplicate prose across multiple docs — link to the single source of truth.
