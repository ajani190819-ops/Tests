# OrcaSlicer plugin-system facts

Established from the OrcaSlicer wiki snapshots and real failures in the
predecessor repos. Several were learned the hard way. **Do not re-derive them.
Do not contradict them.** If you believe one is wrong, prove it on a real
OrcaSlicer build first and update this file with the evidence.

## The preset field

* There is **ONE** preset field: Process preset → Others → **Slicing Pipeline
  Plugin**. That single selection drives every pipeline step, including the
  G-code export step. Print.cpp and PostProcessor.cpp both resolve the same
  preset capability refs.
* **The value a preset stores is just the capability's `get_name()` string.**
  (OrcaSlicer wiki, Plugin Development / Plugin System, read 2026-09-30.) The
  preset also carries a `plugins` array holding the full reference
  `<plugin_name>;<cloud_uuid>;<capability_name>`, used to restore a missing
  plugin — which is why a capability name may not contain `;`.
  **Consequence: renaming a capability orphans every preset that selected it.**
  Orca then "shows a missing-plugin notification and cannot slice until the
  reference is resolved". So capability names are a stable API — do **not** put
  a version number or anything else that changes per release into them.
* `post_process_plugin` **is** a real preset setting — corrected 2026-09-30.
  An earlier version of this file said it "appears nowhere in the official
  documentation". That was wrong. The plugin-system wiki documents it as a
  preset key holding a **list** of capability names, resolved by
  `PostProcessor.cpp`, e.g.

  ```json
  "post_process_plugin": ["G-code Benchmark (.py)", "header-stamp"]
  ```

  Best current reading: the UI field labelled *Slicing Pipeline Plugin* is
  this key under the hood, and it is multi-valued. **Not confirmed on a real
  build** — treat the UI label as the thing to tell users about.

  The separate rule still stands, for a different reason: an earlier Wave
  Overhangs read this key from inside the plugin via
  `ctx.config_value("post_process_plugin")`, got `None` on a real build, and
  disabled itself while printing help pointing at a setting the user could not
  find. **Do not reintroduce config-key introspection to detect wiring** — a
  plugin that is running is, by definition, already wired up.

## What the user can and cannot see (2026-09-30)

These two explain almost every "the plugin does nothing" report, and neither
is a bug in the plugin:

* **The 3D preview never shows post-processing.** Orca builds the preview from
  the slice, and the `psGCodePostProcess` step runs afterwards, on export. The
  preview is not redrawn. This is a known, still-open request
  ([OrcaSlicer#7489](https://github.com/OrcaSlicer/OrcaSlicer/issues/7489)),
  and it is not Orca-specific — BrickLayers tells its users the same thing:
  "none of the slicers show the changes automatically… we need to drag the
  exported gcode file back to the Slicer".
  **To verify a post-processor did anything, export the file and re-open that
  file.**
* **Post-processing runs on "Export G-code file" only.** It does *not* run on
  "Print" or "Send"
  ([SoftFever/OrcaSlicer#4432](https://github.com/SoftFever/OrcaSlicer/issues/4432),
  closed as not-planned). A user who only ever presses Print will never see
  any effect, and nothing in the UI says so.

## The export step

* The export step (`psGCodePostProcess`) can run **TWICE** for one slice:
  file export and network upload are separate calls. Any G-code transform
  must be idempotent (Unlayered Infill stamps `; unlayered-infill v0.3.0` and
  returns the input untouched if the stamp is already there).
* At `psGCodePostProcess`, `ctx.print` and `ctx.object` are `None`. You get
  `gcode_path`, `host`, `output_name`.
* `ctx.config_value(key)` returns `None` if the key is absent.

## The audit hook

* **The hook is installed before any plugin is imported.** The documented
  lifecycle is: `PluginManager::initialize()` -> `PythonInterpreter::initialize()`
  -> "install the audit hook (registers CPython's audit callback)" (step 2),
  and only then `discover_plugins()` (step 3) and `PluginLoader::load_plugin()`
  (step 4, which imports the module). What is absent at import time is the
  per-**call** audit *scope*: `PyPluginTrampoline` opens a
  `ScopedPluginAuditContext` around each capability call, and
  `set_audit_plugin_key()` is only stamped after the import.
* **Therefore: never touch the filesystem at module import time.** A write
  during import is an audited event with no plugin identity attached, so how
  `PluginAuditManager` categorises it (allow / prompt / deny) is unspecified.
  A prompt there lands in the middle of installing; a denial can fail the
  import, and a failed import is what "the plugin won't install" looks like.
  Do all I/O inside capability calls. (v0.3.0 / v0.0.5 got this wrong: they
  logged a "loaded" line at import. Fixed in 0.3.1 / 0.0.6; guarded by
  `tests/test_plugin_audit.py`, which imports each plugin under a hook that
  denies every write.)
* Swallow audit failures with `except BaseException`, not `except Exception`.
  An audit hook is not required to raise an `Exception` subclass.
* The audit hook is **OFF at module load and ON during capability calls**.
  Import every third-party dependency at module load time, never lazily
  inside a capability. During a capability call, any file open is audited,
  and paths containing `conf`, `cert` or `secret` are blocked outright with
  no prompt. `import numpy` reads `numpy/__config__.py` and
  `numpy/_core/_ufunc_config.py` — both contain "conf" — so a lazy numpy
  import inside a capability dies with `PermissionError` (OrcaSlicer issue
  #15944).
* Writes inside `data_dir()` need no prompt, and plugins live at
  `data_dir()/orca_plugins/<plugin>/`. So the state and log files these
  plugins write next to themselves are fine.

## Where a version number is visible to the user

* The Plugins dialog lists each plugin as a row of **Activate · Name · Version
  · Status**, and the *Plugin Info* tab shows source, author, installed version
  and latest version. Orca fills the Version column from the PEP 723
  `# version = "..."` header, so a correct header is already enough to check
  what is installed. (Wiki: Plugin System Overview / Managing Plugins.)
* The PEP 723 `# name = "..."` header is the **display name and a stable
  identity**. The Plugin Development PDF says a preset stores both the
  capability name and a full reference shaped
  `<plugin_name>;<cloud_uuid>;<capability_name>`. Therefore the version must
  not appear in the plugin name either: changing it can leave a preset pointing
  at yesterday's identity even though the new plugin appears installed.
  Session 6's real evidence matched that failure shape for Wave Overhangs:
  installed, activated and reportedly selected, but never invoked and absent
  from the log. This is a strong diagnosis, not yet proven until v0.0.7 runs.
* Package and capability names are permanently version-free: `Wave Overhangs`
  and `Unlayered Infill`. Versions appear only in the Version column, Check
  setup, logs, changelogs, updater output, tools and G-code stamps. The owner
  explicitly chose one final migration over retaining confusing old-looking
  compatibility names. Never rename these identities again. Updater sidecars
  exactly match the PEP 723 package names.

## Where plugins live on disk

* Two roots: `data_dir()/orca_plugins/` (local / side-loaded) and
  `data_dir()/orca_plugins/_subscribed/<user_id>/` (cloud). On Windows
  `data_dir()` is `%APPDATA%\OrcaSlicer` (nightly: `OrcaSlicerNightly`).
* **Each plugin lives in its own subdirectory containing exactly ONE entry
  file** — a single `.py` or a single `.whl`. `find_installed_plugin_entry`
  in `PythonFileUtils.cpp` picks the entry point by scanning the folder, so a
  second `.py` in there makes the choice ambiguous. Subdirectories whose name
  starts with `.` or `__` are ignored. Guarded by the install replay in
  `tests/test_installer.py`.
* A side-loaded folder is **not picked up on its own**: Orca also wants an
  install record next to the `.py`. That is the `.install_state.json` sidecar
  the updater writes.
* Consequence for the updater: the standalone tools are *not* plugins and must
  never be staged beside the plugin copies. They go in
  `Downloads\OrcaPlugins\tools\` (`%TOOLDIR%`), not `Downloads\OrcaPlugins\`.

## When a plugin fails to load, where the reason is

* **Plugins dialog -> select the plugin -> `Diagnostics` tab.** The wiki is
  explicit: "A plugin that fails to load shows its error in the Diagnostics
  tab." This is the first thing to read, before guessing.
* `data_dir()/log/python_*.log` — `PythonInterpreter` tees Python `stderr`
  there, so a traceback from a failed import lands in that file.
* The `Plugin Info` tab shows installed vs latest version; `Refresh` re-runs
  discovery; right-click offers `Reinstall` and `Show in folder`.
* `requires-python` is **read but not enforced** against the bundled
  interpreter, so `>=3.12` cannot by itself block an install.

## Misc

* Never call `orca.host.ui.*` from a slicing capability — wrong thread.
* A capability name may not contain `;` (it is the preset reference
  separator).
* Pipeline steps: `posSlice`, `posPerimeters`, `posPrepareInfill`, `posInfill`,
  `posIroning`, `posContouring`, `posSupportMaterial`, `posSimplifyPath`,
  `psWipeTower`, `psSkirtBrim`, `psGCodePostProcess`.
* Requires OrcaSlicer newer than 2.4.2, or a nightly. Plugins declare
  `requires-python >=3.12`.
* G-code section markers differ by slicer: PrusaSlicer writes
  `;TYPE:Internal infill`, Orca/Bambu write `;TYPE:Sparse infill`; skins are
  `;TYPE:Top surface` / `;TYPE:Bottom surface`, and internal solid is
  `;TYPE:internal solid infill`. Match `;TYPE:` lines case-insensitively.
* Unlayered Infill refuses absolute-E (M82) G-code: move-splitting under
  absolute E corrupts the file. Users must enable *Use relative E distances*.

## Design properties to keep

* **"Check setup" measures, it doesn't infer.** Both plugins record which
  pipeline steps actually fired in a small JSON file next to themselves
  (`*_state.json`, gitignored). Check setup reports the recorded facts. This
  works on any build, including UI neither of us has seen. Keep this
  property.
* **Failure modes are contained.** Test-enforced: geometry steps no-op;
  refusals and internal errors never touch the G-code file; an unexpected
  exception returns Success so a plugin bug cannot fail someone's export.
* **Wave Overhangs will not carve until the splice is proven.** Carving
  without the splice leaves a hole in the part, so carving stays off until
  the G-code splice has been observed running at least once. First slice
  after a fresh install never carves. That is intended.

## Known gaps (read before trusting output)

* Nothing in `plugins/` has run in a real OrcaSlicer. Every test is against
  a fake harness. The first real slice is the real test; if it fails,
  `data_dir()/log/python_*.log` has the traceback.
* Wave Overhangs is `sin(f·x)` only — invariant along Y. Ridges, not a
  lattice; interlocking is directional.
* Defaults are untuned on hardware (`amplitude=-0.2`, `frequency=1.5` are
  guesses; `cell_mm=0.6` vs ~0.42 mm solid line spacing is unverified).
* `full_strength` can displace by the entire gap to the nearest skin in a
  thin part. Off by default for that reason.
* Wave Overhangs' object→bed XY mapping (`_bed_offset`) is unvalidated. Its
  Check setup says so.
