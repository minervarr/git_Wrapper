"""Command line: parse arguments, then hand off to exactly one workflow.

Exit codes match the rest of the project: ``0`` success, ``1`` a step failed,
``2`` the command line was wrong.
"""

from __future__ import annotations

import argparse
import os
import sys
from collections.abc import Sequence

from . import builder, menu, msvc, package, paths, smoke, toolchain, ui
from .errors import BuildError, UsageError
from .prompts import Reader
from .variants import sanitize_arch

PROG = "git-wrapper-build"
COMMANDS = ("build", "all", "package", "smoke", "menu")
MENU_COMMANDS = ("", "menu")
MULTI_VARIANT = ("all", "package")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=PROG,
        description=(
            "Build, package and smoke-test git_wrapper through cmake + ninja. "
            "With no arguments and a terminal, a menu asks for the "
            "microarchitecture and build type; with no terminal it builds the "
            "portable Release binary."
        ),
    )
    parser.add_argument(
        "command",
        nargs="?",
        default=None,
        choices=COMMANDS,
        help="build one variant (default), all four, package them as tarballs, smoke-test, or the menu",
    )
    parser.add_argument(
        "--release",
        dest="build_type",
        action="store_const",
        const="Release",
        help="Release build (default)",
    )
    parser.add_argument(
        "--debug",
        dest="build_type",
        action="store_const",
        const="Debug",
        help="Debug build (built, never packaged)",
    )
    parser.add_argument(
        "--arch",
        default="",
        metavar="NAME",
        help="single-variant -march value: v2, v3, v4, znver4, native, ...",
    )
    parser.add_argument(
        "--ninja",
        default="",
        metavar="PATH",
        help="path to ninja (default: $GIT_WRAPPER_NINJA, then the standard paths)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the cmake commands and run nothing (works without a toolchain)",
    )
    parser.add_argument(
        "--version",
        action="store_true",
        help="print the git_wrapper version from manifest.json",
    )
    parser.set_defaults(build_type=None)
    return parser


def _passthrough(parser: argparse.ArgumentParser, extra: Sequence[str]) -> list[str]:
    """Extra arguments become extra cmake ``-D`` cache entries."""
    rest = list(extra)
    if rest[:1] == ["--"]:
        rest = rest[1:]
    for arg in rest:
        if not arg.startswith("-D"):
            parser.error(
                f"unrecognised argument: {arg} "
                "(extra cmake arguments must be written as -D<VAR>=<value>)"
            )
    return rest


def _version_or_unknown() -> str:
    try:
        return paths.version(paths.find_repo_root())
    except BuildError:
        return "unknown (run inside the git_wrapper checkout)"


def _resolve_command(
    args: argparse.Namespace, target: str, *, reader: Reader = input
) -> tuple[str, str, str]:
    """Return ``(command, arch, build_type)``, asking the menu when none was given."""
    command = args.command or ""
    build_type = args.build_type or "Release"
    arch = sanitize_arch(args.arch) if args.arch else ""

    if command in MENU_COMMANDS:
        if command == "menu" and not sys.stdin.isatty():
            raise UsageError(
                "`menu` needs a terminal: use build, all, package or smoke"
            )
        if not sys.stdin.isatty():
            command = "build"
            ui.note(
                "    no terminal: building the portable Release binary "
                "(see --help for the other modes)"
            )
        else:
            arch, build_everything = menu.choose_arch(
                supported=target == "linux", reader=reader
            )
            build_type = menu.choose_build_type(reader=reader)
            command = "all" if build_everything else "build"

    if arch and target != "linux":
        raise UsageError(
            "--arch is a GCC/Clang feature (MSVC ignores GIT_WRAPPER_ARCH_LEVEL); "
            "drop it on Windows"
        )
    if command in MULTI_VARIANT:
        if target != "linux":
            raise UsageError(
                f"`{command}` builds the four microarch variants, which are a "
                "GCC/Clang feature — it is Linux-only"
            )
        if arch:
            raise UsageError(f"`{command}` builds every variant: drop --arch")
    if command == "smoke" and arch:
        raise UsageError("`smoke` builds the portable binary: drop --arch")

    return command, arch, build_type


def _prepare_env(args: argparse.Namespace, target: str) -> dict[str, str]:
    """Import the MSVC environment on Windows (unless this is a dry run)."""
    env = dict(os.environ)
    if target != "windows":
        ui.step("[1/3] Checking toolchain...")
        return env
    if args.dry_run:
        ui.warn("dry run: skipping the vcvars64 import, so `cl` will look missing")
        ui.step("[1/3] Checking toolchain...")
        return env
    ui.step("[1/3] Setting up MSVC x64 environment...")
    return msvc.load(env=env).env


def run_command(args: argparse.Namespace, extra: Sequence[str]) -> int:
    """Execute one parsed command."""
    target = paths.current_target()
    root = paths.find_repo_root()
    command, arch, build_type = _resolve_command(args, target)
    debug = build_type == "Debug"

    env = _prepare_env(args, target)
    tools = toolchain.discover(
        ninja=args.ninja or None, env=env, target=target, dry_run=args.dry_run
    )
    jobs = toolchain.jobs(env)

    if command == "build":
        build_dir = paths.single_build_dir(root, target, arch=arch, debug=debug)
        binary = builder.build_variant(
            root,
            build_dir,
            toolchain=tools,
            target=target,
            build_type=build_type,
            jobs=jobs,
            arch=arch,
            extra=extra,
            env=env,
            dry_run=args.dry_run,
        )
        ui.ok(f"\nDone: {paths.display(root, binary)}")
        return 0

    if command == "smoke":
        smoke.run_smoke(
            root,
            toolchain=tools,
            target=target,
            build_type=build_type,
            jobs=jobs,
            env=env,
            dry_run=args.dry_run,
        )
        return 0

    packaged = command == "package"
    tarballs = package.build_all(
        root,
        toolchain=tools,
        target=target,
        build_type=build_type,
        jobs=jobs,
        env=env,
        extra=extra,
        dry_run=args.dry_run,
        packaged=packaged,
    )
    if not packaged:
        ui.note("\nAll-variant build done (unpackaged).")
    elif tarballs:
        ui.ok(
            f"\nAll-variant build done. Tarballs in {paths.display(root, paths.dist_dir(root))}:"
        )
        for tarball in tarballs:
            ui.note(f"  {paths.display(root, tarball)}")
    else:
        ui.note("\nAll-variant build done (Debug, not packaged).")
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args, extra = parser.parse_known_args(None if argv is None else list(argv))
    try:
        passthrough = _passthrough(parser, extra)
        if args.version:
            print(f"{PROG} {_version_or_unknown()}")
            return 0
        return run_command(args, passthrough)
    except UsageError as exc:
        ui.fail(str(exc))
        return 2
    except BuildError as exc:
        ui.fail(str(exc))
        return 1
    except KeyboardInterrupt:
        ui.fail("interrupted")
        return 130
