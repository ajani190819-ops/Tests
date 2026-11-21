# APP-03 — native Windows grouped Hub acceptance

Status: **procedure only; not run in the Linux build workspace**. Production-C
filesystem/configuration/geometry mocks and cross-builds are not Windows GDI,
Explorer, real Orca, DPI-transition or owner acceptance evidence.

Use Hub 2.8.66 from the active Arena branch's verified release marker. Record
Windows version, Hub version/build SHA, display resolution/scaling, window size,
profile paths, actions/results and screenshots. Back up Hub `config.ini` and
Orca profiles before destructive checks. Use disposable profiles, not the only
copy of the owner's working presets. Keep unrelated apps/updates closed.

## Layout, scrolling and input

1. Confirm one **General apps**, one **Orca plugins**, and one **Minecraft
   mods** heading. General contains Bluetooth Guard, Keyboard Lighting, DNS
   Pilot, Agent Notes, Audio Switcher and Weather Radar. Orca contains exactly
   **Undahang** and **Unlayered Infill**. Minecraft contains Spatial HUD only.
2. Test default, minimum, tall and maximized sizes at 100%, 125%, 150% and 200%
   scaling, including moving between monitors. Verify readable/ellipsized text,
   complete buttons, rounded cards, correct icons, no title/badge/control
   overlap, and no drawing into the fixed header/footer. At narrow sizes the
   controls occupy a second header row, including when Update Hub is visible.
3. Reach every card with the wheel, arrows, Page Up/Down, Home and End. The last
   card and all its controls must be reachable. Headings must stay with their
   groups; empty space/dividers/headings must never trigger card actions.
4. At several partial scroll positions, test buttons near the header/footer,
   then immediately after resizing/scaling/scrolling before a repaint. Only
   the visible button under the pointer may act. Hidden Update buttons must
   leave no stale clickable rectangle. Confirm each plugin's Folder opens its
   own installed package (legacy-only installations may share `plugins`).
5. Refresh repeatedly with current, missing, malformed/truncated and old
   combined-Orca catalogues in a controlled fixture. Required nine entries
   must remain, with no duplicate plugin/Weather cards or borrowed text/icons.
   An extra already known in this session must retain its slot/definition when
   omitted, including if a later response fills the remaining capacity.

6. Confirm Undahang has the approved rising two-stroke mint/cyan wave icon and
   Unlayered Infill retains its three horizontal green ribbons. Back up/remove
   only the disposable Hub icon cache, start offline, then retry with current
   and old combined-Orca catalogues. Both icons must remain distinct through
   repeated refreshes and 100–200% DPI transitions. Check the rounded slate
   family styling, transparent outer margin, sharp small sizes and no handle
   growth. Native appearance is not proven by ICO decoding or mock handles.

## Independent plugin state/actions

1. With only `OrcaDisabled=1` in a backed-up Hub INI, both cards initially show
   Disabled. Enable only one, restart Hub, and confirm the sibling remains
   disabled. Repeat in the other direction. Hub Disable pauses installs and
   Update All for **that card**; it does not turn off a plugin inside Orca.
2. Use default, beta/nightly and legacy-only disposable `OrcaSlicer*` profiles.
   Cover neither installed, only one installed, both current, one old, mixed
   modern/legacy versions, unreadable/missing version, and unavailable remote
   version. Each badge/action must describe only its plugin. Current/unknown
   remote states do not offer Update; a verified older installed copy does.
   A legacy copy in a nondefault profile must not disappear from status.
3. Hash both plugins' files and record host enable state/settings. Install or
   update **Undahang only**, then **Unlayered Infill only**. Check every target
   profile/current and legacy path, pinned released source, unchanged sibling
   hashes, retained `.install_state.json`/user settings and restart guidance.
   Test the no-profile default fallback separately.
4. In a disposable profile simulate a locked/unwritable modern or legacy file.
   The operation must report incomplete installation/removal, not success.
   A locked script's existing host enable state must remain. Retry after
   releasing the lock and verify the resulting files/status.
5. Remove only one plugin. The sibling, its host enable state, user configuration
   and unrelated files must remain. Repeat removal (honest nothing-to-remove),
   then reinstall. Test in both directions. Do not recursively erase settings.
6. Update All must skip each disabled plugin independently and must never send
   an Orca card through an EXE installer. Check any remaining update badge; a
   finished update pass is not a blanket claim that every operation succeeded.
7. Restart real Orca and confirm both unchanged capabilities of each plugin are
   still available, existing preset references/configuration load, and the
   shared `Orca-Plugins.bat` route still works. Hub's Undahang label must not
   rename plugin capabilities/Python files or duplicate/orphan shortcuts.
   Real-host execution/print validation remains tracked separately as ORCA-03.

## Preserved safety checks

- Re-run `.agent/runbooks/test-hub-open-to-front.md` on representative native
  cards/tray routes. No duplicate process, hidden Open, changed elevation or
  focus-stealing regression is acceptable.
- Confirm verified-current apps/Hub have no Update action. Refuse EXE replacement
  if any managed target remains alive, including a stray mirror. Self-update
  remains the staged close-and-prove handoff, following the release marker.
- Check icon/resource trends during repeated refreshes, Explorer restart,
  sleep/resume, explicit Quit and child fallback using
  `.agent/runbooks/test-hub-longevity.md`. APP-02's original long-session owner
  symptom remains open until its native acceptance is recorded.

The broader Weather Report / Prism Lighting / Orca public-name rollout is
NAME-01, not evidence of this Hub-only organization task being incomplete or
permission to change plugin algorithms or the lighting engine.
