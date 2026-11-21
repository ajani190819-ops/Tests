# Hub Open-to-front — native Windows check

Status: pending owner/native Windows execution. A cross-build and mocked Win32
regression are not a Windows foreground-lock or cross-integrity result.

## Setup

1. In Project Hub choose the current active Arena branch, refresh, and update
   Hub to 2.8.69 or later, plus the Bluetooth Guard panel/status update. The original route shipped
   in 2.8.63; 2.8.67 adds Guard-owned cross-integrity reveal. Keep existing
   elevation settings; do not change UAC policy or run everything elevated
   to mask a failure.
2. Cover Bluetooth Guard, Agent Notes, DNS Pilot, Audio Switcher, Weather Radar,
   and the existing Keyboard Lighting window. Do not change lighting settings,
   keys, or engine files: M4/M5 work is still parked for a fresh `engine.log`.
3. If another catalogue EXE is installed, cover its normal main window too.
   Open targets must refer to that installed image, not an unrelated same-name
   executable. Folder/browser actions are outside this native-app matrix.

## Matrix — both the Hub card and unified-tray Open

For each installed app:

- **Visible, behind Hub:** Open puts the app in front and makes it usable.
- **Minimized:** Open restores the existing window, not a second instance.
- **Tray-hidden:** close/hide normally, then Open reveals the same instance.
- **Maximized:** Open preserves maximized placement; also minimize the maximized
  window and check it restores correctly.
- **Modal dialog:** if the app has one, leave it active. Open brings its dialog
  forward rather than stranding it behind a disabled parent.
- **Repeated click/startup:** from a genuinely stopped app, click Open rapidly
  twice. One normal instance appears; a late window is focused when ready.
- **Already running from a registered stray native copy:** Open reveals that
  instance instead of launching the suite-folder copy on top of it.
- **Newer activity:** open a stopped app and switch to another application
  before startup finishes. The Hub's deferred retry must not steal focus back.

For an existing instance, the process ID should stay the same across Open.
Native single-instance protection must remain intact. Verify ordinary/elevated
child windows from the normal unelevated Hub; an already-running window should
not require a new launch merely to show it. A genuinely new elevated child
retains its existing manifest and consent behavior.

## Bluetooth Guard follow-up

The owner reports basic Bluetooth connection now works, but Hub/tray Open did
not show its panel. The new route adds a single per-window registered show-only
message to the elevated Guard, with scalar ticks, freshness checks and no policy
commands. The legacy asynchronous Hub route remains for old builds/other apps.
Automated mocks reproduce blocked foreign show/foreground calls and execute the
production Guard handler; they do not prove this was the owner's exact cause.

Prioritize the normal unelevated Hub -> elevated Guard case from card and tray,
then Guard's fallback tray (Hub exited), duplicate explicit launch, hidden,
minimized/maximized and any active modal window. Keep PID unchanged and do not
accept merely visible-but-behind as success. Move the pointer after clicking
Open; bounded retries should still work while Hub owns foreground. Switching to
another app must cancel deferred focus; a busy child's stale queued request must
not pull you back later. Record display/focus and any UAC program/path separately.

## Guard launch failure / exact Windows error — APP-01 active

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
