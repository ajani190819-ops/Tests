# 2026-10-08 — Method 4's pitch response became a real plane projection

- Branch: `arena/b4016c28-tests`. Build/run: still blocked on the owner's
  workflow paste (`PASTE-ME-CI-SETUP.md`, action 1).

## What the owner asked for

"The pitch of the camera that adjusts the corner's location should be based
around how perspective is handled in a modern 3D renderer, so it should make
the polygon render as if it was a 3D plane."

## What changed

- `PolygonTestRenderer.respondToPitch(...)` no longer invents a horizontal
  pinch plus a vertical shift. It pitches the card about its own left-to-right
  axis and divides each corner by its own depth
  (`w = 1 - dy*sin(angle)/focal`), which is what a 3D renderer does to a plane.
- The focal length comes from the player's field of view
  (`VirtualHudPlane.focalLengthFor`, extracted so Methods 1-3 and Method 4 use
  one value), so the perspective matches the world at any FOV or GUI scale.
- The tilt clamps at 70 degrees: a straight-down look would otherwise collapse
  the card into a line. The divide has a 0.15 floor for pathological corner
  sets; a normal card never reaches it.
- `Quad` is now a class holding the square-to-quad homography instead of a
  bilinear interpolator, so straight HUD lines stay straight on the tilted
  surface. Crossed/collapsed handles fall back to bilinear.
- Level pitch still returns the saved corners exactly, and 0 % response still
  freezes the quad, so the existing handles keep their meaning.

## Evidence

The formulas were transliterated to Python and simulated before the Java was
written (identical formulas, corners checked at -90..+90 degrees across five
response strengths): exact at level pitch, monotone far-edge narrowing,
finite and un-inverted everywhere, affine and projective homography branches
both corner-exact, straight lines preserved (the old bilinear mapping bowed the
same line by several pixels). No Java compile or in-game test is possible here.
