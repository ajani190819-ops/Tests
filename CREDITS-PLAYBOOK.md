# Credits playbook - keeping the hosted agent affordable

Arena bills by tokens in and out, so **turns are the unit of cost**. A turn
that re-investigates settled facts, re-reads whole files, or retries a failed
approach burns several times what a prepared turn does. This playbook is the
owner-side half of the budget; the agent-side half lives in
`.agent/rules/execution.md` (ranged reads, batched calls, warm caches, one
push per turn, short recaps).

## What already got cheaper (built 2026-11-12)

| Mechanism | What it stopped costing |
|---|---|
| Released marker | retry turns caused by half-finished updates and mid-task nags |
| SuiteReport.bat | diagnostic ping-pong: one double-click replaces a log-hunting conversation |
| Incremental builds + mutation registry | rework turns: failures surface in seconds, not in a rebuilt suite |
| STATE archive + credit discipline | every turn's reading load (55 KB of settled history down to 48 KB and frozen) |
| Self-discovering notes link | lost turns spent re-pasting session addresses |

## Route work to the right brain

Your local model (already set up) is free per-token. The hosted agent is the
expensive tool with real verification. Split deliberately:

**Ask the LOCAL model:** single-file edits and explanations, PowerShell/C#
snippets, Orca G-code post-processing tweaks, regex and formatting chores,
drafting commit messages or docs, summarising a file you paste in.

**Bring to the HOSTED agent:** anything spanning multiple files, anything that
must pass the test suite and mutation checks, suite releases and version
bumps, diagnostics that need whole-repo context, PRs and merges, Windows-side
behaviour only reproducible through your logs.

A good habit: let the local model draft, and let the hosted agent verify and
ship. The draft costs nothing; the verification is what you pay for once.

## Batch like the "one big move" turn

Four asks in one turn cost far less than four turns with one ask each - the
setup, the context reads and the release ritual are paid once. When you sit
down to work:

1. Write the whole list before sending, in priority order.
2. Attach evidence up front: SuiteReport.txt for anything behavioural, exact
   wording for anything visual, and what "done" means for each item.
3. Let ambiguous items carry a default ("if unsure, do X") so the turn does
   not stall waiting on a question.

## Between hosted sessions

Agent Notes is free. Queue non-urgent asks, observations and log excerpts
there as they happen; the next hosted turn starts with full context and zero
back-and-forth. That single habit is probably the largest saving available.

## Warning signs

- Hosted turns arriving full of re-reads and "as established earlier": tell
  the agent to re-archive STATE - settled facts have leaked back into prose.
- Many small turns instead of few big ones: re-batch.
- Local model asked to do multi-file suite work: it will confabulate; that
  work belongs here, and doing it locally costs double in the end.
