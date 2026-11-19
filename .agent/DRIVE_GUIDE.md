# Tests Project Guide (Google Drive)

**Drive guide:** [Tests Project Guide](https://docs.google.com/document/d/1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ/edit?usp=drivesdk)
**Drive file ID:** `1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ`
**Documentation archive:** [Tests Repository Documentation Archive](https://docs.google.com/document/d/1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io/edit?usp=drivesdk)
**Archive file ID:** `1XTRAFNzKzrKLyo5BrGinvtbg-RQoNAymHIWEg6Ba7io`
**Full task list:** [Tests — Master To-Do List](https://docs.google.com/document/d/12TFZmyXuCWWt_-M-SjiW3AaHeoTtnq33Qw7jwfIw1cM/edit)
**To-do file ID:** `12TFZmyXuCWWt_-M-SjiW3AaHeoTtnq33Qw7jwfIw1cM`

The dedicated to-do doc holds the full known plan, including pending/parked
work and historical items needing confirmation. Update it after each completed
task; keep the compact offline snapshot in `.agent/STATE.md`. Preserve stable
task IDs/statuses instead of silently dropping items. The owner reports missing
requests: keep reconciliation open until reviewed, and never infer completion
or new approval from an old narrative. Git remains authoritative for code,
verification, and releases; Drive holds the human-readable planning list.

## Required use before and after edits

- Before changing repository files, read the relevant section of the Drive
  Project Guide. `AGENTS.md` requires this before edits; use the linked current
  guide, not the pre-migration archive, for live product and workflow context.
- After verifying changes to product behavior, app roles, safety, release
  workflow, or repository guidance, update the corresponding section in the
  same Drive Project Guide from verified Git state. Do not treat this as an
  optional end-of-task reminder.
- Prefer a targeted Docs edit. If the available connector only supports full
  content replacement, read the complete current guide first, preserve all
  unchanged sections, apply only the intended localized change, then reread the
  document to verify it. If the complete text cannot be read, do not overwrite
  it; add a visible Drive comment and ask the owner for an editing path.

## Role split

- **Git is canonical.** Source code, tests, exact procedures, release records,
  and branch-specific state live in this repository. A Drive outage or a stale
  document must never block a build, test, release, or recovery.
- **Drive is the extensive readable operating guide.** It gives the owner and
  agents the product map, app roles, important user flows, release background,
  historical context, and links into the repository. It is where narrative that
  is useful to people but not needed to build safely belongs.
- **Agent Notes remains separate.** `Agent Notes - Tests repo` is the live
  quick-capture inbox, not the architecture guide.

## Synchronization rule

When a completed change alters an app's purpose, visible behavior, architecture,
safety constraint, update workflow, release policy, or repository navigation:

1. Update the relevant Git documentation and tests first.
2. Complete build/check/release work using Git as the source of truth.
3. Update the Drive Project Guide from that verified repository state.
4. Record any changed Drive file ID or sync policy here.

Do not mirror full source files, credentials, temporary task notes, or detailed
step-by-step test output into Drive. Keep only the compact operational facts
required for offline implementation in `.agent/`; move long historical and
human-facing narrative into the Drive guide. If Drive and Git disagree, correct
Drive to match Git.
