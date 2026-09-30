# OrcaSlicer plugin lane

Public home of two experimental OrcaSlicer slicing-pipeline plugins, and the
one-click Windows updater that installs them.

| Plugin | Version | Orca folder | Dependencies | What it does |
| --- | --- | --- | --- | --- |
| Wave Overhangs | 0.0.3 | `WaveOverhangs` | numpy>=2.0, shapely>=2.0 | prints steep overhangs support-free by wave-propagating toolpaths into thin air |
| Unlayered Infill | 0.2.0 | `UnlayeredInfill` | none (pure stdlib) | rewrites sparse infill onto a sine wave in Z so layers interlock instead of stacking as clean planes |

Each registers two capabilities: the worker, and a `... - Check setup`
diagnostic. They need OrcaSlicer newer than 2.4.2 (or a nightly) and are
selected per process preset under **Others > Slicing Pipeline Plugin**.

## Updating your installed copies (Windows)

1. Download [`Update-Orca-Plugins.bat`](Update-Orca-Plugins.bat) once (open it
   on GitHub, Raw button) and keep it anywhere -- `Downloads` is fine.
2. Double-click it whenever you want to update.

It fetches `plugins.json` and the plugin files from this repo's `main` branch
and copies them into Orca's data folder (`%APPDATA%\OrcaSlicer*\orca_plugins`,
preferring a nightly folder), writing the `.install_state.json` sidecar so the
plugins show up already enabled. Any other copies under `orca_plugins` are
refreshed too (never `_subscribed` cloud copies), and a copy of each file is
staged in `Downloads\OrcaPlugins` for Orca's UI installer.

No Python, Node or Git needed -- just Windows. Downloads try `curl.exe`, then
PowerShell (TLS 1.2), then bitsadmin.

```
Update-Orca-Plugins.bat [data_dir] [--local] [--help]
```

* `[data_dir]` -- Orca's data directory; found under `%APPDATA%` if omitted
* `--local` -- install plugin files found next to this .bat, no downloads
* Environment: `ORCA_DATA_DIR`, `PLUGIN_BRANCH` (default `main`),
  `PLUGIN_ONLY` (comma-separated plugin ids)

**The repo must stay public**: the updater makes unauthenticated
raw.githubusercontent.com requests; a private repo 404s on every file.

## Changing the plugins

* Replace `wave_overhangs_orca.py` / `unlayered_infill_orca.py` and bump the
  `# version = "..."` line in the PEP 723 header. The updater reads the
  version from that header when it installs, so a bump needs no .bat edit --
  but update `plugins.json` **and** the fallback list inside the .bat in
  lockstep anyway; `python3 test_installer.py` fails if you forget either
  (CI runs it too).
* The .bat must stay CRLF. `*.bat -text` in `.gitattributes` keeps git from
  re-normalising the blob, and the test asserts the line endings -- Python's
  `Path.write_text()` would silently convert them to LF and corrupt it.

## Provenance

Split out of [ajani190819-ops/support-fins](https://github.com/ajani190819-ops/support-fins).
Support Fins itself was superseded by an official cloud plugin and no longer
ships here; its source stays upstream. The former `orca-plugins` repo is gone
-- **this repo is the updater's download source** (`REPO` inside the .bat).
