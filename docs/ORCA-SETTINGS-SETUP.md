# Orca settings and setup — first implementation

Undahang **0.0.53** (stable Orca identity **Wave Overhangs**) and
Unlayered Infill **0.4.11** add settings forms and an explicit setup assistant.
The owner selected **all controls visible**, and **approved process changes
only**. Setup keeps printer and filament profiles **read-only**, including relative E.
No inspector, adaptive toolpath, new capability, preset rewiring or automatic
preset saving is included.

## Settings

Open **Config** for the main slicing capability in Orca's Plugins dialog, or
its existing preset configuration dialog. Every existing tuning key appears
once, in the same labelled groups as its settings guide. Booleans/enums use
selects; numeric/auto/percentage inputs keep their units and inline notes.
Experimental controls stay visible, not hidden under Advanced. The diagrams
are explanatory schematics, **not toolpath previews or safety estimates**.

- The **scope banner** distinguishes global plugin configuration from a
  preset capability override. A preset override takes precedence. The bridge
  does not expose the preset category; check the host dialog title rather than
  assuming a preset configuration is a process configuration.
- **Save plugin settings** is explicit. In global scope it saves that
  capability globally. In preset scope Orca stages the capability override;
  **save that preset yourself in Orca to persist it**.
- Config save/restore is the existing host-controlled, explicitly requested
  capability-configuration workflow, not setup permission to edit printer or
  filament tuning. Setup transactions remain process-only.
- **Restore global defaults** resets global capability values. **Remove
  override (use global)** removes only this preset's override, not the global
  values. Both require inline confirmation (sandboxed config frames cannot
  use modal JavaScript dialogs), then the normal host flow. Restore is disabled
  if the host cannot report its scope or no override exists to remove.
- The **JSON fallback** remains on the same page. Loading a JSON draft does not
  save it. Invalid/unloaded JSON blocks a form save. Unknown keys and untouched
  stored values, including legacy JSON types or enum choices, round-trip.
- New/missing keys display this build's defaults. Existing defaults, backup
  recovery and migration behavior are retained. The HTML getter itself never
  reads/writes a preset, saves configuration or resets values; the existing
  host-driven migration/recovery lifecycle is unchanged.
- Read-only host contexts disable writes. No values are writable before the
  host supplies a configuration object. Late callbacks do not discard a newer
  draft; **Reload from Orca** explicitly discards it after confirmation.
- A host that ignores custom UI methods keeps its normal annotated JSON
  editor. A displayed page without the configuration bridge is non-writing
  and clearly identifies its contents as defaults, not stored values.

The existing Script capabilities also have a small form for their original
`settings_guide` switch. Those forms do not masquerade as the main capability's
configuration. Undahang's written guide labels its last backed-up snapshot
rather than claiming to read another capability's live preset override.

## Save button fix — Undahang 0.0.57 / Infill 0.4.13

The owner reports **Save plugin settings does not save**. The actual defect is
in the shared page: Save was `type="submit"` and persistence ran only from a
form `submit` listener. Both Orca 2.5.2 configuration iframes use
`sandbox="allow-scripts"`, **without `allow-forms`**. A browser blocks form
submission before dispatching that listener, so no `window.orca.saveConfig`
message reaches the host. Calling the listener directly in a DOM double hid
this bug; the old tests did not prove a user click worked.

Save is now **`type="button"` with a direct `click` handler**, including normal
keyboard activation. No host sandbox permissions are relaxed. The same
validation, loaded-value/read-only checks, value-preserving payload and
host-acknowledgment handling are retained. All four existing capability pages
share the correction. Global Save uses Orca's global config route; a preset
Save stages the override and **still requires saving that preset in Orca**.
No automatic preset save, setup transaction or printer/filament tuning write
was added. A request is not called saved before a matching host response.

Host source pinned at `f92a7c065c5718b8ef1baac1cd704df2911238f4`:

- [Plugins iframe](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/resources/web/dialog/PluginsDialog/index.html#L173-L174)
  and [preset iframe](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/resources/web/dialog/PluginsConfigDialog/index.html#L33-L34): both allow scripts, not forms.
- [Shared bridge](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/resources/web/dialog/js/plugin-config-ui.js#L58-L87):
  Save posts the configuration to the parent; `onConfig` delivers its response.
- [Global save response](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/plugin/PluginConfig.cpp#L618-L657)
  writes/echoes global storage; [preset handling](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/GUI/PluginsConfigDialog.cpp#L119-L141)
  stages the override and explicitly asks the owner to save the preset.

The shipped DOM runner now clicks Save rather than manufacturing submit
notifications, verifies one request per click and round-trips edited/unknown
values through simulated acknowledgment/reopening in global and preset scopes.
An independent headless-Chromium check uses the **unmodified pinned Orca
bridge and parent message handlers** in real `allow-scripts` iframes: all eight
page/scope cases reproduce the old blocked submission, then pass direct-click,
Space/Enter activation, response/reopening and refusal-preserves-store checks.
Native C++ storage/responses are simulated in that browser check: it is **not**
an installed Orca/WebView/disk-persistence test, or a physical-print result.
Browser packages/reports remain in the external cache, not Git or Drive.
The 22 settings/setup tests and 504 click-based DOM/bridge assertions pass.
All 562/562 mutations are detected, including four new Save regressions;
prior 558 definitions are unchanged. Clean 304-test pre/post gates pass with
exact restoration and zero skips/survivors/stale/errors (220.354 seconds on
this host). These automated results do not certify native persistence.
Print paths, tuning defaults, migrations, stable identities, fan/wall safeguards
and processed-preview scope are unchanged; only normal version stamps advance.

## Changelog visibility — Undahang 0.0.56 / Infill 0.4.12

In **File → Plugins**, select **Wave Overhangs** (Undahang) or **Unlayered
Infill**, open an existing capability's **Config** page, then use the
**Changelog — recent changes** jump link by the installed version. The
read-only section carries the newest three releases from that plugin's own
`CHANGELOG.md`, also used by Check setup and the Description summary. It is
outside the settings form, works without the host configuration bridge and
never saves settings or touches presets. Both main and Script Config pages
include it. Full Markdown history remains in Git; older history stays archived.

This does **not** populate Orca's separate native **Changelog** tab. The reviewed
**v2.5.2** source, pinned at `f92a7c065c5718b8ef1baac1cd704df2911238f4`, shows:

- The [PEP 723 metadata parser](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/plugin/PluginFsUtils.cpp#L619-L634)
  accepts name/description/author/version and dependency/runtime fields; it
  does not read a local changelog key.
- The [Plugins dialog](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/GUI/PluginsDialog.cpp#L363-L364)
  copies the descriptor's changelog vector into its view-model, then
  [sends it to the page](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/GUI/PluginsDialog.cpp#L220-L228).
- [Cloud changelog lookup](https://github.com/OrcaSlicer/OrcaSlicer/blob/f92a7c065c5718b8ef1baac1cd704df2911238f4/src/slic3r/plugin/CloudPluginService.cpp#L306-L318)
  collects only cloud UUIDs and returns without fetching if none exist.
  Our independent single-file `.py` installs are local, not cloud listings.

Do not invent `changelog=` metadata, spoof a cloud identity, write Orca's
private descriptor storage or claim native-tab support from Check setup tests.
For that changelog increment, 21 settings/setup tests and 460 DOM/bridge-double
assertions covered four Config pages, exact recent notes/version labels,
script escaping, text-only rendering and no-bridge readability. The Save
path was exercised through synthetic submit events, missing the sandbox bug
corrected above; it did not prove real-button persistence. That release's
558/558 mutations were detected, including eight new changelog cases;
303-test clean pre/post gates pass with exact source restoration and no
skips/stale entries/errors/survivors (216.285 seconds on this host). Prior
550 mutation definitions are unchanged. Native Orca WebView visibility remains
unverified. Print engines, tuning defaults, fan/wall safeguards, capabilities,
setup permissions and processed-preview scope are unchanged.

## Explicit setup action

Run **Unlayered Infill - Check setup** or **Wave Overhangs - Settings guide &
check** from Plugins. These remain the same two stable Script identities.
Their existing last-export diagnostics/guide stay available; the setup section
also reads the active process's serialized prerequisite values when the host
supports inspection. Selecting a capability or reading settings is **not**
proof that its export callback ran.

The only built-in process fix in this pass is:

| Plugin | Condition | Exact serialized change | Why |
| --- | --- | --- | --- |
| Unlayered Infill | Process `enable_arc_fitting` is on | `"1"` (or the exact reported true string) → `"0"` | Its existing parser rewrites linear G0/G1 moves, not G2/G3 arcs. |
| Undahang | Any | None unconditionally proposed | Its existing parser supports input arcs; do not invent a prerequisite or replace taste/model settings. |

The native **Yes / No / Cancel** confirmation names the active process, shows
the entire exact before/after diff and explains that it stays unsaved. Only
**Yes** approves that diff. No/Cancel/closing, missing APIs or unreadable candidate values
never open a transaction. The assistant does not guess new infill densities,
skin counts, vase mode or detection/order/flow settings.

After approval it rechecks the copied identity/values, binds the **active
process only** with `begin_update()` (no named preset switch), verifies the
bound target, stages the allowlisted change and commits once. Changes to the
reviewed target/values abort. Host errors/cancellation or an unconfirmed result
are reported without retries or automatic saves. The host remains responsible
for validation, dirty state and normal unsaved-edit dialogs.

**Relative E is a printer setting**, not a process setting. Infill reports its
read-only state if available and gives the manual path **Printer Settings →
Use relative E distances**. It never offers to change it. Printer/filament
transactions and preset saving are not used anywhere in this assistant.

## Host compatibility and limits

Reviewed API/source contracts: Orca **v2.5.1** and main commit
`5841f9652d2fb2ab437797850829c19f3191efaf` (through 2026-11-16).
The owner now confirms **Orca 2.5.2**, using a custom modified-Ender-3 profile.
The refreshed host/background files in the [preview review](ROADMAP.md#processed-g-code-preview--feasibility-review)
match the earlier source snapshot; this is not a full re-audit of every API.
Feature checks, rather than a version-number assumption, govern staged edits,
native confirmation and the config bridge/context. Older or restricted hosts
fall back to non-writing advice and/or their JSON editor.

- [Configuration bridge](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/resources/web/dialog/js/plugin-config-ui.js): `getConfig`, `onConfig`, `saveConfig`, `restoreDefaults`, optional `getContext`.
- [Preset contract](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/docs/HLSD/preset-plugin-api.md): staged serialized values; commits leave the edited preset unsaved. Rollback does not undo every possible GUI/device callback side effect.
- [Preset membership](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/src/libslic3r/Preset.cpp): `enable_arc_fitting` is a process option; `use_relative_e_distances` is a printer option.
- [Host UI](https://github.com/OrcaSlicer/OrcaSlicer/blob/v2.5.1/src/slic3r/plugin/host/PluginHostUi.cpp): native blocking confirmation returns on the Script worker. No UI-thread message handler starts preset work here.

Setup prompts/transactions occur **only in the explicit Script execution**,
not configuration getters or synchronous slicing/export callbacks. No live
slicing references are retained. Preset reads are immediately copied to plain
values; host references are not retained across a confirmation. The public API
has no compare-and-swap lock across a final value read and host commit; the
assistant rechecks immediately before commit and relies on the host's target
validation, not a claim that it can atomically freeze other GUI edits.

Normal slice preview does not prove exported rewrites. Verify an explicit
**Export G-code file** by reopening the output; Print/Send behavior is host
version-dependent and is not certified by these source tests. Existing
experimental print/collision/strength limitations still apply.

## Latest owner feedback / next request

The owner confirms the new menus are visible, working and look good in their
Orca installation. This is **owner-reported settings UI feedback**, not proof
that every save/restore, setup transaction or export path has been tested.
The owner now reports a pre-wave unsupported outer strand and upward-curling
Wave edges on Orca 2.5.2, custom modified Ender 3, PLA at 205°C and Auto fan.
They explicitly request no fan duty above 50% for their 12 V fan. Their supplied
**0.0.53-stamped output** confirms walls before waves and full-speed S255 fan
requests; it is not a trial of the later ceiling. The
[0.0.55 print safeguards](../plugins/wave-overhangs/README.md#print-safeguards)
correct its speed-only wall grouping gap and retraction handoff, retaining the
0.0.54 S127 ceiling. Exact wall-helper diagnostics and synthetic fresh-export
checks do not establish a new owner export or physical/curl acceptance. PWM is
not voltage conversion; wiring and physical outcome remain unverified.
Printer/filament setup stays read-only; the limit affects output, not presets.

The next request is to preview **processed G-code without manually exporting
and reopening it**. [Feasibility findings](ROADMAP.md#processed-g-code-preview--feasibility-review)
remain a review, not a delivered feature or approval for adaptive toolpaths.
Native Preview is preferred; a separate viewer is acceptable if necessary.
Series renumbering remains paused; the patch continues 0.0.x (Infill 0.4.11).

## Verification boundary / development

For the original **0.0.53 / 0.4.11 settings increment**, the eight-target Windows build check and full **uncached**
repository suite pass in both dependency states. All eight EXEs remain
byte-identical. The 20 new settings/setup tests include 432 actual-page
DOM/bridge-double assertions. The full mutation run detects **487/487** cases,
including all 29 new Orca cases in their targeted behavior tests (no fallback),
with clean 271-test pre/post gates, exact source restoration, zero survivors,
stale entries, skips or errors (202.403 seconds). Defaults, section/note
metadata, slicing callback ASTs and normalized embedded-engine hashes match
the pre-increment baseline. These results do not establish native UI/print
acceptance.

`tests/test_orca_settings_ui.py` covers the four shipped forms and setup
transactions using instrumented host doubles. It executes the actual page
JavaScript with Node.js and `tests/orca_settings_dom.cjs`'s small DOM/bridge
double: types/unknown keys, all controls, labels, scope/read-only behavior,
invalid inputs, cancellation, failed saves and out-of-order callbacks. This is
**not browser layout, a native Orca WebView callback, a new export or a physical
print test**. Those checks are not inferred from prior current-use approval.
No broad owner matrix or repeated log collection is requested.

```sh
# Python dependencies as before; Node.js is also needed for page JS regressions.
python3 -m unittest tests.test_orca_settings_ui
python3 tools/sync_orca_ui.py --check
python3 tools/check_all.py
python3 tools/mutate.py
```

Edit `shared/orca/settings_ui.py`, then run `python3 tools/sync_orca_ui.py`.
Each installed plugin still has **one independent entry file**, with copied
helpers and no shared-file/dependency lookup. Existing top-level imports,
no-import-I/O, two capabilities, names/storage, attribution and synchronized
engines remain intact. This increment leaves both engines, tuning defaults and
slicing callback ASTs unchanged apart from the normal versioned Infill stamp.
Inspector/toolpath experiments remain later proposals; ORCA-04/05 stay PARKED,
and further PLAN-02 optimization stays DEFERRED.
