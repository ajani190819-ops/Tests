#!/usr/bin/env bash
# Turn-start git sync for the Arena session branch - SAFE by design.
#
# Why this exists: the Arena sandbox rebuilds `.git` between turns from the
# branch base while KEEPING the working-tree files. Commit objects from the
# previous turn can vanish, and new commits can land on a stale base (the
# push then rejects as non-fast-forward). Working files always survive;
# this script only repairs the git bookkeeping and never loses content:
#   - uncommitted edits are never touched,
#   - good unpushed commits are reported and left alone,
#   - stale-base commits are re-surfaced as ordinary diffs to re-commit.
#
# It also installs the `.githooks/` commit guards (a rebuild wipes
# `.git/hooks/`, so every turn start puts them back).
#
# Usage:  bash tools/sync_branch.sh [branch]
# Default branch: the CURRENT branch, which on an Arena session checkout is
# the `arena/<id>-tests` branch this session is tied to. It is read from git
# rather than hardcoded: a hardcoded default silently synced (and could
# reset) the wrong branch as soon as a new session started, because every
# session gets a fresh `arena/<id>-tests` name.
set -euo pipefail
cd "$(dirname "$0")/.."

CURRENT="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")"
BRANCH="${1:-$CURRENT}"

if [ -z "$BRANCH" ] || [ "$BRANCH" = "HEAD" ]; then
  echo "cannot determine the current branch (detached HEAD?) - pass one explicitly:" >&2
  echo "  bash tools/sync_branch.sh arena/<id>-tests" >&2
  exit 1
fi

case "$BRANCH" in
  arena/*-tests) ;;
  *) echo "refusing to sync non-session branch '$BRANCH'" >&2; exit 1 ;;
esac

# Keep the commit guards installed (the sandbox rebuild wipes .git/hooks).
if [ -d .githooks ]; then
  for h in .githooks/*; do
    [ -f "$h" ] || continue
    cp "$h" ".git/hooks/$(basename "$h")"
    chmod +x ".git/hooks/$(basename "$h")"
  done
  echo "installed commit guards: $(ls .githooks | tr '\n' ' ')"
fi

# Bootstrap: a brand-new session branch has no remote counterpart yet, and the
# pre-commit guard refuses to commit while `origin/$BRANCH` is unknown. Creating
# the remote branch here is what lets the first commit of a session through.
if ! git ls-remote --exit-code --heads origin "$BRANCH" >/dev/null 2>&1; then
  echo "origin/$BRANCH does not exist yet - new session branch, publishing it."
  git push -u origin "$BRANCH"
  echo "clean: origin/$BRANCH created at $(git rev-parse --short HEAD)."
  exit 0
fi

git fetch origin "$BRANCH"

if [ "$(git rev-parse HEAD)" = "$(git rev-parse "origin/$BRANCH")" ]; then
  echo "clean: HEAD == origin/$BRANCH - nothing to do."
  exit 0
fi

if git merge-base --is-ancestor "origin/$BRANCH" HEAD 2>/dev/null; then
  n=$(git rev-list --count "origin/$BRANCH..HEAD")
  echo "HEAD is $n commit(s) ahead of origin/$BRANCH on a good base - NOT resetting."
  echo "push them when ready:  git push origin $BRANCH"
  git log --oneline "origin/$BRANCH..HEAD"
  exit 0
fi

if git merge-base --is-ancestor HEAD "origin/$BRANCH" 2>/dev/null; then
  echo "behind origin/$BRANCH with no local commits - fast-forwarding the branch pointer."
  git reset --mixed "origin/$BRANCH"
  echo "--- working-tree changes vs remote: ---"
  git status --porcelain | head -40
  exit 0
fi

# Diverged: the rebuilt-sandbox case - local commits sit on a stale base.
n=$(git rev-list --count "origin/$BRANCH..HEAD" 2>/dev/null || echo 0)
echo "stale base: $n local commit(s) are not on origin/$BRANCH (sandbox rebuild?)."
echo "resetting the branch pointer to origin/$BRANCH (mixed - working files untouched)."
echo "those commits' content reappears below as ordinary diffs - re-commit and push."
git reset --mixed "origin/$BRANCH"
echo "--- working-tree changes vs remote: ---"
git status --porcelain | head -40
