# Shared, dependency-free Orca settings/setup helpers.
# Copied into each single-file plugin by tools/sync_orca_ui.py. Never import
# this file from an installed plugin: each plugin must remain independent.
# Uses json (already imported at plugin module load); no I/O at import time.

_SETTINGS_PAGE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
:root { color-scheme: light dark; font: 14px/1.5 system-ui,sans-serif; }
* { box-sizing: border-box; }
body { margin: 0; padding: 20px; color: var(--orca-fg,#263445); background: var(--orca-bg,#f3f5f8); }
h1 { font-size: 23px; margin: 0 0 6px; } h2 { font-size: 17px; margin: 0 0 12px; }
p { margin: 6px 0 12px; } code,textarea,pre { font-family: ui-monospace,monospace; }
pre { margin: 0; white-space: pre-wrap; overflow-wrap: anywhere; font-size: 12px; }
a { color: inherit; text-underline-offset: 3px; }
section,.banner { border: 1px solid var(--orca-border,#bcc8d7); border-radius: 12px; padding: 16px; margin: 16px 0; }
.banner { background: var(--orca-control-bg,#e7edf5); }
.grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(min(100%,310px),1fr)); gap: 18px 24px; }
label { display: block; font-weight: 650; } .key,.help { font-size: 12px; overflow-wrap: anywhere; }
.key { display: block; opacity: .8; } .help { margin: 6px 0 0; }
input,select,textarea,button { font: inherit; color: inherit; border: 1px solid var(--orca-border,#a2b2c8); border-radius: 7px; background: var(--orca-input-bg,#fff); }
input,select { width: 100%; padding: 8px; margin-top: 6px; }
textarea { display: block; width: 100%; min-height: 190px; padding: 10px; font-size: 12px; }
button { cursor: pointer; padding: 8px 13px; } button.primary { background: #355d92; color: #fff; }
button:disabled { cursor: default; opacity: .5; } :focus-visible { outline: 3px solid #5789c9; outline-offset: 2px; }
.actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
#status { min-height: 24px; overflow-wrap: anywhere; } .error { color: #b52b37; }
svg { display: block; width: min(100%,540px); height: auto; margin: 10px 0; color: #4a77b1; }
svg text { fill: currentColor; font: 12px system-ui,sans-serif; }
@media (prefers-color-scheme:dark) { body { color: var(--orca-fg,#e2e8f1); background: var(--orca-bg,#202b3a); } input,select,textarea,button { background: var(--orca-input-bg,#28394e); } .banner { background: var(--orca-control-bg,#28394e); } .error { color: #ffb2b9; } }
</style></head><body>
<h1 id="title">Plugin settings</h1>
<p id="intro"></p>
<p><strong id="release"></strong> · <a href="#recentChanges">Changelog — recent changes</a></p>
<div class="banner"><strong id="scope">Waiting for Orca configuration</strong><p id="scopeHelp"></p></div>
<p>Every tuning control is visible below. Nothing is hidden under Advanced. Auto values resolve on export, not in this form.</p>
<div id="diagram"></div>
<form id="settings" novalidate><div id="groups"></div>
<div class="actions"><button class="primary" id="save" type="button">Save plugin settings</button><button id="restore" type="button">Restore defaults</button><button id="reload" type="button">Reload from Orca</button></div>
<p id="status" role="status" aria-live="polite"></p>
<section id="confirmBox" hidden aria-labelledby="confirmText"><p id="confirmText" role="alert"></p><div class="actions"><button id="confirmYes" type="button">Confirm</button><button id="confirmNo" type="button">Keep editing</button></div></section>
<section><h2>JSON fallback</h2><p>Includes notes and unknown keys. Untouched stored values keep their original JSON types. Loading a JSON draft does not save it; use Save plugin settings afterwards.</p>
<label for="raw">Full configuration JSON</label><textarea id="raw" spellcheck="false"></textarea>
<div class="actions"><button id="loadJson" type="button">Load JSON draft into form</button></div></section>
</form>
<section id="recentChanges" aria-labelledby="changelogTitle"><h2 id="changelogTitle">Changelog — recent changes</h2>
<p>Read-only release notes bundled with this installed plugin. Local .py installs do not populate Orca's cloud-backed Changelog tab; read the recent entries here or run Check setup.</p>
<pre id="changelogText"></pre></section>
<script id="schema" type="application/json">__SETTINGS_SCHEMA__</script>
<script>
(function () {
'use strict';
var schema = JSON.parse(document.getElementById('schema').textContent);
var bridge = window.orca || {};
var has = function (obj,key) { return Object.prototype.hasOwnProperty.call(obj,key); };
var latest = null, draft = null, controls = [], revision = 0, dirty = false, rawDirty = false, sentRevision = -1;
var sentConfig = null, restorePending = false, context = {}, writable = false;
var pendingAction = null, confirmationOrigin = null;
function clone(obj) { return JSON.parse(JSON.stringify(obj)); }
function object(value) { return value !== null && typeof value === 'object' && !Array.isArray(value); }
function validateJSON(value) {
  if (typeof value === 'number' && !Number.isFinite(value)) throw new Error('JSON numbers must be finite; an overflowing number would otherwise become null.');
  if (value !== null && typeof value === 'object') Object.keys(value).forEach(function (key) { validateJSON(value[key]); });
}
function canonical(value) {
  if (Array.isArray(value)) return '[' + value.map(canonical).join(',') + ']';
  if (object(value)) return '{' + Object.keys(value).sort().map(function (key) { return JSON.stringify(key) + ':' + canonical(value[key]); }).join(',') + '}';
  return JSON.stringify(value);
}
function merged(cfg) {
  var result = Object.create(null);
  Object.keys(schema.defaults).forEach(function (key) { result[key] = clone(schema.defaults[key]); });
  Object.keys(cfg).forEach(function (key) { result[key] = clone(cfg[key]); });
  return result;
}
function status(text,error) { var node = document.getElementById('status'); node.textContent = text; node.className = error ? 'error' : ''; }
function node(tag,text) { var n = document.createElement(tag); if (text !== undefined) n.textContent = text; return n; }
// The host iframe allows scripts, not modal JS dialogs. Use inline confirmation.
function closeConfirmation() {
  pendingAction = null; document.getElementById('confirmBox').hidden = true;
  if (confirmationOrigin) confirmationOrigin.focus(); confirmationOrigin = null;
}
function askConfirmation(text,action,label,origin) {
  pendingAction = action; confirmationOrigin = origin;
  document.getElementById('confirmText').textContent = text;
  document.getElementById('confirmYes').textContent = label;
  document.getElementById('confirmBox').hidden = false;
  document.getElementById('confirmYes').focus();
}
function updateScope() {
  try { context = typeof bridge.getContext === 'function' ? bridge.getContext() || {} : {}; } catch (_) { context = {}; }
  writable = latest !== null && typeof bridge.saveConfig === 'function' && context.readOnly !== true;
  var preset = context.scope === 'preset', global = context.scope === 'global';
  document.getElementById('scope').textContent = preset ? 'Scope: preset plugin override (category not reported)' : global ? 'Scope: global plugin configuration' : 'Scope: not exposed by this Orca version';
  document.getElementById('scopeHelp').textContent = (preset ? 'Save stages this capability override in the host-selected preset. This bridge does not report its category: check the host dialog title. Save that preset yourself in Orca to persist it. Remove override uses global plugin values; it does not erase them.' : global ? 'Save changes this capability globally, not a process preset. Preset overrides take precedence. Restore defaults replaces the global values with this build\'s defaults.' : 'Check where you opened Config: Plugins edits global values; a preset dialog edits its capability override. Restore is disabled until the host reports the scope.') + (context.readOnly === true ? ' This host surface is read-only.' : '');
  document.getElementById('save').disabled = !writable || rawDirty;
  document.getElementById('loadJson').disabled = !writable;
  document.getElementById('raw').readOnly = !writable;
  var restore = document.getElementById('restore');
  restore.textContent = preset ? 'Remove override (use global)' : 'Restore global defaults';
  restore.disabled = !writable || typeof bridge.restoreDefaults !== 'function' || !(preset || global) || (preset && context.hasPresetOverride !== true);
  controls.forEach(function (item) { item.input.disabled = !writable || rawDirty; });
}
function parseInput(spec,input) {
  if (spec.choices) return clone(spec.choices[Number(input.value)]);
  var text = input.value.trim();
  if (spec.kind === 'number') {
    if (!/^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(text) || !Number.isFinite(Number(text))) throw new Error(spec.key + ': enter a finite number.');
    return Number(text);
  }
  if (spec.specials.indexOf(text.toLowerCase()) !== -1) return text.toLowerCase();
  var scalar = text.replace(spec.key === 'amplitude' ? /(?:%|x)$/i : /$^/, '');
  if (!/^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(scalar) || !Number.isFinite(Number(scalar))) throw new Error(spec.key + ': use a number' + (spec.specials.length ? ' or ' + spec.specials.join('/') : '') + (spec.key === 'amplitude' ? ', optionally followed by % or x.' : '.'));
  return text; // Scalar/auto settings accept strings; never coerce an untouched value.
}
function changed(item) {
  revision++; dirty = true;
  try {
    draft[item.spec.key] = parseInput(item.spec,item.input);
    item.invalid = '';
    document.getElementById('raw').value = JSON.stringify(draft,null,2);
    status('Draft only. Save plugin settings to send it to Orca.');
  } catch (error) { item.invalid = error.message; status(error.message,true); }
  document.getElementById('save').disabled = !writable || rawDirty || controls.some(function (c) { return !!c.invalid; });
}
function render(cfg) {
  draft = merged(cfg); controls = [];
  var groups = document.getElementById('groups'); groups.replaceChildren();
  schema.groups.forEach(function (group) {
    var section = node('section'), grid = node('div'); grid.className = 'grid';
    section.appendChild(node('h2',group.title));
    group.fields.forEach(function (original) {
      var spec = clone(original), box = node('div'), label = node('label'), help = node('p');
      var id = 'setting-' + spec.key, value = draft[spec.key];
      label.htmlFor = id; label.appendChild(node('span',spec.label + (spec.unit ? ' (' + spec.unit + ')' : '')));
      var key = node('code',spec.key); key.className = 'key'; label.appendChild(key);
      var input = node(spec.choices ? 'select' : 'input'); input.id = id;
      if (spec.choices) {
        var index = spec.choices.findIndex(function (option) { return JSON.stringify(option) === JSON.stringify(value); });
        if (index < 0) { index = spec.choices.length; spec.choices.push(clone(value)); }
        spec.choices.forEach(function (option,i) { var opt = node('option',(i >= original.choices.length ? 'Stored: ' : '') + JSON.stringify(option)); opt.value = String(i); input.appendChild(opt); });
        input.value = String(index);
      } else {
        input.type = 'text'; input.value = typeof value === 'string' || typeof value === 'number' ? String(value) : JSON.stringify(value);
        input.setAttribute('inputmode',spec.kind === 'number' ? 'decimal' : 'text');
        input.placeholder = 'Default: ' + JSON.stringify(spec.default);
      }
      help.id = id + '-help'; help.className = 'help';
      help.textContent = spec.help + ' Default: ' + JSON.stringify(spec.default) + '.';
      input.setAttribute('aria-describedby',help.id);
      var item = { spec:spec, input:input, invalid:'' }; controls.push(item);
      input.addEventListener(spec.choices ? 'change' : 'input',function () { changed(item); });
      box.appendChild(label); box.appendChild(input); box.appendChild(help); grid.appendChild(box);
    });
    section.appendChild(grid); groups.appendChild(section);
  });
  document.getElementById('raw').value = JSON.stringify(draft,null,2);
  rawDirty = false; updateScope();
}
function receive(cfg) {
  closeConfirmation();
  try {
    if (!object(cfg)) throw new Error('Orca did not provide a JSON object.');
    validateJSON(cfg);
  } catch (error) { latest = null; updateScope(); status(error.message + ' Nothing was saved; use the host JSON editor.',true); return; }
  latest = clone(cfg);
  var acknowledged = sentRevision === revision && (restorePending || (sentConfig !== null && canonical(sentConfig) === canonical(cfg)));
  if (dirty && !acknowledged) {
    updateScope(); status('Orca sent new values while you were editing. Your draft is kept; Reload from Orca discards it.',true); return;
  }
  var reply = sentRevision >= 0;
  dirty = false; sentRevision = -1; sentConfig = null; restorePending = false; render(latest);
  status(reply ? 'Orca returned the effective configuration. Review the values below; preset overrides still require saving the preset in Orca.' : 'Loaded from Orca. No changes made.');
}
document.getElementById('title').textContent = schema.title;
document.getElementById('intro').textContent = schema.intro;
document.getElementById('release').textContent = schema.release;
document.getElementById('changelogText').textContent = schema.changelog;
// Static schematics only, never a generated toolpath or safety/clearance prediction.
if (!schema.check) document.getElementById('diagram').innerHTML = schema.infill ? '<svg viewBox="0 0 500 96" role="img" aria-label="Schematic of waved infill between flat skins, not a toolpath preview"><path d="M12 15H488M12 76H488" fill="none" stroke="currentColor" stroke-width="4"/><path d="M12 46 Q42 8 72 46 T132 46 T192 46 T252 46 T312 46 T372 46 T432 46 T488 46" fill="none" stroke="currentColor" stroke-width="2"/><text x="15" y="94">Amplitude: Z offset · Frequency: ripples/mm · Schematic only</text></svg>' : '<svg viewBox="0 0 500 96" role="img" aria-label="Schematic of wave fronts growing from a supported edge, not a toolpath preview"><path d="M18 10V74" stroke="currentColor" stroke-width="7"/><path d="M22 20Q100 46 22 66M55 15Q180 46 55 70M95 12Q260 46 95 73M140 10Q340 46 140 76" fill="none" stroke="currentColor" stroke-width="2"/><text x="15" y="94">Supported edge → wave fronts · Spacing and overlap · Schematic only</text></svg>';
document.getElementById('raw').addEventListener('input',function () { revision++; dirty = true; rawDirty = true; updateScope(); status('JSON draft not loaded. Load it into the form or Reload from Orca before saving.'); });
document.getElementById('loadJson').addEventListener('click',function () {
  if (!writable) return;
  try {
    var cfg = JSON.parse(document.getElementById('raw').value);
    if (!object(cfg)) throw new Error('Configuration must be a JSON object, not an array or scalar.');
    validateJSON(cfg);
    revision++; dirty = true; render(cfg); status('JSON loaded as a draft only; nothing saved.');
  } catch (error) { status(error.message,true); }
});
document.getElementById('confirmYes').addEventListener('click',function () {
  var action = pendingAction; closeConfirmation(); if (action) action();
});
document.getElementById('confirmNo').addEventListener('click',function () { closeConfirmation(); status('Canceled. Your draft and saved values are unchanged.'); });
document.getElementById('reload').addEventListener('click',function () {
  if (latest === null) { status('No host configuration has arrived. Nothing changed.',true); return; }
  function reload() {
    dirty = false; sentRevision = -1; sentConfig = null; restorePending = false; revision++;
    render(latest); status('Reloaded the last configuration received from Orca.');
  }
  if (dirty) askConfirmation('Discard this unsaved draft and reload the last values received from Orca?',reload,'Discard draft',document.getElementById('reload'));
  else reload();
});
// Orca's allow-scripts iframe blocks form submission before submit fires.
// A plain button click also works for keyboard activation, without allow-forms.
document.getElementById('save').addEventListener('click',function (event) {
  event.preventDefault(); updateScope();
  if (!writable || !draft || rawDirty) { status('Not saved: read-only/unavailable bridge or an unloaded JSON draft.',true); return; }
  var invalid = controls.find(function (item) { return !!item.invalid; });
  if (invalid) { status(invalid.invalid,true); return; }
  sentRevision = revision; sentConfig = clone(draft); restorePending = false;
  status('Sent to Orca; awaiting its effective configuration. If no reply arrives, check the host error/status. No preset save was requested.');
  try { bridge.saveConfig(clone(draft)); }
  catch (error) { sentRevision = -1; sentConfig = null; status('Orca did not confirm the save: ' + error.message,true); }
});
document.getElementById('restore').addEventListener('click',function () {
  updateScope(); if (document.getElementById('restore').disabled) return;
  var scope = context.scope;
  var message = scope === 'preset' ? 'Remove this capability override and use its global values? The global values are not erased.' : 'Replace global plugin settings with this build\'s defaults? Existing preset overrides are not erased.';
  askConfirmation(message,function () {
    updateScope();
    if (context.scope !== scope || document.getElementById('restore').disabled) { status('Scope or permissions changed. Restore canceled.',true); return; }
    sentRevision = revision; sentConfig = null; restorePending = true;
    status('Restore requested. Awaiting Orca; cancel in the host confirmation to keep existing values.');
    try { bridge.restoreDefaults(); }
    catch (error) { sentRevision = -1; restorePending = false; status('Restore was not confirmed: ' + error.message,true); }
  },'Confirm restore',document.getElementById('restore'));
});
render({});
try {
  if (typeof bridge.onConfig === 'function') bridge.onConfig(receive);
  else if (typeof bridge.getConfig === 'function') receive(bridge.getConfig());
  else status('Orca configuration bridge unavailable. These are defaults, NOT your stored values. Use the host JSON editor on a compatible Orca version.',true);
} catch (error) { latest = null; updateScope(); status('Could not load Orca configuration: ' + error.message,true); }
})();
</script></body></html>'''


def _config_ui(for_check=False):
    """Pure HTML generation: never read/save configuration or open host UI."""
    defaults = dict(_CHECK_DEFAULTS) if for_check else annotated_defaults()
    settings = {k: v for k, v in defaults.items() if not k.startswith("_")}
    sections = [("_CHECK", "Report options", list(settings))] if for_check else _SECTIONS
    notes = defaults if for_check else _NOTES
    groups, placed = [], set()
    # Append stragglers so a future control cannot disappear from the form.
    for _heading, title, keys in list(sections) + [("_OTHER", "Other settings", list(settings))]:
        fields = []
        for key in keys:
            if key in placed or key not in settings:
                continue
            placed.add(key)
            default = settings[key]
            choices = _UI_CHOICES.get(key)
            if isinstance(default, bool) and choices is None:
                choices = [True, False]
            fields.append({"key": key, "label": key.replace("_", " ").capitalize(),
                           "default": default, "help": notes.get("_" + key, ""),
                           "unit": _UI_UNITS.get(key, ""), "choices": choices,
                           "kind": "number" if isinstance(default, (int, float)) and not isinstance(default, bool) else "scalar",
                           "specials": [default.lower()] if isinstance(default, str) and default.lower() in ("auto", "orca") else []})
        if fields:
            groups.append({"title": title.strip("= "), "fields": fields})
    schema = {"title": _UI_PRODUCT + (" — report options" if for_check else " — settings"),
              "intro": ("This capability controls only the written guide. Run " + _UI_CHECK_NAME + " explicitly for setup checks. This form changes report options only; setup fixes run only from the Script action." if for_check else
                        "Run " + _UI_CHECK_NAME + " for setup and last-export diagnostics. Existing experimental controls are not a collision, strength or print-quality guarantee."),
              "defaults": defaults, "groups": groups, "check": for_check,
              "infill": _UI_INFILL, "release": _UI_PRODUCT + " v" + PLUGIN_VERSION,
              "changelog": CHANGELOG_RECENT}
    # A stored </script> must not escape the inert JSON payload.
    payload = json.dumps(schema, ensure_ascii=True).replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    return _SETTINGS_PAGE.replace("__SETTINGS_SCHEMA__", payload)


_SETUP_PROCESS_KEYS = ("enable_arc_fitting", "sparse_infill_density", "top_shell_layers",
                       "bottom_shell_layers", "spiral_mode", "detect_overhang_wall")


def _setup_snapshot():
    """Copy serialized primitive values; no live host references cross a prompt."""
    bundle = orca.host.preset_bundle()
    process = bundle.current_process_preset()
    name = process.name
    if not isinstance(name, str) or not name:
        raise ValueError("The active process has no readable identity")
    values = {key: process.config_value(key) for key in _SETUP_PROCESS_KEYS}
    if any(value is not None and not isinstance(value, str) for value in values.values()):
        raise ValueError("Unexpected process-setting representation")
    result = {"process": name, "dirty": bool(process.is_dirty), "values": values,
              "printer": None, "relative_e": None}
    try:
        printer = bundle.current_printer_preset()
        result["printer"] = str(printer.name)
        relative = printer.config_value("use_relative_e_distances")
        result["relative_e"] = relative if isinstance(relative, str) else None
    except BaseException:
        pass  # Read-only advice can still work without printer introspection.
    return result


def _setup_changes(snapshot):
    """Only a demonstrated parser prerequisite, never taste/geometry defaults."""
    arcs = snapshot["values"].get("enable_arc_fitting")
    if _UI_INFILL and arcs is not None and arcs.strip().lower() in ("1", "true"):
        return [("enable_arc_fitting", arcs, "0",
                 "Unlayered Infill rewrites linear G0/G1 moves, not G2/G3 arcs.")]
    return []


def _setup_assistant_lines():
    """Called ONLY by explicit Script execute, on the host's Script worker.

    Native message() is blocking, so confirmation and the staged transaction
    stay on that worker. No UI-thread callback, custom-config bridge message,
    pipeline hook, printer/filament transaction, named preset switch or save.
    """
    out = ["", "--- active setup / approved process changes ---",
           "Printer and filament profiles are read-only. No preset is saved."]
    try:
        before = _setup_snapshot()
    except BaseException as error:
        out += ["Active preset inspection is unavailable on this host; advice only.",
                "No preset edits attempted. Existing export behavior is unchanged.",
                "Host detail: " + str(error)]
        if _UI_INFILL:
            out.append("Relative E is required: Printer Settings > Use relative E distances (manual only).")
        return out
    out.append("Process preset: " + before["process"] + (" (unsaved edits)" if before["dirty"] else ""))
    for key, value in before["values"].items():
        out.append("  " + key + " = " + (json.dumps(value) if value is not None else "unavailable"))
    if _UI_INFILL:
        out += ["Printer (read-only): " + (before["printer"] or "unavailable"),
                "  use_relative_e_distances = " + json.dumps(before["relative_e"]),
                "Relative E is required. If off/unknown, change it manually in",
                "Printer Settings > Use relative E distances. This assistant will not change it.",
                "Sparse infill and solid skins above AND below are needed for waving.",
                "Density, skin counts and vase mode are advice only; choose them for your model."]
    else:
        out += ["Undahang needs exported Bridge paths over unsupported regions.",
                "It does not need a universal process-setting replacement; none is proposed.",
                "Detection/order/flow tuning stays in the main plugin's settings."]
    changes = _setup_changes(before)
    if not changes:
        out.append("No process-setting changes proposed. No transaction opened.")
        return out
    diff = [key + ": " + json.dumps(old) + " -> " + json.dumps(new) + "\n  " + reason
            for key, old, new, reason in changes]
    out += ["Proposed serialized process-setting diff (not yet applied):"] + diff
    try:
        begin = orca.host.preset.process.begin_update
        confirm = orca.host.ui.message
        if not callable(begin) or not callable(confirm):
            raise AttributeError("Missing staged preset API or native confirmation")
    except BaseException:
        out.append("Read-only fallback: staged process API/native confirmation unavailable; apply manually if wanted.")
        return out
    message = ("Active process preset: " + before["process"] +
               (" (already has unsaved edits)" if before["dirty"] else "") +
               "\n\n" + "\n\n".join(diff) +
               "\n\nApply ONLY these listed process changes? Yes approves the entire diff. "
               "No/Cancel changes nothing. Printer/filament profiles stay read-only. "
               "The process stays unsaved; save it yourself in Orca if wanted.")
    try:
        if confirm(text=message, title=_UI_PRODUCT + " — review process diff",
                   buttons="yes_no_cancel", icon="question") != "yes":
            out.append("Not approved / canceled. No transaction opened.")
            return out
        # Reject stale approval; never switch back to the reviewed preset.
        if _setup_snapshot() != before:
            out.append("Preset or reviewed values changed during review. Nothing applied; run setup again.")
            return out
        tx = begin()  # ACTIVE process only; never request a named preset switch.
        if (tx.target_name != before["process"] or tx.category != "process"
                or tx.selection_changed is not False or _setup_snapshot() != before):
            out.append("Active target changed before staging. Nothing committed; run setup again.")
            return out
        for key, _old, new, _reason in changes:
            if key != "enable_arc_fitting" or new != "0" or not _UI_INFILL:
                raise ValueError("Unapproved process-setting key/value")
            tx.set(key, new)
        # No await/callback/user interaction between the last snapshot and commit.
        if _setup_snapshot() != before:
            out.append("Reviewed values changed before commit. Nothing committed; run setup again.")
            return out
        result = tx.commit()
        after = _setup_snapshot()
        if (result.target_name != before["process"] or result.category != "process"
                or result.selection_changed is not False
                or list(result.keys) != [item[0] for item in changes]
                or after["process"] != before["process"]
                or any(after["values"].get(key) != new for key, _old, new, _reason in changes)):
            out.append("Host did not confirm the exact result. Inspect the active process; no retry or save requested.")
        else:
            out.append("Approved process changes applied in Orca's edited preset. UNSAVED: save the preset yourself if wanted.")
    except BaseException as error:
        out += ["Host did not confirm completion. No retry or automatic save was attempted.",
                "Inspect the active process before continuing. Host detail: " + str(error)]
    return out
