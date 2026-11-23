# Focused context index

Default task context is only `AGENTS.md`, `.agent/STATE.md`, and
`.agent/rules/execution.md`. Read one additional item below only when its topic
matches. Use `git grep` first; `.agent/CODEMAP.md` is on-demand generated
navigation, never automatic context.

| Topic | Read | Verify |
| --- | --- | --- |
| Native suite, Hub, Weather Radar, icons | `.agent/facts/windows-suite.md` | `python3 tools/build_windows.py && python3 tools/check_all.py` |
| App Review / settings-preserving captures | `.agent/runbooks/test-app-review.md` | `python3 tests/test_app_review.py` |
| Spatial HUD | `.agent/facts/spatial-hud.md` | `(cd spatial-hud-template-26.3 && ./gradlew build)` |
| HUD capture scope | `.agent/decisions/0001-selected-hud-only.md` | — |
| Orca behavior | `docs/ORCA-PLUGIN-FACTS.md` | `python3 tools/check_all.py` |
| Orca plans | `docs/ROADMAP.md` | `python3 tools/check_all.py` |
| Release | `.agent/rules/release.md` | `python3 tools/build_windows.py && python3 tools/check_all.py` |
| Mutation execution / timing | `.agent/runbooks/mutation-runner.md` | `python3 tools/mutate.py` |
| Layout / unity parts | `.agent/rules/repo-map.md` | — |
| Drive synchronization | `.agent/DRIVE_GUIDE.md` | — |
| Manual HUD testing | `.agent/runbooks/test-spatial-hud-f5w.md` | — |

Keep Git facts compact, current, and offline-safe. Drive contains extended
product narrative and history; it never overrides source, tests, or release
records in Git.
