# Verification & release playbook

> Split from `AGENTS.md`. Read this before committing and when bumping an
> Orca plugin version.

## Verification & Release Playbook

Run before every commit:
```bash
python3 tools/build_windows.py
python3 tools/check_all.py
```

When bumping an Orca plugin version:
1. Prepend `## <version> — YYYY-MM-DD` (starting with `*` bullets) to `plugins/<id>/CHANGELOG.md` and root `CHANGELOG.md`.
2. Run:
   ```bash
   python3 tools/bump_version.py <plugin-id> <version>
   python3 tools/sync_engine.py
   python3 tools/sync_changelog.py
   python3 tools/dump_default_config.py
   python3 tools/check_all.py
   ```

## Released marker (update-nag protocol)

`.agent/RELEASED` is the release record. Two lines: `commit=<sha>` naming the
commit that holds FINISHED work, and `hub=<version>` matching the HUB_VERSION
that ships with it. The hub and the lighting tray read ONLY this file from the
branch tip; every other probe and every download resolves the commit it
blesses, so half-finished work is invisible and the owner is nudged once per
task, not once per save.

Agent obligations, every turn:
1. Commit the work first. Read its full sha.
2. In a SECOND commit, set `.agent/RELEASED` to `commit=<that sha>` and
   `hub=<current HUB_VERSION>`. Never point the marker at a commit that is not
   on the branch and fully verified.
3. Push once, after the marker commit. A mid-turn push is allowed ONLY as
   crash recovery after a sandbox reset, and counts as one accepted nag.
4. Never push immediately before an ask_user pause: the pause is not the end
   of the task, and the marker must not move before the work is done.
5. Bump HUB_VERSION and HUB_VERSION_W together when the hub itself changes,
   and put the new version in the marker's `hub=` line in the same turn.

The test `test_hub_and_tray_only_consume_released_work` fails if the marker's
`hub=` line disagrees with the source HUB_VERSION, so a forgotten bump cannot
ship silently.
