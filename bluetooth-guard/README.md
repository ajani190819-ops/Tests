# Bluetooth Guard

The Hub-delivered native `BluetoothGuard.exe` lists paired devices even when
disconnected. **Needs approval** rows offer **Accept** (one manual session),
**Deny** (block connections but keep Windows pairing), or **Auto-Connect**
(explicitly approve future connections). Undecided pairs are not disconnected
just because a Windows-settings intent window was missed. Scans never silently
grant trust; unpaired strangers, forgotten devices, and explicit denials remain
blocked. **Forget** is still the separate action that actually unpairs.

Decisions survive restarts; controls target a device address, not a moving row
index. Install/update through Project Hub for this native review workflow; the
legacy PowerShell setup path is unchanged. Real-earbud behavior still needs a
Windows hardware check after the automated regression/build checks.

The full pre-migration narrative and history are preserved in the [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains only material required to build, run, test, release, or recover the project offline.
