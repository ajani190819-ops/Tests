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
