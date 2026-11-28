# KBL-01 - brightness rebuild and saved-state restoration

## Current owner confirmation - M4 fix is working

The owner now confirms that the M4 fix is working.
Record the reported M4 issue as owner-confirmed resolved after the receiver
release. This supersedes earlier M4-failure/open-acceptance wording below;
do not request another M4 log or hold/input-status matrix. It is owner-use
confirmation, not independently executed hardware testing, installed-byte
proof, a newly isolated cause or fresh acceptance of M5/all modes.

Documentation-only receipt `59501ee` recorded that confirmation for release
`e664f7b8417ce788fe2f59b2421190a2416a9b90`, selected by marker `807d0ba`.
The following colour-output correction is a separate implementation, not a
new M4 repair or a claim that tests established the owner's physical cause.

The owner has removed the fresh-log prerequisite for Lighting edits. Logs are
optional diagnostic evidence, not an edit/release gate; no new log is requested
to continue. Earlier fresh-log conditions below are historical, not current.
This is removal of the standing rule, not a one-off waiver. AGENTS and current
style/roadmap/setup guidance no longer reinstate that prerequisite.

## Armoury Crate research - physical flicker still open (2026-11-16)

The owner still reports substantial flicker after the cadence release. This is
research, not another shipped correction or hardware acceptance; the installed
revision and live brightness have not been newly established. No fresh log is
required. The released engine/EXE/settings and working M4 remain unchanged.

- ASUS's [SDK 3.1 tips](https://www.asus.com/microsite/aurareadydevportal/tips.html)
  recommend controls around 60ms and say its own software used 64ms (15.625Hz).
  This is older SDK-era guidance, NOT a measured/current Armoury Crate rate,
  safe HID rate for this keyboard, or a reason to replace Prism's 60 FPS.
- ASUS's [Apply contract](https://www.asus.com/microsite/aurareadydevportal/interface_aura_service_lib_1_1_i_aura_sync_device.html)
  and [C# tutorial](https://www.asus.com/microsite/aurareadydevportal/tutorial_csharp.html)
  compose LED colours before applying a device. Apply always returns S_OK;
  neither this API nor a successful HID transfer proves physical smoothness.
  Public documentation does not establish current proprietary interpolation,
  PWM depth/frequency, dithering, or a notebook-specific anti-flicker algorithm.
- Firmware presets and host-streamed effects are different paths. Do not claim
  Armoury Crate always offloads: [liquidctl's pinned guide](https://github.com/liquidctl/liquidctl/blob/292dc561361cfbc0cdee155c26662131fe160989/docs/asus-aura-led-guide.md)
  documents continuous Armoury direct-mode streaming on specific motherboards,
  not this notebook. [Pinned G-Helper LampArray](https://github.com/seerge/g-helper/blob/064cd7e39ee37477fcb433c8d2d15bef081d7d5f/app/USB/AsusLampArray.cs)
  uses a vendor-channel selector plus autonomy control, RGB with full intensity,
  and COMPLETE on the last batch. These are comparison evidence, not permission
  to inject its model-specific selector into Prism or substitute a preset.
- The [LampArray standard](https://www.usb.org/sites/default/files/hutrr84_-_lighting_and_illumination_page.pdf)
  separates RGB levels, intensity gain, per-lamp attributes and minimum update
  interval; COMPLETE applies buffered batches together. Prism already batches
  this way. Microsoft documents [collection-scoped HID opens](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/opening-hid-collections):
  an exclusive handle does not exclude unrelated vendor or WMI paths. This
  scope caveat is not evidence that a competing writer is actually present.

Prism's released `SetZoneG` dims encoded RGB before quantization; report intensity
stays at `MaxI`. Discovery currently uses lamp 0's RGB/I maxima for every lamp,
although the standard permits per-lamp capabilities. These are verified code
properties, not a measurement of this keyboard's intensity/PWM implementation.

A numerical replay extracted SetZoneG/SeedLamp from released engine `29e6b5d`
and reused the committed LightingColourTests extractor for UseDither/Dither/Push.
Only C#-to-C syntax and synthetic I/O/time were substituted. GCC warnings-as-errors
and UBSan passed. With channel maxima 255, full intensity, Steady rounding and 4,097 input
samples from 0..255, the CURRENT dimming formula gives:

| Combined group brightness x master | Unrounded white target | Distinct channel codes, including off |
| --- | --- | --- |
| 100% | 255.000000 | 256 |
| 50% | 66.413943 | 67 |
| 30% | 24.881367 | 26 |
| 10% | 3.415853 | 4 |
| 4% | 1.039190 | 2 |
| 1% | 0.593032 | 2 |

This is a software precision budget, NOT measured LED output or the owner's
current settings/cause. It follows `gain = 0.0022*ramp + 0.9978*pow(br,1.95)`;
old nearby comments about linear-light dimming do not describe executable code.
In a separate synthetic eight-second/60 FPS rising fade, explicit dither still
had zero carry at 10%/4%/1% because per-frame motion stayed below 0.05. At 30% it
produced reverse one-code steps. Neither increasing FPS nor forcing dither is a
universal remedy; selected policy and the brightness curve remain unchanged.

NEXT within KBL-01: audit capability-gated brightness representation and proper
per-lamp ranges/control routing before another patch. Independent intensity is
worth evaluating only where supported; firmware may multiply back into the same
8-bit PWM, so retaining more RGB codes alone cannot prove smoother light. Do
not silently change master/curve, toggle dithering, offload a different effect,
remove pacing/PinGate(3), or alter vendor/startup settings. Balance/Steady
restoration acceptance and the independent Tray rounded-master audit stay open.

## Continuing flicker: frame transaction and cadence follow-up

The owner reports residual per-update flicker after the Steady-policy release,
especially a half-speed scrolling gradient. That is a continuing failure report,
not physical acceptance of the earlier correction. No new log is required.

Verified wiring: the native and legacy Tray launchers request `-Fps 60`
independently of animation speed. Group speed changes phase advance, not the
HID deadline. Device minimum-interval caps and qualified overload backoff still
apply. The examined G-Helper snapshot (`064cd7e`, 2026-11-16) uses firmware
commands for its legacy standard-effect route, plus Windows Dynamic Lighting on
configured models; its `ApplyGradient` is static, not Prism's streamed scrolling
gradient. This comparison does not identify the owner's firmware/build or prove
a hardware limitation. Sources: [pinned Aura](https://github.com/seerge/g-helper/blob/064cd7e39ee37477fcb433c8d2d15bef081d7d5f/app/USB/Aura.cs),
[maintainer's firmware distinction](https://github.com/seerge/g-helper/issues/3410).

The follow-up corrects independently reproduced software faults:

- The render thread and PowerShell maintenance poller shared a HID handle but
  not a transaction lock. A controlled two-thread replay placed `PinGate(3)` or
  ownership repair between a colour batch and COMPLETE; forced invalidation
  could also be overwritten by in-flight cache promotion. One monitor now covers
  the whole `Push` transaction, gate/ownership writes and open/reopen/upgrade/
  close lifetimes. The exclusive OS handle alone never serialized these two
  threads within our own process. Maintenance is retained, not disabled.
- Successful frames did not reset overload qualification. Isolated costly misses
  could accumulate to 120 and permanently reduce FPS. On-time frames and late
  frames with cheap HID now reset the run. A genuine 120-frame costly run still
  backs off; the integer rate and deadline period agree, clamp at 15 FPS, and
  never raise a lower device-reported cap.
- Unchanged frames clear `WriteUs`, rather than attributing an earlier slow
  transfer to a later frame that sent nothing.
- `Now`/preview/overlay timers receive unscaled stopwatch time. Previously the
  startup speed also scaled this clock, slowing preview publication and
  stretching overlays. This was separate from the physical HID deadline.
  Group position, chosen speeds and measured-dt decay behavior are unchanged.

Before correction, the new 17-test focused replay had nine assertion failures;
all 17 pass after it. The portable fixtures mechanically execute production
output/maintenance methods and the complete `Loop`, substituting only platform
I/O, clock/render workload and exception syntax. Concurrency and virtual timing
are not a model of the owner's EC/USB implementation. Windows CI compiles the
unmodified embedded engine with Framework `/langversion:5`, then executes the
actual C# output, transaction and full-loop replay under PowerShell 5.1 with the
same synthetic seams. Test worker threads are not a started engine or device.

All 203 Windows-app tests and full uncached repository checks pass in both
dependency states. All eight output and seven cadence scenarios also pass GCC
UBSan. All 667/667 registered mutations are detected, including 19 new cases;
the prior 648 definitions remain unchanged. Zero survivors/stale/errors/skips,
clean 346-test pre/post gates, exact restoration, one worker/no verdict or
artifact cache; 327.550 seconds. Registry SHA256:
`86af1ea0b56fdbeea3820d7bd908d105e83c6cc33b7dbcbc94bcd80df4dbadc0`;
mutated-source fingerprint:
`ff42b0cb4d1919b8d1076f7a5bb1ffc2da80b1135313eab5c172e52207d397b7`.
Reports/scratch stay outside Git. CI status is attached to the published tip;
Linux syntax trees are not stock Framework execution or hardware acceptance.

Only Aura and its exactly matched Lighting EXE change among product deliveries;
Tray and the other seven EXEs remain byte-identical. Verified implementation:
`29e6b5dfd8b1cf8b30bf765a2fa4bac0eecc906c`; a separate marker commit selects it.
Update via Project Hub on the same active Arena branch. Lighting EXE: 463360
bytes, SHA256 `1f4f9498f3406490d1aebbe03058f0e413f5ceb5ee9134c23032098c31ffb804`;
Aura SHA256 `71d4a8b7276714ffbb81744b5abf1a4bd6a597e652c891871f38f8e554123dea`.
Hub remains 2.8.72; plugins remain 0.0.58/0.4.13. No forced toggle, palette/speed
change, global dither disable, preference reset, input remap or vendor/startup
change. Preserve exact brightness/master, working M4/M5, all eight Balance/
Steady combinations, `PinGate(3)`, exclusive lock, pacing/retries and one engine.

These are verified software fixes, not proof the remaining physical flicker is
gone or that any one fault caused this owner's symptom. KBL-01 remains CURRENT
for the installed outcome, Balance/Steady restoration acceptance and independent
Tray apply/refresh/resume rounded-master audit. Logs remain optional; no repeated
M4 matrix. Processed G-code preview NEXT after Prism; renumbering PAUSED,
ORCA-04/05 PARKED, further PLAN-02 DEFERRED. All 20 IDs remain retained.

## Colour-transition software correction

`LampEngine.Push` formerly enabled temporal error feedback for a moving target
below peak14 even when `UseDither` was false. Thus saved Steady (`DitherMode=0`)
could still emit alternating/reverse adjacent codes during an increasing fade.
The smallest correction requires `UseDither` AND motion AND a non-black target
at every brightness. Dither is not globally disabled: explicit on and coarse
Auto remain available. Held/off/black targets round straight and clear carry.
Neutral device-space RGB still shares one error and code. Staged promotion only
after every report succeeds, retry/loss accounting, report flags, force/skip
behavior, interleaved/planar layout and between-batch pacing are unchanged.

The new production-method replay failed twice on the old code and passes the
correction. All 37 focused Lighting input/state/output tests and 193 Windows-app
tests pass; all eight owner-handoff tests pass. Six output scenarios exercise
rising/falling fades across the old threshold, full/coarse/mixed channel scales,
live policy changes, held/black/neutral output, partial/failed batches, forced
frames and pacing. The portable C replay mechanically adapts the actual getter,
quantizer and Push body; it is not a separate quantizer implementation. Windows
CI's existing PS5.1 entry also compiles the unmodified full embedded engine with
Framework `/langversion:5`, then executes actual C# methods with only HID/pacing
shims. Neither replay opens devices, starts engine threads or reads owner files.

All 648/648 registered mutations are detected, including 16 output regressions
and the revoked-policy regression; the prior 631 definitions are unchanged.
Zero survivors/stale/errors, 336-test clean pre/post gates, exact restoration,
one worker/no verdict or artifact cache; 240.012 seconds. Canonical registry
SHA256 `14093d93cc01646be9c763138b417597576a4160b3b8b72743646a95629c3490`;
mutated-source fingerprint
`d9a07732a21a76f9a03448a8da12db5841e81b209d8d3c4fe51dc094f6426915`.
Full reports and scratch binaries stay outside Git. All six output scenarios
also pass GCC UBSan. The eight-target cross-build and full uncached repository
checks pass in both dependency states. Only Aura and the matched Lighting EXE
change among delivered product files; Tray and the other seven EXEs are byte-
identical. The separate release marker selects verified implementation
`bd331b919768d70f651f2f1d965a6e44723ace3d` on the active Arena branch.
Update Prism Lighting through Project Hub with that same branch selected;
Hub remains 2.8.72 and plugins remain 0.0.58/0.4.13. No merge to main is needed.

Saved modes, all eight Balance/Steady combinations, exact master, distinct M4/M5
bindings/rates, exclusive lock and single-engine protections are preserved.
No forced toggle, speed tuning, preference reset or vendor/startup change.
This establishes a software policy defect and tested correction, not physical
flicker acceptance or proof of the owner's particular hardware cause.

Balance/Steady restoration acceptance and the separate Tray apply/refresh/
resume rounded-master audit remain open. Orca Save stays owner-confirmed fixed.
Processed preview follows Prism; renumbering PAUSED, ORCA-04/05 PARKED and
further PLAN-02 DEFERRED. All 20 task IDs and prior history remain retained.

## Owner-approved receiver correction release (publication checkpoint)

The owner approved publishing the verified input receiver correction first,
without remapping M4/M5, changing speeds or resetting saved choices. The source
implementation is `5e9abad05f786bbda07d8757b2921f8497493c81`; this publication
only updates release documentation and the separate `.agent/RELEASED` marker.
Product/test/registry bytes remain identical to that verified implementation.
Hub stays 2.8.72 and plugins stay 0.0.58/0.4.13. Only Lighting EXE differs from
the prior released rebuild; Aura/Tray and all other binaries are unchanged.

At publication, the focused installed check was one M4 hold then release:
initial 5% tap, fine dimming while physically held, then stop on release in the
existing sweep mode, preserving the owner's saved mode and bindings. M4
acceptance was open then; the subsequent owner confirmation above now resolves
the reported M4 issue. Balance/Steady acceptance remains separate and open.
This is not a renewed mode/log/input-status matrix or proof of a specific
installed byte identity/cause. No firmware/vendor change or glide substitute
is authorized.

The prior 631/631 mutation campaign and 17+9 synthetic UBSan scenarios below
apply to these identical product/test bytes; publication does not count them
as newly executed. Recheck build, full repository tests, registry/source
fingerprints and public marker/artifact bytes for delivery. Windows CI and
installed physical behavior remain distinct gates. The current marker names
the complete verified release commit, including these delivery notes.

## Engine-9 received - startup/status only, not key evidence

Fully scanned the owner's new `engine-9.log`: 220292 bytes, 866 UTF8-sig
lines; SHA-256
`a6636c238f43a7372df35a836b2601e94eccd4d389d50ffeb81594280965cacb`.
Its first 219809 bytes are byte-identical to engine-8; exactly 483 bytes/four
lines are appended. Those add a shared-device/vendor-process notice, an owner
watch notice, available hardware-backlight control and one device-write sample.
All 848 device samples report SHARED with zero failed writes/reopens; the file
has 854 WARN and 12 INFO lines. The last 60fps sample reports four late frames,
not a failed write. There is no key-down/up/repeat, Custom, WM_INPUT, binding,
mode, saved Equalise/Smooth or implementation-identity trace.

This confirms an appended startup/status group, not physical M4 delivery, an
owner-specific cause, absent hardware release signals, simultaneous engines or
saved-state acceptance. SHARED/GHelper and late frames do not establish a hold
cause or authorize vendor changes. The log gate remains satisfied; no further
engine log or old input-status/hold matrix is requested. At that receipt-only
checkpoint the M4 failure remained unresolved and the receiver was unreleased;
product code, preferences, bindings, rates and marker were unchanged by review.
Publication was subsequently approved as recorded above, not by the log itself.
The raw attachment remains private; only these aggregate findings are recorded.

## Prior owner result - M4 held dimming failed after the rebuild

After the earlier published rebuild, the owner reported that holding M4 did
not produce smooth dimming. The outcome was FAILED at that checkpoint; the
later owner confirmation above supersedes that M4 result after the receiver
release. Prior automated/Windows adapter passes were not hardware acceptance.
The earlier report did not identify exact installed bytes, M5/tap behavior or
Balance/Steady restoration. No equivalent log or old input-status matrix is
requested. Engine-8 met that input-scope gate. No mode/binding/vendor reset.

Read-only upstream review is pinned to G-Helper commit
`a3c3ccae51f005d92d8d4ee161e6661ab511274d`:
https://github.com/seerge/g-helper/tree/a3c3ccae51f005d92d8d4ee161e6661ab511274d/app/Input
`KeyboardListener` passes positive eight-bit 0x5A event IDs, excluding 0xEC;
`MKeyControl` can route a Strix physical M-key software action through event
181 (0xB5). That does not identify the owner's G-Helper build or emitted code.
The pre-correction released decoder turned every unlearned high-bit event
into a release and masked off bit 7; even a saved high-bit binding matches its
own invented release form. The new 0xB5 production-receiver regression went red
against that code before the correction. This establishes an unsupported parser
assumption, independent of ramp speed, not the owner's emitted code or cause.

The owner-approved receiver release corrects the following input defects:

- Preserve all eight event bits. Ignore non-0x5A, missing-device and 0xEC action
  reports. Retain an ignored-event barrier so a later zero cannot be paired
  across it. Pair real zero frames with the most recent report on that same
  raw device, never another device or a guessed press|0x80 code.
- `M4HidRelease/M5HidRelease=2` identifies an observed zero-frame format.
  Preserve old value1, but it can include guessed releases and cannot authorize
  HID hold. A keyboard up or silence timeout does not establish this format.
  No scan/HID binding or mode/preference is reset or copied across directions.
- Prefer a live keyboard source over duplicate HID reports. A remembered but
  inactive keyboard binding neither grants release capability to a HID-only
  gesture nor lets its unrelated VK cancel a verified HID hold.
- A lone press without release capability remains one 5% tap. Genuine repeat
  reports can sustain bounded fine steps; 300ms silence retires that gesture
  without claiming a physical release. A new Custom launch cannot be swallowed
  by the expired gesture, in either Custom/raw queue order. Pending learning
  must not reattach it to the consumed gesture rejected by token arbitration.
- Rates, 30ms pacing, 120ms controller freshness, 10s ceiling, exact master,
  saved state, ASCII/C#5 and exclusive/single-engine safeguards are unchanged.

Important input-contract limit: upstream `KeyboardListener` only forwards zero
release events for Ally, and `RunKeyCommand`/`CustomKey` launch a command without
a corresponding key-up callback. Upstream maintainer explanation of one-shot
software actions versus firmware-held keys:
https://github.com/seerge/g-helper/issues/3653#issuecomment-2614482854
https://github.com/seerge/g-helper/issues/3653#issuecomment-5159250410
These references do not prove that the owner's physical M4 supplies no other
input. They do prove that Custom launch alone cannot satisfy while-held/stop-
on-release behavior. Do not invent a held state, switch to latch/burst, remap
firmware/vendor settings or request another equivalent log/matrix to hide it.
Physical M4 acceptance was open at publication; the owner now confirms that
the fix is working. Do not reopen M4 or demand another input source/matrix on
that superseded failure report. A future regression would need actual input
evidence rather than speed tuning or fabricated hold. Publication and synthetic
checks alone were not proof of the owner's usable hold/release stream.

### Receiver implementation verification (synthetic input, not acceptance)

- The 0xB5 action-ID regression failed against the released decoder. Separate
  legacy-release and silent-gesture tests also failed before their repairs.
  An added Custom-before-raw case exposed pending learning reattaching a new
  command to the already-consumed old gesture; that regression now passes.
- All 30 focused input/state tests and all 186 Windows-app tests pass. All 17
  receiver plus 9 state scenarios pass UBSan with synthetic host I/O. Full
  uncached repository checks pass in both dependency states; eight-target
  Windows cross-build succeeds, with only Lighting EXE changed.
- All 631/631 deliberate breaks detected, no survivors/stale/errors, clean
  328-test pre/post suites, exact source restoration; 255.814s, one worker,
  no verdict cache. All 19 new input-contract mutations also pass their
  separate targeted run. Reports are external at `lighting-m4-physical/`.
- Receiver-release EXE: 462336 bytes, SHA-256
  `078cefd4dcd71830f6bb6d221418b72aaa76208692ffaf39f38af72debb42a74`.
  Aura/Tray remain byte-identical to the prior release; only the separate
  delivery marker advances after verification and the owner's approval.
  Windows CI is a separate adapter gate; even a pass is not owner-device input
  delivery. No owner-specific cause or successful physical hold is established.
- The separate Tray apply/refresh/resume rounded-master audit remains open;
  it is not established as M4's cause and is not changed by this input patch.

## Prior owner request - rebuild; engine-8 received

The owner reports no observable benefit from the previous brightness attempts
and requests rebuilding the brightness adjustment system from the ground up.
They also reopen saved-state restoration: Balance Colours and/or Steady Colours
turn back on after restart; which one is uncertain. Do not require them to
identify the toggle or keep the older broad preferences approval as current.
Orca Save remains resolved; app autorun/Open/UAC reports are not reopened.

The owner retained the fresh-log rule and has now supplied engine-8.log. The
edit gate is satisfied. All 862 lines/219809 bytes were scanned: 847 SHARED
status samples, zero failed writes/reopens, no per-key or saved-toggle trace.
SHA-256: b4c8f96fa8949ec1debfd80d4468850bbaf4d1f2aefd1381c8166727b295856b.
The raw private log is not in Git. Receipt does not prove key delivery or cause.
No further log, old hold/input-status matrix or toggle identification is needed.
The native rebuild/restore implementation is complete; release verification is
recorded below. Owner acceptance remains open. The prior mode guard below is
history, not an owner-accepted repair.

Replace the split brightness command/hold path coherently,
rather than equating another rate/mode patch with the requested rebuild. Keep
M4 decreasing/M5 increasing, a consistent authoritative master level, real
release handling, bounded repeats and existing mode choices unless the owner
explicitly approves a behavior change. Cover command delivery through slider/
engine readback, delayed or duplicate input, bounds and stop/restart behavior.
For persistence, exercise independent Balance/Steady on/off combinations through
change, save, close and reopen, plus external Hub/tray writes and later saves.
Preserve false values, unrelated preferences, zone separation and legacy files;
do not solve restoration by forcing both defaults off. Existing controlled
parser/stale-state findings below are leads, not the owner configuration/cause.
Retain strict ASCII/C# 5.0, pacing, exclusive lock and single-engine safeguards.

Queue: Prism brightness rebuild + preference restore CURRENT; processed preview
NEXT (native preferred/separate acceptable). Renumbering PAUSED; ORCA-04/05
PARKED; further PLAN-02 DEFERRED.

## Rebuilt native master and saved-state path

- `brightness_controller.h` owns exact 0..1000 master state: 0=M4/down,
  1=M5/up, 5% initial tap, then 1% down/1.5% up per 30ms. Real Custom
  commands are presses; raw keyboard/HID reports only confirm held/released
  state. A tap without hold evidence has no trailing glide. Keep distinct
  bindings, numeric/PID tokens, latch stop, exact 20% burst, rails, 120ms
  freshness, no catch-up bursts and the 10s ceiling. New commands after a
  consumed release cannot be swallowed just because their raw event is late.
- `brightness_master.c` publishes immediately and retries failed writes.
  Strict numeric readback is non-echoing; rounding is display-only. Registered
  hotkeys and slider use the same absolute setter. External Fn input cancels
  an old glide. Startup publishes saved master before accepting stale live.txt;
  stop/restart flushes pending state without restarting a gesture.
- Native launches pass `-NativeBrightness`, so Aura mirrors the canonical
  channel instead of consuming Custom tokens a second time. Fn/WMI and
  standalone legacy input can still publish. Theme-only edits never republish
  an old master. No vendor software, startup preference, lock or pacing reset.
- `panel_config.c` scopes direct JSON members independently of property order,
  preserves false/legacy/unknown data, and saves by patching the latest valid
  object after a baseline three-way merge. `MasterLevel` preserves half-percent
  input without breaking legacy percent writers. Invalid/unreadable settings
  are not treated as missing. Failed saves retain local changes for retry.
- Tray uses recursive three-way merging and atomic saves; external preferences
  refresh existing controls, so a second unrelated UI save cannot restore stale
  Balance/Steady/master values. Startup passes Smooth through to Aura, which
  sets SoftBlend/DitherMode before the first frame. Both zones remain separate;
  no defaults are forced off.
- The native EXE embeds its exact Aura script as RCDATA3. Start/off prefer a
  byte-identical adjacent script, otherwise atomically materialize the separate
  `%LOCALAPPDATA%\KeyboardLighting\Aura-Native-Background.ps1`. Do not overwrite
  a legacy Aura file or fetch another branch's protocol. DLL freshness uses the
  actual executing script path. Build caching depends on Aura's source bytes.
- Local diagnostics are now `tests.lighting-input.v2`, with native
  `hold_steps_down/up` instead of synthetic repeat-token writes. Aggregate-only
  privacy, bounded atomic snapshots and the existing one-minute interval remain.

## Rebuild verification and limits

Native input/state harnesses execute production functions with synthetic clock,
Win32 events and disk I/O. Initial saved-state tests went red against the old
parser/ownership path, and a Custom-before-raw regression also went red before
its repair. Coverage includes all eight Smooth x Kbd.Equalise x Bar.Equalise
combinations, scoped/legacy/BOM files, unknown fields/custom slot positions,
Hub/Tray changes followed by later saves, exact master/slider/registered-hotkey
adapters, stale restart input, failed saves/publications, first-token learning,
delayed/duplicate commands, physical release, modes and bounds.

`tests/lighting_engine_state.ps1` AST-imports actual Aura/Tray functions without
starting devices, UI or an engine thread. It exercises startup/live flags, exact
master readback, relative Fn actions, legacy saves, external merges and actual
control adapters for all eight combinations. Stock PowerShell 5.1 execution is
wired into Windows CI; it is not executed by the Linux C harness/source checks.
The matching embedded resource has a byte-for-byte regression. Runtime Windows
resource materialization, WMI, true concurrent processes, installed reopen and
physical M4/M5 are not established by these local tests. No owner-specific
hardware cause or acceptance is inferred. Do not ask for another equivalent log
or the old input-status matrix.

Local verification of this rebuild:

- Eight Windows targets cross-build successfully; only Lighting EXE changes.
  Lighting is 462336 bytes, SHA-256
  `77fa549a2f0f73c472c6935a73e3c6cfaac418ea74e6af9e23f4f11a4f30b5bc`.
  Bundled/adjacent Aura is 222681 bytes, SHA-256
  `1730bd61c386a11ac0cd24d4d0d4e12334b2f172b4b0a2c07795c5986eca6be6`.
- All 26 focused input/state tests and all 182 Windows-app module tests pass.
  All 13 input + 9 state harness scenarios also pass with UBSan; this is still
  synthetic host I/O, not native key or device execution.
- Full repository `check_all.py` passes in both dependency states, including the
  synchronized CI copy, ASCII, code-map, context and protected behavior checks.
- Full mutation campaign: 612/612 detected, zero survivors/stale/mutation or
  gate errors, 324-test clean pre/post suites, restored source bytes, one
  worker, no verdict cache; 259.950 seconds. Report is in the external host
  cache at `lighting-rebuild/mutations-framework-final.json`, not committed owner evidence.
- The first actual Windows adapter execution (run 45665741010) exposed an
  atomic-write compatibility bug: Framework received an empty backup filename
  from plain PowerShell `$null`. Both panel and live replacements now use
  explicit `System.Management.Automation.Language.NullString.Value`. The
  existing runtime test overwrites files through these actual functions; two
  extra deliberate breaks prevent reintroducing this source defect. This is
  the cause of that CI failure, not proof of the owner's original cause.
- Stock PowerShell 5.1 execution is a separate Windows CI gate. Local Linux
  verification does not establish it; CI and installed/physical acceptance
  must be reported separately. Use the active branch's `.agent/RELEASED`
  implementation SHA for the actual delivered build. Hub stays 2.8.72.

## Prior owner status - M4/M5 reopened

The owner says the earlier items are verified and now prioritizes M4/M5:
mirror working M5 brightness-up behavior for M4 brightness-down. Treat earlier
Save/startup preferences as owner-reported verified, not a new code repair,
installed-build identity or independently executed matrix. Processed preview
follows M4/M5; renumbering remains paused and parked/deferred scope is unchanged.

Read-only source review finds a shared native receiver (`PressSignal`,
`ReleaseSignal`, `HoldPump`) and shared engine `Step-BrightRamp` for both
keys. M4/down is direction index 0 and M5/up is index 1, with distinct learned
scan/HID bindings and token files. The engine applies 1% down versus 1.5% up
per 30 ms, both after the same immediate 5% tap. Mirroring speed alone would
not repair missing press/repeat/release input. Do not copy M5's physical key
binding to M4, infer a hardware cause, reset bindings/mode/preferences, or
remove release, pacing and ten-second safety guards to fake a working hold.
Earlier, all eight production-receiver regression tests passed with synthetic
Win32 I/O without exercising native repeats against latch/burst consumption.
Passing those tests did not establish the owner's M4 signal or physical hold.

## Prior mode-conflict correction - reproduced, not hardware-certified

Fresh owner `engine-7.log` satisfied the prior receiver-mode edit gate.
Its complete 828-line, 211614-byte content was scanned: the 816 device-write
status samples report shared ownership with zero failed writes/reopens.
There is no individual M4/M5 press/repeat/release trace. These engine records
cannot establish a per-key hold cause. Do not request another equivalent
engine log, `engine-6.log`, `input-status.txt` or the old hold/input matrix.
Keep private logs and their raw contents out of Git/Drive.

A new production-receiver test reproduced a real mode interaction: `HoldPump`
previously wrote 30 ms repeat tokens regardless of the saved mode. The engine
interprets each token as a toggle in `latch` (press-to-glide) mode and as a fresh
5% tap/restarted 20% movement in `burst` mode. Consequently a learned physical
key can repeatedly cancel/restart its glide while an unlearned key delivering
only its Custom action appears to work. This is a supported theory for the
asymmetry, not proof of the owner's selected mode, current bindings or cause.

`HoldPump` now reads the existing saved mode when a stream is active and clears
both native repeat streams outside `tap`. It does not consume or rewrite the
real Custom tokens, reset bindings, save a preference or force another mode.
HID-down state remains intact so real releases are still observed and learned.
`tap` keeps first-token learning, physical release, 30 ms repeats and the 10 s
ceiling. The direction/rate constants and the PowerShell/C# engine are unchanged.

The added regression failed against the prior production receiver at the
unexpected repeat-token assertion, then passed with the guard. It covers
both keys, raw-keyboard and HID repeats, latch/burst, no mode/profile/token
rewrites, release learning, simultaneous streams and switching from an active
tap hold to latch and back for a fresh hold. Nine `LightingInputTests` now pass.
Six new mode-guard mutations cover disabled/partial/wrong guards, the M5 stream
and accidental HID release-state loss. A first full campaign exposed a gap in
release-state coverage; the test now asserts live HID-down state on every tick
and exercises both high-bit and zero release reports.

### Prior correction verification

- Eight-target Windows cross-build passes with Zig 0.17.1. Only
  `KeyboardLighting.exe` changes: 233984 bytes, SHA-256
  `a12bf2ce378089f349ded4e74c43b148a7637ca9b4f0281e322eb90852c75bb9`.
  Other seven EXEs, engine, tray, CLI, manifests and workflows are unchanged.
- Nine `LightingInputTests` pass; all eight native receiver scenarios also pass
  with undefined-behavior sanitization. The exact prior HEAD receiver fails
  the new unexpected-repeat assertion, rather than a compile/setup error.
- Full uncached repository checks pass with and without optional geometry
  dependencies; the uncached geometry suite takes 45.468 s on this host.
  CODEMAP, ASCII, whitespace and the 14963-character default context pass.
- Final full mutation campaign: 572/572 detected; zero survivors/stale/mutation
  or gate errors, 307-test clean pre/post gates, exact source restoration,
  one worker, no verdict cache, 273.544 s. Reports/scratch binaries stay outside
  Git under the agent cache; no owner evidence is published.
- Delivery uses the active branch's separate `.agent/RELEASED` marker commit;
  Hub remains 2.8.72 and plugins remain 0.0.58/0.4.13. Synthetic I/O and
  cross-builds are not Windows delivery, physical hold or owner-cause proof.

## Prior startup-preference investigation - subsequently reopened

The owner had reported **Balance Colours / Steady Colours start enabled despite
saved off choices**. Broad verification temporarily superseded that unresolved
status, without establishing a cause. The latest request above REOPENS restore.
No Lighting product edits or resets were made in that earlier investigation.
At that earlier rebuild checkpoint the owner retained a fresh-log prerequisite;
it was not a request to repeat the old startup/config collection matrix.
That prerequisite has since been removed, as recorded in the current section.

Read-only replay of the production C parser/default/load/save functions under
GCC ASan/UBSan confirms order-sensitive `Smooth` parsing, nested `Equalise`
shadowing and ignored legacy flat `Equalise`. Native-order direct false values
load correctly. A controlled external panel-file edit followed by a native
save without reloading overwrites the edit with stale in-memory values. These
are synthetic parser/state cases, **not the owner's configuration or established
installed cause**. Preserve saved choices, unrelated preferences, ASCII/C# 5.0,
exclusive-lock, pacing and single-engine safeguards; do not force defaults off.

The previous generally-good/M4/M5 status below is historical. No separate
physical-hold test, installed-build identity or hardware cause is claimed.

## Evidence received; actual hardware cause still open

The owner supplied a fresh `engine-6.log`; do not ask for the same prerequisite
again. The complete 839-line, 213998-byte file was scanned. Its own wall clocks
span 2026-11-22 11:32:44 through 2026-11-23 01:15:46; this is log metadata, not
an established build identity or a claim about the investigation date.

- All 824 device-write status warnings report **SHARED** ownership. Every sample
  reports zero failed writes and zero reopens; that does not prove smooth or
  correct lighting. The worst reported write is 814675 us against a 16667 us
  budget at 60 fps. Late-frame values are interval counters, not additive totals.
- Shared ownership, GHelper's presence in several warnings and write latency are
  leads, not proof that it holds the lighting interface or caused the key issue.
  The log does not establish why exclusive opens failed or which process owns
  the endpoint. No other software was stopped, restarted or reconfigured for
  this investigation. Existing engine/vendor behavior is unchanged by this fix.
- The engine log contains no raw-input registration, binding or hold-state
  diagnostics. That absence does not prove no physical M4/M5 events occurred.
  No owner-hardware reproduction or successful hold is claimed.

Private logs, raw input, user/device paths and settings stay out of public
Git/Drive. The owner's shorthand `2/3. kindarandom/ all` is retained without
assigning it to a specific UAC executable or assuming Hub was running during
APP-05's multi-icon condition.

## Small receiver change, not an engine-timing or vendor repair

`m4_m5_hold.c` previously assigned a second `GetRawInputDeviceList` result to an
unsigned count and iterated it. If devices change between sizing and filling,
Windows can return UINT_MAX for failure; this is not a device count and could
walk beyond the 64-entry stack array. The receiver now checks both API returns,
uses the full bounded fill capacity, rejects oversized counts and keeps the
existing generic-keyboard fallback. Exactly 64 devices is supported. An invalid
window is rejected rather than registering a sink against NULL.

The receiver also writes one local `input-status.txt` after registration and
at most once per minute from the existing housekeeping timer. It atomically
replaces this one small file in `%LOCALAPPDATA%\KeyboardLighting`; it does not
append a history or upload anything. Path/format overflow skips the write. An
unwritable directory does not stop input or create a retry loop.

The status contains a schema, panel PID/sample uptime, receiver registration
success/errors, requested/registered usage counts, aggregate keyboard/ASUS
report counts, observed brightness-token/successful repeat-write counts, and
bound/release-source/active booleans for each direction. Token observations can
include repeats written by the panel itself; they are not a count of physical
presses. Counters start again with each panel process. The snapshot does not
contain typed characters, scan/virtual-key values, HID report payloads, device
identities, user paths or configuration contents. Do not infer release behavior
from a zero counter in one snapshot alone.

No low-level hook, synthetic input, extra timer/engine, startup registration,
manifest/elevation change, hold-mode/profile reset, lighting rebranding or
PowerShell/C# engine change is included. Existing 15 ms input timer, 30 ms repeat
pacing, physical-release polling and 10 s ceiling remain intact, as do engine
exclusive-lock, pacing, single-engine and strict ASCII/C# 5.0 contracts.

## Earlier M4/M5 evidence boundary - no repeated collection request

The owner does not want another log/reproduction loop. The former focused
`input-status.txt` observation request is superseded, not silently left pending.
Lighting Open and general Lighting use are now owner-reported good. The prior
silent failure and 2.8.72 offscreen source regression remain history, not proof
of an established owner cause. No separate physical-hold test or engine repair
is inferred. This is not a request for another engine log or the old full
matrix. Do not ask again for
`input-status.txt` as the next step, or change GHelper/security/mode/startup to
force evidence. Existing diagnostics remain local and optional to inspect.

## Automated verification boundary

`tests/native/lighting_input_harness.c` executes the production receiver with
synthetic Win32 I/O only. `LightingInputTests` covers:

- stable/deduplicated usage registration, the 64-device boundary, failed initial
  enumeration, failed/growing fills and invalid/oversized returns;
- keyboard fallback success/failure, real reported errors and invalid windows;
- first-token learning, both directions, paced repeats, release and HID batches;
- physical polling, the safety ceiling, modes and failed repeat writes;
- snapshot size/path/scalar allowlist, no preferences/input writes, refresh
  limit, unwritable output and long-path refusal;
- initial setup and housekeeping wiring, keeping disk snapshots off the fine
  input timer.

An isolated red-baseline probe using the exact prior `InitRawInput` enters the
invalid-list path when a fill reports a list-growth error. The corrected
receiver passes the focused tests. This establishes a source regression, not
what the owner's keyboard emitted. Run full build/check/mutations and regenerate
CODEMAP before committing; cross-build/mock results do not prove Windows raw
input delivery, physical M4/M5 hold, brightness smoothness or startup behavior.


## Prior receiver-only release verification record

- Eight-target Windows cross-build passes with Zig 0.17.1. Only
  `KeyboardLighting.exe` changes: 232960 bytes, SHA-256
  `07d50e455a3b31f4a6403db328bb9dcab79b4e8e47cdeaff0455f9b0e8248d8b`.
  The other seven shipped EXEs, engine, manifests and workflows are unchanged.
- Twelve focused lighting tests pass (eight new receiver tests plus the four
  existing input/hold contracts); all seven native receiver scenarios also pass
  with undefined-behavior sanitization. These execute synthetic host-C mocks,
  not Windows raw input or owner hardware.
- Full uncached repository checks pass with and without optional geometry
  dependencies. The handoff contract now retains all 20 IDs, actual broad
  owner approval and fresh-log receipt instead of obsolete blanket-pending
  wording. CODEMAP and the 14983-character default context check pass.
- Full mutation campaign: 423/423 detected, zero survivors/stale/mutation or
  gate errors; 247-test clean pre/post gates, exact source-byte restoration,
  one worker, no verdict cache, 202.743 seconds on this host.
- Use the active branch's `.agent/RELEASED` implementation SHA for delivery.
  That receiver-only release kept Hub at 2.8.70; the later 2.8.71 Open follow-up
  is documented separately in `test-hub-open-to-front.md`. Build/check gates
  must precede implementation and the
  separate marker commit. Native Windows delivery and physical M4/M5 hold,
  smoothness, Open, startup-tray and UAC results are not certified here.
