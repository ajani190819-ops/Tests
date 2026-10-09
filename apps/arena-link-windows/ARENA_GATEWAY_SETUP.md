# Arena Gateway handoff and API-key boundary

Arena Link intentionally does **not** handle a user's API key. The current
Arena Gateway documentation directs the user to create a dedicated key in the
Arena dashboard, where the secret is shown only once:

- Docs: <https://portal.api.preview.arena.ai/docs/claude-code>
- Key dashboard: <https://portal.api.preview.arena.ai/dashboard/keys>

## What the first app version supports

The documented gateway routes Anthropic-style `/v1/messages` traffic and the
standard interactive Claude Code CLI. Arena Link uses that narrow messages
surface only for an **explicit read-only consultation**: the user enters a
prompt, presses **Ask Arena**, and the app sends that prompt plus a compact,
visible repository snapshot. The key remains in the Electron main process and
is never exposed to the renderer.

The documentation also explicitly says that print mode (`claude -p`), `claude
agents`, and the Anthropic Agent SDK are not supported. Arena Link therefore
does not claim to launch an unattended agent loop. Full coding work should use
the documented interactive `claude` CLI alongside the permissioned local
repository view.

## Credential rules

1. Create a dedicated, revocable key in the dashboard yourself.
2. Do not paste a key into the app UI, a repository, a shell history transcript,
   an AI chat, or a configuration file committed to Git.
3. Prefer a session-scoped PowerShell environment variable when testing.
4. If you later choose persistent storage, use Windows Credential Manager or a
   dedicated credential helper—not Electron local storage, JSON, `.env`, or
   source code.
5. Revoke and replace a key if it is exposed.
6. Arena Link only sends its optional consultation request to
   `https://api.preview.arena.ai`; it blocks a different `ANTHROPIC_BASE_URL`
   rather than forwarding the token to an unexpected host.

## Supported interactive Windows route

In a PowerShell terminal, configure the documented gateway root (not `/v1`) and
then start normal interactive Claude Code:

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

The app can be open at the same time to inspect the selected repository,
GitHub Actions, and PR state. The repository permission model in the app and
the model connection in Claude Code are intentionally separate.

## Next implementation gate

Before adding file content, persistent credentials, tools, or any action-taking
flow to the consultation feature, add all of the following:

- an additional per-request context preview and opt-in for source/log content;
- a Windows Credential Manager-backed key provider;
- request history and usage/spend visibility in the app;
- an allowlisted tool/action protocol rather than raw terminal access;
- a documented compatibility test against the live Gateway;
- a user-visible disconnect/revoke path.

Do not substitute a scraped dashboard session, browser cookie, or a copied
credential helper for these controls.
