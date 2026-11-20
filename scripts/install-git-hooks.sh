#!/usr/bin/env bash
# Points a repository's core.hooksPath at the versioned hooks in scripts/git-hooks (Task 8.12.8).
set -euo pipefail
REPO_DIR="${1:-.}"
git -C "$REPO_DIR" config core.hooksPath scripts/git-hooks
echo "core.hooksPath -> scripts/git-hooks in $(git -C "$REPO_DIR" rev-parse --show-toplevel)"
