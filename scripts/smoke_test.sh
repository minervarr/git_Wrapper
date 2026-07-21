#!/usr/bin/env bash
# Linux counterpart to smoke_test.ps1: build, then verify `git_wrapper help`
# and `git_wrapper version` both exit 0.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="$root/build/linux/git_wrapper"

echo "[1/2] Building..."
if ! "$root/scripts/build.sh"; then
    echo "FAIL: build failed"
    exit 1
fi

echo "[2/2] Running smoke test (git_wrapper help / version)..."
if "$exe" help > /dev/null && "$exe" version > /dev/null; then
    echo "PASS: git_wrapper help/version exited 0"
    exit 0
else
    echo "FAIL: git_wrapper help/version exited non-zero"
    exit 1
fi
