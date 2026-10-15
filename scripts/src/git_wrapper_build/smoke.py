"""Smoke test: build the portable binary, then check it runs.

Replaces ``smoke_test.sh`` / ``smoke_test.ps1`` with the stricter of the two:
both ``help`` and ``version`` must exit 0.
"""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path

from . import builder, paths
from .errors import BuildError
from .process import run
from .toolchain import Toolchain
from .ui import ok, step


def run_smoke(
    root: Path,
    *,
    toolchain: Toolchain,
    target: str,
    build_type: str = "Release",
    jobs: int = 1,
    env: Mapping[str, str] | None = None,
    dry_run: bool = False,
) -> Path:
    """Build and assert ``git_wrapper help`` / ``git_wrapper version`` exit 0."""
    build_dir = paths.single_build_dir(root, target, debug=build_type == "Debug")
    step(f"[1/2] Building ({build_type})...")
    binary = builder.build_variant(
        root,
        build_dir,
        toolchain=toolchain,
        target=target,
        build_type=build_type,
        jobs=jobs,
        env=env,
        dry_run=dry_run,
    )

    step("[2/2] Running smoke test (git_wrapper help / version)...")
    for args in (["help"], ["version"]):
        result = run([str(binary), *args], env=env, dry_run=dry_run)
        if result.returncode != 0:
            raise BuildError(
                f"`{paths.display(root, binary)} {' '.join(args)}` exited {result.returncode}"
            )

    ok(f"PASS: {paths.display(root, binary)} help/version exited 0")
    return binary
