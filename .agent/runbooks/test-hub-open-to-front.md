# APP-01 - Bluetooth Guard and Keyboard Lighting Open

## Current owner clarification and scope

The affected apps are **Bluetooth Guard and Keyboard Lighting**. Their existing
processes sometimes fail to reveal/foreground until kill/reopen, or Lighting's
**Unfreeze**. Open currently works, and the owner sees one tray icon. This is not
permanent acceptance or a new startup repair. Do not ask which apps again, request
more logs, or send the owner through the former full matrix.

## Controlled source findings and correction

- **Guard UI lock:** `PollPairedDevices` held `g_lock` across Bluetooth first,
  next and close enumeration calls. Paint and the hidden window's housekeeping/
  heartbeat also acquire it; a delayed driver can therefore strand the UI before
  it dispatches a queued reveal. The regression pauses fake driver callbacks
  while a second thread attempts that same lock. The exact released poll fails;
  the correction passes without sleeps or timing assumptions. Enumeration now
  runs outside the lock; each observation and final cleanup remain protected.
  Actual Deny/Disconnect choices injected between records retain their current
  address-based approval/pause semantics. No Bluetooth policy/API/backoff changes.
- **Lighting cooperation:** Hub only posted the permitted app-owned reveal to
  Guard. Lighting had no receiver and relied on foreign show/focus calls. A
  controlled blocked-foreign-show case fails the old route without any restart.
  Hub 2.8.71 sends the existing scalar-tick, show-only message to both; Lighting
  opts in on its own window and dispatches its own reveal on the owning UI thread.
  Both real receivers pass hidden/minimized/maximized/modal, expiry, newer-input,
  quitting, registration/post failure and legacy fallback cases. Direct duplicate
  Lighting Open also delegates foreground permission before its existing flag/
  event signal. All Lighting Open paths use the same placement-preserving helper.
- **Open versus Unfreeze:** `RestartKeyboardLightingCleanly` explicitly stops
  `running.lock` and `engine.pid` processes and launches Lighting again. It is a
  separate action. Open never calls it, kills a process or restarts the engine.
  The workaround does not identify the original cause.

These are verified source-level defects/regressions, **not proof of the owner's
exact intermittent cause**, native foreground/UIPI behavior or all possible UI
hangs. Existing engine/input/pacing/exclusive/single-engine behavior, startup,
tray ownership, manifests, UAC/security policy and no-duplicate launch gates stay
unchanged. Historical Windows 225 refusal is a separate security boundary below.

## Release and automated verification

The verified payload includes Hub **2.8.71**, Guard and Lighting EXEs; the
other five suite EXEs are byte-identical. Eight-target Zig 0.17.1 cross-build and
full uncached checks pass with and without optional geometry dependencies.
Fifteen focused activation/presentation/poll tests pass, including both actual
WM_CREATE bodies; the production Hub/child reveal and Guard poll probes also
pass address/undefined-behavior sanitization. Initialization coverage executes
production C rather than accepting a source-text mention of the enabling call.

All **443/443 mutations** are detected, including 20 new activation/lock-boundary
breaks, with zero survivors/stale/mutation or gate errors. Clean 249-test pre/post
gates and exact source-byte restoration pass; one worker/no verdict cache,
173.053 seconds on this host. CODEMAP/default-context/whitespace checks pass.
Build/check gates must still immediately precede each implementation and marker
commit. These are not Windows foreground/UIPI, native hardware or soak results.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| ProjectHub.exe | 891904 | e13738e8d51b16df489a2a9fdb8e8c4f6ea7d668915d7f08b2558647e0c9b2c4 |
| BluetoothGuard.exe | 197120 | 19ccceee9d340df01b84b8bd69e7d3987c85bb253c09b99bee9149a089e40be8 |
| KeyboardLighting.exe | 233984 | 94916acd2bda3827a53b34c0db4656422c45e1601520b32c44f65aee3ffc1ae1 |

Delivery uses the active branch's `.agent/RELEASED` implementation SHA, not an
unreleased branch-tip download. No security block is cleared by a changed hash;
never retry a blocked app or alter protection for acceptance testing.

## Native acceptance reference - not another owner test request

Normal use of the active branch's released Hub **2.8.71** and both affected app
updates is the remaining real-world acceptance boundary; no new logs are needed
for this source task. Never launch a blocked executable or bypass security just
to test. Keep current manifests/elevation and settings. Do not elevate Hub or
change UAC policy to make focus appear to work.

If a focused native check is later appropriate, card and unified-tray Open must
show the **same PID in front**, whether behind, tray-hidden or minimized. Preserve
maximized placement and an active modal dialog. Repeated Open must not create a
second instance. A new launch retains existing consent behavior. Pointer motion
while Hub owns foreground can retry within the existing bound; switching to a
new app must cancel deferred focus. A busy child's expired request may not steal
focus later. Agent Notes, DNS Pilot, Audio Switcher, Weather and generic catalogue
activation remain automated regression coverage, not a demand to rerun all apps.
Fallback tray/direct duplicate Open use each affected app's own helper. No
Task Manager/Unfreeze workaround counts as successful Open acceptance.

## Historical Guard launch failure / exact Windows error - separate security reference

Owner evidence (2026-11-16): the Hub error dialog shows **Windows 225** for
`BluetoothGuard.exe` in `%LOCALAPPDATA%\Programs\TestsSuite\BluetoothGuard\`:
"Operation did not complete successfully because the file contains a virus or
potentially unwanted software." The earlier report established **no UAC prompt**.
This confirms a security-product virus/PUA block on the launch, not a
window-reveal failure or proof of an elevation-policy cause. It does **not**
identify the antivirus provider/detection rule or establish a false positive.
The error reporter is now owner-observed; successful launch/foreground remain
unaccepted. Do not publish the screenshot's personal Windows account path.

Microsoft's definition: [ERROR_VIRUS_INFECTED, 225 (0xE1)](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/Debug/system-error-codes--0-499-.md#error_virus_infected).
The old Hub discarded GetLastError; its NO_UI flag did not suppress security
prompts. Do not conflate this Guard launch block with OPS-01's other UAC reports.

Hub 2.8.69 captures the Windows code immediately, before handle/timer cleanup,
and shows one local Hub-owned error dialog with executable, working folder,
code and Windows description. The footer retains the app filename and code.
Missing/unknown descriptions or absent codes are explicit, never called success.
Nothing is logged/uploaded, no automatic retry/download is added, and existing
child manifests/elevation, installed-image lookup, reveal and update gates stay.

Next evidence: open Windows Security -> Virus & threat protection ->
Protection history, select the matching BluetoothGuard.exe entry, and capture
its **detection name, security provider, time, action and affected file**.
If another antivirus is active, use that provider's history/quarantine log;
the launch dialog alone does not establish Defender as the provider. This is
a read-only check: do not choose Allow on device/Restore, add exclusions,
disable protection or repackage/rename/rebuild merely to change detection/hash.
Keep the block in place while provenance and the actual classification are
reviewed. No blind retry or redownload is a root-cause repair.

Published artifact checked at `1ad9d9d` (marker blesses `543be95`):
`bluetooth-guard/BluetoothGuard.exe`, 197120 bytes, SHA-256
`4ad58a32f01a94713c37061cb79e2bf596e7c61ce8f6255b33caccb96938a2a6`.
The artifact last changed in `1839e1e`; its PE certificate-table offset/size are
both zero (no embedded Authenticode signature). Neither this fingerprint nor
being unsigned proves the installed file's safety or explains the detection.
The owner's installed bytes have **not** been compared. If the file remains
readable without restoring/bypassing quarantine, an optional read-only check is:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath "$env:LOCALAPPDATA\Programs\TestsSuite\BluetoothGuard\BluetoothGuard.exe"
```

A matching hash establishes byte identity to that artifact, not a clean malware
verdict. If inaccessible/quarantined, retain the security record; do not restore
it just to hash it. With the detection/provider and provenance, review the
source/build and pursue the vendor's legitimate analysis/false-positive review
if warranted; do not call it a false positive in advance. A signing/publishing
change is separate work, not a guaranteed antivirus fix. No app code, EXE,
manifest/elevation or security setting is changed by this triage or collector;
a tooling-only release marker does not mean a new Guard build or an accepted
launch/focus repair. No new mutation-performance comparison is claimed.

### Optional offline Guard security report

`tools/guard_security_report.ps1` is a standalone **Windows PowerShell 5.1+**
collector, separate from the existing network-based `project-hub/SuiteReport.ps1`.
Save the standalone script on the affected PC. From an **ordinary, unelevated**
Windows PowerShell window, use its actual local path, for example after saving
it to Downloads:

```powershell
& "$env:USERPROFILE\Downloads\guard_security_report.ps1"
```

No execution-policy override, Unblock-File, Run as administrator or antivirus
exception is part of this procedure. If script policy or security software
refuses it, **stop and use the security-history UI instead**. Do not relax a
policy just to run this report. Do not run the broader Suite Report for this
check: it downloads comparison executables and reads unrelated lighting data.

The script prints its output filename:
`%LOCALAPPDATA%\GuardSecurityReport-<UTC timestamp>.txt`. It creates only that
new report; it does not launch an editor, create a directory or overwrite a
previous file. Optional `-OutputPath` must name a **new .txt file in an existing
local directory**. UNC/device/relative/alternate-stream paths, network drives
and reparse-point parents are refused. These checks are safeguards, not a
security boundary against a malicious concurrent filesystem change.

Collected evidence and limits:

- Locally registered antivirus names and **raw** product states. Registration
  does not identify the active blocking product; there is no undocumented
  product-state decoding or attribution from provider presence alone.
- Defender mode/signature status and the newest 50 Guard-resource detection
  records (any age), with threat-name lookup by ID, timestamps and raw
  action/status IDs/outcomes. Only affected resources are matched: a Guard
  mention in `ProcessName` is **not** evidence that Guard was detected.
- Defender Operational events 1116-1119, selected by time/ID then filtered on
  affected `Path`/`Resources`, not localized message/process text. Defaults:
  last 14 days, newest 256 events across those IDs. `-LookbackDays` (1-90) and
  `-EventLimit` (16-2000) alter this event window only. Reaching a cap is visible;
  malformed records are counted rather than silently treated as no detections.
  Schema-named threat/action fields are retained; raw messages and user fields
  are omitted. Filename-only matches are explicitly candidates for other copies.
- Full SHA-256/size of readable installed Guard and Hub files, compared against
  **frozen** artifacts at `543be95ad860d6e11ffadf30248d413e0dcc839c`, not a mutable
  branch tip. File-version metadata is untrusted. Missing/blocked files stay
  unavailable; no restore, retry, replacement, scan or launch is attempted.
  Hash identity is not a malware verdict. Signature trust/catalog/revocation
  checks are deliberately not attempted; the script makes no network request.
- Missing commands, denied access and query failures are **UNAVAILABLE**,
  distinct from returned empty history. Third-party private histories are not
  collected. Empty or unavailable history never establishes a clean file. The
  actual matching third-party or Protection history entry may still be needed.

Only the new report is intentionally written. No download/upload, process
launch, UAC request, registry/task/service/security change or lighting read is
performed. Normal OS/antivirus on-access behavior is not disabled or controlled;
reading a blocked file can itself encounter that protection. Known profile
prefixes/control characters are sanitized, **not a promise of full anonymity**:
review the file before sharing it privately. Do not post raw owner evidence in
Git/Drive; default `GuardSecurityReport*.txt` filenames are ignored by Git.

Portable regressions enforce source/read-only/output/privacy contracts and run
the actual resource regex against synthetic paths; deliberate-break coverage
is in `tools/mutations.json`. Both PowerShell files also receive portable syntax
checks. This is **not** proof of Windows command/runtime behavior. The separate
mocked/offline Windows fixture command is:

```powershell
& .\tests\native\guard_security_report_fixtures.ps1
```

Those fixtures mock all provider queries and use only temporary synthetic
`.bin`/`.txt` files. They do not launch Guard or establish a real detection,
malware verdict, active provider, driver result or foreground acceptance.
Native fixture execution and real owner report collection remain pending until
actually run. The stock Windows PowerShell AST parser passed both unchanged
scripts in CI run `44957095411`; parsing is not mocked-fixture execution or a
real provider/launch result.

Verified in this sandbox (2026-11-16): eight new portable source/regex checks,
full uncached `check_all.py` in both dependency states, and all **344** mutations
(335 retained plus nine collector safety breaks) detected with zero survivors,
stale entries or infrastructure/gate errors; both clean gates ran **177** tests
and source restoration was exact. The required seven-EXE build reproduced
**all seven published files byte-for-byte**, including Guard's pinned hash.
Portable PowerShell grammar checks passed for both new scripts; no PowerShell
runtime/Windows provider, owner history or launch/focus execution occurred here.

The first CI Linux job failed: the provenance test incorrectly required old
Git objects that `actions/checkout`'s default depth-one clone does not contain.
A depth-one clone on the same active branch reproduced that exact `git show`
error. The corrected oracle uses independently audited, frozen known-answer
commit/full SHA-256 values instead of fetching or assuming history. It still
checks the exact pinned commit, both complete hashes and full-length comparison,
not a prefix or mutable tip. All eight collector checks and normal full checks
in both dependency states pass in that shallow/offline reproduction. No script,
app, baseline artifact or workflow was changed to repair this test dependency.

Focused source review identified the existing `SetAutostart` Run-key/Explorer
startup-delay writes and hidden `schtasks.exe` creation of a HighestAvailable
logon task (`bluetooth-guard/src/parts/autostart_rules.c`, `bt_actions.c`). The
LeastPrivilege comment disagrees with that XML. These are review findings, **not
the cause of the unknown antivirus detection**; no persistence/elevation change
is made speculatively. Source/build review is not a complete malware audit or
vendor analysis. The official read-only cmdlet descriptions are
[Get-MpThreatDetection](https://github.com/MicrosoftDocs/windows-powershell-docs/blob/main/docset/winserver2025-ps/Defender/Get-MpThreatDetection.md),
[Get-MpThreat](https://github.com/MicrosoftDocs/windows-powershell-docs/blob/main/docset/winserver2025-ps/Defender/Get-MpThreat.md),
and [Get-MpComputerStatus](https://github.com/MicrosoftDocs/windows-powershell-docs/blob/main/docset/winserver2025-ps/Defender/Get-MpComputerStatus.md).
They do not say which provider blocked this owner's file.

## Failures / boundaries

- A failed/cancelled launch shows its actual Windows failure without
  installing/updating a replacement or claiming success. A running but
  inaccessible app must not be blindly launched again. A late failed shell
  must not show a stale error over a newer successful Open.
- A window that never becomes available stops retrying after the bounded
  startup interval (10 seconds after the shell returns). The Hub stays usable.
- Explicit Quit, update stop/verify gates, and hidden/visible update restoration
  are unchanged. Test updates separately; do not turn this check into an
  elevation or startup-policy change.

Record Hub version/released SHA, app, card versus tray, starting state, resulting
visibility/focus, whether its PID changed, and any prompting program/path.
Do not mark APP-01 hardware/native verification complete from automated results.
