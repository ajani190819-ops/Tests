# App Review 0.1.0 — local screenshots, without changing your setup

A standalone, on-demand Windows 10 (1903+) / Windows 11 utility. It captures
**only the already-running windows you check**, without Print Screen or the
clipboard, and packages images with read-only checks in one shareable HTML file.
It is not a Hub button, installer, background recorder or automatic test driver.
No Python, PowerShell, .NET download, account or administrator permission is
needed to run the shipped `AppReview.exe`.

**Verification status:** the cross-build and portable safety checks pass;
native Windows capture and installed-app compatibility are **not yet verified**.
This is a portable pilot, not automatic acceptance. No Hub install/update button
is added. The Windows-native test kit is included for explicit manual use, not
integrated into CI; existing GitHub workflows are unchanged.

## Use it

1. Download **AppReview.exe** from this folder at the verified, released commit
   on the active Arena branch. Run it normally, **not as administrator**.
   If Windows/security blocks it, stop and report the actual message. Do not
   disable protection, add exclusions, change policy or rename/repackage it.
2. Leave the panels you want reviewed open in their current state. **Do not
   open/update blocked Bluetooth Guard for a capture.** Hub and Weather can be
   reviewed independently. Hidden/minimized panels will not be restored for you.
3. Check the desired running windows, then click **Capture + read-only checks**.
   Every row starts unchecked. **Show other running windows** adds choices such
   as Orca or Minecraft; it does not select or capture them. At most 16 windows
   per run. Windows may briefly show its normal capture border; it is not hidden.
4. Click **Open report** to inspect the result. Click **Open folder**, review
   **report.html** for private contents/titles, then attach that one file here.
   Its PNG images are embedded; no separate image upload is necessary. If you
   prefer, share only particular `window-XX.png` files instead. `report.json`
   has structured metadata for deeper inspection; it is optional to share.

Output goes to a **new** local folder each run:

```text
%LOCALAPPDATA%\TestsSuiteReview\AppReview-<random-guid>\
    report.html
    report.json
    window-01.png ...       # selected-window artifacts; check report status
```

Reports are not uploaded, sent to Agent Notes, put on Drive, put in Git, cleaned
up automatically or used to approve a baseline. Delete your own report folders
in Explorer when you no longer need them. Interrupted/failed runs can leave
partial files; only images explicitly referenced by a complete report are usable
evidence, and even those remain unverified. **Open report/folder** launches the
normal local viewer/Explorer only when you click it.

## Hard settings-preservation boundary

- **No target input at all:** no clicking, typing, tab navigation, menu commands,
  toggles, reset/restore tests, saving, launching, installing, updating or removal.
- **No app configuration is read or written.** No temporary setting changes
  followed by a rollback. No device, Bluetooth profile, network/DNS preference,
  location permission, lighting, Orca preset, game option or security change.
- No target activation, moving, resizing, minimizing, restoring, scrolling or
  closing. Apps continue their own normal background activity; the utility does
  not freeze them or promise that their own files stop changing.
- No whole-desktop copy, clipboard fallback, protected-content workaround,
  capture-border suppression, policy/elevation change, startup task or service.
- Only this utility's capture subprocess can be timed out/cancelled. A stalled
  capture never terminates or restarts the selected application. Each subprocess
  is job-owned with an eight-second limit; closing the chooser cancels its work.
- The chooser is single-instance. A second launch reveals **the chooser only**.

This first version deliberately uses a **zero-action allowlist** for target UI.
The owner approved safe checks only if their intended settings remain intact;
navigation with uncertain persistence is not treated as safe merely because it
looks like a tab. More active functional tests are not silently enabled.

## What its checks mean — and do not mean

The tool verifies the selected live HWND/PID/process-creation identity, session,
window class, visibility/minimization/cloaking, sensible size, a bounded
**WM_NULL responsiveness ping**, and unchanged window rectangle/placement/DPI
around capture. Standard direct-child bounds can flag **possible clipping**;
custom-painted UI and intentionally scrolled children are not fully covered.

Screenshots come from **Windows.Graphics.Capture / CreateForWindow**, using a
one-frame, free-threaded capture session. They are OS-composited images of the
selected window, not offscreen app fixtures or `WM_PRINT` renderer exports.
Capture sessions are closed immediately after copying the frame. Ordinary
Windows capture permission/protection still applies. A successful read of
nonzero display affinity skips the window. That getter is not available for
every non-layered window; when unknown, the OS capture API still enforces the
restriction. Failure is **never** retried via PrintWindow, desktop copying or
another capture API. No display-affinity flags are changed.

A blank/nearly uniform image, missing frame, identity/placement change, storage
error, protected/hidden/minimized window or timeout is an **evidence gap**, not
a successful review or proof of an app defect. GPU/HDR/fullscreen/remote or
unsupported content can still be incomplete. Successful images are labelled
**IMAGE_UNVERIFIED**, never PASS. They still need visual inspection.

Reports contain only selected titles/images, executable basenames, PID/creation
identity, class, file-version labels, capture time/dimensions/DPI and check
results. No full executable paths, process command lines, configuration contents
or unselected-window inventory is exported. Version resources identify on-disk
labels, **not** the loaded image hash, signature or security verdict.

**No automatic acceptance:** visual baselines, planner logic, map/location
accuracy, connection behavior, settings persistence, Hub longevity, real Orca
execution, Minecraft behavior and physical prints require their own evidence.
A ping reply cannot establish those functions. Upload a reviewed report for the
agent's visual assessment; report behavior you actually observe separately.

## Developer verification / ABI sources

`python3 tools/build_windows.py` cross-builds the seven existing native apps plus
this standalone utility. Existing app EXEs/settings/versions need not change.
`python3 tests/test_app_review.py` runs portable policy, safety and result-schema
regressions. Full gates and deliberate-break mutations remain required.

The standalone Windows command `tools/test_app_review_windows.py` requires
preinstalled `ziglang==0.17.1`. Its custom-painted **synthetic WM_PAINT-only
fixture** exercises the real subprocess, OS capture, PNG decoding, unchanged
settings sentinel and placement, protected/minimized/identity skips, blank
detection, overwrite refusal, timeout/cancellation containment and the real
unchecked chooser. The kit is **not CI-integrated and has not run natively**.
Existing Repository Checks cover portable/source/schema regressions, not WGC.
Even a native fixture pass is **not installed-app or owner-desktop acceptance**.
See [the focused runbook](../.agent/runbooks/test-app-review.md).

Minimal WinRT ABI declarations in `src/parts/wgc.c` follow Microsoft's SDK,
including default-interface QI and IInspectable/IUnknown vtable layout:

- [Windows.Graphics.Capture interfaces](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/winrt/windows.graphics.capture.h)
- [CreateForWindow interop](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/Windows.Graphics.Capture.Interop.h)
- [Direct3D interop](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/windows.graphics.directx.direct3d11.interop.h)
- [Direct3D default device interface](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/winrt/windows.graphics.directx.direct3d11.h)
- [IClosable](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/winrt/windows.foundation.h)

No complete SDK headers, downloaded frameworks or owner captures are vendored.
