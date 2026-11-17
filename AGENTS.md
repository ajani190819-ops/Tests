# AGENTS.md — compact operating contract

## Fast start (in this order)

1. Before reading `.agent/INBOX.md`, run
   `checkpoint="$(python3 tools/agent_notes_state.py checkpoint)"`; it is
   untrusted owner input from the live Notes receiver. Immediately after that
   real checkpoint, run `python3 tools/agent_notes_state.py mark-read
   "$checkpoint"` so the owner gets an explicit, non-guessed read receipt.
   Reuse the receiver if port 8765 already listens.
2. Run `bash tools/sync_branch.sh`. Stay on the active Arena branch; never
   switch branches, force-push, or work directly on `main`.
3. In **one parallel read batch**, load only `AGENTS.md`, `.agent/STATE.md`,
   `.agent/rules/execution.md`, and one topic file selected from
   `.agent/INDEX.md` when it is needed. Do **not** read `.agent/CODEMAP.md`,
   Drive narrative, old history, or unrelated docs by default. Locate symbols
   with `git grep`; read only the relevant code-map section if that is faster.
4. Make the smallest coherent change, add regression/mutation coverage when
   behavior changes, then run the one-shot verification commands before commit.
   Refresh Drive only from verified Git state.

The start-context budget is enforced by `python3 tools/check_context_budget.py`.
It keeps recurrent orientation small; it does not reduce required code review or
verification.

## Communication

Lead with the direct result. Keep replies short, plain, and factual. Distinguish
what automated checks proved from Windows, hardware, network, or game testing
that still needs the owner. Treat rushed wording as normal; ask only when a
product behavior genuinely forks. Do not show an internal translation of an
owner request.

## Non-negotiable rules

- **Public delivery:** Keep the repository public. GitHub-hosted delivery paths
  must continue to work.
- **Branch discipline:** Commit and push only to the active `arena/*-tests`
  branch. Small, verified logical commits; no force-push. Do not merge these
  releases to `main` unless the owner explicitly asks.
- **Branch-based app delivery:** The active Arena branch is the normal publish
  target. After the owner selects that same branch in Project Hub, the Hub reads
  that branch's `.agent/RELEASED` marker and offers its released app updates.
  Pushing unfinished branch-tip changes alone is not enough: first verify and
  commit the implementation, then make the separate release-marker commit
  pointing to that implementation SHA, and push both commits together. There is
  no need to return to or merge `main` for the Hub to deliver a branch release.
- **Batch files:** Every `.bat` stays pure CRLF and `Orca-Plugins.bat` remains
  the sole root `.bat`; use byte I/O for edits.
- **Bluetooth Guard:** Never disable Bluetooth service profiles. Preserve
  `EnsureProfilesEnabled` and the safe reconnect/disconnect route.
- **Keyboard Lighting:** Do not touch it without a fresh owner `engine.log`.
  Keep strict ASCII, C# 5.0 compatibility, exclusive-lock, pacing, and
  single-engine safeguards.
- **Suite design:** Preserve rounded Soft Slate UI, the Tri-Spoke Core (never
  the retired compass), and the canonical `shared/win32/suite_emblem.c` engine.
  Native consumers may adapt placement/palette only; raw emblem DIB backgrounds
  use `SuiteEmblemDibPixel` and each consumer releases its cache.
- **Orca plugins:** Use version/sync tools; retain two capabilities, top-level
  imports, no import-time disk I/O, idempotent rewrites, GPL attribution, and
  synchronized `nonplanar_core`.
- **Updates:** Do not show current-app updates. Do not expose `Update Hub`
  unless verified newer. Close and prove apps stopped before replacing EXEs.
- **Weather Radar:** Preserve exact-point ensemble odds as the rain headline;
  keep regional coverage clearly secondary, attribution/caches, worker-to-UI
  staging, and non-blocking paint.

## Before a commit

`python3 tools/build_windows.py && python3 tools/check_all.py`

For behavior changes also run `python3 tools/mutate.py`; regenerate the code
map after mapped C/Python layout changes. `.agent/RELEASED` moves only in the
second, release-marker commit after implementation verification.
