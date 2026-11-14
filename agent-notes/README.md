# Agent Notes

The direct channel from you to the agent: type a note, press **Send to
Agent**, and it lands in the agent's inbox at the start of the next turn.

## How a note travels

1. This app posts the note to the agent's workspace receiver.
2. If the receiver is not listening (the agent is away, or the workspace
   reset), the note stays in an outbox folder on your disk -
   `%LOCALAPPDATA%\AgentNotes\outbox` - and is retried every 60 seconds.
   A note shows **Queued** while its outbox file exists and **Sent** once a
   delivery was confirmed. Nothing is ever deleted before confirmation, so
   nothing is ever lost.
3. The agent reads its inbox at every turn start and treats what it finds
   there as instructions from you.

## The link - you never paste it again

The agent commits `.agent/AGENT_LINK.txt` (the public address of its current
workspace) to your branch every turn it works. This app reads your hub's
branch setting, fetches that file, and adopts the address **only after its
health probe answers** - so a dead or half-written link can never replace a
working one. The link box remains for unusual cases; leave it alone and
discovery does the work.

## Status strip, plain English

- **Found the agent's live session - link updated.** Discovery just followed
  the agent to a new workspace.
- **Delivered N notes | last note sent HH:MM.** All caught up.
- **Agent offline - N queued notes stay safe here and deliver the moment a
  session is live.** Normal while the agent is away. Not an error.
- **No live session yet - notes queue safely here and deliver on the agent's
  next turn.** First run before the agent has worked once.

## Privacy note

The committed link file makes the inbox address discoverable by anyone who
can read the repository. The receiver rate-limits, sanitises and tags the
provenance of every entry, and the agent treats inbox content as untrusted
input regardless of source.
