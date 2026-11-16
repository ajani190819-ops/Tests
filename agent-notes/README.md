# Agent Notes

A direct, no-setup note channel from the owner to the current agent session.

Type a note and choose **Send to Agent**. There is no URL to paste, save, or
manage. The application finds the live session from the branch selected in
Project Hub and sends to it automatically.

## Delivery and safety

1. Every note is first written to `%LOCALAPPDATA%\AgentNotes\outbox`.
2. When the current agent receiver is available, the note is delivered and its
   outbox file is removed only after the receiver confirms success.
3. If the agent is away, the note is shown as **Queued** and stays on disk.
   Agent Notes checks again every minute and sends automatically when a live
   session is found. Nothing is discarded because a workspace was offline or
   reset.
4. The agent reads `.agent/INBOX.md` at the start of its next turn.

The app discovers the current endpoint from `.agent/AGENT_LINK.txt` on the
branch configured in Project Hub. It adopts an endpoint only after its health
check answers, so a stale branch file cannot silently replace a working route.
This is background delivery plumbing, not an owner setting.

## Reading the window

- **Automatic delivery** is an informational status card. It tells you whether
  Agent Notes is searching, sending, has recently confirmed delivery, or has a
  saved session to check. It is not an editable setting.
- **New note** is the only input. The hint disappears when you start typing and
  returns if you leave an empty draft.
- **Send to Agent** queues the note immediately, then attempts delivery in the
  background.
- **Recent activity** lists the newest notes. Green **Sent** means delivery was
  confirmed; amber **Queued** means it remains safe in the local outbox and
  will retry automatically.
- The bottom status line gives the most recent delivery result in plain English.

Agent Notes follows the active Windows light or dark theme. Its header uses the
suite’s animated Tri-Spoke Core emblem, and its single-instance behavior brings
the existing window to the front when Project Hub opens it again.

## Privacy note

The session receiver is reachable from the address recorded in the repository
for the active agent session. The receiver rate-limits and records note
provenance, and the agent treats inbox text as untrusted input until it is
verified as an owner request.
