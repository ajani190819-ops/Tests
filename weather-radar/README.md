# Weather Radar

Pick a place, read an honest chance of rain, watch the radar move.

## The number that makes this app worth having

Every mainstream weather app headlines a "chance of rain" that is **PoP =
confidence x areal coverage**. A forecaster who is certain that half the
metro gets wet reports 50% - and so does an app that blends 40 km grid cells
across your street, whether the rain lands on you or twenty miles away. It is
a statement about an *area*, sold as a statement about *you*.

This app answers the question you meant - "will it rain **here**?" - with the
only honest method available without a forecaster: the ensemble. Open-Meteo
serves every member of the GFS ensemble as its own hourly precipitation
series, so the fraction of members that put at least **0.1 mm** (the WMO
trace threshold - below it a gauge reports nothing) on these exact
coordinates in the window **is** the probability, by definition, with no
coverage term smuggled in.

Two details keep that number honest:

- **Silence is not dry.** Past a member's own forecast horizon the API writes
  `null`. Those members are dropped from the denominator instead of being
  counted as "no rain", which would quietly deflate every longer-range
  number. The card tells you the denominator ("13 of the 31 members...").
- **The blended figure stays visible, demoted.** The coverage-blended PoP is
  drawn as a hollow yellow tick beside each honest bar and next to the
  headline, labelled as what it is. Compare them; that gap is the point.

If the ensemble does not answer, the card shows `--`. No honest number beats
a borrowed one.

## The radar

RainViewer publishes observed frames (past ~2 h) and nowcast frames (next
~2 h) as transparent slippy-map tiles. This app stitches a 3x3 tile grid
centred on your place over an OpenStreetMap basemap, and lets you step or
play the frames - observed frames say "(observed)", modelled ones
"(forecast)", because those are different kinds of truth.

- Tiles decode through WIC (Windows' own image codec), reached by COM so the
  codec never enters the executable's import table.
- Everything is cached under `%LOCALAPPDATA%\WeatherRadar\cache\`: basemap
  tiles for a month, radar frames for two days (their timestamp is in the
  path, so a cache hit is exactly the same frame). The cache is pruned at
  startup; it is a courtesy to the tile hosts, not a database.
- If the tile host answers a zoom level with an empty placeholder (the free
  tier caps out), the app retries one zoom step out and says so on the pane.
- Attribution: radar data © RainViewer, map data © OpenStreetMap
  contributors. Both are credited on the pane itself.

## Places

Search a city or postcode (Open-Meteo's geocoding, no key), click the match
you mean, and it is remembered. Places live in the app's own file,
`%LOCALAPPDATA%\WeatherRadar\config.ini`, under `[Places]` - the same
one-config-per-app rule every other suite app follows. **Keep** re-saves the
current place, **Drop** removes it.

The window follows your Windows light/dark theme, and a second launch simply
shows the window that is already running instead of opening a twin.

## Where the data comes from

- Forecast and ensemble: [Open-Meteo](https://open-meteo.com) (free, no key).
- Radar frames and tiles: [RainViewer](https://www.rainviewer.com).
- Basemap tiles: [OpenStreetMap](https://www.openstreetmap.org).

All three are fetched on a background thread; a slow host slows the pane,
never the window.
