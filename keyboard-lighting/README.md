# Keyboard Lighting

The executable, configuration, and strict compatibility constraints remain in Git; this page keeps the owner-facing diagnostics needed offline.

## Open the existing panel

The Hub 2.8.71 follow-up adds Lighting's own show-only reveal handler for an
existing, possibly elevated panel. Hub card/tray, fallback tray, first launch
and existing-instance signals preserve maximized placement and an active dialog.
A direct second Open delegates foreground permission before signaling the
running panel. No extra panel/engine is started merely to reveal the window.
**Unfreeze** is a separate restart, not a required part of Open.

Only the registered show-only message is permitted on this window across UIPI;
it carries scalar ticks, expires after one second and rejects newer input.
Engine/input behavior, settings and existing elevation contracts are unchanged.
Controlled production-C tests are not native Windows acceptance or proof of the
owner's exact intermittent failure. The prior report that Open did nothing
visible is now superseded: the owner says Lighting is fine and UAC/startup seem
fixed. Record current-use approval, not a permanent-cause diagnosis or a new
Guard outcome. No additional log/test request. The 2.8.72 follow-up adds recovery
for a fully offscreen normal
panel/dialog on the nearest monitor's work area, retaining its size and leaving
connected/partial or maximized placement alone. The Hub no longer counts an
offscreen active window as completed Open. This source regression is not proof
that the owner's window was offscreen. See the [Open record](../.agent/runbooks/test-hub-open-to-front.md).

## Hardware diagnostic

- `lock SHARED` says this engine fell back from exclusive to shared access. It does not identify the other handle owner or prove a flicker/hold cause. A process name in the log is a lead, not a diagnosis; do not reconfigure other software on that evidence alone.
- A `range` below 255 means the device has fewer native colour steps, so a smooth animation can show visible stepping even when the app is sending frames correctly.
- `%LOCALAPPDATA%\KeyboardLighting\input-status.txt` is one small local receiver-health snapshot, written after raw-input registration and at most once per minute. It contains registration errors, aggregate report/Custom-token/fine-step counts and bound/release/active booleans, never typed characters, learned key codes, raw HID payloads, device identities or settings. Counts reset with the panel process. Nothing is uploaded.
- **M4 held dimming still fails according to the owner after the released rebuild.** General Lighting/Open approval does not supersede that result. The fresh-log gate is already met by engine-8; no further log or `input-status.txt` request is pending. Do not repeat the evidence loop or change G-Helper/settings to force collection. See the [focused evidence and test boundary](../.agent/runbooks/test-lighting-input.md).
- The branch receiver correction treats ASUS 0x5A byte1 as a full-byte action ID, including high-bit presses. A matching zero from the same raw device establishes HID release; guessed high-bit key-ups and old guessed-release metadata do not. A Custom launch alone cannot say whether M4 is still physically held. Without a separate release or genuine repeat stream, one press must remain a tap, not an invented glide. The correction is **unreleased** and is not a claim that physical M4 is fixed.

The full installation guide, controls, calibration narrative, and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.
