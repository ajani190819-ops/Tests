#!/usr/bin/env bash
# Turn-start git sync for the Arena session branch.
#
# Why this exists: the Arena sandbox rebuilds `.git` between turns from the
# branch base while KEEPING the working-tree files. Commit objects made in a
# previous turn can vanish, and mid-turn commits can land on a stale base
# (push then rejects as non-fast-forward). This script re-points the local
# branch at the remote tip WITHOUT touching working files, so:
#   - uncommitted edits survive (they show up as normal diffs),
#   - history is continuous (fast-forward pushes, never force).
#
# Usage:  bash tools/sync_branch.sh [branch]
# Default branch: arena/e895b94f-tests (the current Arena session branch).
set -euo pipefail
BRANCH="${1:-arena/e895b94f-tests}"
cd "$(dirname "$0")/.."

git fetch origin "$BRANCH"
# --mixed keeps the working tree; only the branch pointer and index move.
git reset --mixed "origin/$BRANCH"
echo "--- synced to origin/$BRANCH; working-tree changes vs remote: ---"
git status --porcelain | head -40
