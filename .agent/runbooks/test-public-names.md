# NAME-01 — public branding, compatibility acceptance

Approved brands: Weather Report, Undahang, Prism Lighting. Unlayered Infill and
Bluetooth Guard are unchanged. This release does not authorize lighting-file
changes without a fresh owner `engine.log`, or any parked Orca research.

## Automated boundary

Hub 2.8.70 labels the weather/lighting cards, unified tray, Settings and update
notices. Weather Report changes human-readable title, tray and notices only.
Undahang 0.0.52 changes its description, setup report, log headings and installer
headings. The Orca PEP package name remains `Wave Overhangs`; both capabilities
remain `Wave Overhangs` and `Wave Overhangs - Settings guide & check`. Orca saves
these strings in preset/global references. Installer plans/sidecars, module,
class, storage and G-code identities stay unchanged. Do not rename them to make
the UI look uniform.

`tests.test_windows_apps.HubGroupedEntriesTests` executes production catalogue
code against offline, old, partial, duplicate and malformed responses. The
public-name tests replay production shortcut code with mocked file/ShellLink
I/O, check stable launch/data/preset identities and simulate a future plugin
version bump. Existing geometry, settings-recovery, import-audit, updater and
idempotence suites still apply. Mutation tests deliberately undo the branding
or break those compatibility boundaries. Mocks/cross-builds do not establish
native rendering, ShellLink behavior, foreground permission or real Orca use.

## Verified release checks

The seven-target Windows cross-build and uncached full repository checks passed
in both dependency states. All 358 mutations were detected (the prior 344 plus
14 naming cases): 184-test clean pre/post gates, zero surviving/stale/mutation
or gate errors, exact restoration, 150.593 seconds. Only Hub and Weather EXEs
changed; Guard, lighting and the other three EXEs rebuilt byte-identically.
Lighting/Infill tracked bytes and the embedded wave geometry/defaults/sections
were also compared with the preceding release and stayed identical. Native
Windows/Orca acceptance below has not been run.

## Windows checks (owner, not yet accepted)

1. Select the active Arena branch in Hub and install its released Hub update.
   Verify the same nine entries and General apps / Orca plugins / Minecraft
   mods grouping. Expect Weather Report, Prism Lighting, Undahang and separate
   Unlayered Infill. Offline/restart/refresh must not restore retired Hub names.
2. Check the unified tray and Settings labels at the owner's actual DPI. Update
   notices must use the approved names. Open, show/hide, stop and update routes
   must still reach `WeatherRadar.exe` / `KeyboardLighting.exe` in their
   existing folders. Do not infer that APP-01's Guard security block is fixed.
3. Open Weather Report. Check title, tray tooltip/menu, close-to-tray hint and
   foreground-only location guidance. Saved places, selected-point coordinates,
   map caches, attribution and rain-watch episodes must remain intact under
   `%LOCALAPPDATA%\WeatherRadar\`. Do not force a rain alert or confuse this
   check with WX-03 actual-position/permission acceptance.
4. Existing `Weather Radar.lnk` / `Keyboard Lighting.lnk` filenames are retained
   compatibility aliases. Public-name aliases are also supported if already
   pinned. Test none/legacy-only/public-only/both on an ordinary Desktop; updates
   must not create a missing icon, rename/delete an owner pin, or leave an
   existing alias aimed at a replaced copy. Lighting discovery recognizes both
   names on the Desktop and in the per-user Start Menu. Native ShellLink/Explorer
   execution remains separate from the mocked replay.
5. Prism Lighting's own UI/installer can still say Keyboard Lighting. No file
   under `keyboard-lighting/`, including its EXE/engine, is changed here. A fresh
   owner `engine.log` is required before its remaining own-app branding.

## Orca checks (owner, not yet accepted)

1. Back up the existing preset/config as usual. Install Undahang 0.0.52 via its
   separate Hub entry or `Orca-Plugins.bat`. The installer headline says
   Undahang, while plan/progress/package rows may correctly say Wave Overhangs.
2. In File → Plugins, the package still reads **Wave Overhangs**, Version
   **0.0.52**, with **Undahang** in its description. Existing saved process
   selection/config must remain selected; do not reselect it to hide a failure.
3. Run **Wave Overhangs - Settings guide & check**. Expect
   `Undahang v0.0.52 -- setup check`, followed by the stable-Orca-identity note.
   Logs use Undahang; settings recovery and backups keep the original keys.
4. Export a known small job. It should still use `; wave-overhangs v0.0.52`
   stamps and existing wave-block markers; a second pass must not double-apply.
   Preserve Unlayered Infill's independent enable/settings/install state.
5. Record build, entry surface and observed labels/settings. Do not call a
   cross-build, portable mock or this branding review physical-print acceptance.
   ORCA-01/02/03 and parked ORCA-04/05 remain on the master list.
