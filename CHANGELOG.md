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

## 2026-10-01 — Wave Overhangs Geometry preview prototype

**Wave Overhangs Geometry:** 0.1.2.

Added a separate `posSlice` geometry-stage prototype. It replaces reachable
unsupported slice area with obstacle-aware Wave ribbon geometry before Orca
creates perimeters and infill, so the intended result can appear in the normal
preview. The current Python bindings expose existing extrusion paths as
read-only, so this first version does not inject raw `ExtrusionPath` objects;
it documents that limitation and fails closed when live geometry mutation is
unavailable. Version 0.1.2 cleans dot fragments, preserves a continuous outer
bridge shell, marks generated Wave geometry as bridge surfaces, and hands the
bridge pieces to Orca in supported-edge-to-outer-edge order.

## 2026-10-01 — Wave controls and cleaner component order

**Wave Overhangs:** 0.0.15. **Updater:** 1.4.0.

Wave now exposes documented controls for propagation, order, cleanup, speed,
fan, and safety. The optional nearest component order reduces long same-distance
travel moves around holes without allowing farther fronts to print before nearer
ones. Safe defaults remain `auto` propagation, support-first components, and the
smart pattern. Endpoint cleanup now preserves interior curve points while
removing only short end stubs; the captured Cube fixture emits 399 smoothed
Wave moves.

## 2026-10-01 — Wave topology-safe diffraction

**Wave Overhangs:** 0.0.14. **Updater:** 1.3.0.

Wave propagation now treats holes entirely inside the unsupported plane as
obstacles, so fronts continue around both sides instead of only reacting to
holes on the supported boundary. Front simplification is also checked against
the unsupported-region boundary; curved fronts keep their original points when
a simplified chord would cross a hole or concave void. Internal-hole and
circular-hole regressions protect against the stray diagonal visible in the
comparison preview.

## 2026-10-01 — Wave handoff, cleanup, and pattern controls

**Wave Overhangs:** 0.0.13. **Updater:** 1.2.9.

Wave now makes an explicit non-extruding travel to each replaced bridge
segment's original endpoint before any retained original extrusion resumes.
This prevents a sharp handoff line from a Wave endpoint. Cleanup now removes
short complete Wave fronts and short endpoint stubs, while retaining substantial
uncovered bridge material. The captured fixture emitted 386 cleaned Wave moves.
New `smart`, `monotonic`, and `zigzag` patterns plus deterministic endpoint
policies are available; monotonic keeps a consistent front direction without
extruding between separate fronts. Uniform absolute-E bridge sections restore
`M82` and the prior command value safely; mixed E-mode sections fail closed.

## 2026-10-01 — repository organization and documentation cleanup

* Grouped the supplied OrcaSlicer reference PDFs under `docs/reference/` and
  the `Cube^2.STL` model plus captured G-code under `tests/fixtures/`.
* Removed the captured `orca-plugins.log` from version control and ignored
  runtime logs so a user's machine output cannot be mistaken for source.
* Reconciled the README, plugin guides, roadmap, facts, and handoff with the
  current permanent names, Wave 0.0.12 behavior, Unlayered Infill 0.3.4, and
  the tests that actually run in this repository.
* No plugin algorithm or installer behavior changed in this cleanup.

## 2026-10-01 — align and clean real Wave paths

**Wave Overhangs:** 0.0.12. **Updater:** 1.2.8.

The owner's visible Wave result exposed a 0.25 mm downward shift: Orca's layer
comment omitted the profile's `z_offset`, while the real bridge moves included
it. Wave now uses the bridge move's actual modal Z. Wavefronts are simplified,
sub-0.15 mm chatter is merged, line width follows Orca's real bridge width, and
isolated remnants shorter than half a line width are removed. The captured
cube drops from roughly 1,850 tiny Wave extrusion moves to 436 while preserving
25 substantial uncovered bridge fragments.

## 2026-10-01 — rebuild Wave as one transactional G-code pass

**Wave Overhangs:** 0.0.11. **Updater:** 1.2.7.

Research into the production WaveOverhangs forks confirmed that wave paths
intercept bridge residuals. The plugin now performs that interception directly
on Orca's exported `Bridge` and `Internal Bridge` toolpaths instead of passing
plans between two unreliable Orca callbacks. On the owner's real 0.30 mm cube
export, the offline regression creates three wave layers, removes only covered
bridge extrusion, retains 158 uncovered fragments, restores fan state, fails
closed, and is idempotent. Real-Orca export and physical-print verification are
still outstanding.

## 2026-10-01 — fix Wave's measured Orca integration failures

**Wave Overhangs:** 0.0.10. **Updater:** 1.2.6.

The owner's 0.30 mm `Cube^2.STL` export proved that Wave 0.0.9 planned four
layers but inserted zero wave blocks. Orca's internal `slice_z` values did not
match exported layer Z values, and its released Polygon binding rejected the
plugin's list constructor. Wave now keys plans by `print_z` and builds host
polygons through the supported empty constructor plus appended Points. Failure
still retains Orca's original bridge; replacement is not yet declared proven.

## 2026-10-01 — chooser uses the selected branch's updater

The branch chooser now downloads and validates `Update-Orca-Plugins.bat` from
exactly the selected branch and runs it from the Windows temporary folder. This
ensures a branch test includes its updater changes, not merely its plugin files.
A missing or invalid branch updater stops safely and never borrows `main`.
Neither downloaded batch file is overwritten while running.

## 2026-09-30 — final version-free plugin names

**Wave Overhangs:** 0.0.9. **Unlayered Infill:** 0.3.4.
**Updater:** 1.2.5.

* Both permanent package names are now simple and version-free: `Wave
  Overhangs` and `Unlayered Infill`. Capability names match and also remain
  version-free. Release numbers appear only in explicit version displays.
* Removed every compatibility-name special case from the catalogue, updater
  sidecars, tests and instructions.
* Pinned the complete Unlayered 0.3.0 control set as defaults so percentage,
  nozzle-grid, blending, frequency, segment length and full-strength controls
  do not depend on an old version-named configuration slot.
* This is the final identity migration. Reselect each capability once after
  installing; future releases will not rename either package.

## 2026-09-30 — restore Wave Overhangs' saved identity

**Wave Overhangs:** 0.0.8. **Updater:** 1.2.4.

* Restored `Wave Overhangs v0.0.6` as its permanent compatibility identity so
  Orca can reconnect to the pipeline selection/configuration saved before the
  unsuccessful 0.0.7 rename. The actual version remains visible separately.
* Updater output now prints both compatibility names explicitly: select
  `Wave Overhangs v0.0.6` and `Unlayered Infill v0.3.0`; check the Version
  column for the actual 0.0.8 / 0.3.3 releases.
* Replacement remains the chosen Wave behavior. Same-export, geometry-bounded
  bridge removal is still under development and is not claimed working here.

## 2026-09-30 — restore Unlayered Infill's working configuration identity

**Unlayered Infill:** 0.3.3. **Updater:** 1.2.3.

* Restored the permanent compatibility name `Unlayered Infill v0.3.0` so Orca
  reconnects to the configuration slot containing the owner's percentage
  amplitude, nozzle grid, blending, frequency and full-strength settings.
* Kept every current import-safety, logging and engine fix; this is not a code
  rollback to 0.3.0. The real version remains visible everywhere except the
  compatibility name.
* Wave Overhangs 0.0.7 was tested on real Orca and still did not replace the
  normal bridge in reopened exported G-code. The stable-name diagnosis is
  therefore disproven; Wave remains unresolved.

## 2026-09-30 — stable plugin identities, audited against Orca's PDF

**Plugins:** Wave Overhangs **0.0.7**, Unlayered Infill **0.3.2**.
**Updater:** 1.2.2.

* Both plugin names are now permanent and version-free. Orca's Plugin
  Development PDF says a preset's full capability reference contains the
  plugin name. Renaming the plugin every release could leave a preset pointing
  at yesterday's identity even while the new plugin appeared installed and
  activated. Versions remain visible in Orca's Version column, Check setup,
  logs, G-code stamps, standalone tool, and updater output.
* The updater sidecar now writes the same stable PEP 723 name and keeps the
  version only in `installed_version`.
* Added PDF-backed contract checks for PEP 723 dependency placement, one
  package class, typed capability bases, execute signatures, and capability
  registration. Wave Overhangs continues to declare numpy and shapely for
  Orca's bundled `uv` installer; it does not run `pip` itself.
* Recent changes continue to appear in the Plugins menu's Description tab and
  through Check setup. The dedicated Changelog tab cannot be populated by a
  side-loaded `.py`; Orca fills it from a cloud listing.
* After installing, reselect each pipeline capability once so Orca stores its
  corrected stable reference.

## 2026-09-30 — first Windows run fixes

**Updater:** 1.2.1. **Plugin versions unchanged:** Unlayered Infill 0.3.1,
Wave Overhangs 0.0.6.

The owner's first real Windows run found two failures that Linux static checks
could not expose:

* Windows PowerShell 5.1 kept GitHub's branch response as one nested
  `System.Object[]`. The chooser now enumerates the response directly and
  converts each commit URL to one string before requesting it.
* The 747-byte `plugins.json` catalogue was rejected by the 2,000-byte safety
  floor intended for large plugin files. Catalogue downloads now use a
  separate 100-byte floor and still have to parse as valid JSON before use.

The strict safety behavior itself worked: the updater reported the selected
branch, refused to borrow from `main`, and changed no Orca plugin files.
Regression checks pin both fixes. The corrected menu and successful install
still need a second Windows run.

## 2026-09-30 — choose and test any branch by double-clicking

**Updater:** 1.2.0. **Plugin versions unchanged:** Unlayered Infill 0.3.1,
Wave Overhangs 0.0.6.

* Added `Choose-Orca-Plugin-Version.bat`: a numbered menu of live GitHub
  branches, with released `main`, the five newest test branches, all branches,
  and manual entry. It remembers the chooser's last selection and has an
  obvious return to released `main`. No Command Prompt or token is needed.
* Test branches are now strict and all-or-nothing. The updater downloads and
  validates the catalogue and every plugin before changing Orca's folders. A
  missing test-branch file stops the install; it never silently borrows the
  released copy from `main`.
* Large start and finish banners show the selected branch and plugin versions.
  A plain double-click of `Update-Orca-Plugins.bat` still uses `main`.
* Extended `tests/test_installer.py` with static checks and a branch-preflight
  replay. **Not verified on Windows:** neither batch file can run in this
  sandbox; the first real double-click is still the real test.

## 2026-09-30 — released to `main`

Everything below this line was merged into `main` (PR #2), so
`Update-Orca-Plugins.bat` now installs **Unlayered Infill 0.3.1** and **Wave
Overhangs 0.0.6**. Until this merge the updater was still handing out
wave-overhangs 0.0.3 and unlayered-infill 0.2.0, which is why version numbers
appeared not to change.

Updaters older than **v1.1.0** cannot upgrade themselves. If you have one of
those, download `Update-Orca-Plugins.bat` once more; after that it keeps
itself current.

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
