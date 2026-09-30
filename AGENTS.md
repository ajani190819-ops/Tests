# AGENTS.md — read this before doing anything in this repo

This is the rulebook for any AI assistant (or human contributor) working here.
If you are an AI, everything in this file is an instruction from the repository
owner and it is **high priority**: it outranks your own defaults wherever the
two disagree.

**Start every session by reading `MEMORY.md`.** It is the handoff file: where
the work stands, what was already decided, what was tried and failed, and what
to do next. A new chat has no memory of the last one, so that file is the
memory — and you are expected to **update it before your session ends**
(`MEMORY.md` §9 is the checklist).

Companion documents, also mandatory when relevant:

* `MEMORY.md` — state of the work + session log. Read first, update last.
* `docs/ORCA-PLUGIN-FACTS.md` — hard-won facts about OrcaSlicer's plugin system.
  Do not re-derive them; do not contradict them.
* `docs/ROADMAP.md` — what is planned, what is in flight, and every open
  question. Read it before planning work; update it as part of the work.

The division of labour between the three: `MEMORY.md` is **where we are**,
`docs/ROADMAP.md` is **where we are going**, `docs/ORCA-PLUGIN-FACTS.md` is
**what is already known**. Keep them from contradicting each other.

---

## 1. Who you are working for

The owner of this repo is **a beginner** at both Python and 3D-printing
internals. That is a design constraint, not an apology:

* **Explain plainly.** Short sentences. Define every piece of jargon the first
  time it appears (there is a glossary at the bottom of this file — use and
  extend it). "The .bat reads plugins.json" beats "the installer consumes the
  manifest".
* **Never hide a failure.** If something did not work, or was not tested, say
  so in plain words. A honest "I could not test this because there is no
  Windows here" is worth more than a green checkmark.
* **Beginner-safe defaults.** Tools must never overwrite the user's input
  files, must say exactly what they did, and must refuse (with a plain-English
  fix) rather than produce broken output.

## 2. How to work — the standards

These are the owner's explicit expectations for how AI assistance goes:

1. **Write out what is about to happen, before it happens.** For any task that
   needs more than a couple of minutes of work: post the plan first — what you
   will do, in what order, what you will NOT do, and where the risks are. The
   owner should never watch a 15-minute silent build with no idea what is
   coming. Update `docs/ROADMAP.md` so the plan also lives in the repo.
2. **Ask instead of guessing.** Whenever a decision is user-facing, ambiguous,
   or a matter of taste, ask a clarifying question with concrete options and a
   recommended default — batch the questions so they can all be answered at
   once. Asking one round of good questions is efficient, not annoying.
3. **Work in small, reviewable steps.** One logical change at a time, verified
   before moving on. If a step turns out wrong, it should be cheap to throw
   away.
4. **Verify, then say what you verified.** Run the repo's tests (`python3
   tests/test_installer.py`) after touching the updater, the catalogue, or the
   .bat. After any file edit, confirm the edit actually landed — silent
   no-op edits happen. In your report, separate "verified" from "not tested".
5. **Report in three parts:** what I did / what it means for you / what is
   next. Keep it short enough to actually read.
6. **Be honest about uncertainty.** Label guesses as guesses. If the docs say
   something has never been tested on real hardware, keep saying so.
7. **Leave the next session a memory.** Before you finish, update `MEMORY.md`
   — state, decisions, answered questions, next actions, and a session-log
   entry (its §9 is the checklist). The owner should be able to open a brand
   new chat, point it at this repo, and have it pick up mid-stride.

## 3. Repo map

```
Update-Orca-Plugins.bat     ONE-CLICK UPDATER. Download once into Downloads,
                            double-click to install/update every plugin below.
plugins.json                the catalogue the updater reads (what + where + version)
AGENTS.md                   this rulebook
MEMORY.md                   handoff: state of the work + session log. Read at the
                            start of a session, update at the end of it.
README.md                   human-facing front door / tour
docs/
  ORCA-PLUGIN-FACTS.md      OrcaSlicer plugin-system facts (do not re-derive)
  ROADMAP.md                what's planned and every open question
plugins/
  wave-overhangs/           OrcaSlicer pipeline plugin: support-free steep overhangs
  unlayered-infill/         OrcaSlicer pipeline plugin: sine-wave interlocking infill
tools/
  nonplanar-infill-tool/    standalone double-click tool (the predecessor of the
                            unlayered-infill plugin; kept as reference, GPL-3.0)
keyboard-lighting/          unrelated personal project; DO NOT reorganize or "fix" it
tests/
  test_installer.py         contract test: catalogue / .bat / files must agree
```

The `plugins.json` entry `path` is a URL path into this repo (forward slashes,
relative to the repo root). The updater downloads
`https://raw.githubusercontent.com/ajani190819-ops/Tests/main/<path>`, so after
moving a plugin file you must update `path` in `plugins.json` **and** the
hardcoded fallback list inside the .bat — `tests/test_installer.py` fails if
you forget, and it is the safety net for exactly this.

## 4. Hard rules — breaking these breaks real users

1. **The repo must stay public.** The updater makes unauthenticated
   raw.githubusercontent.com downloads; a private repo 404s every file.
2. **`Update-Orca-Plugins.bat` must stay CRLF.** All lines, byte-exact.
   `*.bat -text` in `.gitattributes` keeps git from re-normalizing it. Never
   edit it with tools that convert line endings (Python `Path.write_text`
   does — use binary mode). After editing, assert the CRLF count.
3. **Version bumps take edits in lockstep:** the PEP 723 `# version = "..."`
   header in the plugin file, `plugins.json`, and the .bat fallback list. The
   test enforces all three agree. (The .bat stamps the installed version from
   the downloaded file's header, so the header is the source of truth.)
4. **No `|`, `^`, `%`, `!` in any catalogue field** — they corrupt the .bat's
   pipe-delimited plan or cmd.exe parsing.
5. **Exactly two capabilities per plugin** in the catalogue — the .bat's plan
   has two capability slots.
6. **Never delete or rewrite `keyboard-lighting/`** — it is here for storage,
   not review.
7. **GPL-3.0 attribution must survive.** `unlayered-infill` and
   `tools/nonplanar-infill-tool` derive from Roman Tenger's NonPlanarInfill
   (GPL-3.0). Keep the copyright headers; keep the licence when distributing.
8. **Git discipline:** all work happens on **the session branch you were
   handed** (`arena/<id>-tests` — it is a different one every chat; session 1
   was `arena/01a0f42b-tests`). Push only to that branch, open PRs only from
   it, never commit straight to `main`, never force-push, never commit
   credentials or generated junk. Don't trust a branch name hardcoded in a
   doc — the current branch is whatever this session was given.
9. **Orca plugin facts live in `docs/ORCA-PLUGIN-FACTS.md`** and they are
   binding: e.g. import third-party deps at module load (never inside a
   capability), never read `post_process_plugin` from config, G-code
   transforms must be idempotent because the export step can run twice.

## 5. How to verify your work

```bash
python3 tests/test_installer.py        # catalogue / .bat / files agree
git ls-files --eol Update-Orca-Plugins.bat   # must say i/crlf
```

The .bat itself is Windows-only and cannot be executed in this sandbox. When
you change it: keep it one logical change at a time, re-read the whole file
afterwards, and simulate the pieces you can (the repo test already simulates
the `for /f` tokenization the .bat performs — extend it rather than trusting
your eyes).

Anything touching real slicing behaviour is verified against a fake harness at
best. Say so in the PR. The first real slice on real OrcaSlicer is the real
test; `data_dir()/log/python_*.log` holds the traceback if it fails.

## 6. Glossary (extend as needed)

* **catalogue** — `plugins.json`: the list of plugins the updater can install.
* **updater** — `Update-Orca-Plugins.bat`. Double-clickable Windows script.
* **PEP 723 header** — the `# /// script` comment block at the top of a
  plugin file; carries the plugin's name/version/dependencies.
* **sidecar** — Orca's `.install_state.json` next to a plugin; says it is
  installed, enabled, and which capabilities are on. The updater writes it so
  plugins appear already enabled.
* **pipeline plugin** — an OrcaSlicer plugin that hooks into slicing steps
  (e.g. `posSlice`, `psGCodePostProcess`). Needs Orca newer than 2.4.2 or a
  nightly.
* **post-processing script** — a plain script run on the finished G-code,
  either by hand or via Orca's *Post-processing scripts* setting. Works on any
  Orca version; this is what `tools/nonplanar-infill-tool` is.
* **CRLF** — Windows line endings (`\r\n`). The .bat needs them.
* **raw.githubusercontent.com** — GitHub's "give me this file as-is" URL
  service; how the updater downloads.
* **GPL-3.0** — a copyleft licence; derived code must keep attribution and the
  same licence.
