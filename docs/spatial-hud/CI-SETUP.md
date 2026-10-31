# GitHub Actions Workflow Setup

Both workflows now live in [`.github/workflows/`](../../.github/workflows/) and run
automatically:

| Workflow | Triggers | What it does |
| --- | --- | --- |
| [`ci.yml`](../../.github/workflows/ci.yml) | push to `main` / `arena/**`, every pull request | `tools/check_all.py` (Orca plugins, `Orca-Plugins.bat` contract, docs sync, code map) on Ubuntu, plus a Windows job that compiles the C# sources with the stock Framework64 `csc.exe` and parses every `.ps1` AST |
| [`build-spatial-hud.yml`](../../.github/workflows/build-spatial-hud.yml) | push to `main` / `arena/**` touching `spatial-hud-template-26.3/**`, every pull request touching it | Builds the Fabric mod with Gradle, then publishes the jar (see below) |

`tests/test_installer.py` fails if [`docs/ci/`](../ci/) and
[`.github/workflows/`](../../.github/workflows/) drift apart, so these copies cannot
silently diverge again — which is how the live workflow ended up missing `main`,
`pull_request`, and the concurrency group while `docs/ci/` had all three.

## How the Spatial HUD jar is published

Two release feeds, and the distinction matters:

* **`spatial-hud-latest`** — the rolling feed `ProjectHub.exe` and
  `Update-SpatialHUD.bat` install from. **Only updated from `main`.** A scratch
  branch can never overwrite what users install.
* **`spatial-hud-build-<branch>`** — one feed per branch, so test builds stay
  selectable from `Update-SpatialHUD.bat`'s build picker.

Every publish uploads the new jar **and then deletes every other asset on that
release**. That deletion is load-bearing, not cosmetic:
`gh release upload --clobber` overwrites same-named files but never removes old
ones, and the hub takes the **first** `browser_download_url` it is given. Assets
come back sorted by name, so a leftover `spatial-hud-1.0.0.jar` outranks
`spatial-hud-1.0.4.jar` forever and the hub would install the oldest build. This
used to be a manual `gh release delete-asset` step after every publish.

## If Arena cannot push a workflow change

GitHub restricts workflow-file writes to tokens holding the special `workflows`
permission, so an automated push to `.github/workflows/` can be rejected. If that
happens the change still lands in `docs/ci/` (which has no such restriction), and
applying it by hand takes about a minute:

1. Open the target file on GitHub — **Add file → Create new file** if it does not
   exist yet, or the pencil (**Edit**) icon if it does.
2. Type the path, e.g. `.github/workflows/ci.yml`.
3. Paste the exact contents of the matching file in [`docs/ci/`](../ci/).
4. **Commit changes** to `main`.
