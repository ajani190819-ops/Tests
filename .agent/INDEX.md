# Agent context index

Read only the files needed for the current task. Do not load historical notes
or unrelated product documentation by default.

## Required at the start of every task

1. `AGENTS.md` — permanent rules and communication standards.
2. `.agent/STATE.md` — current work, verified status, risks, and next action.
3. The applicable topic file below.

## Topic map

| Task | Read |
|---|---|
| Spatial HUD rendering, F5W, or its updater | `.agent/facts/spatial-hud.md` |
| Getting a Spatial HUD build (agent cannot touch workflows) | `PASTE-ME-CI-SETUP.md` |
| Testing Spatial HUD in F5W | `.agent/runbooks/test-spatial-hud-f5w.md` |
| Any change that might capture or affect other GUIs | `.agent/decisions/0001-selected-hud-only.md` |
| OrcaSlicer plugin behavior | `docs/ORCA-PLUGIN-FACTS.md` |
| Orca plugin plans or open work | `docs/ROADMAP.md` |
| Orca plugin release | `AGENTS.md` §5 and §5a, then the relevant plugin changelog |
| User-facing repository overview | `README.md` |
| Release history | `CHANGELOG.md` and the relevant component changelog |

## Evidence and history

- `latest.log` is the supplied F5W evidence. Search it; do not load it in
  full unless the task needs a log detail.
- `.agent/history/legacy-memory-through-2026-10-07.md` preserves the former
  large `MEMORY.md`. Read it only to recover older Orca work or a dated detail.
- Git history is the source for exact old implementation changes and commits.

## Maintenance rules

- Keep `.agent/STATE.md` current and under 200 lines.
- Put stable, confirmed technical information in a topic file under
  `.agent/facts/`.
- Put a lasting design choice and its reason in `.agent/decisions/`.
- Put reproducible test steps in `.agent/runbooks/`.
- Put dated outcomes in `.agent/history/`; do not append them to `STATE.md`.
- Prefer a link over duplicating text. Each fact should have one source of
  truth.
- User-facing documentation remains in its existing location. Do not move or
  hide it merely because it is also useful to an agent.

## File template

Use this shape for new agent notes when it fits:

```md
# Short, specific title

- Status: confirmed | planned | unverified
- Last verified: YYYY-MM-DD
- Read when: <task>

## Facts

## Constraints

## Evidence

## Next action
```
