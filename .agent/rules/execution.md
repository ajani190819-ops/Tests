# Execution protocol

## Fast, safe work

- Batch independent reads, edits, and checks in one tool response. Read narrow
  ranges after a symbol search; never load long source, generated maps, or
  historical prose merely for orientation.
- Use the repository one-shot commands, which keep caches outside the worktree:
  - `python3 tools/build_windows.py` — cached parallel Windows build.
  - `python3 tools/check_all.py` — parallel test and consistency suite.
  - `python3 tools/mutate.py --check` — fast mutation-needle drift check;
    `python3 tools/mutate.py` for behavior changes.
- Start every turn with `bash tools/sync_branch.sh`. It safely realigns the
  sandbox history and installs commit guards without touching working files.
- Keep one logical change per commit. Before every commit run the build and
  check suite; push the active branch normally. If a push rejects, run sync,
  re-stage the surfaced diff, commit, and push again — never force-push.
- Keep venvs and compiler/test caches under `/home/user/.venv` or
  `/home/user/.cache`, never in the repository.

## Context and credit discipline

Arena pricing cannot be changed from this repository. The avoidable part is
repeated context and redundant work. The enforced policy is:

1. Default orientation is capped and contains only current safety facts.
2. Topic facts, generated code maps, Drive narrative, and history are opt-in.
3. Use one focused grep/read batch instead of serial exploratory reads.
4. Trust verified state recorded in `.agent/STATE.md`; do not rediscover settled
   facts. Rebuild incrementally and run full mutation only after behavior edits.
5. Put durable long-form narrative in the Drive guide/archive, retaining short
   Git stubs and offline build/safety facts.

`python3 tools/check_context_budget.py` enforces the first two points in every
`check_all.py` run.

## Agent Notes wait and receipt protocol

- `.agent/INBOX.md` is the durable delivery checkpoint. Immediately **before**
  reading it, run `checkpoint="$(python3 tools/agent_notes_state.py checkpoint)"`;
  immediately **after** that real checkpoint, run `python3 tools/agent_notes_state.py mark-read "$checkpoint"`.
  The helper
  acknowledges only receipts that existed before the read, so a note arriving
  mid-checkpoint cannot be falsely called read. Never infer read from time,
  HTTP success, or a process being alive.
- Immediately before an Arena reply or question tool that leaves the agent
  waiting for owner input, run `python3 tools/agent_notes_state.py waiting`.
  It publishes only a generic wait state; do not put private Arena question
  text in the public receiver status.
- When input arrives or no owner response is needed, run
  `python3 tools/agent_notes_state.py clear-wait`.
- This protocol proves local inbox persistence and a later checkpoint read.
  It cannot promise a displayed notification or live delivery through a
  sleeping device, disconnected client, network, server, or Arena outage.
  `notes_server.py` remains the direct channel; Drive is guide/archive only.

## Owner preferences

Ask before a genuinely new product behavior; solve technical root causes rather
than asking the owner to diagnose them. Keep apps single-instance. Preserve
open/tray state across updates. Use honest update states: only verified newer
or different released builds may offer an update. The live Notes inbox
`.agent/INBOX.md` is the interruption channel; reread it after two or three
commits and before a final push. Reuse `tools/notes_server.py` when it already
listens on 8765, then refresh `.agent/AGENT_LINK.txt` from `$E2B_SANDBOX_ID`
when available so Agent Notes discovers delivery automatically.
