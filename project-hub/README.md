# Project Hub (`ProjectHub.exe`)

Standalone native 64-bit Windows desktop application (`ProjectHub.exe` — ~101 KB, zero installer, zero console window, zero `.NET` / PowerShell dependency).

## How to Use

1. Download **`ProjectHub.exe`** (from GitHub Releases or `project-hub/ProjectHub.exe`) and place it anywhere (for example on your Desktop).
2. Double-click **`ProjectHub.exe`**.

From inside **Project Hub**, you can view live status and one-click update or launch every project in this repository:

* **Bluetooth Guard (`BluetoothGuard.exe`)** — Install, update, start, or stop phone-earbud auto-connect protection in one click.
* **Keyboard Lighting (`keyboard-lighting/`)** — Check running health, unfreeze/restart if hung, or download & apply updates.
* **Spatial HUD (`spatial-hud-template-26.3/`)** — Pick your Modrinth `mods` folder and download/install the latest Fabric `.jar` directly.
* **OrcaSlicer Plugins (`Wave Overhangs` + `Unlayered Infill`)** — Compare installed versions against GitHub (`plugins.json`) and install updates directly into OrcaSlicer without opening a terminal.
* **Arena Link (`arena-link-windows/`)** — View linked Git folder count, launch the app, or sync source updates.
* **Self-Update (`Update Hub`)** — Updates `ProjectHub.exe` in place from GitHub and automatically reopens.

## Building from Source (`project-hub/src/`)

Compiled cleanly with Clang/Zig targeting `x86_64-windows-gnu` (`-Wall -Wextra -Werror -Wl,--subsystem,windows`):

```bash
zig rc project-hub/src/hub.rc hub.res
zig cc -target x86_64-windows-gnu -Os -s -Wall -Wextra -Werror \
  -Wl,--subsystem,windows -municode \
  project-hub/src/project_hub.c hub.res \
  -luser32 -lgdi32 -lshell32 -lcomctl32 -lole32 -luuid -lwinhttp \
  -o project-hub/ProjectHub.exe
```
