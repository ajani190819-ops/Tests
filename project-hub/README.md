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
unchanged. Hub 2.8.70 applies the approved public names **Weather Report** and
**Prism Lighting**, including offline startup, old/partial catalogues, the
unified tray, Settings and update notices. Weather Report also updates its
native title, tray and guidance. Lighting's own branding still says Keyboard
Lighting; the fresh-log receiver/Open follow-ups do not rename its files/UI.

Undahang 0.0.52 brands its description, setup report and logs. Its Orca package
and capability names remain **Wave Overhangs** / **Wave Overhangs - Settings
guide & check** because Orca stores them in presets/config references. Installer
headings explain the public brand without changing plan/sidecar identities.
There is no executable, module, saved-data, mutex, window-class or G-code-stamp
rename. Unlayered Infill and Bluetooth Guard keep their names.

Existing Desktop shortcuts under Weather Radar / Keyboard Lighting remain
compatibility paths; updates repoint either public or legacy name **only if
already present**. They do not create, rename or delete pins. Both lighting
shortcut names are recognized for existing-copy discovery. Native Windows and
real-Orca acceptance are in [the naming runbook](../.agent/runbooks/test-public-names.md).

Required entries exist before a network check. Partial/old catalogues cannot
recombine the plugin cards; remembered extra definitions are merged by id and
retain priority/slots for the current Hub session. Duplicate ids cannot create
extra cards. Activation, EXE close-and-prove gates and icon/tray ownership are
unchanged. Automated C mocks are not native fitting/interaction acceptance;
see `.agent/runbooks/test-hub-grouped-entries.md`.

## Independent plugin icons

Hub 2.8.66 gives Undahang its approved abstract wave-overhang icon in the
existing suite style. Unlayered Infill keeps its original three-ribbon icon.
`docs/brand/undahang-wave-v1.ico` has six 32-bit RGBA frames, a new cache-safe
filename and its own embedded resource. Both identities remain pinned after
old/incomplete catalogue responses, including offline startup. The approved
PNG master and generator preserve the selected art; no plugin algorithm,
capability, saved state or broader public-name rollout changes here.

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

The 2.8.71 follow-up extends Guard's app-owned show-only reveal to Keyboard
Lighting, including its existing elevated panel. Both need their matching app
updates; older builds retain the async fallback. Guard also releases its
UI-shared model lock during driver enumeration so a slow query does not hold
paint/heartbeat behind it. Open never substitutes Lighting's **Unfreeze** restart.
These source gaps failed controlled regressions before correction; the owner's
exact intermittent cause and native foreground behavior remain unconfirmed.
No additional logs, startup registration or elevation/security change is needed.

The owner previously reported that Lighting Open did **nothing visible**; the
latest report now says Lighting is fine and UAC/startup seem fixed. This is
current-use approval, not proof of the exact cause or an independent matrix.
The 2.8.72 follow-up covers a further source case: Hub must not accept
WS_VISIBLE/foreground as success when the active window is outside every
connected monitor. Lighting's owning UI can recover a fully offscreen normal
panel/dialog into the nearest work area, retaining its size. Connected/partial
and maximized placement, fresh intent and no-restart behavior are preserved.
This is not proof that the owner's window was offscreen. APP-01 is now
owner-reported good for Lighting current use; no new Guard result or additional
log/test request is inferred.

## Suite Settings

The Soft Slate Settings window centralizes Prism Lighting hold behavior,
keyboard Balance/Steady Colours, DNS Pilot scheduling, and Hub branch following.
Prism Lighting preferences update both `panel.json` (saved state) and
`theme.json` (the live engine input); its tray reloads an external panel change
before it can write stale in-memory values back. DNS Pilot still applies its
saved INI changes after restart.

Project Hub itself runs as the current user, so opening the Hub does not need an
administrator prompt. Apps that declare elevated access continue to request it
when explicitly opened, and update actions retain their stop-and-verify gates;
cancelled elevation or inaccessible targets do not bypass those checks.
