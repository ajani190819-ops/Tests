# Project Hub

A clean, fast native Windows control center for checking live health, launching/unfreezing apps, and downloading updates across every project in this repository:

* **Keyboard Lighting (`keyboard-lighting/`)** — Shows live running status + active effects (`Keys` / `Light bar`), detects hung heartbeats automatically, and lets you **Open**, **Restart / Unfreeze**, or **Update / Repair** in one click.
* **Bluetooth Guard (`bluetooth-guard/`)** — Shows live guard status + how many paired earbuds/devices are protected from auto-connecting, with one-click **Open**, **Restart**, and **Install / Update**.
* **Spatial HUD (`spatial-hud-template-26.3/`)** — Inspects your Modrinth `mods` folder (`%APPDATA%\ModrinthApp\profiles\F5W\mods` or a custom folder), checks GitHub's `spatial-hud-latest` release feed, and installs/updates the verified Fabric `.jar` in one click.
* **OrcaSlicer Plugins (`Wave Overhangs` + `Unlayered Infill`)** — Compares installed plugin versions in `%APPDATA%\OrcaSlicer\plugins` against `plugins.json` on GitHub, updates plugins directly, or opens `Orca-Plugins.bat`.
* **Arena Link (`arena-link-windows/`)** — Shows app version + linked local Git repository count from AppData and syncs/launches the desktop app.

## Install / Launch

Double-click **`project-hub\Setup.bat`** (no Administrator prompt needed).

It builds **`ProjectHub.exe`**, adds **Desktop** and **Start Menu** shortcuts, and opens the dashboard. You can even download just `project-hub\Setup.bat` into any empty folder on Windows and double-click it — it automatically downloads the rest of Project Hub and can install/update every other project straight from the UI.
