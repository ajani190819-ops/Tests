# APP-04 — settings-preserving App Review

Owner scope: selected-window captures plus safe checks **only if intended app
settings remain intact**. The first release uses no target UI actions at all:
read-only identity/visibility/size/DPI, WM_NULL, placement and child-bound hints.
No settings snapshots, temporary changes/rollback, app launch or active driver.
User instructions and exact boundary: [`app-review/README.md`](../../app-review/README.md).

## Local gates

```sh
python3 tools/build_windows.py
python3 tools/check_all.py --no-cache
python3 tools/mutate.py
```

The builder now includes seven native apps plus AppReview.exe and returns
nonzero for any compiler failure. The seven pre-existing EXEs must remain
byte-identical for this tooling-only release. Check the patch for lighting,
product/plugin/HUD source, catalogue and other unintended changes. Regenerate
CODEMAP; keep private reports outside public Git/Drive. Build/check precede both
implementation and the separate `.agent/RELEASED` marker commit.

Portable tests compile the actual policy with the host C11 compiler, exercise
all gate combinations, bounds/overflow, name categorization, near-blank pixels,
inert Unicode escaping and non-accepting statuses. Source contracts also guard
selection, the job-owned self worker, local exclusive/reparse-safe output,
OS-protected one-frame capture, cleanup, no screen/print fallback, no target
input/config access, nominal PNG/metadata validation and native fixture schema.
These do not simulate or claim native WGC/D3D/WIC execution.

## Standalone synthetic native Windows fixture (manual, unrun)

This kit is **not CI-integrated**. With Python and `ziglang==0.17.1` already
installed, ordinary Windows:

```powershell
python tools/test_app_review_windows.py --output "$env:TEMP\AppReview-native-new"
```

Use a new short absolute local directory outside the checkout. Existing/reparse
or remote output is refused. The runner does not install dependencies, alter
policy, elevate, find app profiles or launch installed suite apps. It compiles
`tests/native/app_review_windows.c`, which includes production source and creates
only its own WM_PAINT-only synthetic window; OS compositing does not depend on a
custom print handler. It exercises:

- actual process/job/IPC/capture, WIC PNG decode and known color pixel;
- no target input, preserved in-memory preference and unchanged sentinel file;
- unchanged placement; process creation/class mismatch rejects capture;
- minimized stays minimized; protected content is not captured;
- blank images are not usable; existing files are not overwritten;
- stalled worker is timed out/cancelled while its synthetic target survives;
- protected content cannot produce an image even when its getter is unknown,
  and normal capture works again after fixture-only protection is removed;
- attempted-worker timestamps remain real; unscheduled cancellations have none;
- local reports remain non-accepting/inert; chooser starts unchecked.

`native-review.json` is written only after exit 0 plus the strict native schema
and all required checks, plus validation of the actual PNG/JSON/embedded HTML.
Artifacts/logs remain in that new local output directory; there is no automatic
upload or retention job. Compile failure, timeout, missing/coerced fields or
nonzero input cannot turn into a pass. A headless/unsupported OS is a failure to
obtain native evidence, not permission to substitute synthetic PNGs. No dispatch
permission workaround or blocked Guard launch is part of this procedure.

## Owner pilot — acceptance remains OPEN

1. Run the released AppReview.exe normally. If security refuses it, stop and
   retain the real message; no Allow/Restore/exclusion/elevation/policy workaround.
2. Select already-open Hub/Weather panels, without opening blocked Guard.
   Other apps/Orca/Minecraft are opt-in unchecked choices, not automatic launches.
3. Capture once and inspect report.html. Confirm correct current images, no
   obscuring unselected-window contents, honest skips/failures, usable actual DPI
   and expected chooser behavior. Do not reset app settings for this test.
4. Review private selected contents before manually sharing report.html (images
   embedded) or selected PNGs. No automatic upload or Agent Notes/Drive export.
5. Agent reviews visuals jointly with owner observations; close only evidenced
   parts. A ping/bounds hint is not functional testing, a baseline approval,
   APP-01 trust clearance, BT hardware success, location accuracy, Orca callback,
   F5W behavior, lifetime soak or physical-print acceptance.

The original Print Screen failure is not diagnosed by this tool or a CI fixture.
Unsupported/blank capture results stay open evidence gaps, not app-defect claims.

## Local verification record — 2026-11-16

- Eight-target Windows build and strict final native-fixture cross-compilation
  pass. AppReview.exe: **210432 bytes**, SHA-256
  `523443b573c2f904352732ac396586581e191231d2027a5070bde985180a060d`.
- **29** portable policy/source/protocol regressions; full uncached repository
  checks pass with and without optional geometry dependencies.
- **410/410** deliberate-break mutations detected, zero survivors/stale/mutation
  or gate errors; **237-test** clean pre/post gates, exact byte restoration.
  One worker, no verdict cache, **226.416 seconds** on this host.
- All **seven pre-existing app EXEs** remain byte-identical to the pre-task hash
  snapshot. No pre-existing app/plugin/HUD/lighting source or settings changed.
- These local results are not native WGC execution. Normal Repository Checks
  cover portable/source/schema checks only. The standalone native kit's output
  and source/artifact manifest must be assessed separately. The installed-app/
  owner-desktop pilot and all joint acceptance remain OPEN even after a manual
  synthetic native pass.

### Delivery boundary — optional CI change excluded

The original unpublished package also edited `.github/workflows/ci.yml`, and
GitHub refused that push because its App lacks `workflows` permission. That
optional CI hook is **not part of this delivery**. Existing workflow files and
`docs/ci/ci.yml` stay byte-identical to the published baseline. No credential,
permission or policy change, alternate execution route, force-push, or native
execution hidden inside an existing CI script is used.

Only this session's two never-published commits were unpacked on the same
branch to separate the optional workflow edit; no published history was changed.
The native runner/fixture and every portable safety test remain included. The
native CI hook remains deferred; it would require legitimate workflow permission
before a future integration. Publishing ordinary tool/test/docs files does not
establish native capture success or close the original Print Screen report.
