# OrcaSlicer plugin lane (and friends)

Home of two experimental **OrcaSlicer slicing-pipeline plugins**, the
**one-click Windows updater** that installs them, and a couple of standalone
tools. Written to be readable by a beginner — if something here is unclear,
that's a bug; open an issue.

| Plugin | Version | What it does |
| --- | --- | --- |
| [Wave Overhangs](plugins/wave-overhangs/) | 0.0.3 | prints steep overhangs support-free by wave-propagating toolpaths into thin air |
| [Unlayered Infill](plugins/unlayered-infill/) | 0.2.0 | rewrites sparse infill onto a sine wave in Z so layers interlock instead of stacking as clean planes |

Also here:

* [`tools/nonplanar-infill-tool/`](tools/nonplanar-infill-tool/) — standalone
  double-click G-code tool (the predecessor of the Unlayered Infill plugin)
* [`keyboard-lighting/`](keyboard-lighting/) — unrelated personal project,
  stored as-is
* [`docs/`](docs/) — the roadmap, and the hard-won OrcaSlicer plugin facts

**Working on this repo with an AI assistant?** Read
[`AGENTS.md`](AGENTS.md) first — it's the rulebook.

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
Update-Orca-Plugins.bat [data_dir] [--local] [--help]
```

* `[data_dir]` — Orca's data directory; found under `%APPDATA%` if omitted
* `--local` — install plugin files found next to this .bat, no downloads
* Environment: `ORCA_DATA_DIR`, `PLUGIN_BRANCH` (default `main`),
  `PLUGIN_ONLY` (comma-separated plugin ids)

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

## After installing

1. Restart OrcaSlicer (needs newer than 2.4.2, or a nightly).
2. File → Plugins → confirm both plugins are enabled, two capabilities each.
3. Unlayered Infill needs **"Use relative E distances"** enabled (Printer
   Settings → Advanced).
4. Select a plugin per process preset under **Others → Slicing Pipeline
   Plugin**, slice something small, run its "... - Check setup" capability,
   and inspect the G-code preview before printing.

## Changing things

* Replace a plugin file and bump its `# version = "..."` (PEP 723 header).
  A version bump is **three edits in lockstep**: the header,
  `plugins.json`, and the fallback list inside the .bat.
  `python3 tests/test_installer.py` fails if you forget.
* The .bat must stay **CRLF**; `.gitattributes` keeps git from re-normalizing
  it. Don't let an editor convert it.
* Plans and open questions live in [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Verification

```bash
python3 tests/test_installer.py     # catalogue / updater / files agree
```

## Provenance

Split out of
[ajani190819-ops/support-fins](https://github.com/ajani190819-ops/support-fins);
Support Fins itself was superseded by an official cloud plugin and its source
stays upstream. The former `orca-plugins` repo is gone — **this repo is the
updater's download source**.
