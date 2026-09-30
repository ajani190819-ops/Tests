# MEMORY.md — the handoff file. Read this second, right after `AGENTS.md`.

A new chat starts with no memory of the last one. **This file is that memory.**
It records where the work stands, what was already decided, what was tried, and
what to do next — so a fresh session can pick up mid-stride instead of
re-discovering the repo and re-asking questions the owner already answered.

If you are an AI assistant: this file carries the same weight as `AGENTS.md`.
Read it before touching anything, and **update it before your session ends** —
§9 is the checklist. A session that changed something and did not update this
file has left the next session worse off.

* **Last updated:** 2026-09-30 (session 3)
* **Verified against the repo:** 2026-09-30 — `python3 tests/test_installer.py`
  passed and was mutation-tested, `git ls-files --eol Update-Orca-Plugins.bat`
  says `i/crlf`, `plugins.json` confirmed live on `main`.

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
| Updater | `Update-Orca-Plugins.bat`, 486 lines, CRLF, **live** — a user can download it and it works |
| Catalogue | `plugins.json` — confirmed reachable at `raw.githubusercontent.com/.../main/plugins.json` |
| Wave Overhangs | v0.0.4, `plugins/wave-overhangs/`, ships, never run in real Orca |
| Unlayered Infill | v0.2.1, `plugins/unlayered-infill/`, ships, never run in real Orca |
| Contract test | `python3 tests/test_installer.py` → **passing** |
| CI | **None.** See the known gap in §3. |
| Work C (Wave Overhangs post-processing script) | not started — **unblocked**, design settled (C1 + C2 answered) |
| Work D (make Unlayered Infill actually work) | not started — can start; one diagnostic detail still owed (D1) |

**What changed most recently and matters:** PR #1 merging flipped the updater
from "would 404" to "actually works". Before the merge, `main` had no
`plugins.json` and no plugin files, so a downloaded .bat fell back to its
hardcoded list. Now the catalogue path is the live one. `docs/ROADMAP.md`
still had the pre-merge caveat; it has been corrected.

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
  only piece of the whole project with real-world evidence behind it.

**Not proven — say so every time:**

* **Nothing in `plugins/` has ever run inside a real OrcaSlicer.** Every test
  is against a fake harness. The first real slice is the real test; the
  traceback would land in `data_dir()/log/python_*.log`.
* The .bat has never been executed. There is no Windows in the sandbox. It was
  audited statically and simulated, not run.
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

---

## 5. Waiting on the owner

C1 and C2 were answered on 2026-09-30 and have moved to §4 as decisions. One
question is still open.

**D1 — what actually happened when Unlayered Infill was tried in
OrcaSlicer?** Asked 2026-09-30. The owner answered **"something else"** —
which rules out three of the four possibilities, so we do know this much:

* it was **not** "never actually tried" — it *was* tried;
* it did **not** simply fail to show up in Orca;
* it did **not** run quietly and leave the G-code unchanged.

So something more specific happened — an error message, a crash, a refusal,
mangled G-code, a missing dropdown entry, a dependency install that failed.
**The detail has not been given yet. Ask for it before planning work D**, and
ask in concrete terms: what was on screen, at what point (install / restart /
slice / export), and whether `data_dir()/log/python_*.log` has a traceback in
it. If the owner cannot remember, the cheapest path is to reinstall with the
updater, slice something small, and read that log.

Do not invent a diagnosis to fill this gap. Work D's plan (§6) is deliberately
written so that step 1 does not depend on knowing the answer.

---

## 6. Next actions, in priority order

1. **Get the D1 detail** (§5) — one question, asked concretely. It is cheap
   and it decides how much of work D is a bug hunt versus a rewrite. Do not
   block step 2 on it.
2. **Work D — Unlayered Infill** — ship
   `plugins/unlayered-infill/unlayered_infill_post.py`: the existing engine
   wrapped in the reference tool's proven UX (double-click window, CLI,
   `--inplace`, never overwrite the input, refuse absolute E, report what it
   did). Fastest credible path to "it actually works", because it does not
   depend on the plugin system at all — which is why it is safe to start
   before D1 is answered. Per the C1 decision, the plugin version stays and
   shares the engine.
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
