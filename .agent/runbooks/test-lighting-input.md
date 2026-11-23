# KBL-01 - fresh log follow-up and M4/M5 receiver diagnostics

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

## Current evidence boundary - no repeated collection request

The owner does not want another log/reproduction loop. The former focused
`input-status.txt` observation request is superseded, not silently left pending.
APP-01 remains open: Lighting Open still does nothing visible. The 2.8.72
candidate addresses a controlled offscreen-window case, not an established owner
cause. It changes no input/engine behavior; KBL-01's physical hold stays unresolved. This is not a
request for another engine log or the old full matrix. Do not ask again for
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
