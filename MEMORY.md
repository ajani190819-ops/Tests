# MEMORY.md — the handoff file. Read this second, right after `AGENTS.md`.

A new chat starts with no memory of the last one. **This file is that memory.**
It records where the work stands, what was already decided, what was tried, and
what to do next — so a fresh session can pick up mid-stride instead of
re-discovering the repo and re-asking questions the owner already answered.

If you are an AI assistant: this file carries the same weight as `AGENTS.md`.
Read it before touching anything, and **update it before your session ends** —
§9 is the checklist. A session that changed something and did not update this
file has left the next session worse off.

* **Last updated:** 2026-09-30 (session 4)
* **Verified against the repo:** 2026-09-30 — `tests/test_installer.py` and
  `tests/test_post_script.py` both pass and were mutation-tested (12 mutations,
  12 caught), `git ls-files --eol Update-Orca-Plugins.bat` says `i/crlf`,
  `plugins.json` confirmed live on `main`.

---

## 0. Sixty-second orientation

**The project.** One GitHub repo (`ajani190819-ops/Tests`, public) that holds
two experimental **OrcaSlicer slicing-pipeline plugins**, a **one-click Windows
updater** that installs them, and some standalone tools. The owner is a
beginner at Python and at 3D-printing internals — explain plainly, never hide
a failure (`AGENTS.md` §1).

**Read in this order:**

1. `AGENTS.md` — the rulebook. How to work, and the hard rules that break real
   users if you ignore them.
2. `MEMORY.md` — this file. Current state, decisions, next actions.
3. `docs/ROADMAP.md` — the plan for work C and D, in detail.
4. `docs/ORCA-PLUGIN-FACTS.md` — OrcaSlicer plugin facts. Binding. Do not
   re-derive.
5. `README.md` — the human-facing tour, if you need the user's-eye view.

**One-line status:** the updater is built, tested and **live on `main`**; both
plugins ship but **neither has ever run in a real OrcaSlicer**; the design
questions for the next two work items are settled (§4), and the only thing
still owed by the owner is the detail of how Unlayered Infill failed (§5) —
which does not block starting.

---

## 1. Where things stand right now

| Thing | State |
| --- | --- |
| Repo | `ajani190819-ops/Tests`, **public** (must stay public — the updater downloads unauthenticated) |
| Default branch | `main`, at commit `8f2f6f1` "Recreate the plugin updater + reorganize the repo (PR #1)" |
| PR #1 | **MERGED** 2026-09-30 22:54 UTC, from branch `arena/01a0f42b-tests` |
| Updater | `Update-Orca-Plugins.bat`, ~596 lines, CRLF, **live**. Now self-updating (v1.1.0) and stages the standalone tool. |
| Catalogue | `plugins.json` — confirmed reachable at `raw.githubusercontent.com/.../main/plugins.json` |
| Wave Overhangs | v0.0.4, `plugins/wave-overhangs/`, ships, never run in real Orca |
| Unlayered Infill | v0.2.1, `plugins/unlayered-infill/`, ships, never run in real Orca |
| Standalone tool | `plugins/unlayered-infill/unlayered_infill_post.py` v0.2.1 — **works, functionally tested** |
| Contract test | `python3 tests/test_installer.py` → **passing** |
| Functional test | `python3 tests/test_post_script.py` → **passing** |
| CI | **None.** See the known gap in §3. |
| Work C (Wave Overhangs post-processing script) | not started — **unblocked**, design settled (C1 + C2 answered) |
| Work D (make Unlayered Infill actually work) | **D1 answered + standalone shipped**; step 3 (plugin-side diagnostics) open |
| Work E (updater self-update) | **done in code, never run on Windows** |

**What changed most recently and matters (session 4):** the owner finally gave
the D1 detail — *"they don't do anything: no change in the preview, and none
when reopening the G-code file."* Research turned up that **half of that
symptom is expected behaviour**: no slicer redraws its preview after
post-processing, and Orca's post-processing only runs on *Export G-code file*,
never on Print or Send. So the most likely explanation is that the plugin
never ran. In response, session 4 shipped
`plugins/unlayered-infill/unlayered_infill_post.py` — the same engine as a
standalone tool that does not need the plugin system at all — and made the
updater self-updating. **The standalone tool is the first piece of this repo
after `tools/nonplanar-infill-tool` with real evidence that it works.**

---

## 2. Facts that are already written down — do not re-derive them

Two files exist specifically so nobody has to learn these twice. Read them;
do not contradict them; if you think one is wrong, prove it on a real
OrcaSlicer build first and then update the file with the evidence.

* **`docs/ORCA-PLUGIN-FACTS.md`** — how OrcaSlicer's plugin system really
  behaves. The expensive ones: there is only **one** preset field (Others →
  Slicing Pipeline Plugin); `post_process_plugin` does not exist and reading it
  once bricked a plugin; the export step can run **twice**, so G-code
  transforms must be idempotent; the audit hook blocks lazy imports inside a
  capability, so import third-party deps at module load.
* **`AGENTS.md` §4** — the nine hard rules. The ones easiest to break by
  accident: the .bat must stay **CRLF** (never write it with a tool that
  converts line endings); a version bump is **three edits in lockstep**
  (PEP 723 header + `plugins.json` + the .bat's fallback list); no
  `|`, `^`, `%`, `!` in any catalogue field; never touch
  `keyboard-lighting/`; keep the GPL-3.0 attribution on Unlayered Infill and
  `tools/nonplanar-infill-tool`.

---

## 3. The honesty ledger — proven vs. not proven

Keep this distinction visible in every report. It is the thing most likely to
get quietly lost between sessions.

**Verified (by something that actually ran):**

* The catalogue, the .bat's fallback list, and the shipped plugin files all
  agree — `tests/test_installer.py`, which also replays the install loop
  (first run installs, second overwrites, sibling copies refresh,
  `_subscribed` cloud copies are left alone, `PLUGIN_ONLY` narrows the plan).
* The .bat keeps CRLF in the git blob, so the raw download is valid on
  Windows.
* The plan round-trip: catalogue → PowerShell plan lines → the .bat's
  `for /f` tokenization. This simulation caught a real bug in session 1
  (`tokens=2` parsed the word `version` instead of `0.0.3`).
* The engine inside Unlayered Infill has six test-pinned fixes over the
  reference tool it came from.
* The version is consistent everywhere it is claimed: the PEP 723 header, the
  display name, `PLUGIN_VERSION`, the G-code stamp, `plugins.json`, the .bat
  fallback list and the sidecar the updater writes. Mutation-tested: breaking
  any one of them fails `tests/test_installer.py`.
* `tools/nonplanar-infill-tool` — **the owner ran this successfully**,
  retroactively, on real exported G-code from their own slicer. This is the
  only piece of the whole project with *real-world* evidence behind it.
* **`plugins/unlayered-infill/unlayered_infill_post.py` genuinely transforms
  G-code** — `tests/test_post_script.py` builds a synthetic sliced cube and
  checks the result: 54 infill moves become 1080 wavy segments, extrusion is
  conserved to 1.6e-12 mm, the nozzle is returned to the layer plane at every
  section exit, a second pass is a no-op, absolute-E is refused, the input
  file is never touched without `--inplace`, and a wall-only file is reported
  as "nothing changed, here is why" instead of a false success. This is the
  strongest evidence in the repo short of a real print.
* **The engine is genuinely shared**, not shared-in-spirit: the test compares
  the copy in the plugin with the copy in the standalone byte-for-byte and
  fails on drift.
* **The .bat is structurally sound** — every `goto`/`call` target resolves,
  parentheses balance, and the self-update guards are pinned by tests. That
  is static analysis, not execution (see below).

**Not proven — say so every time:**

* **Nothing in `plugins/` has ever run inside a real OrcaSlicer.** Every test
  is against a fake harness. The first real slice is the real test; the
  traceback would land in `data_dir()/log/python_*.log`.
* The .bat has never been executed. There is no Windows in the sandbox. It was
  audited statically and simulated, not run.
* **`:self_update` and `:stage_tools` have never run** (added session 4). The
  logic is guarded and test-pinned, but network-dependent batch code that has
  never executed is not proven. If self-update misbehaves, the fallback is
  `--no-self-update`, and the on-disk .bat is never modified so it cannot be
  corrupted.
* The standalone tool has only seen **synthetic** G-code generated by the
  test, not a real slicer's output. The synthetic file was modelled on Orca's
  conventions (relative E, `;TYPE:` markers, `;HEIGHT:`/`;Z:`), but a real
  file will have variations.
* The tkinter window has never been displayed — there is no display in the
  sandbox. Only the CLI paths were executed.
* Wave Overhangs' object→bed XY mapping (`_bed_offset`) is unvalidated.
* Tuning defaults (`amplitude=-0.2`, `frequency=1.5`, `cell_mm=0.6`) are
  guesses, untested on hardware.
* Wave Overhangs is `sin(f·x)` only — invariant along Y. Ridges, not a
  lattice.

**Known gap — CI was never set up.** Session 1 wrote a GitHub Actions workflow
to run the contract test on every PR, but the push token was not allowed to
create files under `.github/workflows/`, so it never landed. There is no
`.github/` directory in the repo today. The workflow body is in the PR #1
description if someone wants to paste it in through the GitHub web UI — note
it references the old path `test_installer.py`, which is now
`tests/test_installer.py`.

---

## 4. Decisions already made — do not reopen these

* **This repo is the updater's download source.** The old chain is dead:
  Support Fins was superseded by an official cloud plugin (source stays in
  `ajani190819-ops/support-fins`, do not resurrect it here), and the
  `orca-plugins` repo no longer exists on GitHub.
* **The updater is a `.bat`, downloaded once into `Downloads` and
  double-clicked.** No Python, Node or Git required on the user's machine.
* **No signed `.exe` installer.** It would remove Windows' "Unknown Publisher"
  prompt, but costs a yearly code-signing certificate and a packaging
  pipeline. The one-time "Unblock" checkbox is good enough. Revisit only if
  this project ever grows up.
* **Version is read from the downloaded file's PEP 723 header**, so the
  sidecar can never record a wrong version. The catalogue/fallback pair the
  test guards is now a display-and-fallback concern only.
* **Wave Overhangs will not carve until the splice is proven** — carving
  without the splice leaves a hole in the part. First slice after a fresh
  install never carves. That is intended, not a bug.
* **`keyboard-lighting/` is storage, not a project.** Unrelated personal code,
  parked here. Do not reorganize, review, lint or "fix" it.
* **Both form factors stay — plugin *and* standalone script, sharing one
  engine.** (Owner's answer to C1, 2026-09-30.) The pipeline plugin auto-runs
  inside Orca when it works; the standalone script always works, on any Orca
  version. An engine fix must therefore land in both, so the engine stays a
  separate, front-end-agnostic module. Do not retire the plugins.
* **The version lives in the plugin's display name, never in a capability
  name.** (Owner's answer, 2026-09-30.) The PEP 723 `name` header is spelled
  `<Name> v<version>`, so Orca's Plugins dialog shows it in the Name column
  next to its own Version column. Capability names stay fixed forever, because
  a process preset stores the capability name as its value — renaming one
  orphans the preset and Orca refuses to slice (see
  `docs/ORCA-PLUGIN-FACTS.md`). The version also appears in each plugin's
  *Check setup* first line and as a G-code stamp. A version bump is now a
  six-place lockstep edit; `tests/test_installer.py` enforces every one, and
  `AGENTS.md` §4 rule 3 lists them.
* **In the wave region, the waves REPLACE the slicer's own extrusions.**
  (Owner's answer to C2, 2026-09-30.) This matches the upstream
  WaveOverhangs fork: no double material, at the cost of careful G-code
  surgery — the slicer's infill/perimeter moves inside the wave-covered area
  have to come out. A safer "add waves on top" mode was offered and **not**
  chosen; do not build a `--reinforce` flag unless the owner asks for it.
* **D1 is answered and diagnosed — do not re-ask the open-ended version.**
  (Owner, 2026-09-30: *"they don't do anything — no change in the preview, and
  none when reopening the G-code file."*) The diagnosis, in order:
  1. **The preview can never show post-processing.** Expected behaviour in
     every slicer, not a bug ([OrcaSlicer#7489]). Half the symptom is a false
     alarm. To check a post-processor, export and re-open the exported file.
  2. **Post-processing runs on "Export G-code file" only** — not Print, not
     Send ([#4432]). **Prime suspect**: if the owner pressed Print, the
     plugin never ran, which also explains the unchanged reopened file.
  3. Unlayered Infill refuses absolute-E G-code by design; visible only in
     the result message.
  4. The default wave is ~0.09 mm on a 0.2 mm layer — real but subtle.
     `--full-strength` doubles it.
  5. A load failure (numpy/shapely for Wave Overhangs) would show as a
     traceback in `data_dir()/log/python_*.log`.

  [OrcaSlicer#7489]: https://github.com/OrcaSlicer/OrcaSlicer/issues/7489
  [#4432]: https://github.com/SoftFever/OrcaSlicer/issues/4432
* **The updater must never overwrite itself while running.** (Design decision,
  session 4.) `cmd.exe` streams a .bat from disk by byte offset as it
  executes; a self-overwrite can jump into garbage, and a bad download would
  leave the owner with no working updater. `:self_update` downloads to
  `%TEMP%`, verifies, and delegates. Now hard rule 11 in `AGENTS.md`.

---

## 5. Waiting on the owner

C1, C2 and D1 have all been answered and moved out of this section. **No
question is currently blocking work.**

Two things are owed *by us* to the owner, and should be asked at the next
natural opportunity rather than guessed at:

**D1-follow-up — which button, and was a plugin even selected?** D1 is
answered and diagnosed (§4), but two facts would collapse the remaining
uncertainty to zero:

* Did the owner press **Export G-code file**, or **Print / Send**? Post-
  processing only runs on export
  ([#4432](https://github.com/SoftFever/OrcaSlicer/issues/4432)). If it was
  Print, the plugin never ran and there is no bug to find.
* Was a plugin actually selected in **Process → Others → Slicing Pipeline
  Plugin** for the preset being used?

Do not block on these. The standalone tool sidesteps both, and is the thing
to point the owner at first.

**The Windows run.** `:self_update`, `:stage_tools` and the whole .bat have
still never executed on Windows. The first real run *is* the test. If it
fails, that is expected-unknown territory, not a surprise — say so plainly.

---

## 6. Next actions, in priority order

1. **Get the owner to actually run the standalone tool** — this is now the
   highest-value action in the whole repo. It is the only piece other than
   `tools/nonplanar-infill-tool` with functional evidence behind it, and a
   single real run converts "should work" into "works". Point them at
   `%USERPROFILE%\Downloads\OrcaPlugins\unlayered_infill_post.py` after an
   updater run, or straight at the file on GitHub.
2. **Work D step 3 — plugin-side diagnostics.** The standalone tool now
   explains *why* it changed nothing (no infill / no skin / nothing
   bracketed). The plugin shares the engine but not that reporting: it should
   surface the same detail through its result message and its Check setup
   capability, so the pipeline version stops being a black box. This is the
   direct fix for "it doesn't do anything" on the plugin side.
3. **Work C — Wave Overhangs** — design is now settled (C1 + C2 answered), so
   this is unblocked. Ship `plugins/wave-overhangs/wave_overhangs_post.py`:
   rebuild each layer's footprint from the G-code, feed `wave_core`, splice
   the wave moves back in, and **remove** the slicer's own extrusions inside
   the wave region (C2). This also deletes the unvalidated `_bed_offset`
   problem, because G-code is already in bed coordinates. Keep the pipeline
   plugin alongside it (C1).
4. **Get CI in place** (see §3) — small, and it protects the contract test.
5. **First real slice on real OrcaSlicer**, whenever the owner is at the
   machine. That is the only test that counts.

---

## 7. Environment notes (this sandbox)

* **No Windows, no OrcaSlicer, no 3D printer here.** The .bat cannot be
  executed; plugins cannot be loaded. Everything is static analysis,
  simulation, and the fake harness.
* `git` and `gh` are authenticated and work. Pushing `.github/workflows/*`
  has failed before (token scope) — expect it to fail again.
* Plain `curl` to `raw.githubusercontent.com` returns `000` (no direct egress).
  Use `gh api` or the agent's page-fetch tool to check what is live on `main`.
* Verification commands:

  ```bash
  python3 tests/test_installer.py            # catalogue / .bat / files agree
  git ls-files --eol Update-Orca-Plugins.bat # must say i/crlf
  ```

* **Branch names change every session.** Arena hands each chat a fresh
  `arena/<id>-tests` branch, and all work goes on the one you were given —
  never `main`, never a branch you invent. The branch history so far is in §8;
  do not trust a branch name hardcoded in any doc.

---

## 8. Session log — newest first

Append one entry per session. Keep entries short: what happened, what landed,
what was verified, what was left undone.

### Session 4 — 2026-09-30 — branch `arena/01a0f48b-tests`

**The owner said the plugins "don't do anything — no change in the preview,
and none when reopening the G-code file", and asked whether the installer
could update itself.**

Landed:

* **Diagnosed D1.** Two findings reframe it, and both are documented in
  `docs/ORCA-PLUGIN-FACTS.md`: (a) no slicer redraws its preview after
  post-processing, so "no change in the preview" is *expected*, not a bug
  (OrcaSlicer#7489; BrickLayers says the same); (b) post-processing runs on
  **Export G-code file only**, never on Print or Send (#4432) — the prime
  suspect for the plugin never having run.
* **Shipped `plugins/unlayered-infill/unlayered_infill_post.py`** — the same
  engine, verbatim, as a standalone double-click / CLI / `--inplace` tool
  that needs no plugin system. Added `--dry-run` and `--full-strength`, and
  a report that explains *why* when nothing changed.
* **Made the updater self-updating** (v1.1.0) by delegating to a verified
  temp copy rather than overwriting itself, and made it stage the standalone
  tool into `Downloads\OrcaPlugins`.
* **Corrected a wrong fact**: `post_process_plugin` *is* a documented preset
  key (a list of capability names). The old claim that it "appears nowhere in
  the official documentation" was wrong. The separate rule against reading it
  from inside a plugin still stands, for a different reason.
* Added `tests/test_post_script.py`; added hard rules 10 and 11 to
  `AGENTS.md`; fixed a stale "three edits in lockstep" in `README.md` (it is
  six).

Verified: both test files pass; 12 mutations, 12 caught (including four
weaknesses the harness exposed in the *new* checks, which were then
tightened). CRLF intact at 596/596.

Not done: nothing ran on Windows or in real OrcaSlicer; the tkinter window
was never displayed; the plugin still lacks the standalone's diagnostics
(work D step 3).

### Session 3 — 2026-09-30 — branch `arena/01a0f48b-tests`

* **Asked for:** put the version in the plugin name, so the owner can confirm
  in OrcaSlicer that the right build is installed.
* **Researched first (and it changed the answer):** the official wiki says the
  Plugins dialog already has a Version column, and that a preset stores the
  **capability** name as its value. So the version went into the PEP 723
  *display name* (free) and not into capability names (would orphan the
  owner's process preset on every update and block slicing). Both facts are
  now in `docs/ORCA-PLUGIN-FACTS.md`.
* **Landed:** Wave Overhangs **0.0.3 → 0.0.4**, Unlayered Infill
  **0.2.0 → 0.2.1** (bumped deliberately, so the very next update has a new
  number to verify against). Each plugin gained a `PLUGIN_VERSION` constant
  feeding its display name, its *Check setup* first line and a G-code stamp
  (`; wave-overhangs v0.0.4`, `; unlayered-infill v0.2.1`). The .bat now
  composes the sidecar's `plugin_name` as `"%PL_NAME% v%PL_VER%"` from the
  downloaded file's own header, so it cannot drift; its install summary was
  de-duplicated to `[UPDATED] Wave Overhangs v0.0.4 (was v0.0.3)`.
* **Verified:** `tests/test_installer.py` extended with five new assertions and
  **mutation-tested — all six deliberate breakages were caught** (name missing
  its version, `PLUGIN_VERSION` drift, stamp drift, .bat dropping the composed
  sidecar name, catalogue/file version drift, sidecar name not matching the
  installed file's header). Both plugins `py_compile` clean; the .bat is still
  490/490 CRLF with no bare CR.
* **Not done / unverified:** nothing ran in a real OrcaSlicer, as always. One
  residual unknown recorded in the facts file: whether Orca derives its
  per-plugin key from the display name (if it did, a rename could read as a new
  plugin). Nothing suggests it does, and the install folder is unchanged.

### Session 2 — 2026-09-30 — branch `arena/01a0f48b-tests`

* **Asked for:** a memory file, like `AGENTS.md` but for picking up the thread
  in a new chat after merging.
* **Landed:** this file (`MEMORY.md`), linked from `AGENTS.md` and
  `README.md`; corrected `docs/ROADMAP.md`, which still claimed PR #1 was
  unmerged and the updater not yet live; generalized the hardcoded session
  branch name in `AGENTS.md` §4 rule 8 (it named session 1's branch).
* **Answers obtained from the owner:** **C1 → keep both** the plugin and the
  standalone script, sharing one engine. **C2 → replace** the slicer's
  extrusions in the wave region (the `--reinforce` escape hatch was offered
  and declined). Both are now decisions in §4, and work C is unblocked.
  **D1 → "something else"** — it *was* tried, and the failure was not "didn't
  show up" and not "silently did nothing"; the specifics are still owed
  (§5).
* **Verified:** `python3 tests/test_installer.py` passes; `git ls-files --eol`
  still reports `i/crlf` for the .bat; PR #1 confirmed MERGED via `gh`;
  `plugins.json` confirmed served from `main`.
* **Not done:** no code touched, D1's detail still missing, still no CI.

### Session 1 — 2026-09-30 — branch `arena/01a0f42b-tests` → PR #1, merged

* **Landed:** recreated `Update-Orca-Plugins.bat` (one-click Windows updater)
  pointed at this repo; added `plugins.json`, `tests/test_installer.py`,
  `.gitattributes` (`*.bat -text`); reorganized the repo into `plugins/`,
  `tools/`, `tests/`, `docs/`; wrote `AGENTS.md`, `README.md`,
  `docs/ORCA-PLUGIN-FACTS.md`, `docs/ROADMAP.md`.
* **Verified:** contract test passing; static audit of the .bat (every
  `goto`/`call` label resolves, PowerShell line is batch-safe, no
  read-after-set inside parenthesised blocks); both `for /f` tokenizations
  simulated — **caught a real bug** (`tokens=2` → `tokens=3`); the contract
  test was mutation-tested; CRLF confirmed in the git blob.
* **Not done:** CI workflow blocked by token scope; nothing run on Windows or
  in a real OrcaSlicer.

---

## 9. End-of-session checklist — how to leave this file

Before you finish a session in which anything changed, do all of these:

1. **Update the header** — "Last updated" date, and what you actually verified.
2. **Update §1** if the state of the world moved (a PR opened or merged, a
   version bumped, a test started failing).
3. **Move anything you proved** from the "not proven" list in §3 into
   "verified" — and add anything newly known to be broken.
4. **Record answered questions** — an answer from the owner becomes a decision
   in §4, and comes out of §5.
5. **Re-cut §6** so the top item is genuinely the next thing to do.
6. **Add a session-log entry in §8** — what happened, what landed, what was
   verified, what was left. Be honest about the "left undone" line; it is the
   most useful line in the file.
7. **Mirror plans into `docs/ROADMAP.md`.** This file is the state; the
   roadmap is the plan. Neither should contradict the other.
8. **Re-run the checks** in §7 and paste the real result, not a remembered one.
