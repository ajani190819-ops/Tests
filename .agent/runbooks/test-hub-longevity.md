# Project Hub longevity — Windows acceptance

Status: **procedure, not a completed Windows result** (2026-11-16).
Scope: APP-02, Hub 2.8.64. The owner's original description is that the tray
app can "expire" during prolonged use. Its precise Windows symptom/cause has
not been captured; do not mark that report resolved from source tests alone.

## Reproduced source defects

- Every successful catalogue refresh loaded new private built-in icon handles
  without releasing the preceding handles. Seven built-ins could accumulate
  seven additional USER handles per pass. The deterministic production-C test
  fails on the second pass before the fix. Replacement now frees the old icon
  under the lock also held through its draw; a failed load retains the old icon.
- A missing shell icon could leave `g_hubTrayVisible` true indefinitely when
  `TaskbarCreated` was missed. The production-C test reproduces this by removing
  the mocked shell icon without sending that message. The two-second local poll
  now verifies the shell, restores the same identity, and drops the capability
  marker while registration is unconfirmed. Duplicate registration/broadcasts,
  transient shell/marker failures, and explicit Quit/removal are covered.

The harness executes 10,000 icon-refresh rounds (70,000 successful loads) and
10,000 tray polls with repeated silent icon loss. It mocks Win32 I/O and is NOT
an accelerated real Windows soak, Explorer test, or proof of the reported cause.

## Native checks

1. Select the active Arena branch in Hub and **Update Hub** to 2.8.64. Verify
   the running image/path in Task Manager; do not use the legacy PowerShell
   `Setup.ps1` to replace the native EXE. Keep only one Hub process.
2. Let its initial catalogue/icons load. In Task Manager's Details columns,
   record PID, USER objects, GDI objects, handles and memory. Close the Hub
   window normally; its one tray icon/menu should remain available.
3. Leave it running through normal background checks for several hours, then
   through an interval at least as long as the previously observed failure
   (overnight/24+ hours if the interval is unknown). Sample the same PID and
   resource counts periodically. Counts may vary with open windows, DPI,
   menus and caches; look for a continuing per-refresh rise after warm-up,
   not an exact fixed native count. Do not hammer GitHub with refresh loops.
4. While Hub is hidden, check tray Open/Hide and safe app controls. Repeatedly
   Open Hub and a running app; verify foreground behavior and no duplicate
   processes. Closing the Hub window must still only hide it.
5. Save any Explorer work first. If comfortable, restart **Windows Explorer**
   using Task Manager. The Hub's one tray icon should recover without opening
   its window or relaunching Hub/child apps. No duplicate icon should persist.
6. Sleep/resume and lock/unlock the PC. Recheck the tray, menus, same Hub PID,
   local controls and resource trend. Temporarily losing network access must
   not terminate Hub or remove its local tray controls; remote checks may
   report unavailable and recover later. Agent Notes delivery depends on its
   separate receiver/session and is not guaranteed by Hub tray recovery.
7. Choose **Exit Project Hub** explicitly. Hub must stop, must not recreate its
   icon/relaunch itself, and already-running suite apps must return to their
   individual tray fallbacks on their normal timers. Reopening Hub manually
   should consolidate them again. No startup task/Run key/service is added.
8. Retain the existing APP-01 Open matrix and normal safe update checks. Close
   and prove managed targets stopped before replacement; respect UAC/elevation
   and cancel/refusal paths. The repair must not bypass these gates.

## Evidence / failure classification

Record the build, local time/uptime, triggering action (idle, sleep/resume,
Explorer restart, Open, update or Quit), Hub PID/path and resource samples.
If it recurs, distinguish missing icon with a live responsive Hub, a hung Hub,
an exited process, a stopped child app, or a disconnected Agent Notes receiver.
Use Windows Application Error/Windows Error Reporting events for an actual
crash; do not infer a timer, token expiry or licensing cause from the word
"expires". Keep APP-02 native acceptance open until this is checked.
