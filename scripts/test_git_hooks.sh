#!/usr/bin/env bash
# Zinsser-style checks for the pre-push startup hook (Task 8.12.8). Uses a stub benchmark command.
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FAILURES=0

it() {
    local sentence="$1"; shift
    if "$@" >/dev/null 2>&1; then echo "  [it] $sentence: ok"; else echo "  [it] $sentence: FAILED"; FAILURES=$((FAILURES + 1)); fi
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
git -C "$WORK" init -q
mkdir -p "$WORK/scripts/git-hooks"
cp "$QT_DIR/scripts/git-hooks/pre-push" "$WORK/scripts/git-hooks/pre-push"

hook_blocks_on_failure() { ! (cd "$WORK" && MARKD_PRE_PUSH_BENCHMARK_CMD="exit 1" ./scripts/git-hooks/pre-push); }
hook_allows_with_warnings() { (cd "$WORK" && MARKD_PRE_PUSH_BENCHMARK_CMD="echo 'WARNING: drift'; exit 0" ./scripts/git-hooks/pre-push); }
install_sets_hooks_path() {
    bash "$QT_DIR/scripts/install-git-hooks.sh" "$WORK" &&
    [[ "$(git -C "$WORK" config core.hooksPath)" == "scripts/git-hooks" ]]
}
index_marks_hook_executable() { [[ "$(git -C "$QT_DIR" ls-files -s scripts/git-hooks/pre-push | cut -d' ' -f1)" == "100755" ]]; }

echo "--- Shell suite: pre-push startup check ---"
it "blocks the push when the benchmark fails" hook_blocks_on_failure
it "allows the push when the benchmark passes with drift warnings" hook_allows_with_warnings
it "installs through task install-hooks into core.hooksPath" install_sets_hooks_path
it "carries the executable bit in the git index (mode 100755)" index_marks_hook_executable

if (( FAILURES > 0 )); then echo "FAILED: $FAILURES pre-push hook check(s)"; exit 1; fi
echo "PASS: pre-push startup check"
