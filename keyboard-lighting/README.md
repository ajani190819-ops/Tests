# Keyboard Lighting

The executable, configuration, and strict compatibility constraints remain in Git; this page keeps the owner-facing diagnostics needed offline.

## Hardware diagnostic

- `lock SHARED` says this engine fell back from exclusive to shared access. It does not identify the other handle owner or prove a flicker/hold cause. A process name in the log is a lead, not a diagnosis; do not reconfigure other software on that evidence alone.
- A `range` below 255 means the device has fewer native colour steps, so a smooth animation can show visible stepping even when the app is sending frames correctly.
- `%LOCALAPPDATA%\KeyboardLighting\input-status.txt` is one small local receiver-health snapshot, written after raw-input registration and at most once per minute. It contains registration errors, aggregate report/token/repeat counts and bound/release/active booleans, never typed characters, learned key codes, raw HID payloads, device identities or settings. Counts reset with the panel process; observed tokens can include the panel's own repeats. Nothing is uploaded.
- For the remaining M4/M5 issue, use the verified release, hold/release M4 then M5, leave the panel running for about a minute, and review/share that status file. No settings or GHelper change is needed. The fresh owner engine log has already been received. See the [focused evidence and test boundary](../.agent/runbooks/test-lighting-input.md).

The full installation guide, controls, calibration narrative, and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.
