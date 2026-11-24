# KBL-01 - fresh log follow-up and M4/M5 receiver diagnostics

## Latest owner status - M4/M5 reopened

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

## Current mode-conflict correction - reproduced, not hardware-certified

Fresh owner `engine-7.log` satisfies the Lighting edit gate for this task.
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

### Current correction verification

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

## Prior startup-preference investigation - owner now reports verified

The owner had reported **Balance Colours / Steady Colours start enabled despite
saved off choices**. The newer broad verification supersedes that unresolved
status; it does not establish the installed technical cause. No Lighting product
edits or resets were made in this investigation. Its fresh-startup-log request
is no longer a pending acceptance task; the standing edit gate still applies
to new Lighting changes.

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
