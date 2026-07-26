#!/usr/bin/env bash
# Native Linux build entrypoint, mirrors build_msvc.ps1's shape:
# 1. verify toolchain, 2. cmake configure, 3. cmake build.
#
# Usage: scripts/build.sh [--debug|--release|--share] [cmake args...]
# Passing a mode flag explicitly (scripts, CI) always skips straight to the
# build — same for non-interactive stdin (defaults to Release, Universal).
#
# Run with no mode flag on an interactive terminal and two prompts run in
# sequence — microarchitecture target, then build type:
#   Scene 1 — microarch target:
#     1) Universal (default) -- portable generic x86-64 baseline
#     2) Native               -- tuned to this exact CPU (-march=native)
#     3) Custom                -- enter any -march value (v2/v3/v4/znver4/...)
#     4) All                   -- build universal/v3/v4/zen4 in one pass
#   Scene 2 — build type:
#     1) Release (default)
#     2) Debug
#
# All (formerly --share): builds four binaries — Universal plus the v3/v4
# x86-64 microarch levels plus a Zen4-tuned build (see GIT_WRAPPER_ARCH_LEVEL
# in CMakeLists.txt). Release variants are packaged as tarballs under
# dist/linux/; a Debug + All combo (only reachable interactively) is left
# unpackaged in build/linux_share_debug/ — Debug output isn't the kind of
# thing you hand someone.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BUILD_TYPE=Release
SHARE=0
MODE_SET=0
ARCH_LEVEL=""
ARCH_SUFFIX=""
CMAKE_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --debug)   BUILD_TYPE=Debug; MODE_SET=1 ;;
        --release) BUILD_TYPE=Release; MODE_SET=1 ;;
        --share)   SHARE=1; MODE_SET=1 ;;
        *)         CMAKE_ARGS+=("$arg") ;;
    esac
done

if [ "$MODE_SET" -eq 0 ] && [ -t 0 ]; then
    echo "Select microarchitecture target:"
    echo "  1) Universal (default) -- portable generic x86-64 baseline"
    echo "  2) Native -- tuned to this exact CPU (-march=native)"
    echo "  3) Custom -- enter a specific -march value (v2/v3/v4/znver4/...)"
    echo "  4) All -- build universal/v3/v4/zen4 in one pass"
    read -r -p "Enter choice [1-4, default 1]: " arch_choice
    case "$arch_choice" in
        ""|1) ;;
        2) ARCH_LEVEL="native"; ARCH_SUFFIX="_native" ;;
        3)
            read -r -p "Enter -march value (e.g. v3, v4, znver4): " custom_level
            if [ -z "$custom_level" ]; then
                echo "error: no value entered" >&2
                exit 2
            fi
            ARCH_LEVEL="$custom_level"
            ARCH_SUFFIX="_custom-${custom_level}"
            ;;
        4) SHARE=1 ;;
        *) echo "error: invalid choice '$arch_choice'" >&2; exit 2 ;;
    esac

    echo "Select build type:"
    echo "  1) Release (default)"
    echo "  2) Debug"
    read -r -p "Enter choice [1-2, default 1]: " type_choice
    case "$type_choice" in
        ""|1) BUILD_TYPE=Release ;;
        2)     BUILD_TYPE=Debug ;;
        *)     echo "error: invalid choice '$type_choice'" >&2; exit 2 ;;
    esac
fi

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

if [ "$SHARE" -eq 1 ]; then
    declare -A share_variants=(
        [universal]=""
        [v3]="v3"
        [v4]="v4"
        [zen4]="znver4"
    )

    if [ "$BUILD_TYPE" = "Debug" ]; then
        share_root="$root/build/linux_share_debug"
    else
        share_root="$root/build/linux_share"
    fi
    dist_dir="$root/dist/linux"
    [ "$BUILD_TYPE" = "Release" ] && mkdir -p "$dist_dir"

    for variant in "${!share_variants[@]}"; do
        level="${share_variants[$variant]}"
        variant_dir="$share_root/$variant"
        arch_arg=()
        [ -n "$level" ] && arch_arg=(-DGIT_WRAPPER_ARCH_LEVEL="$level")

        echo
        echo "==> Configuring '$variant' ($BUILD_TYPE) -> $variant_dir..."
        cmake -S "$root" -B "$variant_dir" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
            "${arch_arg[@]}" "${CMAKE_ARGS[@]}"
        cmake --build "$variant_dir" -j"$(nproc)"

        if [ "$BUILD_TYPE" = "Release" ]; then
            pkg_name="git_wrapper-linux-$variant"
            pkg_dir="$dist_dir/$pkg_name"
            rm -rf "$pkg_dir"
            mkdir -p "$pkg_dir"
            cp "$variant_dir/git_wrapper" "$pkg_dir/"
            tar -C "$dist_dir" -czf "$dist_dir/$pkg_name.tar.gz" "$pkg_name"
            rm -rf "$pkg_dir"
            echo "==> Packaged $dist_dir/$pkg_name.tar.gz"
        else
            echo "==> Built $variant_dir/git_wrapper (Debug, not packaged)"
        fi
    done

    echo
    if [ "$BUILD_TYPE" = "Release" ]; then
        echo "All-variant build done. Tarballs in $dist_dir/:"
        for variant in "${!share_variants[@]}"; do
            echo "  $dist_dir/git_wrapper-linux-$variant.tar.gz"
        done
    else
        echo "All-variant build done (Debug, unpackaged). Binaries:"
        for variant in "${!share_variants[@]}"; do
            echo "  $share_root/$variant/git_wrapper"
        done
    fi
    exit 0
fi

if [ "$BUILD_TYPE" = "Debug" ]; then
    build_dir="$root/build/linux${ARCH_SUFFIX}_debug"
else
    build_dir="$root/build/linux${ARCH_SUFFIX}"
fi

arch_arg=()
[ -n "$ARCH_LEVEL" ] && arch_arg=(-DGIT_WRAPPER_ARCH_LEVEL="$ARCH_LEVEL")

echo "[2/3] Configuring ($BUILD_TYPE${ARCH_LEVEL:+, arch=$ARCH_LEVEL})..."
cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    "${arch_arg[@]}" "${CMAKE_ARGS[@]}"

echo "[3/3] Building..."
cmake --build "$build_dir" -j"$(nproc)"

echo
echo "Done: $build_dir/git_wrapper"
