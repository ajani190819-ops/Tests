# OrcaSlicer plugin lane (and friends)

Home of three experimental **OrcaSlicer slicing-pipeline plugins**, the
**one-click Windows updater** that installs them, and a couple of standalone
tools. Written to be readable by a beginner — if something here is unclear,
that's a bug; open an issue.

| Plugin | Version | What it does |
| --- | --- | --- |
| [Wave Overhangs](plugins/wave-overhangs/) | 0.0.32 | replaces covered Bridge extrusion with support-anchored wave toolpaths whose ends land on the real wall and hole perimeters, optionally as G2/G3 arcs |
| [Unlayered Infill](plugins/unlayered-infill/) | 0.4.1 | rewrites sparse infill onto a wave in Z so layers interlock instead of stacking as clean planes — now in both directions at once, with a choice of wave shape |

These are the versions in the current test branch. Direct updater runs use
released `main`; use the chooser section below to install these branch files.

Also here:

* [`tools/nonplanar-infill-tool/`](tools/nonplanar-infill-tool/) — standalone
  double-click G-code tool (the predecessor of the Unlayered Infill plugin)
* [`keyboard-lighting/`](keyboard-lighting/) — unrelated personal project,
  stored as-is
* [`docs/`](docs/) — the roadmap, OrcaSlicer facts, and the reference PDFs
* [`tests/fixtures/`](tests/fixtures/) — the supplied `Cube^2.STL` model and
  captured real Orca export used by the Wave regression test

**Working on this repo with an AI assistant?** Point it at
[`AGENTS.md`](AGENTS.md) (the rulebook) and [`MEMORY.md`](MEMORY.md) (the
handoff file: where the work stands, what's already decided, what's next).
Starting a new chat? Those two files are how it picks up where the last one
left off.

## Download one file

**[`Orca-Plugins.bat`](Orca-Plugins.bat)** — this is the only file you need.
It is at the **top level of this repository**, in the file list above. Click
it, then press the **Raw** (or **Download**) button.

Or save this link directly (right-click → *Save link as…*):

<https://raw.githubusercontent.com/ajani190819-ops/Tests/main/Orca-Plugins.bat>

Double-click it and press **Enter**. That installs or updates every plugin.

You only need to download it once — it updates itself from then on.

It remembers the two things you'd otherwise retype each time: which build you
want and which OrcaSlicer folder to install into. Both are shown at the top of
its menu, and the menu can change either, or forget them and start fresh.

## What changed recently

* [`CHANGELOG.md`](CHANGELOG.md) — the whole project
* [`plugins/unlayered-infill/CHANGELOG.md`](plugins/unlayered-infill/CHANGELOG.md)
* [`plugins/wave-overhangs/CHANGELOG.md`](plugins/wave-overhangs/CHANGELOG.md)

Inside OrcaSlicer, run the plugin's **Settings guide & check** capability — it
prints the running version, what changed in the last three releases, and a
plain-English explanation of every setting with your current value for each.

## Updating your installed plugins (Windows)

**Download `Orca-Plugins.bat` from the top level of this repository.** Direct
link (right-click → *Save link as…*):

<https://raw.githubusercontent.com/ajani190819-ops/Tests/main/Orca-Plugins.bat>

1. Save it once and keep it anywhere — `Downloads` is fine.
2. Double-click it and press **Enter**.

It keeps itself up to date: on each run it checks whether a newer launcher
exists and hands over to it, so you only ever download it by hand once. It
never overwrites itself while running, so a failed download can't leave you
without a working launcher.

### The updater

There is one supported Windows entry point: `Orca-Plugins.bat`. It installs,
updates, validates, and refreshes the plugins. The former compatibility
launchers have been removed; download and run this file directly.

When run interactively, it lets you type the GitHub branch to install from;
press Enter for the released `main` build. For unattended runs, set
`PLUGIN_BRANCH`. It then fetches `plugins.json` and the plugin files from that
branch and copies them into Orca's data folder
(`%APPDATA%\OrcaSlicer*\orca_plugins`, preferring a nightly folder), writing
the `.install_state.json` sidecar so the plugins show up already enabled.
Any other copies under `orca_plugins` are refreshed too (never `_subscribed`
cloud copies), and a copy of each plugin is staged in `Downloads\OrcaPlugins`
for Orca's UI installer. The standalone tools — which are **not** plugins —
go in `Downloads\OrcaPlugins\tools\`, deliberately kept out of the plugin
folder: Orca requires exactly one `.py` per plugin folder and a stray second
file stops the plugin loading. No Python, Node or Git needed — just Windows.

```
Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
```

* `[data_dir]` — Orca's data directory; found under `%APPDATA%` if omitted
* `--local` — install plugin files found next to this .bat, no downloads
* `--no-self-update` — don't hand over to a newer copy of the updater
* Environment: `ORCA_DATA_DIR`, `PLUGIN_BRANCH` (default `main`),
  `PLUGIN_ONLY` (comma-separated plugin ids), `ORCA_NO_SELF_UPDATE`

### Do I need to re-download the .bat when something changes?

**Almost never.** Download it once and keep double-clicking the same file.

* **New plugin versions** — no re-download, ever. The .bat fetches the
  catalogue and every plugin file fresh from `main` on each run, so it always
  installs the latest.
* **Changes to the updater itself** — it now handles that too. On each run it
  checks whether a newer updater exists and, if so, runs the newer copy for
  that run.

  It deliberately does **not** overwrite itself. `cmd.exe` reads a `.bat`
  from disk as it executes it, so a file that rewrites itself mid-run can
  jump into garbage — and a failed update would leave you with no working
  updater. Instead the new copy is downloaded to your temp folder, checked
  (it must be the right size *and* actually look like this updater), and
  handed the job. Your file on disk stays exactly as it is and keeps
  fetching the newest version every time.

  So the only thing that never auto-updates is the ~40 lines that do the
  handover itself. If those ever need fixing, you'll be told to re-download.

**The repo must stay public**: the updater makes unauthenticated
raw.githubusercontent.com requests; a private repo 404s on every file.

### Windows says "Unknown Publisher" — expected, and one-time

The first double-click shows a security prompt because the .bat was
downloaded from the internet and we are not a company with a code-signing
certificate (batch files can't even carry one — there's nowhere in the
format to put a signature). It is not a warning about this file in
particular; Windows shows it for every downloaded .bat on earth.

To make it never appear again:

1. Right-click `Orca-Plugins.bat` → **Properties**.
2. On the General tab, tick **Unblock** at the bottom → **OK**.

(Or in PowerShell:
`Unblock-File "$env:USERPROFILE\Downloads\Orca-Plugins.bat"`.)

You only download the .bat once — plugin updates flow through it, so the
prompt does not come back on every update.

## Checking which version you actually have

Package and capability names are permanently version-free. This is important:
Orca saves those names inside process-preset references, so putting a release
number into them can orphan a saved preset.

| Where | What you see |
| --- | --- |
| File → Plugins, **Name** column | `Wave Overhangs` |
| File → Plugins, **Version** column | `0.0.32` (Orca reads the PEP 723 header) |
| The updater's output | `Wave Overhangs v0.0.32` |
| **Settings guide & check**, first line | `Wave Overhangs v0.0.32 -- setup check` |

The exported G-code is stamped too — search it for `; wave-overhangs v` or
`; unlayered-infill v` to see which build produced the file. The same rule
applies to `Unlayered Infill`; its current test-build version is `0.4.1`.

## After installing

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm both plugins are enabled, with two capabilities each.
3. Unlayered Infill needs **"Use relative E distances"** enabled (Printer
   Settings → Advanced).
4. Select the plugin you want in the process preset under **Others → Slicing
   Pipeline Plugin**, slice something small, and run its check capability —
   "Wave Overhangs - Settings guide & check" or "Unlayered Infill - Check
   setup". Both print what happened on the last run, what changed in recent
   versions, and what every setting means.
5. **Export G-code file** — then drag that exported `.gcode` back into
   OrcaSlicer to see the result. Neither plugin's work shows in the normal
   slice preview: both run after slicing, on the exported file.

## "It doesn't seem to do anything"

Work through these in order:

1. **The preview will not show the result.** Orca draws
   the preview from the slice; that plugin runs afterwards at export. Nothing
   redraws the preview — this is a known open request
   ([OrcaSlicer#7489](https://github.com/OrcaSlicer/OrcaSlicer/issues/7489)),
   and every slicer behaves this way. **Export the file, then drag that file
   back into OrcaSlicer** and look at the result.
2. **Press "Export G-code file", not "Print" or "Send".** Post-processing
   only runs on export
   ([#4432](https://github.com/SoftFever/OrcaSlicer/issues/4432)). Nothing in
   the UI tells you this.
3. **Unlayered Infill refuses absolute-E G-code.** Turn on Printer Settings →
   Advanced → **Use relative E distances**. Without it the plugin stops on
   purpose, because splitting a move under absolute E corrupts the file. The
   refusal shows in the result message, not in the G-code.
4. **The default wave is deliberately small.** It tapers to nothing at the
   solid skins and peaks at *half* the amplitude. On a 0.2 mm layer that is
   about 0.09 mm — real, but easy to miss by eye. Turn on `full_strength`, or
   use a bigger amplitude, to see it clearly.
5. **Your infill lines may be running the wrong way.** Up to 0.3.4 the wave
   varied along X only, so an infill line running along Y was lifted to one
   height for its whole length instead of waving. In 0.4.0 set `pattern` to
   `cross` (ripples in both directions at once), or aim the ripples across
   your infill with `wave_angle`.
6. **Wave Overhangs needs exported Bridge sections.** Version 0.0.32 performs
   planning and replacement in one G-code transaction, snaps Wave endpoints to
   walls/holes, and tapers endpoint flow for cleaner terminations. It removes only bridge
   extrusion covered by generated waves and retains every uncovered fragment.
7. **Check the log** if a plugin failed to load: `<data dir>/log/python_*.log`.
   Wave Overhangs needs numpy and shapely.

## If a plugin will not install, or never appears in the Plugins list

OrcaSlicer does not pop up an error when a plugin fails to load. It records it
quietly. Here is where to look, in order:

1. **Plugins dialog → click the plugin → `Diagnostics` tab.** This is the
   answer in almost every case: Orca prints the actual load error there. If
   the plugin is not in the list at all, go to step 3.
2. **`Plugin Info` tab** — check the *installed version*. The current test
   build reads **0.4.1** for Unlayered Infill and **0.0.32** for Wave
   Overhangs. Their permanent names are simply `Unlayered Infill` and `Wave
   Overhangs`; version numbers appear only in the separate Version column.
3. **Is the folder right?** Each plugin needs its own folder holding exactly
   **one** `.py` file plus the `.install_state.json` record:

   ```
   %APPDATA%\OrcaSlicer\orca_plugins\UnlayeredInfill\unlayered_infill_orca.py
   %APPDATA%\OrcaSlicer\orca_plugins\UnlayeredInfill\.install_state.json
   ```

   A **second** `.py` in that folder stops the plugin loading — Orca cannot
   tell which file is the plugin. (Use `OrcaSlicerNightly` instead of
   `OrcaSlicer` if you run the nightly build.)
4. **`%APPDATA%\OrcaSlicer\log\python_*.log`** — Orca sends Python error
   messages here. Open the newest one and look at the bottom.
5. Press **Refresh** in the Plugins dialog, or restart OrcaSlicer. Orca
   captures plugins at load time.

> **If the version looks old:** check whether your copy of the updater is an
> early one. Updaters before v1.1.0 cannot upgrade themselves — download
> `Orca-Plugins.bat` again (link at the top of this page) and run it
> once. From v1.1.0 on it keeps itself current automatically.

## Installing a test build from a branch

The updater downloads from **`main`** by default, which is where released
versions live. The current Wave 0.0.32 / Unlayered 0.4.1 work is on the test
branch, so put these two files together
in Downloads and double-click the chooser:

* `Orca-Plugins.bat`
* `Orca-Plugins.bat`

The chooser fetches the repository's real branches from public GitHub. It
shows released `main`, the five newest test branches, an option to show every
branch, and an option to type a branch yourself. Pick a number; no Command
Prompt and no GitHub login or token are needed.

To install a test branch, set `PLUGIN_BRANCH` before running the updater.
Leaving it unset installs the released `main` build.

A test build is all-or-nothing. Before changing Orca's folders, the updater
downloads and checks every plugin from the selected branch. If even one is
missing or invalid, it stops and names the file. It never fills the gap with a
plugin from `main`. Large banners at the beginning and end show the branch and
the plugin versions so it is clear what was installed.

After installing this final naming migration, reselect `Wave Overhangs` or
`Unlayered Infill` once in **Process → Others → Slicing Pipeline Plugin**.
Those exact version-free package/capability names will not change again. Use
the Plugins dialog's separate Version column to confirm 0.0.32 / 0.4.1.

## The log file — start here when something seems wrong

Plugins append a plain-text record of every run to their own Orca plugin
storage folder by default. That location is deliberate: Orca allows plugins to
write there without asking you to approve a log or state-file write during
slicing. Run the plugin's **Settings guide & check** action to print the exact path, or use
Plugins → select the plugin → **Show in folder** and open `orca-plugins.log`.

The log answers, in order: did the plugin load, was it selected, did the
export step run, and what did it do? Read it as a ladder:

| What you see | What it means |
| --- | --- |
| no file at all | not installed, or Orca never loaded it |
| `loaded` only | installed, but not selected in a process preset — or you never sliced |
| `pipeline step '...' seen`, no export step | post-processing plugin is running, but you pressed **Print/Send** instead of **Export G-code file** |
| `EXPORT STEP RUNNING` then `NOTHING CHANGED` | it ran, and the log names the reason |
| `EXPORT STEP RUNNING` then `DONE` | it worked |
| `REFUSED` | usually absolute E — turn on relative E distances |

It rolls over at ~1 MB. `ORCA_PLUGIN_LOG_DIR` is only a debug override; pointing
it outside plugin storage can reintroduce approval prompts. `"log": false` in
the plugin's config turns Unlayered logging off.

## The standalone tool (no plugin needed)

`plugins/unlayered-infill/unlayered_infill_post.py` runs the **same engine**
as the plugin, but on a finished `.gcode` file. It does not care whether
Orca's plugin system is wired up correctly, which makes it both the easy
option and the way to prove the engine itself works.

The updater drops a copy in `%USERPROFILE%\Downloads\OrcaPlugins\tools`.
That separate folder is intentional: Orca plugin folders must contain one entry
file, so the standalone tool must not sit beside a plugin `.py`.

* **Double-click it** → a small window. Choose your exported `.gcode`, tick
  *Full strength* if you want the wave obvious, press **MAKE IT WAVY**. It
  writes `<name>_unlayered.gcode` next to the original and never touches your
  input. Print that file.
* **From a terminal:** `python unlayered_infill_post.py part.gcode`
  (add `-n` to report without writing anything).
* **As a slicer post-processing script** — Process → Others →
  *Post-processing scripts*:

  ```
  "C:\Path\To\python.exe" "C:\Path\To\unlayered_infill_post.py" --inplace
  ```

  Keep the quotes; Orca appends the G-code path as the last argument.

It always reports what it did — how many infill moves it waved and the
largest Z shift — and if it changed nothing it says which of the reasons
above applies rather than claiming success.

## Where everything lives

| Path | What it is |
| --- | --- |
| `Orca-Plugins.bat` | **the one file you download.** Installs, updates, and validates the plugins |
| `plugins/` | **the two plugins that ship.** One entry `.py` per folder, each with its own README and changelog |
| `plugins.json` | the catalogue the launcher reads |
| `tools/` | the standalone post-processing tool, plus the sync/check scripts |
| `tests/` | the verification suite. `tests/fixtures/` holds the G-code the tests read |
| `docs/` | `ROADMAP.md` for plans, `ORCA-PLUGIN-FACTS.md` for what Orca actually does, `reference/` for saved wiki pages |
| `archive/` | things no longer shipped, kept rather than deleted — the retired Geometry plugin, and real test prints kept as evidence |
| `keyboard-lighting/` | unrelated to OrcaSlicer; a separate project that shares this repo |
| `AGENTS.md` | the rulebook for anyone (human or AI) changing this repo |
| `MEMORY.md` | where the work currently stands, and the full session history |
| `CHANGELOG.md` | everything that changed in the repository as a whole |

Real `.gcode` and `.3mf` files from the owner's printer live in
[`archive/test-prints/`](archive/test-prints) with a README explaining what
each one proved. They are evidence, not fixtures — no code reads them.

## Changing things

* Replace a plugin file and bump its `# version = "..."` (PEP 723 header).
  Keep every version surface in lockstep — see rule 3 in
  [`AGENTS.md`](AGENTS.md). `python3 tests/test_installer.py` fails if you
  forget one; run the changelog synchronizer after release-note edits.
* The plugin and the standalone tool share one engine, stored as a verbatim
  copy in each. Change one, copy it into the other;
  `python3 tests/test_post_script.py` fails if they drift.
* The .bat must stay **CRLF**; `.gitattributes` keeps git from re-normalizing
  it. Don't let an editor convert it.
* Plans and open questions live in [`docs/ROADMAP.md`](docs/ROADMAP.md);
  the current state of the work lives in [`MEMORY.md`](MEMORY.md).

## Verification

```bash
python3 tests/test_installer.py          # catalogue / updater / files agree
python3 tests/test_post_script.py        # the engine really does rewrite G-code
python3 tests/test_plugin_runtime.py     # the plugin itself runs, and logs
python3 tests/test_plugin_audit.py       # import-time writes are blocked
PYTHONPATH=/tmp/wavedeps python3 tests/test_wave_gcode.py  # real fixture
python3 tools/sync_engine.py --check     # the two engine copies match
python3 tools/sync_changelog.py --check  # embedded plugin notes match source
```

The second one builds a synthetic sliced cube and checks the tool waves it,
conserves the extrusion, puts the nozzle back on the layer plane, refuses
absolute-E, and does nothing on a second pass.

## Provenance

Split out of
[ajani190819-ops/support-fins](https://github.com/ajani190819-ops/support-fins);
Support Fins itself was superseded by an official cloud plugin and its source
stays upstream. The former `orca-plugins` repo is gone — **this repo is the
updater's download source**.
