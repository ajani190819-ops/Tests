# Project Hub

This concise Git entry point preserves the active artifact and its offline role.

The full pre-migration narrative and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.

## Grouped app catalogue

Hub 2.8.65 organizes the nine current entries under **General apps**, **Orca
plugins**, and **Minecraft mods**. Spatial HUD belongs to Minecraft mods;
**Undahang** and **Unlayered Infill** have separate Orca cards. Headings scroll
with their cards. Narrow windows put header controls on a second row; mouse
wheel, Page Up/Down, Home/End and arrow keys scroll the list.

Each plugin has its own Install/Update, Folder, Remove and Disable/Enable
routes. Disable pauses **Hub installs/updates only**; it does not change Orca's
saved enable state. The previous combined `OrcaDisabled` preference supplies
both initial defaults; per-plugin preferences then persist independently.
Status checks include current and legacy files in every `OrcaSlicer*` profile.
Updates preserve existing `.install_state.json` and user settings; removal is
nonrecursive and touches only the selected plugin's managed files. Failed
copies/removals are reported rather than claimed successful.

The old `orca` catalogue id now identifies the Undahang Hub card; the new
`unlayered-infill` Hub card retains its existing package id. Python filenames,
capabilities, presets, data paths and the shared `Orca-Plugins.bat` route are
unchanged. This is not the broader NAME-01 rename rollout: the plugin still
identifies itself to Orca as Wave Overhangs, and Weather Radar / Keyboard
Lighting retain their current names for now.

Required entries exist before a network check. Partial/old catalogues cannot
recombine the plugin cards; remembered extra definitions are merged by id and
retain priority/slots for the current Hub session. Duplicate ids cannot create
extra cards. Activation, EXE close-and-prove gates and icon/tray ownership are
unchanged. Automated C mocks are not native fitting/interaction acceptance;
see `.agent/runbooks/test-hub-grouped-entries.md`.

## Notification-area ownership

When Project Hub is running and its notification-area icon is available, it is
the suite's single tray host. Its menu opens, hides, or explicitly quits active
suite apps and exposes only verified safe toggles. Closing Hub hides its window;
choose **Exit Project Hub** to stop the host. If Hub exits, crashes, or Explorer
rebuilds its notification area, each already-running app restores its own tray
fallback automatically. See the Drive guide for the owner-facing workflow and
troubleshooting narrative.

The two-second local housekeeping poll verifies that Explorer still accepts the
Hub icon, rather than trusting a stale registration flag. It restores a silently
lost icon, retries temporary shell failures while Hub runs, and releases shared
ownership when registration cannot be confirmed. It never starts another Hub,
adds sign-in persistence, or revives the icon after Quit.

Catalogue refreshes release replaced file-loaded icons under the same short
lock used to draw them. Failed loads keep the last good icon. This prevents
unbounded USER-handle growth during long sessions. These are reproduced source
fixes, not proof that the owner's original "expires" report is resolved; Windows
soak/restart/resume checks remain in `.agent/runbooks/test-hub-longevity.md`.

## Open an app

Card and unified-tray **Open** actions share one show-or-launch route. An
already-running app is revealed/restored and brought forward, including a
tray-hidden window or its active modal dialog, without starting another copy.
Maximized placement is preserved. Native startup mutexes and catalogue image
process checks distinguish a starting app from an absent one; rapid repeated
clicks reuse the pending request.

The Hub retries a slow window on a short, bounded UI timer, not a blocking
wait. Switching to another app cancels deferred focus so a late startup does
not pull you back. Failed/inaccessible launches are reported, not silently
retried as extra instances. Child elevation contracts, update close-and-prove
gates, and Keyboard Lighting's engine are unchanged. Native Windows validation
remains separate from the automated checks; see
`.agent/runbooks/test-hub-open-to-front.md`.

## Suite Settings

The Soft Slate Settings window centralizes Keyboard Lighting hold behavior,
keyboard Balance/Steady Colours, DNS Pilot scheduling, and Hub branch following.
Keyboard Lighting preferences update both `panel.json` (saved state) and
`theme.json` (the live engine input); its tray reloads an external panel change
before it can write stale in-memory values back. DNS Pilot still applies its
saved INI changes after restart.

Project Hub itself runs as the current user, so opening the Hub does not need an
administrator prompt. Apps that declare elevated access continue to request it
when explicitly opened, and update actions retain their stop-and-verify gates;
cancelled elevation or inaccessible targets do not bypass those checks.
