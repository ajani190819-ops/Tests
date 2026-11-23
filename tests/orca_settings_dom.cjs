'use strict';
// Executes the shipped page JavaScript with a small DOM/Orca bridge double.
// Not a browser rendering test, an Orca WebView callback, or native acceptance.
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const pages = JSON.parse(fs.readFileSync(0,'utf8'));
const copy = value => JSON.parse(JSON.stringify(value));
let checks = 0;
function ok(value,message) { checks++; assert.ok(value,message); }
function equal(a,b,message) { checks++; assert.deepEqual(copy(a),copy(b),message); }
class Element {
  constructor(tag) { this.tagName = tag; this.children = []; this.listeners = {}; this.attributes = {}; this._text = ''; this.value = ''; this.disabled = false; }
  set textContent(text) { this._text = String(text); this.children = []; }
  get textContent() { return this._text + this.children.map(c => c.textContent).join(''); }
  appendChild(child) { this.children.push(child); return child; }
  replaceChildren(...children) { this.children = children; this._text = ''; }
  setAttribute(name,value) { this.attributes[name] = value; }
  focus() { this.focused = true; }
  addEventListener(event,handler) { this.listeners[event] = handler; }
  fire(event) { if (this.listeners[event]) this.listeners[event]({preventDefault(){}}); }
  descendants() { return [this,...this.children.flatMap(c => c.descendants())]; }
}
function harness(page,config = {},context = {scope:'global'},options = {}) {
  const schema = JSON.parse(page.match(/<script id="schema" type="application\/json">([\s\S]*?)<\/script>/)[1]);
  const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
  const roots = {};
  for (const id of ['schema','groups','title','intro','scope','scopeHelp','status','diagram','save','restore','reload','raw','loadJson','settings','confirmBox','confirmText','confirmYes','confirmNo']) roots[id] = new Element('div');
  roots.schema.textContent = JSON.stringify(schema);
  const document = {getElementById(id) { return Object.values(roots).flatMap(root => root.descendants()).find(node => node.id === id) || roots[id] || null; },createElement(tag) { return new Element(tag); }};
  const saves = [];
  let receive, restores = 0;
  const bridge = {getContext: () => context,getConfig: () => copy(config),saveConfig: cfg => { if (options.saveError) throw new Error('save refused'); saves.push(copy(cfg)); },restoreDefaults: () => { if (options.restoreError) throw new Error('restore refused'); restores++; }};
  if (!options.getOnly) bridge.onConfig = cb => { if (options.loadError) throw new Error('load refused'); receive = cb; if (!options.delayed) cb(copy(config)); };
  const window = {}; // Sandbox disallows window.confirm; tests must not provide it.
  if (!options.noBridge) window.orca = bridge;
  vm.runInNewContext(script,{window,document},{timeout:1500});
  return {schema,document,saves,context,bridge,get restores(){return restores;},
    node: id => document.getElementById(id),receive: cfg => receive(copy(cfg)),save: () => roots.settings.fire('submit'),
    edit(key,value,event='input') { const input = document.getElementById('setting-'+key); input.value = value; input.fire(event); },
    json(text) { roots.raw.value = text; roots.raw.fire('input'); },load: () => roots.loadJson.fire('click')};
}
for (const {html,name} of pages) {
  const schema = harness(html).schema;
  const fields = schema.groups.flatMap(group => group.fields);
  const boolean = fields.find(field => field.choices && field.choices.includes(true) && field.choices.includes(false)).key;
  let h = harness(html);
  equal(h.saves,[],name+': opening never saves');
  equal(h.restores,0,name+': opening never restores');
  for (const field of fields) {
    const input = h.node('setting-'+field.key);
    ok(input && !input.disabled,name+': every control editable/visible after host load');
    const all = h.node('groups').descendants();
    ok(all.some(n => n.tagName === 'label' && n.htmlFor === input.id),name+': control has a label');
    ok(input.attributes['aria-describedby'] && h.node(input.attributes['aria-describedby']),name+': help association');
  }
  ok(h.node('scope').textContent.includes('global'),name+': global scope');
  ok(h.node('scopeHelp').textContent.includes('Preset overrides take precedence'),name+': scope semantics');
  h.save(); equal(h.saves[0],schema.defaults,name+': defaults keep types');
  // Initial values must arrive before Save can ever send defaults.
  h = harness(html,{},undefined,{delayed:true});
  ok(h.node('save').disabled,name+': no save before initial callback');
  h.save(); equal(h.saves.length,0,name+': delayed load guard');
  h.receive({[boolean]:false}); h.save(); equal(h.saves[0][boolean],false,name+': loaded false preserved');
  // Legacy values, unknown enum entries, future keys and dangerous object keys round-trip unchanged.
  const cfg = JSON.parse('{"future":{"text":"</script><img src=x onerror=bad()>","nested":[1,false]},"__proto__":{"polluted":"no"}}');
  cfg[boolean] = 'false';
  const enumeration = fields.find(f => f.choices && typeof f.default === 'string');
  const numeric = fields.find(f => f.kind === 'number');
  if (enumeration) cfg[enumeration.key] = 'future-choice';
  if (numeric) cfg[numeric.key] = ['legacy-type',0];
  h = harness(html,cfg); h.save();
  for (const key of Object.keys(cfg)) equal(h.saves[0][key],cfg[key],name+': untouched '+key);
  for (const field of fields.filter(field => Object.prototype.hasOwnProperty.call(cfg,field.key))) {
    const input = h.node('setting-'+field.key), stored = cfg[field.key];
    if (field.choices) ok(input.children[Number(input.value)].textContent.endsWith(JSON.stringify(stored)),name+': displays stored select value');
    else equal(input.value,typeof stored === 'string' || typeof stored === 'number' ? String(stored) : JSON.stringify(stored),name+': displays stored scalar value');
  }
  ok(!Object.prototype.hasOwnProperty.call({},'polluted'),name+': no prototype pollution');
  h.edit(boolean,'1','change'); h.save(); equal(h.saves[1][boolean],false,name+': edited boolean becomes bool');
  // Raw JSON must be loaded explicitly and must be an object. No autosave.
  h = harness(html,cfg); h.json('{broken'); h.save(); h.load();
  equal(h.saves.length,0,name+': invalid JSON never saved');
  ok(h.node('save').disabled,name+': unloaded JSON blocks Save');
  h.json('[]'); h.load(); h.save(); equal(h.saves.length,0,name+': array JSON rejected');
  ok(h.node('status').textContent.includes('object') || h.node('status').textContent.includes('unloaded'),name+': JSON error explained');
  h.json('{"future":1e999}'); h.load(); h.save(); equal(h.saves.length,0,name+': overflowing raw number never turns into null');
  h.json(JSON.stringify(cfg)); h.load(); equal(h.saves.length,0,name+': loading JSON is local only');
  h.save(); equal(h.saves[0].future,cfg.future,name+': JSON unknown fields retained');
  // Read-only host context is enforced by both presentation and handlers.
  h = harness(html,cfg,{scope:'preset',hasPresetOverride:true,readOnly:true});
  ok(h.node('save').disabled && h.node('restore').disabled && h.node('raw').readOnly,name+': readonly UI');
  h.save(); h.node('restore').fire('click'); equal(h.saves.length,0,name+': readonly save guard'); equal(h.restores,0,name+': readonly restore guard');
  ok(fields.every(f => h.node('setting-'+f.key).disabled),name+': readonly controls');
  // Restore distinguishes deleting an override from resetting global values.
  h = harness(html,cfg,{scope:'preset',hasPresetOverride:false});
  ok(h.node('restore').disabled,name+': no override to remove'); h.node('restore').fire('click'); equal(h.restores,0);
  h.context.hasPresetOverride = true;
  h.node('restore').fire('click'); h.node('confirmNo').fire('click'); equal(h.restores,0,name+': canceled restore');
  h.node('restore').fire('click');
  ok(h.node('confirmText').textContent.includes('global values are not erased'),name+': preset restore meaning');
  equal(h.restores,0,name+': restore waits for inline confirmation'); h.node('confirmYes').fire('click'); equal(h.restores,1);
  ok(h.node('scopeHelp').textContent.includes('Save that preset yourself'),name+': preset persistence explicit');
  h = harness(html,cfg); h.node('restore').fire('click');
  ok(h.node('confirmText').textContent.includes('Replace global plugin settings'),name+': global restore meaning');
  h.node('confirmYes').fire('click'); equal(h.restores,1,name+': global restore confirmation');
  equal(h.saves.length,0,name+': restore never calls saveConfig');
  h = harness(html,cfg,{}); ok(h.node('restore').disabled,name+': unknown scope cannot restore');
  h.save(); equal(h.saves.length,1,name+': legacy bridge save still available');
  h = harness(html,cfg,{scope:'preset',hasPresetOverride:true}); h.node('restore').fire('click');
  h.context.scope = 'global'; h.node('confirmYes').fire('click'); equal(h.restores,0,name+': changed restore scope rejected');
  h = harness(html,cfg); h.node('restore').fire('click'); h.receive(cfg); h.node('confirmYes').fire('click');
  equal(h.restores,0,name+': new host config cancels pending inline action');
  // No host bridge, getConfig-only legacy bridge, invalid callback and synchronous errors.
  h = harness(html,cfg,undefined,{noBridge:true}); h.save(); equal(h.saves.length,0,name+': no bridge writes');
  ok(h.node('status').textContent.includes('unavailable'),name+': honest missing bridge');
  h = harness(html,cfg,{}, {getOnly:true}); h.save(); equal(h.saves[0][boolean],'false',name+': legacy getConfig');
  h = harness(html,cfg,undefined,{loadError:true}); ok(h.node('status').textContent.includes('Could not load'),name+': load exception explained'); h.save(); equal(h.saves.length,0,name+': load exception blocks writes');
  h = harness(html,cfg); h.receive([]); h.save(); equal(h.saves.length,0,name+': malformed host data cannot be saved over');
  h = harness(html,cfg,undefined,{saveError:true}); h.edit(boolean,'0','change'); h.save();
  ok(h.node('status').textContent.includes('did not confirm'),name+': failed save not called success');
  equal(JSON.parse(h.node('raw').value)[boolean],true,name+': failed save keeps draft');
  h = harness(html,cfg,undefined,{restoreError:true}); h.node('restore').fire('click'); h.node('confirmYes').fire('click');
  ok(h.node('status').textContent.includes('not confirmed'),name+': restore error truthful');
  // Edits and even a later save must survive stale host callbacks from an earlier request.
  h = harness(html,cfg); h.edit(boolean,'0','change'); h.save(); const first = copy(h.saves[0]);
  h.edit(boolean,'1','change'); h.save(); const second = copy(h.saves[1]);
  h.receive(first); equal(JSON.parse(h.node('raw').value)[boolean],false,name+': stale first response kept newer draft');
  h.receive(Object.fromEntries(Object.entries(second).reverse())); equal(JSON.parse(h.node('raw').value)[boolean],false,name+': latest response accepted');
  ok(h.node('status').textContent.includes('effective configuration'),name+': actual host reply distinguished');
  h.edit(boolean,'0','change'); h.receive(cfg);
  equal(JSON.parse(h.node('raw').value)[boolean],true,name+': external update kept local draft');
  h.node('reload').fire('click'); h.node('confirmNo').fire('click'); equal(JSON.parse(h.node('raw').value)[boolean],true,name+': reload cancellation');
  h.node('reload').fire('click'); h.node('confirmYes').fire('click'); equal(JSON.parse(h.node('raw').value)[boolean],'false',name+': reload explicitly discards draft');
  if (numeric) {
    for (const invalid of ['', 'NaN','Infinity','1e999','oops']) {
      h = harness(html,cfg); h.edit(numeric.key,invalid); h.edit(boolean,'0','change'); h.save(); equal(h.saves.length,0,name+': rejects numeric '+invalid);
    }
    h = harness(html,cfg); h.edit(numeric.key,'1.25e-2'); h.save(); equal(h.saves[0][numeric.key],0.0125,name+': numeric conversion only on edit');
  }
  const scalar = fields.find(f => !f.choices && f.specials.includes('auto'));
  if (scalar) {
    h = harness(html,{}); h.edit(scalar.key,'Auto'); h.save(); equal(h.saves[0][scalar.key],'auto',name+': auto retained');
    h.edit(scalar.key,'1e999'); h.save(); equal(h.saves.length,1,name+': nonfinite scalar rejected');
  }
  if (schema.infill && !schema.check) {
    for (const value of ['150%','-2x','0.12']) {
      h = harness(html,{}); h.edit('amplitude',value); h.save(); equal(h.saves[0].amplitude,value,name+': amplitude units '+value);
    }
  }
}
console.log('ok -- '+checks+' DOM/bridge-double assertions across '+pages.length+' shipped settings pages; not a native/browser-rendering test');
