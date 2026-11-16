# Weather Radar

Choose and save a place. The app headlines the exact-location ensemble chance of measurable rain, while showing regional coverage-blended probability as a labelled comparison. Radar data comes from RainViewer over OpenStreetMap; both stay attributed and cached locally.

## Rain watch

While Weather Radar is running, closing its window keeps it in the Windows notification area. It alerts only when the currently selected saved place reaches an exact-location ensemble chance of **66% or more within 3 hours**. Each episode is persisted, has a three-hour cooldown, and is never announced again while rain is already underway. Right-click the notification-area icon to exit.

Detailed data semantics, map/cache behavior, and the historical product narrative are in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
