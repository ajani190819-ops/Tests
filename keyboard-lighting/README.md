# Keyboard Lighting

The executable, configuration, and strict compatibility constraints remain in Git; this page keeps the owner-facing diagnostics needed offline.

## Hardware diagnostic

- `lock SHARED` means another process retained a shared HID handle. Close competing lighting software before diagnosing flicker.
- A `range` below 255 means the device has fewer native colour steps, so a smooth animation can show visible stepping even when the app is sending frames correctly.

The full installation guide, controls, calibration narrative, and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.
