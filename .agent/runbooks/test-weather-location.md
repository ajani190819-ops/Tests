# Weather Radar — explicit Windows location (WX-03)

Status: procedure prepared; **no native Windows or actual-position result recorded**.
Do not mark WX-03's physical-position acceptance complete from a cross-build,
portable mocked-provider test, city selection, or WX-02 tile alignment review.

## Setup / evidence boundaries

- Use the delivered `WeatherRadar.exe` from the active Arena branch selected
  in Project Hub. Record implementation SHA, Windows build and display scaling.
- Preserve existing `%LOCALAPPDATA%\WeatherRadar\config.ini` before testing.
  It contains saved coordinates; do not commit it or exact home coordinates,
  sensor output, or screenshots exposing them to this public repository.
- Run the actual installed app with its existing elevation boundary. This
  change does not alter Hub/child elevation policies. Check consent in the
  real installed context, not just a stand-alone UWP sample or mock provider.
- Windows controls location availability/permission. A desktop can have no
  GPS and return a broad estimate. An approximate result is not a proven repair
  of the owner's reported offset. Do not disable security/privacy controls
  merely to force a passing result.

## Consent / absence / cancellation

1. Start with a saved city. Open/reopen Radar, refresh weather and wait through
   a 10-minute auto-refresh. There must be **no** location prompt or new device
   fix. Source must say city/postcode search, with no invented accuracy.
2. Click **Use my location**. If Windows needs consent, verify the system prompt
   belongs to this app. The window should remain responsive. Deny/cancel it:
   the old selected point must remain and the location notice must explain the
   denial/unavailability. An unrelated weather update must not erase that notice.
3. Click **Windows Location settings**. It should open the system location page,
   not silently enable permissions. If the user chooses to allow access, click
   Use my location again. Windows may remember the earlier choice and omit a
   second prompt; do not promise a new prompt on every click.
4. Test with service/access disabled or no working provider. Expect a clear
   unavailable/denied/timeout notice and the old selection unchanged. A fix
   request has a 30-second OS timeout plus a small local dispatch margin; consent
   has a separate two-minute guard. There must be no silent city/IP fallback.
5. While a request is pending, try **Cancel location**, selecting a different
   search result/saved place (including the already selected saved row), Drop,
   minimize, hide/close-to-tray and explicit Quit. A late completion must not
   reselect My location, resurrect a removed place, or start new location access.
   Rapid repeat clicks must not open overlapping requests/prompts.

## Successful fix / actual-position accuracy

1. At a privately known physical position, request one Windows fix. Check the
   displayed source, coordinates, reported accuracy estimate and local fix time.
   Confirm the marker, map crop, forecast and ensemble all use that same point.
2. Compare with independently known coordinates or an identifiable physical
   landmark. Privately record the separation in meters and the Windows-reported
   accuracy. Do not call the marker physically accurate merely because the city
   name is correct or the map is internally aligned. Investigate any mismatch
   outside the reported uncertainty; retain WX-03 as open until confirmed.
3. If Windows reports a radius of 1 km or more, verify **Approx.** and the warning
   remain visible. It is a valid coarse estimate, not a GPS claim. Missing/invalid
   accuracy or a fix older than two minutes must not replace the prior point.
4. Click Use my location after moving. Exactly one My location saved row should
   update, including accuracy/time; other saved search rows must remain distinct.
5. Restart Radar and refresh weather. The saved point/date/accuracy should remain
   visible, with no automatic new device lookup. After moving, only the explicit
   button gets another fix. Test selecting other cities during slow weather/tile
   requests: no old-place imagery or weather may appear under the new marker.
6. Check 96/144/192 DPI and the existing 55–100% fit range, with eight matches and
   eight saved places. Location controls, source/coordinates/accuracy/time, lists,
   planner, radar and status must remain separate and legible. Check physical
   monitor transitions; mathematical rectangle tests are not native visual proof.

## Automated evidence / API references

`tests/native/radar_location_harness.c` executes the production lifecycle,
saved-place load/save and selection/completion handlers with Windows/provider
I/O mocked. It covers permission/absence/error paths, stale and invalid data,
coarse fixes, cancellation/generations, persistence, timeouts, dispatch failure
and old-config compatibility. Repository tests also check API/GUI wiring and
WinRT ABI declarations; the normal seven-EXE build cross-compiles the provider.
None of these is a real Windows location or live-map result.

The implementation uses modern `Windows.Devices.Geolocation`, not the legacy
Win32 `ILocation` API. Permission is initiated by the foreground UI thread;
only already-requested asynchronous operations are polled by the UI timer.
It does not subscribe to position events or enable consentless fallback.

- [Microsoft RequestAccessAsync contract](https://github.com/MicrosoftDocs/winrt-api/blob/docs/windows.devices.geolocation/geolocator_requestaccessasync_380675631.md)
- [Bounded single-fix overload / cache-age caveat](https://github.com/MicrosoftDocs/winrt-api/blob/docs/windows.devices.geolocation/geolocator_getgeopositionasync_726098746.md)
- [Reported accuracy semantics](https://github.com/MicrosoftDocs/winrt-api/blob/docs/windows.devices.geolocation/geocoordinate_accuracy.md)
- [WinRT C ABI declaration source](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/winrt/windows.devices.geolocation.h)
