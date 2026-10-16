#!/usr/bin/env bash
# Merge the latest upstream Hyprland into the hyprstack branch.
#
#   scripts/hyprstack/sync-upstream.sh            # fetch + update main mirror + merge into hyprstack
#   scripts/hyprstack/sync-upstream.sh --no-push  # don't push main
#
# Conflicts are expected only around the hooks listed in docs/hyprstack/DESIGN.md
# ("Haki w plikach upstreamu"). After resolving them: build, then run
#   STACK_ONLY=1 scripts/e2e/run-hyprtester
# and the upstream groups (compare with a build of upstream, the VM is flaky for some tests).

set -euo pipefail

PUSH=1
[[ "${1:-}" == "--no-push" ]] && PUSH=0

cd "$(git rev-parse --show-toplevel)"

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "working tree not clean" >&2
    exit 1
fi

git remote get-url upstream >/dev/null 2>&1 || git remote add upstream https://github.com/hyprwm/Hyprland.git
git fetch upstream --quiet

# main mirrors upstream
git branch -f main upstream/main
[[ "$PUSH" == "1" ]] && git push --quiet origin main

git checkout --quiet hyprstack
if ! git merge --no-edit upstream/main; then
    echo
    echo "Merge conflicts — resolve them (see the hooks table in docs/hyprstack/DESIGN.md), then:"
    echo "  git add -u && git commit"
    exit 1
fi

git submodule update --init --recursive --quiet
echo "merged $(git rev-parse --short upstream/main) into hyprstack"
