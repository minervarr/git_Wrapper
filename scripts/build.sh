#!/usr/bin/env bash
# Native Linux build entrypoint, mirrors build_msvc.ps1's shape:
# 1. verify toolchain, 2. cmake configure, 3. cmake build.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build/linux"

echo "[1/3] Checking toolchain..."
missing=()
command -v cmake >/dev/null 2>&1 || missing+=("cmake")
command -v ninja >/dev/null 2>&1 || missing+=("ninja")
if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    missing+=("g++ or clang++")
fi
if [ "${#missing[@]}" -ne 0 ]; then
    echo "error: missing required tool(s): ${missing[*]}" >&2
    exit 1
fi

echo "[2/3] Configuring..."
cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "[3/3] Building..."
cmake --build "$build_dir" -j"$(nproc)"

echo
echo "Done: $build_dir/git_wrapper"
