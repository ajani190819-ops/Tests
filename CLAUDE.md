# CLAUDE.md

Read **`AGENTS.md`** and **`.agent/STATE.md`** first in a single parallel batch before any other tool call.

- Follow **Section 0b ("Fast Execution Protocol")** in `AGENTS.md` strictly to minimize user wait time without sacrificing quality:
  1. Batch all independent `read_file` / `edit_file` / `write_file` / `bash` calls in parallel within a single turn step.
  2. Build both Win32 `.exe` apps in parallel with one command: `python3 tools/build_windows.py`
  3. Regenerate suite icons in one command when icon assets change: `python3 tools/generate_suite_icons.py`
  4. Verify all repository suites concurrently in one command (~4s): `python3 tools/check_all.py`
  5. Keep Python venvs/caches strictly outside the repo (`/home/user/.venv`, `/home/user/.cache`).
  6. Never echo the internal translation step in your visible reply; answer directly and concisely.
