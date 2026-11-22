# Weather Report: component review and desktop acceptance

**WX-01/02 remain OPEN.** This review kit is cross-buildable tooling, not a
recorded native Windows run or an installed-app update. The delivered Weather
EXE and location implementation are unchanged. WX-03 uses the separate
[`test-weather-location.md`](test-weather-location.md) procedure.

## Opt-in native component review

- `tests/native/radar_windows_review.c` includes production Weather source.
- `tools/test_radar_windows.py` builds/runs it on Windows and rejects incomplete
  results. `tools/test_radar_windows.ps1` is an optional thin wrapper.
- Default fixtures refuse HTTP **before opening a session**. This is an explicit
  test-only transport guard, not proof of a live-network pass. There are no
  Win32/GDI/WIC mocks and no test hooks/edits in the shipped app.
- `--live` / `-Live` explicitly enables production HTTP functions for fixed
  public Tallahassee/Seattle coordinates, never the owner's location/profile.
- No uploads, installs, app replacement, elevation or policy/security changes.
  If script policy or security blocks the review, stop; do not bypass the block.

Use ordinary Windows Python with `ziglang==0.17.1` already installed. Choose a
**NEW** short absolute path on a local fixed/removable drive, outside the repo;
its parent must already exist. Existing directories, including empty ones,
remote/device/ADS paths and symlink/reparse parents are refused.

```powershell
$out = Join-Path $env:TEMP ('WeatherReview-' + [Guid]::NewGuid().ToString('N'))
python .\tools\test_radar_windows.py --output-directory $out
# Separate optional run, NEW output; public weather/tile requests require network:
$liveOut = Join-Path $env:TEMP ('WeatherLive-' + [Guid]::NewGuid().ToString('N'))
python .\tools\test_radar_windows.py --output-directory $liveOut --live
```

The PowerShell wrapper accepts `-OutputDirectory $out` and optional `-Live`.
Choose one normally permitted invocation; do not switch invocation to evade a
policy/security refusal. The runner does not install prerequisites itself.

Output includes build/source hashes, compiler/native logs, isolated per-mode
cache/config, result JSON and 13 fixture PNGs (plus six live PNGs when opted in).
`review.json` exists only after all requested gates succeed and explicitly says
`owner_acceptance: false`. `captures.json` records exact PNG hashes. Keep the
whole new output private until reviewed: compiler logs can contain local paths.
No file is automatically shared. Timeouts, crashes, missing data/captures and
schema/type errors fail rather than becoming skips/passes.

### Fixture scope

- Five daily rows × four value cells plus date clicks at injected `(dpi, fit)`
  pairs `(96,100)`, `(96,75)`, `(96,55)`, `(144,75)`, `(192,55)`.
- Native GDI date/category/value extent findings; headings, hourly values,
  units and readings; synthetic 20% point odds stay distinct from 75% regional
  PoP. Captures still need visual review, particularly the two-color rain cell.
- Synthetic PNGs pass through WIC/cache/stitch/composite. Two independently
  calculated Mercator control points check crosshairs at display zooms 7–10;
  display crops reuse source-level-7 imagery.
- Obsolete place builds cannot publish; a stale ready bit cannot paint the old
  city. A background builder overlaps 120 place/frame/paint cycles. This is
  bounded stress, not exhaustive race freedom.
- Any attempted fixture HTTP session fails the offline gate.

PNGs are offscreen production-component renders, **not desktop screenshots**.
Injected scale parameters do not exercise real monitor DPI transitions,
entry-point fitting, actual mouse/focus UX or Windows location permissions.

### Optional live scope and limits

A separate process/cache runs forecast, ensemble, fresh catalog and tile
functions synchronously for each fixed city. It requires complete five-day
forecast data, at least two ensemble members, frames, a current published view
and decoded basemap pixels. Per-city JSON and PNGs are retained. Missing or
rate-limited provider data fails; prior-city catalog success is not reused as
this city's result. The run does not test production worker scheduling or prove
nonempty rain echoes during dry weather.

The kit is **not integrated into CI**. The old untracked
`.agent/pending-radar-native-review.patch` contains stale workflow/CODEMAP edits;
do not apply it blindly. Workflow-write/dispatch permissions were denied in the
sandbox. Manual native review does not require changing those workflows.

## Required owner desktop acceptance

Use the released active branch in Hub; do not call a harness run an app update.

1. Open Weather Report on the actual normal/small screen. Check light/dark,
   100/125/150/175/200% DPI as available, and moving between monitors: readable
   dates/values, no overlap/clipping, usable controls and hourly axes/units.
2. Select all five dates and rain/temperature/cloud/wind. Exact-point ensemble
   odds remain primary, regional PoP explicitly secondary; readings agree with
   chart/category/selected date.
3. Search and keep two distant places. At zooms 7–10, step/play frames and
   rapidly change places while loading. The basemap/crosshair must match the
   selected city, with no prior-city image under a new marker.
4. Check attribution, observed/nowcast labels, meaningful basemap, loading and
   provider-unavailable states, and responsiveness on the real network.
5. Test **Use my location** separately with the location runbook, recording
   reported accuracy/time and the original actual-position mismatch.

Report branch/released SHA, Windows version, screen/DPI/theme, places and exact
failing action. Do not publish precise private coordinates automatically.
Leave WX-01/02/03 acceptance open until actual owner evidence is reviewed.
