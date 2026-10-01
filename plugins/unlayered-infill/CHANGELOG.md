# Changelog — Unlayered Infill

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog (the Name column reads `Unlayered Infill v…`),
and running the **Unlayered Infill - Check setup** capability prints it along
with a short version history.

Dates are the day the change was made, not a release date.

## 0.3.1 — 2026-09-30

* **Fixed: the plugin could fail to load, showing up in the Plugins list as
  failed or disabled.** v0.3.0 wrote its first log line while OrcaSlicer was
  still loading the plugin. Orca watches file activity during loading, so that
  write could either throw a permission prompt at you mid-install or stop the
  plugin loading altogether. The first log line now waits until the plugin is
  actually used.
* **Fixed:** the updater was putting the standalone tool in the same folder as
  the plugin copies. Orca needs exactly one `.py` file per plugin folder, so a
  stray second file can stop a plugin loading. Tools now go in
  `Downloads\OrcaPlugins\tools\`.
* A log line that cannot be written can no longer interfere with a slice under
  any circumstances.

## 0.3.0 — 2026-09-30

* **Amplitude is now a percentage of layer height**, default `"200%"`. The old
  fixed millimetre value meant the effect scaled wrongly whenever you changed
  layer height. You can still give a plain number for an absolute millimetre
  amplitude.
* **The grid is now one nozzle wide by default** (`cell_mm: "auto"`). Infill is
  grouped into columns the width of your nozzle, so neighbouring extrusions
  move together instead of tearing away from each other.
* **A readable log now lands in your Downloads folder** as
  `orca-plugins.log`, so you can tell what the plugin did without guessing.
  Set `"log": false` in the capability config to switch it off.
* The standalone tool gained the same defaults, so the plugin and the
  script behave identically.

## 0.2.1 — 2026-09-30

* **The version is now part of the plugin's display name**, so the Plugins
  dialog shows exactly which build is installed.
* **Exported G-code carries a version stamp**, so a saved file records which
  version produced it.
* **Check setup prints the running version** as its first line.

## 0.2.0 — 2026-09-30

* First version published in this repository, installable with
  `Update-Orca-Plugins.bat`.
* Non-planar sparse infill: rides a sine wave in Z so successive layers
  interlock instead of stacking as clean planes, tapering flat where it meets
  the solid skin.
* Derived from Roman Tenger's NonPlanarInfill (GPL-3.0).
