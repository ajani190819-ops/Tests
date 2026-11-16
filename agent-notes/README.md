# Agent Notes

Agent Notes is the direct, durable companion channel for the active Arena
session. It automatically discovers `.agent/AGENT_LINK.txt`, preserves unsent
notes in `%LOCALAPPDATA%\AgentNotes\outbox`, and reports two distinct states:
**Saved** only after the API bridge persisted the live `.agent/INBOX.md` write,
then **Read** only after the agent's required inbox checkpoint. The receiver is
an API bridge, not a second browser-based notes page; the native app remains
the owner-facing delivery UI.

While the app is running, closing its window keeps a notification-area watcher
alive. It shows a native visual Windows alert when the agent publishes a generic
waiting-for-owner-input state. Sound is a persistent, opt-in tray-menu setting
defaulting **off**; no custom audio engine, task, service, or run-at-sign-in
registration is created. Use the tray **Exit** command to stop the watcher.

The full pre-migration narrative and history are preserved in the [Tests
Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk). Git retains
only material required to build, run, test, release, or recover the project
offline.
