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
* `post_process_plugin` appears nowhere in the official plugin documentation.
  An earlier version of Wave Overhangs read that config key and gated
  behaviour on it; on a real build the key isn't there, and the plugin
  disabled itself while printing help text pointing at a setting the user
  cannot find. **Do not reintroduce config-key introspection to detect
  wiring.**

## The export step

* The export step (`psGCodePostProcess`) can run **TWICE** for one slice:
  file export and network upload are separate calls. Any G-code transform
  must be idempotent (Unlayered Infill stamps `; unlayered-infill v0.2.1` and
  returns the input untouched if the stamp is already there).
* At `psGCodePostProcess`, `ctx.print` and `ctx.object` are `None`. You get
  `gcode_path`, `host`, `output_name`.
* `ctx.config_value(key)` returns `None` if the key is absent.

## The audit hook

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
* The PEP 723 `# name = "..."` header is the **display name** in that dialog.
  Both plugins here spell it `<Name> v<version>` so the version is readable in
  the Name column too — the column people actually look at — and so the
  updater's sidecar, which must carry the same name, proves which build is on
  disk. This is safe precisely because the *plugin* name is not what a preset
  stores (see "The preset field" above).
* The capability names stay version-free for the same reason. The place a
  version therefore cannot appear is the **Slicing Pipeline Plugin** dropdown;
  use *Check setup*, which prints the running version as its first line.
* Not verified on a real build: whether Orca's per-plugin key is derived from
  the display name. If it is, a rename could make an update look like a new
  plugin. Nothing observed says it does, and the install folder
  (`orca_plugins/<orca_dir>/`) does not change.

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
