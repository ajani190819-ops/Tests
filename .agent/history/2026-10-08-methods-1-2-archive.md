# 2026-10-08 — Methods 1 and 2 archived; repo tidy

- Status: outcome record. Not a handoff; read `.agent/STATE.md` for current work.

## What changed

- Methods 1 (classic affine) and 2 (captured projective mesh) removed from the
  live code and config. Only Method 3 (real 3D panel) and Method 4 (purple 2.5D
  panel) remain. Record: `archive/spatial-hud-methods-1-2/README.md`.
- Capture failure now shows the vanilla HUD plus a red marker above the hotbar's
  top-left corner. The Method 4 outline on failure was removed.
- Dead code removed: `preserveCompanionStatusLayout`, `VirtualHudPlane.projectAt`
  and its `lerp`, and the composite and warp helpers used only by Method 2.
- Repo moves (staged): logs to `.agent/evidence/logs/`; v0 draft, demo mod, and
  the spatial-gui jar to `archive/`; Orca audit and Spatial HUD CI and
  compatibility docs to `docs/`; the old handoff to `.agent/history/`; the
  Electron app to `apps/arena-link-windows/`. References fixed.
- Docs rewritten: `docs/spatial-hud/CI-SETUP.md` (now the build and updater
  process, not a one-time paste), `docs/spatial-hud/COMPATIBILITY.md` (rendering
  contract), the mod README, the F5W runbook, and the facts file.
- Decision recorded: `.agent/decisions/0002-methods-3-and-4-only.md`.

## Verification

- Tree-sitter Java parse of all client sources: no errors.
- Lang JSON valid; 80 keys.
- Not compiled. No JDK in the sandbox. CI on push is the compile check.
