# Arena Link for Windows — developer companion (initial build)

Arena Link is a **local-first Electron desktop companion** for developer work.
It is intentionally separate from a browser session: the app can inspect a Git
repository you choose on your own computer, call your local Git/GitHub CLI, and
open the official Arena Gateway setup pages. It does **not** silently access all
of your files, run an arbitrary shell command, or save an API key.

> **Status:** first runnable scaffold. It provides a repository approval flow,
> Git/GitHub Actions/PR snapshots, tool checks, and the secure Arena Gateway
> handoff. It is not yet a full autonomous coding agent.

## What it does now

- Lets you choose a directory and verifies that it is a Git worktree.
- Stores only the canonical roots you explicitly approve in the app data folder.
- Shows the branch, working-tree status, latest commit, GitHub Actions runs, and
  open PRs for an approved repository.
- Uses your existing `git` and `gh` CLIs, with fixed argument arrays and no
  `shell: true` / arbitrary command execution endpoint.
- Checks whether `git`, `gh`, and `claude` are available locally.
- Opens the official Arena gateway documentation and key dashboard.
- Detects whether an Arena/Anthropic key exists **in its process environment**
  without exposing, logging, or persisting the value.
- Offers an optional **read-only Arena consultation** when a key is present. It
  sends the developer's prompt plus a compact visible snapshot—repository name,
  branch, latest commit, GitHub slug, and working-tree status. It does not send
  source files, absolute paths, logs, credentials, or a shell transcript.

## What it deliberately does not do

- It does not create, scrape, request, or store your Arena API key.
- It does not implement an unattended agent loop or claim to support the
  unsupported Claude Agent SDK.
- It does not make commits, pushes, PRs, releases, installations, deletions, or
  resets. Those will be later opt-in actions with per-action confirmation.
- It does not grant a renderer window direct filesystem, process, shell, or
  credential access.

## Requirements

On Windows, install:

1. [Node.js 22 or newer](https://nodejs.org/)
2. [Git for Windows](https://git-scm.com/download/win)
3. [GitHub CLI](https://cli.github.com/) and authenticate it with `gh auth login`
4. Optional: the standard interactive Claude Code CLI, if you want to use the
   Arena gateway with it.

## Run locally

```powershell
cd arena-link-windows
npm install
npm start
```

Package an installer and portable executable:

```powershell
npm run dist:win
```

The output is written under `dist/`. Do not run an unsigned build from an
untrusted location.

The initial release workflow needs repository workflow-write permission before
it can publish a Windows installer. Until that automation is enabled, package
from source with the command above. The first installer will not be code-signed,
so verify its release/tag points to this repository before opening it.

## Arena Gateway setup

The Arena gateway documentation is the source of truth:

- [Claude Code / Claude Desktop gateway guide](https://portal.api.preview.arena.ai/docs/claude-code)
- [Create a dedicated key](https://portal.api.preview.arena.ai/dashboard/keys)

The key must be created in your own signed-in dashboard. It is displayed only
once there; do not paste it into chat, source code, Git, or an issue.

For a one-session native Windows PowerShell setup with the interactive Claude
Code CLI, the documented pattern is:

```powershell
$env:ANTHROPIC_BASE_URL = "https://api.preview.arena.ai"
$env:ANTHROPIC_AUTH_TOKEN = "<your-dedicated-Arena-key>"
$env:CLAUDE_CODE_DISABLE_UNKNOWN_MODEL_WINDOW_ENFORCEMENT = "1"
$env:ANTHROPIC_MODEL = "coding-router-preview"
$env:ANTHROPIC_SMALL_FAST_MODEL = "coding-router-preview"
$env:CLAUDE_CODE_SUBAGENT_MODEL = "coding-router-preview"
$env:CLAUDE_CODE_MODEL_CAPABILITIES = "coding-router-preview=-effort"
claude
```

Use the standard **interactive** `claude` CLI for full coding work. The current
gateway docs state that print mode (`claude -p`), `claude agents`, and the
Anthropic Agent SDK are not gateway-supported. Arena Link's own optional
consultation is a single, explicit `/v1/messages` request with compact
read-only context—not an autonomous agent loop or a replacement for the
interactive CLI.

See [ARENA_GATEWAY_SETUP.md](ARENA_GATEWAY_SETUP.md) for the security boundary
and the planned integration steps.

## Security design

- **User-selected repository allowlist:** all repository inspection calls
  re-check the selected directory against the local approved list.
- **No raw shell bridge:** the renderer can invoke only named IPC actions;
  command names and argument lists are defined in `main.js`.
- **No credential bridge:** the preload API has no method to read environment
  variables, files, or secrets. It receives only a boolean gateway-key state.
- **No remote browser content:** the Electron window loads local packaged files;
  setup/keys links are opened in the operating system browser.
- **Read-only first release:** it is safe to explore project state before adding
  explicit, reviewable write actions.

## Planned next increments

1. Add an approved repository command catalog (`npm test`, Gradle build, etc.)
   with command preview and confirmation.
2. Add build/log capture and an explicit “copy diagnostic bundle” workflow.
3. Add a diff review screen and confirmation-gated Git/PR actions.
4. Add a local Minecraft instance diagnostics module as an optional separate
   permission group.
