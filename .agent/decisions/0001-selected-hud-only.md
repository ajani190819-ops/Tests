# 0001 — Capture only the selected lower HUD

- Status: accepted.
- Last verified: 2026-10-07.
- Read when: changing the Spatial HUD render path.

## Decision

Capture and deform only the selected lower-HUD region: hotbar, status bars,
experience, held-item label, backing, and compatible pixels injected into the
same vanilla roots.

## Reason

The older global/offscreen true-3D renderer interfered with unrelated GUI
screens. The user explicitly rejected restoring that behavior.

## Consequences

- Never cancel or broadly reroute GUI rendering.
- Never capture unrelated screens, menus, chat, maps, debug overlays, or
  arbitrary mod GUIs.
- The capture redirect must remain private and object-identity scoped.
- Keep the affine safe mode as the default until experimental compatibility is
  proven in the F5W profile.
