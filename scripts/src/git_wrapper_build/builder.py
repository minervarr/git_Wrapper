"""cmake configure + build: the only place that spells out cmake commands."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from pathlib import Path

from . import paths
from .process import run_checked
from .toolchain import Toolchain
from .ui import step
from .variants import ARCH_LEVEL_FLAG


def configure_command(
    root: Path,
    build_dir: Path,
    *,
    toolchain: Toolchain,
    target: str,
    build_type: str,
    arch: str = "",
    extra: Sequence[str] = (),
) -> list[str]:
    """The exact ``cmake`` configure line.

    Windows pins the generator's toolchain explicitly (``cl`` + the resolved
    ninja) because vcvars alone does not tell CMake which compiler to pick;
    Linux lets CMake find ``g++``/``clang++`` itself.
    """
    cmd = [toolchain.cmake, "-S", str(root), "-B", str(build_dir), "-G", "Ninja"]
    if target == "windows":
        cmd += [
            f"-DCMAKE_MAKE_PROGRAM={toolchain.ninja}",
            "-DCMAKE_C_COMPILER=cl",
            "-DCMAKE_CXX_COMPILER=cl",
        ]
    cmd.append(f"-DCMAKE_BUILD_TYPE={build_type}")
    if arch:
        cmd.append(f"-D{ARCH_LEVEL_FLAG}={arch}")
    cmd += [str(arg) for arg in extra]
    return cmd


def build_command(toolchain: Toolchain, build_dir: Path, jobs: int) -> list[str]:
    """``cmake --build`` for an already-configured directory."""
    return [toolchain.cmake, "--build", str(build_dir), "-j", str(jobs)]


def configure(
    root: Path,
    build_dir: Path,
    *,
    toolchain: Toolchain,
    target: str,
    build_type: str,
    arch: str = "",
    extra: Sequence[str] = (),
    env: Mapping[str, str] | None = None,
    dry_run: bool = False,
    step_label: str = "[2/3]",
) -> None:
    """Run the configure step, echoing the command first."""
    where = paths.display(root, build_dir)
    details = build_type + (f", arch={arch}" if arch else "")
    step(f"{step_label} Configuring ({details}) -> {where}")
    run_checked(
        configure_command(
            root,
            build_dir,
            toolchain=toolchain,
            target=target,
            build_type=build_type,
            arch=arch,
            extra=extra,
        ),
        what="cmake configure",
        env=env,
        dry_run=dry_run,
    )


def compile(
    build_dir: Path,
    *,
    toolchain: Toolchain,
    jobs: int,
    env: Mapping[str, str] | None = None,
    dry_run: bool = False,
    step_label: str = "[3/3]",
) -> None:
    """Run the build step, echoing the command first."""
    step(f"{step_label} Building ({jobs} jobs)...")
    run_checked(
        build_command(toolchain, build_dir, jobs),
        what="cmake build",
        env=env,
        dry_run=dry_run,
    )


def build_variant(
    root: Path,
    build_dir: Path,
    *,
    toolchain: Toolchain,
    target: str,
    build_type: str,
    jobs: int,
    arch: str = "",
    extra: Sequence[str] = (),
    env: Mapping[str, str] | None = None,
    dry_run: bool = False,
) -> Path:
    """Configure and build one variant; return where the binary should be."""
    configure(
        root,
        build_dir,
        toolchain=toolchain,
        target=target,
        build_type=build_type,
        arch=arch,
        extra=extra,
        env=env,
        dry_run=dry_run,
    )
    compile(build_dir, toolchain=toolchain, jobs=jobs, env=env, dry_run=dry_run)
    return paths.binary_path(build_dir, target)
