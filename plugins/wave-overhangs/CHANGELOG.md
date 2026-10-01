# Changelog — Wave Overhangs

What changed in each version, newest first. The version you have is shown in
OrcaSlicer's **Plugins** dialog (the Name column reads `Wave Overhangs v…`),
and running the **Wave Overhangs - Check setup** capability prints it along
with a short version history.

**This plugin is still experimental and has never completed a verified real
print.** Treat every version here as a work in progress.

Dates are the day the change was made, not a release date.

## 0.0.6 — 2026-09-30

* **Fixed: the plugin could fail to load, showing up in the Plugins list as
  failed or disabled.** v0.0.5 wrote its first log line while OrcaSlicer was
  still loading the plugin. Orca watches file activity during loading, so that
  write could either throw a permission prompt at you mid-install or stop the
  plugin loading altogether. The first log line now waits until the plugin is
  actually used.
* A log line that cannot be written can no longer interfere with a slice under
  any circumstances.

## 0.0.5 — 2026-09-30

* **A readable log now lands in your Downloads folder** as
  `orca-plugins.log`, shared with Unlayered Infill. It records whether the
  plugin loaded, whether it was selected in your preset, and whether the
  export step ran. Set `"log": false` in the capability config to switch it
  off.

## 0.0.4 — 2026-09-30

* **The version is now part of the plugin's display name**, so the Plugins
  dialog shows exactly which build is installed.
* **Check setup prints the running version** as its first line.

## 0.0.3 — 2026-09-30

* First version published in this repository, installable with
  `Update-Orca-Plugins.bat`.
* Experimental: aims to print steep overhangs support-free by replacing the
  overhang region with wave-propagated toolpaths.
* Port of the WaveOverhangs idea from Dennis Klappe's OrcaSlicer fork.
* Requires `numpy` and `shapely`, which OrcaSlicer downloads for you the first
  time the plugin is installed. If that download fails the plugin will not
  load — fully quit and reopen OrcaSlicer to let it retry.
