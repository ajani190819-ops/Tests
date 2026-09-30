# OrcaSlicer plugin lane (and friends)

Home of two experimental **OrcaSlicer slicing-pipeline plugins**, the
**one-click Windows updater** that installs them, and a couple of standalone
tools. Written to be readable by a beginner — if something here is unclear,
that's a bug; open an issue.

| Plugin | Version | What it does |
| --- | --- | --- |
| [Wave Overhangs](plugins/wave-overhangs/) | 0.0.4 | prints steep overhangs support-free by wave-propagating toolpaths into thin air |
| [Unlayered Infill](plugins/unlayered-infill/) | 0.2.1 | rewrites sparse infill onto a sine wave in Z so layers interlock instead of stacking as clean planes |

Also here:

* [`tools/nonplanar-infill-tool/`](tools/nonplanar-infill-tool/) — standalone
  double-click G-code tool (the predecessor of the Unlayered Infill plugin)
* [`keyboard-lighting/`](keyboard-lighting/) — unrelated personal project,
  stored as-is
* [`docs/`](docs/) — the roadmap, and the hard-won OrcaSlicer plugin facts

**Working on this repo with an AI assistant?** Point it at
[`AGENTS.md`](AGENTS.md) (the rulebook) and [`MEMORY.md`](MEMORY.md) (the
handoff file: where the work stands, what's already decided, what's next).
Starting a new chat? Those two files are how it picks up where the last one
left off.

## Updating your installed plugins (Windows)

1. Download [`Update-Orca-Plugins.bat`](Update-Orca-Plugins.bat) once (open it
   on GitHub, Raw button) and keep it anywhere — `Downloads` is fine.
2. Double-click it whenever you want to install or update.

It fetches `plugins.json` and the plugin files from this repo's `main`
branch and copies them into Orca's data folder
(`%APPDATA%\OrcaSlicer*\orca_plugins`, preferring a nightly folder), writing
the `.install_state.json` sidecar so the plugins show up already enabled.
Any other copies under `orca_plugins` are refreshed too (never `_subscribed`
cloud copies), and a copy of each file is staged in `Downloads\OrcaPlugins`
for Orca's UI installer. No Python, Node or Git needed — just Windows.

```
Update-Orca-Plugins.bat [data_dir] [--local] [--no-self-update] [--help]
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

1. Right-click `Update-Orca-Plugins.bat` → **Properties**.
2. On the General tab, tick **Unblock** at the bottom → **OK**.

(Or in PowerShell:
`Unblock-File "$env:USERPROFILE\Downloads\Update-Orca-Plugins.bat"`.)

You only download the .bat once — plugin updates flow through it, so the
prompt does not come back on every update.

## Checking which version you actually have

The version is written into the plugin's own name, so you can see it in four
places without digging:

| Where | What you see |
| --- | --- |
| File → Plugins, **Name** column | `Wave Overhangs v0.0.4` |
| File → Plugins, **Version** column | `0.0.4` (Orca reads this itself) |
| The updater's output | `[UPDATED] Wave Overhangs v0.0.4 (was v0.0.3) -- 39649 bytes` |
| **Check setup**, first line | `Wave Overhangs v0.0.4 -- setup check` |

The exported G-code is stamped too — search it for `; wave-overhangs v` or
`; unlayered-infill v` to see which build produced the file.

The one place the version is deliberately **not** shown is the *Slicing
Pipeline Plugin* dropdown in your process preset. That dropdown stores the
capability name, so if the name changed with every release, every update would
orphan your preset and Orca would refuse to slice until you re-picked it. The
capability names stay fixed on purpose; use Check setup instead.

## After installing

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm both plugins are enabled, two capabilities each.
3. Unlayered Infill needs **"Use relative E distances"** enabled (Printer
   Settings → Advanced).
4. Select a plugin per process preset under **Others → Slicing Pipeline
   Plugin**, slice something small, and run its "... - Check setup"
   capability.
5. **Export G-code file** — then drag that exported `.gcode` back into
   OrcaSlicer to look at it. See below for why.

## "It doesn't seem to do anything"

Work through these in order. The first two are not bugs, and between them
they explain most reports:

1. **The preview will never show it.** Orca draws the preview from the slice;
   plugins run afterwards, at export. Nothing redraws the preview — this is a
   known open request
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
   about 0.09 mm — real, but easy to miss by eye. Use the standalone tool's
   **Full strength** option, or a bigger amplitude, to see it clearly.
5. **Wave Overhangs never carves on the first slice of a session** (by
   design), and needs a genuine overhang to work on.
6. **Check the log** if a plugin failed to load: `<data dir>/log/python_*.log`.
   Wave Overhangs needs numpy and shapely.

## The standalone tool (no plugin needed)

`plugins/unlayered-infill/unlayered_infill_post.py` runs the **same engine**
as the plugin, but on a finished `.gcode` file. It does not care whether
Orca's plugin system is wired up correctly, which makes it both the easy
option and the way to prove the engine itself works.

The updater drops a copy in `%USERPROFILE%\Downloads\OrcaPlugins`.

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

## Changing things

* Replace a plugin file and bump its `# version = "..."` (PEP 723 header).
  A version bump is **six edits in lockstep** — see rule 3 in
  [`AGENTS.md`](AGENTS.md). `python3 tests/test_installer.py` fails if you
  forget one.
* The plugin and the standalone tool share one engine, stored as a verbatim
  copy in each. Change one, copy it into the other;
  `python3 tests/test_post_script.py` fails if they drift.
* The .bat must stay **CRLF**; `.gitattributes` keeps git from re-normalizing
  it. Don't let an editor convert it.
* Plans and open questions live in [`docs/ROADMAP.md`](docs/ROADMAP.md);
  the current state of the work lives in [`MEMORY.md`](MEMORY.md).

## Verification

```bash
python3 tests/test_installer.py     # catalogue / updater / files agree
python3 tests/test_post_script.py   # the engine really does rewrite G-code
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
