# Orca real-host evidence: ORCA-01 / ORCA-02 / ORCA-03

**ORCA-01/02/03 are owner-reported good for current use.** The older procedure
below is a retained reference, not a request to repeat the full matrix or logs.
The approved settings/setup increment has no new native UI/callback/export or
physical-print evidence here; that does not revoke the current-use approval.
Do not guess effective settings, print automatically, change algorithms or
revive ORCA-04/05 research. Keep original evidence unchanged. The narrower
current boundary is in [settings/setup](../../docs/ORCA-SETTINGS-SETUP.md).

## 1. Establish evidence identity before reproducing

`tools/inspect_orca_export.py` is a stdlib-only, read-only inspector for explicitly
chosen UTF-8/ASCII G-code files. It hashes exact bytes and summarizes recognized
rewrite stamps/blocks, numeric slicer-profile metadata and known capability
references. It never executes/reprocesses G-code, imports the plugins, discovers
host profiles, changes settings or uploads anything. It prints JSON to stdout;
no file is created unless the caller deliberately redirects output.

```powershell
python .\tools\inspect_orca_export.py 'C:\YourEvidence\control.gcode' --reject-processed
python .\tools\inspect_orca_export.py 'C:\YourEvidence\wave.gcode' 'C:\YourEvidence\infill.gcode' 'C:\YourEvidence\both.gcode'
```

Exit 1 means unavailable/unsupported evidence; exit 2 with `--reject-processed`
means recognized prior rewrite evidence was found. Exit 0 **does not certify**
pristine/fresh input, successful host invocation, correct geometry or print
safety. Comments can be edited; selections are not execution. Conflicting/missing
config metadata stays explicit. Input filenames, paths, arbitrary comments,
machine G-code and unknown plugin/cloud identifiers are not copied to the
summary. This is data minimization, not a full-anonymity promise.

### Audited archived inputs (not new exports)

| Input | Bytes | SHA-256 | Finding |
| --- | ---: | --- | --- |
| `test-prints/t2-curved-perimeters/clean.gcode` | 336750 | `59fc20e5ed42c5040213cc6a91c55579102702abc1911be4c9376783bbef4168` | Infill 0.4.8 stamp despite the `clean` filename |
| `test-prints/t3-multi-overhang/t3_20m37s.gcode` | 1924925 | `75ada6006f12bcb796ea6ad8993f59a6aa989577b3b4e2c55600660e87e2e35d` | Infill 0.4.8 stamp; metadata selects both plugins, but no Wave stamp/block |
| `tests/fixtures/Cube^2_3m53s.gcode` | 159768 | `c013be94834f0631433436cfd5168e1244076ae36ff9783bced1780177c58b60` | Wave selection metadata, no recognized rewrite marker; not proof of freshness |

All three embed 0.6 mm nozzle / 0.3 mm layer profile values. They do **not**
supply the owner's effective plugin Config/preset-override JSON. Do not strip
stamps, reverse-engineer an alleged clean control or substitute current defaults
and call it the owner's actual configuration. Do not diagnose missing host
invocation from a missing stamp alone: a no-op/applicability gate is possible.

## 2. Capture the real host and actual effective settings — ORCA-01 / ORCA-03

Before any reset/reinstall, record:

- Orca build/version and OS; installed **Undahang 0.0.53** (package/capabilities
  still **Wave Overhangs**) and **Unlayered Infill 0.4.11**, or the exact versions
  actually installed. Older versions are not current-release acceptance.
- The Plugins dialog's activation/version and each slicing capability's Config
  JSON, plus any process/filament/printer preset plugin overrides. Preserve
  values; do not Restore defaults to make a reproduction easier. Mark the exact
  source of each settings snapshot and keep a private copy before changing it.
- The process preset's visible **Slicing Pipeline Plugin** selection and any
  configured ordinary post-processing. Use the host UI, not guessed
  `ctx.config_value()` wiring introspection. If this host only exposes one
  capability, record that limitation rather than editing hidden fields blindly.
- A representative curved wall/hole/multiple-overhang model and orientation,
  nozzle/layer/material, plus the exact jagged segment/location expected.

## 3. Export four independent comparisons — ORCA-01 / ORCA-02 / ORCA-03

Use copies of the test process preset so the owner's original stays intact.
Keep geometry/profile identical except for the explicitly recorded capability
selection. Export to different NEW filenames; never feed an old postprocessed
file back through a plugin to manufacture a slicer control.

1. A freshly sliced control with these plugins not selected.
2. A fresh **Wave Overhangs** / Undahang-only export with the captured settings.
3. A fresh **Unlayered Infill**-only export.
4. A fresh both-selected export, **only if the actual UI supports it**.

Use **Export G-code file**, not Print/Send, and reopen each exported file in the
G-code viewer: the ordinary pre-export 3D preview does not show the rewrites.
Run the inspector on all outputs and correlate hashes with same-run plugin
setup/export logs, effective settings, operation order and export time. Logs
can contain private paths; review before sharing, keep raw evidence out of
public Git/Drive. A setup check alone is not proof an export callback ran.

Verify each plugin independently before judging the combined result. For an
eligible overhang/sparse-infill test, correlate actual output changes/stats and
stamps with logs. If unchanged, preserve the explicit no-op/error reason; do
not force wiring changes or print an unexplained result.

## 4. Physical print — ORCA-02 (owner action only)

Only after reviewing the actual exported paths, start with a small representative
part on the owner's normal, supervised printer procedure. Check travel/nozzle
clearance, material limits and the printer profile; the inspector does not
assess any of these. No automatic printing is performed by repository tooling.

Record the exact output SHA, settings/versions, layer/feature photos, underside
and curved-wall quality, holes, multiple overhang layers, seam/wake artifacts,
collisions/strings and timing. Keep algorithm research ORCA-04/05 parked. A
successful simulation/replay, selected capability, log stamp or source test
cannot close physical-print acceptance.
