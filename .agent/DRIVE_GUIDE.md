# Tests Project Guide (Google Drive)

**Drive guide:** [Tests Project Guide](https://docs.google.com/document/d/1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ/edit?usp=drivesdk)
**Drive file ID:** `1Fp4zZcQrljlHDMMu6h2u5WMRgBju1_M5D3yafTJgslQ`

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
