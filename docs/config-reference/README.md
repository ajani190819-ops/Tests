# What the Config tab should contain

Generated from the shipped plugin files, so these are exactly what
`get_default_config()` returns on the version named below. If your Config tab
disagrees, the panel is showing a copy saved by an older build (OrcaSlicer
keeps it in `data_dir()/orca_plugins/config.json`, not in the preset) — or the
installed plugin FILE is older than this repository.

Regenerate after any change to the defaults:

```
python3 tools/dump_default_config.py
```

| File | Capability | Plugin version |
| --- | --- | --- |
| `unlayered-infill.json` | Unlayered Infill | 0.4.8 |
| `unlayered-infill-check-setup.json` | Unlayered Infill - Check setup | 0.4.8 |
| `unlayered-infill-minimal.json` | Unlayered Infill, notes stripped | 0.4.8 |
| `wave-overhangs.json` | Wave Overhangs | 0.0.46 |
| `wave-overhangs-check.json` | Wave Overhangs - Settings guide & check | 0.0.46 |

Keys beginning with `_` are notes. The plugin ignores them; they exist so the
JSON editor explains itself. The `-minimal` file is the same configuration
with them stripped, for when you just want to see the settings.

## If "Restore defaults" did nothing

Restore defaults reinstates the defaults of **the plugin file that is actually
installed**. If that file is an old one, you get the old defaults back and
nothing appears to change. Check the Version column in the Plugins dialog, or
the first line of "Check setup", against the table above before concluding the
panel is broken.
