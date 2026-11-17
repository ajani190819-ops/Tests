# Weather Radar

Choose and save a place. The app headlines the exact-location ensemble chance of measurable rain, while showing regional coverage-blended probability as a labelled comparison. Radar data comes from RainViewer over OpenStreetMap; both stay attributed and cached locally. When published, the map opens on the nearest future nowcast, labels its forecast-step position, and keeps a longer time history for smoother playback.

The rain card also gives an approximate ETA for the selected place: the first hour when at least half of reporting ensemble members forecast measurable rain. If the ensemble odds stay lower, it shows the hour with the strongest nonzero signal as possible; a single-model hourly estimate is labelled separately when the ensemble has no rain signal. Times are local to the selected place and are hourly, not minute-precise.

## Rain watch

While Weather Radar is running, closing its window keeps it in the Windows notification area. It alerts only when the currently selected saved place reaches an exact-location ensemble chance of **66% or more within 3 hours**. Each episode is persisted, has a three-hour cooldown, and is never announced again while rain is already underway. Right-click the notification-area icon to exit.

Detailed data semantics, map/cache behavior, and the historical product narrative are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
