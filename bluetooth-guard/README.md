# Bluetooth Guard

Install/update the native `BluetoothGuard.exe` through Project Hub on the active
Arena branch. The legacy PowerShell/C# setup route is unchanged.

## Opening the panel

Update **both Project Hub (2.8.67 or later) and Bluetooth Guard** on the active
branch. Hub card and unified-tray Open grant foreground permission and post a
show-only request to Guard's own UI thread, so an elevated, tray-hidden Guard
can restore itself. Its own tray, explicit first launch and existing-instance
show event share the same restore helper. Minimized windows restore, maximized
placement is retained, and an active modal dialog receives focus. An existing
instance is not relaunched merely to reveal it.

Guard permits only that registered message on its window, not arbitrary commands
or a process-wide UIPI exemption. The payload contains only input/time ticks;
queued requests expire after one second or newer input. Hub's bounded retries,
newer-intent cancellation, legacy async fallback, elevation manifests and update
stop/verify gates remain intact. This repairs a source-level cross-integrity
cooperation gap; it is not a claim that the owner's exact focus failure was
reproduced on Windows.

The Hub 2.8.71 follow-up also keeps Bluetooth driver enumeration outside the
shared device-model lock used by paint and the hidden UI's heartbeat. A slow
first/next/close query must not hold that UI lock and strand queued Open.
Per-observation updates remain locked and recheck current owner decisions;
approval, pause, enabled profiles, address-based actions and retry delays are
unchanged. A deterministic second-thread lock probe fails the old poll and
passes the correction. This does not certify native responsiveness or every
possible hang. No additional owner logs are requested for this source repair.

## Devices and decisions

The native app requests authenticated, remembered and connected Windows
Bluetooth records without inquiry. Saved/disconnected devices remain available
for review; a temporarily missing name/class does not erase known metadata.
**Needs approval** rows offer **Accept** (one manual/current session), **Deny**
(block connections but keep Windows pairing), or **Auto-Connect** (explicitly
approve future connections). Remembered is not authenticated: an unpaired saved
record is reviewable, but is not exempt from stranger blocking. Scans and audio
observations never silently grant trust. **Forget** separately unpairs and keeps
a tombstone against stale Windows history.

Decisions survive restarts in `%LOCALAPPDATA%\BluetoothGuard\rules.txt`; existing
format-2 K/D/F records remain compatible. Session permission, retry timing,
audio observations and the explicit-Disconnect pause are not persisted as trust.
Card/tray actions and worker results resolve device addresses, not moving rows.
Queued connects, policy drops and explicit disconnects recheck current choices.

## Guard and automatic connection

- **Guard ON** blocks unsolicited links from approved manual-only devices
  outside their permitted session. **OFF** pauses that restriction; explicit
  denials, forgotten devices and unpaired-stranger safeguards still apply.
  Neither setting disables the radio or service profiles.
- **Auto-Connect ON** allows future connections. For approved audio devices the
  worker also actively requests audio reconnection with Guard **ON or OFF**,
  including after startup and later connection loss. Failed/unconfirmed requests
  wait **5, 10, 20, 40, then at most 60 seconds** between attempts; they do not
  run at the 80/250 ms discovery cadence. A real audio-ready observation resets
  that backoff. No automatic requests run while the radio is unavailable or the
  app is suspending/shutting down. Phone/HID links remain Windows-managed, not
  falsely treated as audio devices.
- **Disconnect**, in a card or tray menu, pauses this app's automatic retries
  for that device for the current run. **Connect**, re-enabling Auto-Connect,
  or restarting the app resumes them. The Auto preference is retained; this
  pause does not revoke Windows' permission to reconnect independently.
- **Accept/Connect with Auto OFF** requests one manual session, not ongoing
  retries. That session ends after an observed disconnect, or 45 seconds without
  observing a connection. Startup only controls launch with Windows.

## Connection status and native limits

The worker samples address-matched CoreAudio endpoints at most once a second,
separately from the classic Bluetooth link. **Audio connected** means Windows
reports an ACTIVE endpoint, not that music is playing. A Bluetooth ACL/link or
a successful one-shot request alone is not reported as confirmed audio.
The status pill and card accent now use **green for confirmed connection** and
**blue for paired / Auto-configured, not-connected states**. Audio **Link only**
and **Connecting...** are also blue, never a claim of ready audio. **Needs
approval** stays amber, **Denied** red, and an unavailable radio or an unconfirmed
saved-only record stays neutral. Labels accompany colors.

A separate line explicitly says **Auto-Connect on / off / paused**; that saved
preference never replaces a real connected status. Connection/request details
have their own full-width row, including retry, missing-endpoint and Windows
HRESULT failures. A missing classic record does not hide an already-known
device's positively matched active audio endpoint. These are display changes,
not changes to approval, reconnect or disconnect policy.

Reconnect/disconnect obtains the adapter ID from the endpoint connector, gets
that adapter's `IMMDevice`, and **activates `IKsControl` on the adapter**, as
[Microsoft documents](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/CoreAudio/using-the-ikscontrol-interface-to-access-audio-properties.md).
It does not query the topology object for that interface or route by a friendly
name shared with another device. Service profiles stay enabled; disconnect
retains the radio IOCTL fallback. Drivers that expose no address-mappable audio
adapter are reported as unavailable rather than guessed at by name.

Windows' classic `BluetoothFindFirstDevice` inventory does **not** discover
LE-only devices; this is not a new WinRT/LE Audio pairing implementation. The
owner reports the original connection problem is now working. The new panel
reveal/colors and remaining persistence, manual-control and lifecycle cases
still need native checks. Automated regressions execute production activation,
presentation, policy, discovery, retries and COM traversal with Windows I/O
mocked; cross-builds are not native acceptance. Use the
[Windows checklist](../.agent/runbooks/test-bluetooth-reconnect.md) for BT-01.

The pre-migration history remains in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk).
