# Weather Radar

Choose and save a place. The app headlines the exact-location ensemble chance of measurable rain, while showing regional coverage-blended probability as a labelled comparison. Radar data comes from RainViewer over OpenStreetMap; both stay attributed and cached locally. Native radar requests are capped at validated host level 7; display levels 8-10 are local crops centered on the selected place, not unsupported higher-level requests. Switching places discards late tile results for the previous place and keeps the map in a loading state until imagery for the current selection is ready, so the marker is not drawn over another city's map. When published, the map opens on the nearest future nowcast, labels its forecast-step position, and keeps a longer time history for smoother playback.

The rain card also gives an approximate ETA for the selected place: the first hour when at least half of reporting ensemble members forecast measurable rain. If the ensemble odds stay lower, it shows the hour with the strongest nonzero signal as possible; a single-model hourly estimate is labelled separately when the ensemble has no rain signal. Times are local to the selected place and are hourly, not minute-precise.

The five-day planner groups the forecast into one row per local date, with aligned rain, high/low temperature, cloud-cover, and wind values instead of a separate box for every measure. Select a value to see a larger hourly chart with time/value axes and a plain-English reading. Rain values keep exact-point ensemble odds (only members reporting the full day count) separate from regional coverage-blended PoP; the six-hour point-rain card remains the headline. Humidity stays supporting current-conditions context.

## Use my location

Search results are **city/postcode coordinates**, not necessarily your physical position. **Use my location** explicitly asks Windows for one position, after its location-permission check. The asynchronous request leaves the window responsive. It never runs at startup, on weather refresh, or as background tracking; another click cancels, and selecting a place, hiding/minimizing the window, or quitting cancels an outstanding request.

The left column shows the selected point's source and coordinates. Windows points also show the provider's reported accuracy estimate and the local date/time of the fix. High accuracy is requested, not guaranteed: Windows can use different providers, and a result of 1 km or more is prominently labelled **approximate**. Missing/invalid accuracy, invalid map coordinates, stale/future-dated fixes, denied permission, unavailable providers and timeouts keep the previous place rather than silently falling back to a city or an app-side IP lookup. **Windows Location settings** opens the system privacy page only when clicked; permissions and provider availability remain controlled by Windows.

A successful fix updates one **My location** saved snapshot in `%LOCALAPPDATA%\WeatherRadar\config.ini`, including its source, accuracy and fix time. Reopening or refreshing weather uses those saved coordinates for the map, forecast and ensemble; it does **not** obtain a new device location. Click **Use my location** again after moving. Existing saved search places remain compatible and never acquire an invented accuracy claim. Selecting a point sends its coordinates to the existing weather/map providers as before.

Builds and portable regression tests do not prove Windows permission behavior or accuracy at your actual position. The Windows acceptance procedure is in [the location runbook](../.agent/runbooks/test-weather-location.md); actual-position validation remains separate from selected-place tile alignment.

## Rain watch

While Weather Radar is running, closing its window keeps it in the Windows notification area. It alerts only when the currently selected saved place reaches an exact-location ensemble chance of **66% or more within 3 hours**. Each episode is persisted, has a three-hour cooldown, and is never announced again while rain is already underway. Right-click the notification-area icon to exit.

Detailed data semantics, map/cache behavior, and the historical product narrative are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
