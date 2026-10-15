#!/usr/bin/env bash
# Linux entry point for the build tooling: a thin shim, so `scripts/build.sh`
# keeps working for muscle memory and manifest.json. It prefers uv (which can
# fetch its own Python), then python3. The real work lives in
# git_wrapper_builder.pyz, built by build_pyz.py if needed.
#
# Everything after this script's name is passed straight to the builder, e.g.
#   ./scripts/build.sh --debug
#   ./scripts/build.sh package
#   ./scripts/build.sh build -- --GIT_WRAPPER_ARCH_LEVEL=znver4
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if command -v uv >/dev/null 2>&1; then
    exec uv run python build_pyz.py --run "$@"
fi
if command -v python3 >/dev/null 2>&1; then
    exec python3 build_pyz.py --run "$@"
fi

echo "error: neither uv nor python3 is on PATH" >&2
echo "       install uv: https://docs.astral.sh/uv/   (or Python 3.12+)" >&2
exit 1
