# Changelog — the whole project

Everything that changed in this repository, newest first. Each plugin also has
its own changelog, which is the one to read if you only care about what
OrcaSlicer will do differently:

* [`plugins/unlayered-infill/CHANGELOG.md`](plugins/unlayered-infill/CHANGELOG.md)
* [`plugins/wave-overhangs/CHANGELOG.md`](plugins/wave-overhangs/CHANGELOG.md)

This file covers the repository as a whole: the updater, the tests, the
documentation and the handoff notes as well as the plugins. It is part of the
memory system described in [`MEMORY.md`](MEMORY.md) — a new chat should be
able to read `AGENTS.md`, `MEMORY.md` and this file and know where things
stand.

Dates are the day the work was done. "Not verified" means exactly that: no
real OrcaSlicer was involved.

---

## 2026-09-30 — install fixes

**Plugins:** Unlayered Infill **0.3.1**, Wave Overhangs **0.0.6**.
**Updater:** 1.1.0.

Reported by the owner: the new versions would not install, and the updater
appeared to be missing.

* **Fixed a bug that can stop a plugin loading.** Both plugins wrote their
  first log line while OrcaSlicer was still importing them. Orca installs a
  file-activity watcher before it imports any plugin, so a write at that
  moment has no plugin identity attached to it; it can prompt the owner
  mid-install or fail the load. Logging now happens on first use instead.
  Guarded by a new test that imports each plugin under a watcher that refuses
  every write.
* **Fixed the updater staging a non-plugin file beside the plugin copies.**
  Orca requires exactly one `.py` per plugin folder. Standalone tools now go
  to `Downloads\OrcaPlugins\tools\`.
* **Ruled out** `requires-python = ">=3.12"` as a cause — OrcaSlicer reads
  that field but does not enforce it.
* **Documentation:** README gained "If a plugin will not install" (read the
  Diagnostics tab first) and "Installing a test build" (`PLUGIN_BRANCH`).
  `AGENTS.md` gained hard rules 13 and 14. `docs/ORCA-PLUGIN-FACTS.md` gained
  the corrected audit-hook timing, the on-disk layout, and where a load
  failure is reported.
* **Not verified:** whether either fix is what the owner actually hit. The
  decisive evidence is the Plugins dialog's Diagnostics tab.

## 2026-09-30 — tuning that matches the owner's own build

**Plugins:** Unlayered Infill **0.3.0**, Wave Overhangs **0.0.5**.

* Unlayered Infill amplitude became a **percentage of layer height** (default
  `"200%"`) and the grid became **one nozzle wide** (`"auto"`), matching the
  behaviour the owner had in their own version.
* Both plugins now write a plain-English log to
  `<Downloads>\orca-plugins.log`, with a documented ladder for reading it.
* `tools/sync_engine.py` added: the shared engine exists twice (plain source
  in the standalone tool, an escaped string inside the plugin) and this keeps
  them byte-identical.
* `tests/fake_orca.py` and `tests/test_plugin_runtime.py` added: the plugin
  itself now runs in the test suite, not just the engine.
* **Not verified:** the fake Orca harness is reconstructed from the wiki, so a
  green run proves our logic is self-consistent, not that Orca drives us that
  way.

## 2026-09-30 — a standalone tool, and a self-updating updater

**Updater:** 1.1.0.

* Shipped `plugins/unlayered-infill/unlayered_infill_post.py`: the same engine
  as a double-clickable tool with a small window, so it works even when the
  plugin system does not. Decision C1 was to keep **both** forms.
* The updater now updates itself: it fetches a newer copy to `%TEMP%`,
  verifies it and hands over, never overwriting the file it is running from.
* **Not verified:** the `.bat` has never been executed on Windows from this
  sandbox.

## 2026-09-30 — the version is visible from inside OrcaSlicer

**Plugins:** Unlayered Infill **0.2.1**, Wave Overhangs **0.0.4**.

* Each plugin's display name now ends in `v<version>`, so the Plugins dialog
  shows which build is installed.
* Check setup prints the running version; exported G-code carries a stamp.
* **Capability names deliberately left alone** — a process preset stores the
  capability name, so renaming them would orphan presets and make OrcaSlicer
  refuse to slice.

## 2026-09-30 — the handoff file

* Added [`MEMORY.md`](MEMORY.md), wired into `AGENTS.md` and `README.md`, so a
  new chat can pick up mid-stride after a merge.

## 2026-09-30 — the updater, rebuilt

**Plugins:** Unlayered Infill **0.2.0**, Wave Overhangs **0.0.3**.

* Recreated `Update-Orca-Plugins.bat` after the old download sources were
  deleted, and made this repository the source of truth.
* Reorganised into `plugins/`, `tools/`, `tests/`, `docs/`.
* Added `AGENTS.md` (the rulebook), `docs/ROADMAP.md` and
  `docs/ORCA-PLUGIN-FACTS.md`.
